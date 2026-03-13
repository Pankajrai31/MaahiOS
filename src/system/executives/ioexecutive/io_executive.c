/**
 * MaahiOS I/O Executive Implementation
 * 
 * Description:
 *   Device input executive. Opens input devices (keyboard, future: mouse),
 *   and serves read requests from user-mode libraries via SHM queues.
 * 
 *   This executive is the ONLY process that calls SYS_DEV_READ on input
 *   devices. All apps/libraries go through libio → I/O Executive → kernel.
 * 
 *   Flow:
 *     App → libio → SHM queue → I/O Executive → SYS_DEV_READ → kernel
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "io_executive.h"
#include "../common/executive_queue.h"
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"

/*=============================================================================
 * CONVENIENCE WRAPPERS
 *===========================================================================*/

static inline void exe_yield(void) {
    syscall0(SYS_YIELD);
}

static inline int exe_shm_create(uint32_t size) {
    return syscall1(SYS_SHM_CREATE, (int)size);
}

static inline void* exe_shm_attach(int shm_id) {
    return (void*)syscall2(SYS_SHM_ATTACH, shm_id, 0);
}

static inline int exe_dev_open(int device_id, int flags) {
    return syscall2(SYS_DEV_OPEN, device_id, flags);
}

static inline int exe_dev_read(int device_id, void *buffer, int size) {
    return syscall3(SYS_DEV_READ, device_id, (int)(uint32_t)buffer, size);
}

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t  *g_ecb = NULL;
static exec_request_queue_t  *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id  = -1;
static int g_resp_queue_shm_id = -1;

/* Device handles */
static int g_kbd_handle = -1;
static int g_mouse_handle = -1;

/* Keyboard ring buffer (shared with libio consumers) */
static io_kbd_ring_t *g_kbd_ring = NULL;
static int g_kbd_ring_shm_id = -1;

/* Mouse state slot (shared with libio consumers) */
static io_mouse_state_t *g_mouse_state = NULL;
static int g_mouse_state_shm_id = -1;

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_io_handle_ping(const exec_request_t *req,
                               exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

static void exe_io_handle_dev_read(const exec_request_t *req,
                                   exec_response_t *resp) {
    io_dev_read_req_t *rreq = (io_dev_read_req_t *)req->payload;
    io_dev_read_resp_t *rresp = (io_dev_read_resp_t *)resp->payload;

    /* Clamp read size to response payload capacity */
    uint32_t max = rreq->max_size;
    if (max > sizeof(rresp->data)) max = sizeof(rresp->data);

    /* Call kernel device manager on behalf of the client */
    int result = exe_dev_read(rreq->device_id, rresp->data, (int)max);

    rresp->bytes_read = (result > 0) ? (uint32_t)result : 0;
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = result;   /* Same semantics as SYS_DEV_READ return */
    resp->payload_size = sizeof(io_dev_read_resp_t);
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_io_dispatch(const exec_request_t *req,
                            exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));

    switch (req->func_id) {
        case EXEC_OP_PING:
            exe_io_handle_ping(req, resp);
            break;

        case IO_OP_DEV_READ:
            exe_io_handle_dev_read(req, resp);
            break;

        default:
            resp->msg_id = req->msg_id;
            resp->status = EXEC_ERR_INVALID;
            resp->payload_size = 0;
            break;
    }

    if (resp->status == EXEC_OK) {
        EXEC_STAT_SUCCESS(g_ecb);
    } else {
        EXEC_STAT_FAILURE(g_ecb);
    }
}

/*=============================================================================
 * KEYBOARD RING BUFFER POLLING
 *===========================================================================*/

/**
 * Poll keyboard device and write any events to the shared ring buffer.
 * Called from the main loop on every iteration. Lock-free SPSC:
 * only this executive writes head, only libio consumers write tail.
 */
static void exe_io_poll_keyboard(void) {
    if (!g_kbd_ring) return;

    uint8_t buf[IO_KBD_RING_ENTRY_SIZE];
    int r = exe_dev_read(IO_DEV_KEYBOARD, buf, sizeof(buf));
    if (r > 0) {
        uint32_t h = g_kbd_ring->head;
        uint32_t next_h = (h + 1) % g_kbd_ring->capacity;
        if (next_h != g_kbd_ring->tail) {  /* Not full */
            uint8_t *slot = &g_kbd_ring->data[h * g_kbd_ring->entry_size];
            for (int i = 0; i < r && i < IO_KBD_RING_ENTRY_SIZE; i++)
                slot[i] = buf[i];
            /* Compiler barrier — ensure data written before head update */
            __asm__ volatile("" ::: "memory");
            g_kbd_ring->head = next_h;
        }
    }
}

/**
 * Poll mouse device and write latest state to shared memory slot.
 * Called from the main loop on every iteration. Single-value overwrite:
 * only this executive writes, libio consumers read.
 */
