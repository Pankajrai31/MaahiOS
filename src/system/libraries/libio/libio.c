/**
 * MaahiOS I/O Library (libio) - Implementation
 * 
 * Description:
 *   User-space library for device input via the I/O Executive.
 * 
 *   Keyboard uses a fast path: the I/O Executive continuously polls
 *   keyboard events and writes them to a shared ring buffer. libio
 *   reads directly from this buffer — zero syscalls, zero IPC latency.
 * 
 *   Other devices use the standard SHM request/response round-trip.
 *   Falls back to direct SYS_DEV_READ if I/O Executive not running.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libio.h"
#include "../core/syscall_helpers.h"
#include "../../executives/common/executive_queue.h"

/*=============================================================================
 * LIBRARY STATE — SHM REQUEST/RESPONSE (generic device path)
 *===========================================================================*/

static exec_request_queue_t  *g_req_queue  = (void*)0;
static exec_response_queue_t *g_resp_queue = (void*)0;
static uint32_t g_msg_id      = 1;
static int      g_initialized = 0;
static uint32_t g_my_pid      = 0;

/*=============================================================================
 * LIBRARY STATE — KEYBOARD RING BUFFER (fast path)
 *===========================================================================*/

static io_kbd_ring_t *g_kbd_ring = (void*)0;

/*=============================================================================
 * INTERNAL: KEYBOARD RING BUFFER INIT
 *===========================================================================*/

/**
 * Discover and attach to the I/O Executive's keyboard ring buffer.
 * Called on every kbd read until successful (no "tried" flag — retries
 * are cheap since it's just one cell read syscall per attempt).
 */
static void _libio_init_kbd_ring(void) {
    if (g_kbd_ring) return;

    int ring_shm_id = -1;
    int result = syscall3(SYS_CELL_READ,
                          (uint32_t)"system.io.keyboard.ring_shm",
                          (uint32_t)&ring_shm_id, sizeof(int));
    if (result < 0 || ring_shm_id < 0) return;

    io_kbd_ring_t *ring = (io_kbd_ring_t *)syscall2(SYS_SHM_ATTACH,
                                                     ring_shm_id, 0);
    if (!ring || (uint32_t)ring == 0xFFFFFFFF) return;

    g_kbd_ring = ring;
}

/*=============================================================================
 * INTERNAL: SHM REQUEST/RESPONSE INIT (for non-keyboard devices)
 *===========================================================================*/

static int _libio_try_init(void) {
    if (g_initialized) return 0;

    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }

    int req_shm_id = -1;
    int result = syscall3(SYS_CELL_READ,
                          (uint32_t)"system.exec.io.req_shm",
                          (uint32_t)&req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) return -1;

    int resp_shm_id = -1;
    result = syscall3(SYS_CELL_READ,
                      (uint32_t)"system.exec.io.resp_shm",
                      (uint32_t)&resp_shm_id, sizeof(int));
    if (result < 0 || resp_shm_id < 0) return -1;

    g_req_queue = (exec_request_queue_t *)syscall2(SYS_SHM_ATTACH,
                                                    req_shm_id, 0);
    if (!g_req_queue || (uint32_t)g_req_queue == 0xFFFFFFFF) {
        g_req_queue = (void*)0;
        return -1;
    }

    g_resp_queue = (exec_response_queue_t *)syscall2(SYS_SHM_ATTACH,
                                                      resp_shm_id, 0);
    if (!g_resp_queue || (uint32_t)g_resp_queue == 0xFFFFFFFF) {
        g_resp_queue = (void*)0;
        return -1;
    }

    g_initialized = 1;
    return 0;
}

/**
 * Send request to I/O Executive and wait for response.
 * Used for non-keyboard devices that need a round-trip.
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;

    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = EXEC_ID_IO;

    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;

    for (int i = 0; i < 100; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue,
                                                       my_id, resp);
        if (pop_result == EXEC_OK) return EXEC_OK;
        syscall1(SYS_SLEEP, 1);
    }

    return EXEC_ERR_TIMEOUT;
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

int libio_dev_read(uint32_t device_id, void *buffer, uint32_t max_size) {
    /*
     * Keyboard fast path: read directly from shared ring buffer.
     * Zero syscalls, zero IPC — just a memory read from shared memory.
     * The I/O Executive continuously polls keyboard and writes events here.
     */
    if (device_id == IO_DEV_KEYBOARD) {
        if (!g_kbd_ring) _libio_init_kbd_ring();

        if (g_kbd_ring) {
            uint32_t t = g_kbd_ring->tail;
            uint32_t h = g_kbd_ring->head;
            if (t == h) return 0;  /* Ring empty — no events */

            uint8_t *slot = &g_kbd_ring->data[t * g_kbd_ring->entry_size];
            uint32_t copy_size = max_size;
            if (copy_size > g_kbd_ring->entry_size)
                copy_size = g_kbd_ring->entry_size;
            uint8_t *dst = (uint8_t *)buffer;
            for (uint32_t i = 0; i < copy_size; i++)
                dst[i] = slot[i];
            /* Compiler barrier — ensure data read before tail update */
            __asm__ volatile("" ::: "memory");
            g_kbd_ring->tail = (t + 1) % g_kbd_ring->capacity;
            return (int)copy_size;
        }

        /* Fallback: direct kernel syscall */
        return syscall3(SYS_DEV_READ, (int)device_id,
                        (int)(uint32_t)buffer, (int)max_size);
    }

    /*
     * Generic device path: SHM request/response round-trip.
     * Used for devices that don't have a ring buffer fast path.
     */
    if (!g_initialized) _libio_try_init();

    if (g_initialized) {
        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));

        req.func_id = IO_OP_DEV_READ;
        io_dev_read_req_t *payload = (io_dev_read_req_t *)req.payload;
        payload->device_id = device_id;
        payload->max_size  = max_size;
        req.payload_size = sizeof(io_dev_read_req_t);

        int result = _send_and_wait(&req, &resp);
        if (result != EXEC_OK) return result;

        if (resp.result > 0) {
            io_dev_read_resp_t *rresp = (io_dev_read_resp_t *)resp.payload;
            uint32_t copy_size = rresp->bytes_read;
            if (copy_size > max_size) copy_size = max_size;
            const uint8_t *src = rresp->data;
            uint8_t *dst = (uint8_t *)buffer;
            for (uint32_t i = 0; i < copy_size; i++) {
                dst[i] = src[i];
            }
        }

        return resp.result;
    }

    /* Fallback: direct kernel syscall */
    return syscall3(SYS_DEV_READ, (int)device_id, (int)(uint32_t)buffer,
                    (int)max_size);
}
