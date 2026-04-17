/**
 * MaahiOS Window Manager Client Library — Implementation
 *
 * Description:
 *   Connects to the WM Executive via SHM request/response queues
 *   (standard executive pattern).  Provides functions for creating,
 *   destroying, moving windows, and signaling damage for compositing.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "libwm.h"
#include "../core/syscall_helpers.h"
#include "../libcell/libcell.h"
#include "../shared/wm_types.h"
#include "../../executives/common/executive_queue.h"

/*=============================================================================
 * STATE
 *===========================================================================*/

static exec_request_queue_t  *g_req_queue  = (void *)0;
static exec_response_queue_t *g_resp_queue = (void *)0;
static int g_initialized = 0;
static uint32_t g_msg_counter = 1;
static uint32_t g_my_pid = 0;

/* Per-handle tracking for queue-based heartbeat */
#define LIBWM_MAX_LOCAL  4
static int g_hb_handles[LIBWM_MAX_LOCAL];
static int g_hb_handle_count = 0;

/* Throttle: send queue heartbeat at most once per interval */
#define LIBWM_HB_INTERVAL 50   /* ticks (~1 sec at 50Hz) */
static uint32_t g_hb_last_sent_tick = 0;

/*=============================================================================
 * HELPERS
 *===========================================================================*/

/* Forward declaration — defined below init */
static void _libwm_heartbeat_all(void);

static void str_copy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

/** Send a request and wait for the response.  Returns response. */
static exec_response_t send_request(exec_request_t *req) {
    exec_response_t resp;
    exe_memset(&resp, 0, sizeof(resp));

    if (!g_my_pid) g_my_pid = (uint32_t)syscall0(SYS_GETPID);
    req->msg_id     = (g_my_pid << 16) | (g_msg_counter++ & 0xFFFF);
    req->sender_pid = g_my_pid;

    /* Push to WM request queue */
    int retries = 0;
    while (exe_request_queue_push(g_req_queue, req) != EXEC_OK) {
        syscall0(SYS_YIELD);
        if (++retries > 200) {
            resp.status = EXEC_ERR_QUEUE_FULL;
            return resp;
        }
    }

    /* Wait for matching response */
    for (int i = 0; i < 500; i++) {
        if (exe_response_queue_pop_by_id(g_resp_queue, req->msg_id, &resp) == EXEC_OK)
            return resp;
        _libwm_heartbeat_all();
        syscall0(SYS_YIELD);
    }

    resp.status = EXEC_ERR_TIMEOUT;
    return resp;
}

/** Send a request without waiting for a response (fire & forget) */
static void send_request_async(exec_request_t *req) {
    if (!g_my_pid) g_my_pid = (uint32_t)syscall0(SYS_GETPID);
    req->msg_id     = (g_my_pid << 16) | (g_msg_counter++ & 0xFFFF);
    req->sender_pid = g_my_pid;
    req->flags     |= EXEC_FLAG_NO_RESPONSE;

    int retries = 0;
    while (exe_request_queue_push(g_req_queue, req) != EXEC_OK) {
        syscall0(SYS_YIELD);
        if (++retries > 100) return;
    }
}

/*=============================================================================
 * GLOBAL HEARTBEAT HOOK (called from all library poll loops)
 *===========================================================================*/

/**
 * Send throttled queue-based heartbeat for ALL local window handles.
 * Registered into exe_poll_heartbeat_hook so that blocking IPC in any
 * library (libnet, libcell, libfs, etc.) keeps WM heartbeat alive.
 */
static void _libwm_heartbeat_all(void) {
    if (g_hb_handle_count == 0 || !g_initialized) return;
    uint32_t now = (uint32_t)syscall0(SYS_TIME_GET_TICKS);
    if (now - g_hb_last_sent_tick < LIBWM_HB_INTERVAL) return;
    g_hb_last_sent_tick = now;

    for (int i = 0; i < g_hb_handle_count; i++) {
        exec_request_t req;
        exe_memset(&req, 0, sizeof(req));
        req.func_id = WM_CMD_HEARTBEAT;
        wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
        p->handle = g_hb_handles[i];
        req.payload_size = sizeof(wm_handle_payload_t);
        send_request_async(&req);
    }
}

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

int libwm_init(void) {
    if (g_initialized) return 0;

    /* Wait for WM Executive to be ready */
    int32_t ready = 0;
    for (int attempt = 0; attempt < 100; attempt++) {
        libcell_read(CELL_WM_READY, &ready, sizeof(ready));
        if (ready == 1) break;
        syscall0(SYS_YIELD);
        syscall1(SYS_SLEEP, 1);
    }
    if (ready != 1) return -1;

    /* Discover WM queue SHM IDs via cells */
    int req_shm_id = -1, resp_shm_id = -1;
    libcell_read(CELL_WM_REQ_QUEUE,  &req_shm_id,  sizeof(int));
    libcell_read(CELL_WM_RESP_QUEUE, &resp_shm_id, sizeof(int));
    if (req_shm_id < 0 || resp_shm_id < 0) return -1;

    /* Attach to SHM queues */
    g_req_queue = (exec_request_queue_t *)syscall2(SYS_SHM_ATTACH, req_shm_id, 0);
    g_resp_queue = (exec_response_queue_t *)syscall2(SYS_SHM_ATTACH, resp_shm_id, 0);
    if (!g_req_queue || !g_resp_queue) return -1;

    g_initialized = 1;

    /* Register global poll heartbeat hook so ALL library _send_and_wait()
     * loops keep our WM heartbeat alive during blocking IPC. */
    exe_poll_heartbeat_hook = _libwm_heartbeat_all;

    return 0;
}

/*=============================================================================
 * WINDOW LIFECYCLE
 *===========================================================================*/