static void exe_io_poll_mouse(void) {
    if (!g_mouse_state) return;

    /* Read raw mouse state from kernel */
    uint8_t buf[12];  /* Enough for mouse_state_t (int x, int y, uint8_t buttons) */
    int r = exe_dev_read(IO_DEV_MOUSE, buf, sizeof(buf));
    if (r > 0) {
        /* Copy fields individually to avoid struct alignment issues */
        int *ibuf = (int *)buf;
        g_mouse_state->x       = ibuf[0];
        g_mouse_state->y       = ibuf[1];
        g_mouse_state->buttons = buf[8];
        /* Compiler barrier + increment seq so readers see consistent state */
        __asm__ volatile("" ::: "memory");
        g_mouse_state->seq++;
    }
}

/*=============================================================================
 * INITIALIZATION & MAIN LOOP
 *===========================================================================*/

static int exe_io_init(void) {
    liblog(LOG_INFO, "IOEXEC", "I/O Executive initializing...");

    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to create request queue SHM");
        return -1;
    }

    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "IOEXEC", "Request queue SHM ID:", g_req_queue_shm_id);

    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to create response queue SHM");
        return -1;
    }

    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "IOEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);

    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "io_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_IO;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;

    /* Write queue SHM IDs to cells for discovery */
    libcell_write("system.exec.io.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.io.resp_shm", &g_resp_queue_shm_id, sizeof(int));

    /* Open keyboard device */
    g_kbd_handle = exe_dev_open(IO_DEV_KEYBOARD, 0);
    if (g_kbd_handle < 0) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to open keyboard device");
        return -1;
    }
    liblog_hex(LOG_INFO, "IOEXEC", "Keyboard device opened, handle:", 
               (uint32_t)g_kbd_handle);

    /* Open mouse device */
    g_mouse_handle = exe_dev_open(IO_DEV_MOUSE, 0);
    if (g_mouse_handle < 0) {
        liblog(LOG_WARN, "IOEXEC", "Mouse device not available (non-fatal)");
    } else {
        liblog_hex(LOG_INFO, "IOEXEC", "Mouse device opened, handle:",
                   (uint32_t)g_mouse_handle);
    }

    /* Create keyboard ring buffer in shared memory.
     * libio consumers read directly from this buffer — no IPC round trip. */
    g_kbd_ring_shm_id = exe_shm_create(sizeof(io_kbd_ring_t));
    if (g_kbd_ring_shm_id < 0) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to create keyboard ring SHM");
        return -1;
    }
    g_kbd_ring = (io_kbd_ring_t *)exe_shm_attach(g_kbd_ring_shm_id);
    if (!g_kbd_ring) {
        liblog(LOG_ERROR, "IOEXEC", "Failed to attach keyboard ring SHM");
        return -1;
    }
    g_kbd_ring->head = 0;
    g_kbd_ring->tail = 0;
    g_kbd_ring->capacity = IO_KBD_RING_CAPACITY;
    g_kbd_ring->entry_size = IO_KBD_RING_ENTRY_SIZE;
    libcell_write("system.io.keyboard.ring_shm",
                  &g_kbd_ring_shm_id, sizeof(int));
    liblog_hex(LOG_INFO, "IOEXEC", "Keyboard ring buffer SHM ID:",
               (uint32_t)g_kbd_ring_shm_id);

    /* Create mouse state slot in shared memory.
     * Single overwrite slot — IO Executive writes latest mouse state,
     * libio consumers read directly. Zero IPC, zero syscalls. */
    if (g_mouse_handle >= 0) {
        g_mouse_state_shm_id = exe_shm_create(4096); /* Page-aligned */
        if (g_mouse_state_shm_id >= 0) {
            g_mouse_state = (io_mouse_state_t *)exe_shm_attach(g_mouse_state_shm_id);
            if (g_mouse_state) {
                g_mouse_state->x       = 0;
                g_mouse_state->y       = 0;
                g_mouse_state->buttons = 0;
                g_mouse_state->seq     = 0;
                libcell_write("system.io.mouse.state_shm",
                              &g_mouse_state_shm_id, sizeof(int));
                liblog_hex(LOG_INFO, "IOEXEC", "Mouse state SHM ID:",
                           (uint32_t)g_mouse_state_shm_id);
            }
        }
    }

    liblog(LOG_INFO, "IOEXEC", "I/O Executive initialized successfully");
    return 0;
}

void exe_io_main(void) {
    if (exe_io_init() != 0) {
        liblog(LOG_ERROR, "IOEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "IOEXEC", "Entering main loop...");

    exec_request_t req;
    exec_response_t resp;

    while (!EXEC_SHOULD_STOP(g_ecb)) {
        /* Process any pending SHM requests (generic device access) */
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_io_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        }

        /* Poll keyboard and buffer events for libio consumers */
        exe_io_poll_keyboard();

        /* Poll mouse and write latest state for libio consumers */
        exe_io_poll_mouse();

        exe_yield();
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "IOEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
