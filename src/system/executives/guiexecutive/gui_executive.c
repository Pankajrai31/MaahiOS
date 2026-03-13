/**
 * MaahiOS GUI Executive Implementation
 * 
 * Description:
 *   Display and framebuffer executive. Opens the display device,
 *   retrieves framebuffer address and screen dimensions, publishes
 *   them to the cell registry for libgui discovery.
 * 
 *   Future: window management, compositing, z-order, double buffering.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "gui_executive.h"
#include "../common/executive_queue.h"
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"

/*=============================================================================
 * CONVENIENCE WRAPPERS (thin layer over syscall_helpers.h)
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

/*=============================================================================
 * DEVICE MANAGER SYSCALL WRAPPERS
 *===========================================================================*/

static inline int exe_dev_open(int device_id, int flags) {
    return syscall2(SYS_DEV_OPEN, device_id, flags);
}

static inline int exe_dev_ioctl(int device_id, int cmd, void *arg) {
    return syscall3(SYS_DEV_IOCTL, device_id, cmd, (int)arg);
}

/*=============================================================================
 * DEVICE MANAGER CONSTANTS (must match device_manager.h / display.h)
 *===========================================================================*/

#define DEV_DISPLAY             3
#define DISPLAY_IOCTL_GET_FB    3
#define DISPLAY_IOCTL_GET_INFO  1

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t  *g_ecb = NULL;
static exec_request_queue_t  *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id  = -1;
static int g_resp_queue_shm_id = -1;

/* Cached display info */
static gui_display_info_t g_display_info;
static int g_dev_handle = -1;

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_gui_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

static void exe_gui_handle_get_display_info(const exec_request_t *req,
                                             exec_response_t *resp) {
    gui_display_info_resp_t *payload = (gui_display_info_resp_t *)resp->payload;
    payload->info = g_display_info;

    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = g_display_info.framebuffer;
    resp->payload_size = sizeof(gui_display_info_resp_t);
}

/*=============================================================================
 * REQUEST DISPATCHER
 *===========================================================================*/

static void exe_gui_dispatch(const exec_request_t *req, exec_response_t *resp) {
    /* Clear response */
    exe_memset(resp, 0, sizeof(exec_response_t));

    switch (req->func_id) {
        case EXEC_OP_PING:
            exe_gui_handle_ping(req, resp);
            break;

        case GUI_OP_GET_DISPLAY_INFO:
            exe_gui_handle_get_display_info(req, resp);
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
 * INITIALIZATION & MAIN LOOP
 *===========================================================================*/

static int exe_gui_init(void) {
    liblog(LOG_INFO, "GUIEXEC", "GUI Executive initializing...");

    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "GUIEXEC", "Failed to create request queue SHM");
        return -1;
    }

    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "GUIEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "GUIEXEC", "Request queue SHM ID:", g_req_queue_shm_id);

    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "GUIEXEC", "Failed to create response queue SHM");
        return -1;
    }

    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "GUIEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "GUIEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);

    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "gui_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_GUI;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;

    /* Write queue SHM IDs to cells for discovery */
    libcell_write("system.exec.gui.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.gui.resp_shm", &g_resp_queue_shm_id, sizeof(int));

    /* Open display device */
    g_dev_handle = exe_dev_open(DEV_DISPLAY, 0);
    if (g_dev_handle < 0) {
        liblog(LOG_ERROR, "GUIEXEC", "Failed to open display device");
        return -1;
    }
    liblog_hex(LOG_INFO, "GUIEXEC", "DEV_DISPLAY open handle:", (uint32_t)g_dev_handle);

    /* Get framebuffer address */
    uint32_t fb_addr = (uint32_t)exe_dev_ioctl(DEV_DISPLAY, DISPLAY_IOCTL_GET_FB, 0);
    if (fb_addr == 0 || fb_addr == 0xFFFFFFFF) {
        liblog(LOG_ERROR, "GUIEXEC", "Failed to get framebuffer address");
        return -1;
    }
    liblog_hex(LOG_INFO, "GUIEXEC", "Framebuffer at:", fb_addr);

    /* Query actual display dimensions from device driver */
    /* display_info_t layout: u16 width, u16 height, u16 bpp, u32 fb_addr, u32 fb_size, u32 driver */
    struct { uint16_t w, h, bpp; uint32_t fb, fbsz, drv; } dinfo;
    exe_memset(&dinfo, 0, sizeof(dinfo));
    int info_ret = exe_dev_ioctl(DEV_DISPLAY, DISPLAY_IOCTL_GET_INFO, &dinfo);

    uint32_t scr_w, scr_h, scr_bpp;
    if (info_ret == 0 && dinfo.w > 0 && dinfo.h > 0) {
        scr_w   = dinfo.w;
        scr_h   = dinfo.h;
        scr_bpp = dinfo.bpp;
        liblog_hex(LOG_INFO, "GUIEXEC", "Display width:", scr_w);
        liblog_hex(LOG_INFO, "GUIEXEC", "Display height:", scr_h);
    } else {
        /* Fallback to compile-time defaults */
        scr_w   = 1024;
        scr_h   = 768;
        scr_bpp = 32;
        liblog(LOG_WARN, "GUIEXEC", "GET_INFO failed, using fallback 1024x768");
    }

    /* Populate display info */
    g_display_info.framebuffer = fb_addr;
    g_display_info.width       = scr_w;
    g_display_info.height      = scr_h;
    g_display_info.bpp         = scr_bpp;
    g_display_info.pitch       = scr_w * (scr_bpp / 8);

    /* Publish display info to cell registry for libgui discovery */
    uint32_t val;

    val = fb_addr;
    libcell_write("system.gui.framebuffer", &val, sizeof(uint32_t));

    val = scr_w;
    libcell_write("system.gui.width", &val, sizeof(uint32_t));

    val = scr_h;
    libcell_write("system.gui.height", &val, sizeof(uint32_t));

    val = scr_bpp;
    libcell_write("system.gui.bpp", &val, sizeof(uint32_t));

    val = scr_w * (scr_bpp / 8);
    libcell_write("system.gui.pitch", &val, sizeof(uint32_t));

    liblog(LOG_INFO, "GUIEXEC", "Display info published to cells");
    liblog(LOG_INFO, "GUIEXEC", "GUI Executive initialized successfully");
    return 0;
}

void exe_gui_main(void) {
    if (exe_gui_init() != 0) {
        liblog(LOG_ERROR, "GUIEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "GUIEXEC", "Entering main loop...");

    exec_request_t req;
    exec_response_t resp;

    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_gui_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            exe_yield();
        }
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "GUIEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