int libwm_create(int x, int y, int w, int h, const char *title,
                 int *out_shm_id) {
    if (!g_initialized && libwm_init() < 0) return -1;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_CREATE;

    wm_create_payload_t *p = (wm_create_payload_t *)req.payload;
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    str_copy(p->title, title, WM_TITLE_MAX);
    req.payload_size = sizeof(wm_create_payload_t);

    exec_response_t resp = send_request(&req);
    if (resp.status != EXEC_OK) return -1;

    wm_create_response_t *rp = (wm_create_response_t *)resp.payload;
    if (out_shm_id) *out_shm_id = rp->surface_shm_id;

    /* Remember handle for queue-based heartbeat */
    if (g_hb_handle_count < LIBWM_MAX_LOCAL) {
        g_hb_handles[g_hb_handle_count++] = rp->handle;
    }

    return rp->handle;
}

void libwm_destroy(int handle) {
    if (!g_initialized) return;

    /* Remove handle from heartbeat tracking */
    for (int i = 0; i < g_hb_handle_count; i++) {
        if (g_hb_handles[i] == handle) {
            for (int j = i; j < g_hb_handle_count - 1; j++)
                g_hb_handles[j] = g_hb_handles[j + 1];
            g_hb_handle_count--;
            break;
        }
    }

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_DESTROY;

    wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
    p->handle = handle;
    req.payload_size = sizeof(wm_handle_payload_t);

    send_request(&req);  /* Wait for completion so damage is flushed */
}

/*=============================================================================
 * WINDOW STATE
 *===========================================================================*/

void libwm_move(int handle, int new_x, int new_y) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_MOVE;

    wm_move_payload_t *p = (wm_move_payload_t *)req.payload;
    p->handle = handle;
    p->new_x  = new_x;
    p->new_y  = new_y;
    req.payload_size = sizeof(wm_move_payload_t);

    send_request(&req);  /* Sync — wait for compositor to finish before client redraws */
}

void libwm_damage(int handle, int x, int y, int w, int h) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_DAMAGE;

    wm_damage_payload_t *p = (wm_damage_payload_t *)req.payload;
    p->handle = handle;
    p->x = x;
    p->y = y;
    p->w = w;
    p->h = h;
    req.payload_size = sizeof(wm_damage_payload_t);

    send_request_async(&req);
}

void libwm_damage_full(int handle) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_DAMAGE;

    /* Use x=0,y=0 and very large w,h — WM clips to window size */
    wm_damage_payload_t *p = (wm_damage_payload_t *)req.payload;
    p->handle = handle;
    p->x = 0;
    p->y = 0;
    p->w = 4096;
    p->h = 4096;
    req.payload_size = sizeof(wm_damage_payload_t);

    send_request_async(&req);
}

void libwm_minimize(int handle) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_MINIMIZE;

    wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
    p->handle = handle;
    req.payload_size = sizeof(wm_handle_payload_t);

    send_request(&req);  /* Sync — wait for compositing before hiding */
}

void libwm_restore(int handle) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_RESTORE;

    wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
    p->handle = handle;
    req.payload_size = sizeof(wm_handle_payload_t);

    send_request(&req);  /* Sync */
}

int libwm_maximize(int handle, int *out_shm_id) {
    if (!g_initialized) return -1;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_MAXIMIZE;

    wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
    p->handle = handle;
    req.payload_size = sizeof(wm_handle_payload_t);

    exec_response_t resp = send_request(&req);
    if (resp.status != EXEC_OK) return -1;

    wm_create_response_t *rp = (wm_create_response_t *)resp.payload;
    if (out_shm_id) *out_shm_id = rp->surface_shm_id;
    return 0;
}

void libwm_raise(int handle) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_RAISE;

    wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
    p->handle = handle;
    req.payload_size = sizeof(wm_handle_payload_t);

    send_request_async(&req);
}

void libwm_set_title(int handle, const char *title) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_SET_TITLE;

    wm_title_payload_t *p = (wm_title_payload_t *)req.payload;
    p->handle = handle;
    str_copy(p->title, title, WM_TITLE_MAX);
    req.payload_size = sizeof(wm_title_payload_t);

    send_request_async(&req);
}

void libwm_full_composite(void) {
    if (!g_initialized) return;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_FULL_DAMAGE;
    req.payload_size = 0;

    send_request_async(&req);
}

int libwm_is_focused(int handle) {
    /* Read the WM registry cell and check which window has the highest z_order */
    wm_window_registry_t reg;
    int rd = libcell_read(CELL_WM_REGISTRY, &reg, sizeof(reg));
    if (rd < (int)sizeof(int32_t) || reg.count <= 0) return 0;

    int max_z = -1;
    int top_handle = -1;
    for (int i = 0; i < reg.count; i++) {
        if (reg.windows[i].state != WM_STATE_MINIMIZED &&
            (int)reg.windows[i].z_order > max_z) {
            max_z = reg.windows[i].z_order;
            top_handle = reg.windows[i].handle;
        }
    }
    return (top_handle == handle) ? 1 : 0;
}

void libwm_heartbeat(int handle) {
    if (!g_initialized) return;

    /* Throttled queue-based heartbeat */
    uint32_t now = (uint32_t)syscall0(SYS_TIME_GET_TICKS);
    if (now - g_hb_last_sent_tick < LIBWM_HB_INTERVAL) return;
    g_hb_last_sent_tick = now;

    exec_request_t req;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = WM_CMD_HEARTBEAT;

    wm_handle_payload_t *p = (wm_handle_payload_t *)req.payload;
    p->handle = handle;
    req.payload_size = sizeof(wm_handle_payload_t);

    send_request_async(&req);
}
