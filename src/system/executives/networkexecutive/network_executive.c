/**
 * MaahiOS Network Executive Implementation
 *
 * Layer 3 (Executive). Ring 3.
 * Receives requests via SHM queue from libnet.
 * Calls kernel Network Manager via SYS_NET_* syscalls.
 *
 * Operations:
 *   NET_OP_PING       → SYS_NET_PING
 *   NET_OP_GET_CONFIG → SYS_NET_GET_CONFIG
 *   NET_OP_GET_STATUS → SYS_NET_GET_STATUS
 */

#include "network_executive.h"
#include "../common/executive_queue.h"
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"

/* Executive ID for network (10 = next available after IO=9) */
#define EXEC_ID_NETWORK     10

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

/*=============================================================================
 * NETWORK SYSCALL WRAPPERS (must match kernel syscall_numbers.h)
 *===========================================================================*/

#define SYS_NET_GET_CONFIG  144
#define SYS_NET_PING        145
#define SYS_NET_GET_STATUS  146
#define SYS_NET_GET_PKT_LOG 149

/* Socket syscalls (150-155) */
#define SYS_NET_SOCK_CREATE  150
#define SYS_NET_SOCK_CONNECT 151
#define SYS_NET_SOCK_SEND    152
#define SYS_NET_SOCK_RECV    153
#define SYS_NET_SOCK_CLOSE   154
#define SYS_NET_SOCK_SENDTO  155

static inline int exe_net_get_config(void *config) {
    return syscall1(SYS_NET_GET_CONFIG, (int)config);
}

static inline int exe_net_ping(uint32_t dst_ip, uint32_t timeout_ms, void *result) {
    return syscall3(SYS_NET_PING, (int)dst_ip, (int)timeout_ms, (int)result);
}

static inline int exe_net_get_status(void *stats) {
    return syscall1(SYS_NET_GET_STATUS, (int)stats);
}

static inline int exe_net_get_pkt_log(void *log) {
    return syscall1(SYS_NET_GET_PKT_LOG, (int)log);
}

/* Socket syscall wrappers */
static inline int exe_net_sock_create(int type) {
    return syscall1(SYS_NET_SOCK_CREATE, type);
}

static inline int exe_net_sock_connect(int sock, uint32_t ip, uint16_t port) {
    return syscall3(SYS_NET_SOCK_CONNECT, sock, (int)ip, (int)port);
}

static inline int exe_net_sock_send(int sock, const void *data, uint16_t len) {
    return syscall3(SYS_NET_SOCK_SEND, sock, (int)data, (int)len);
}

static inline int exe_net_sock_recv(int sock, void *buf, uint16_t max) {
    return syscall3(SYS_NET_SOCK_RECV, sock, (int)buf, (int)max);
}

static inline int exe_net_sock_close(int sock) {
    return syscall1(SYS_NET_SOCK_CLOSE, sock);
}

static inline int exe_net_sock_sendto(int sock, uint32_t ip, uint16_t port,
                                       const void *data, uint16_t len) {
    return syscall5(SYS_NET_SOCK_SENDTO, sock, (int)ip, (int)port, (int)data, (int)len);
}

/*=============================================================================
 * EXECUTIVE STATE
 *===========================================================================*/

static exec_control_block_t  *g_ecb = NULL;
static exec_request_queue_t  *g_req_queue = NULL;
static exec_response_queue_t *g_resp_queue = NULL;
static int g_req_queue_shm_id  = -1;
static int g_resp_queue_shm_id = -1;

/* Dedicated SHM for packet log (too large for response payload) */
static int g_pkt_log_shm_id = -1;
static net_exec_pkt_log_t *g_pkt_log_buf = NULL;

/*=============================================================================
 * REQUEST HANDLERS
 *===========================================================================*/

static void exe_net_handle_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_ping_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_ping_req_t *ping_req = (const net_ping_req_t *)req->payload;

    liblog_hex(LOG_INFO, "NETEXEC", "ping target IP:", ping_req->dst_ip);

    /* Call kernel via syscall */
    net_exec_ping_result_t result;
    exe_memset(&result, 0, sizeof(result));

    int ret = exe_net_ping(ping_req->dst_ip, ping_req->timeout_ms, &result);

    /* Copy result into response payload */
    net_exec_ping_result_t *resp_result = (net_exec_ping_result_t *)resp->payload;
    exe_memcpy(resp_result, &result, sizeof(net_exec_ping_result_t));

    resp->status = (ret == 0) ? EXEC_OK : (ret == -1 ? EXEC_ERR_TIMEOUT : EXEC_ERR_INVALID);
    resp->result = (uint32_t)result.success;
    resp->payload_size = sizeof(net_exec_ping_result_t);

    if (result.success) {
        liblog_hex(LOG_INFO, "NETEXEC", "ping reply rtt_ms:", result.rtt_ms);
    } else {
        liblog(LOG_WARN, "NETEXEC", "ping: timeout or error");
    }
}

static void exe_net_handle_get_config(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    net_exec_config_t config;
    exe_memset(&config, 0, sizeof(config));

    int ret = exe_net_get_config(&config);

    net_exec_config_t *resp_config = (net_exec_config_t *)resp->payload;
    exe_memcpy(resp_config, &config, sizeof(net_exec_config_t));

    resp->status = (ret == 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = config.initialized;
    resp->payload_size = sizeof(net_exec_config_t);
}

static void exe_net_handle_get_status(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    net_exec_stats_t stats;
    exe_memset(&stats, 0, sizeof(stats));

    int ret = exe_net_get_status(&stats);

    net_exec_stats_t *resp_stats = (net_exec_stats_t *)resp->payload;
    exe_memcpy(resp_stats, &stats, sizeof(net_exec_stats_t));

    resp->status = (ret == 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = 0;
    resp->payload_size = sizeof(net_exec_stats_t);
}

static void exe_net_handle_get_pkt_log(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    /* Lazy-create the SHM for packet log */
    if (g_pkt_log_shm_id < 0) {
        g_pkt_log_shm_id = exe_shm_create(sizeof(net_exec_pkt_log_t));
        if (g_pkt_log_shm_id < 0) {
            liblog(LOG_ERROR, "NETEXEC", "Failed to create pkt_log SHM");
            resp->status = EXEC_ERR_INVALID;
            resp->payload_size = 0;
            return;
        }
        g_pkt_log_buf = (net_exec_pkt_log_t *)exe_shm_attach(g_pkt_log_shm_id);
        if (!g_pkt_log_buf) {
            liblog(LOG_ERROR, "NETEXEC", "Failed to attach pkt_log SHM");
            g_pkt_log_shm_id = -1;
            resp->status = EXEC_ERR_INVALID;
            resp->payload_size = 0;
            return;
        }
        /* Publish the SHM ID so clients can attach */
        libcell_write("system.exec.net.pktlog_shm", &g_pkt_log_shm_id, sizeof(int));
        liblog_hex(LOG_INFO, "NETEXEC", "Pkt log SHM ID:", g_pkt_log_shm_id);
    }

    /* Fetch latest packet log from kernel into the SHM */
    int ret = exe_net_get_pkt_log(g_pkt_log_buf);

    resp->status = (ret == 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)g_pkt_log_shm_id;
    resp->payload_size = 0;
}

static void exe_net_handle_framework_ping(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;
    resp->status = EXEC_OK;
    resp->result = 1;
    resp->payload_size = 0;
}

/*=============================================================================
 * SOCKET HANDLERS
 *===========================================================================*/

static void exe_net_handle_sock_create(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_sock_create_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_sock_create_req_t *r = (const net_sock_create_req_t *)req->payload;
    int ret = exe_net_sock_create((int)r->type);

    resp->status = (ret >= 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)ret;     /* socket handle or error */
    resp->payload_size = 0;
}

static void exe_net_handle_sock_connect(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_sock_connect_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_sock_connect_req_t *r = (const net_sock_connect_req_t *)req->payload;
    int ret = exe_net_sock_connect(r->sock, r->remote_ip, r->remote_port);

    resp->status = (ret == 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)ret;
    resp->payload_size = 0;
}

static void exe_net_handle_sock_send(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_sock_send_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_sock_send_req_t *r = (const net_sock_send_req_t *)req->payload;
    const uint8_t *data = req->payload + sizeof(net_sock_send_req_t);
    uint16_t data_avail = (uint16_t)(req->payload_size - sizeof(net_sock_send_req_t));
    uint16_t len = (r->len < data_avail) ? r->len : data_avail;

    int ret = exe_net_sock_send(r->sock, data, len);

    resp->status = (ret >= 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)ret;     /* bytes sent or error */
    resp->payload_size = 0;
}

static void exe_net_handle_sock_recv(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_sock_recv_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_sock_recv_req_t *r = (const net_sock_recv_req_t *)req->payload;

    /* Clamp to payload capacity */
    uint16_t max = r->max_len;
    if (max > EXEC_MSG_MAX_PAYLOAD) max = EXEC_MSG_MAX_PAYLOAD;

    int ret = exe_net_sock_recv(r->sock, resp->payload, max);

    if (ret > 0) {
        resp->status = EXEC_OK;
        resp->result = (uint32_t)ret;
        resp->payload_size = (uint32_t)ret;
    } else if (ret == 0) {
        /* No data available right now */
        resp->status = EXEC_OK;
        resp->result = 0;
        resp->payload_size = 0;
    } else {
        resp->status = EXEC_ERR_INVALID;
        resp->result = (uint32_t)ret;
        resp->payload_size = 0;
    }
}

static void exe_net_handle_sock_close(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_sock_close_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_sock_close_req_t *r = (const net_sock_close_req_t *)req->payload;
    int ret = exe_net_sock_close(r->sock);

    resp->status = (ret == 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)ret;
    resp->payload_size = 0;
}

static void exe_net_handle_sock_sendto(const exec_request_t *req, exec_response_t *resp) {
    resp->msg_id = req->msg_id;

    if (req->payload_size < sizeof(net_sock_sendto_req_t)) {
        resp->status = EXEC_ERR_INVALID;
        resp->payload_size = 0;
        return;
    }

    const net_sock_sendto_req_t *r = (const net_sock_sendto_req_t *)req->payload;
    const uint8_t *data = req->payload + sizeof(net_sock_sendto_req_t);
    uint16_t data_avail = (uint16_t)(req->payload_size - sizeof(net_sock_sendto_req_t));
    uint16_t len = (r->len < data_avail) ? r->len : data_avail;

    int ret = exe_net_sock_sendto(r->sock, r->dst_ip, r->dst_port, data, len);

    resp->status = (ret >= 0) ? EXEC_OK : EXEC_ERR_INVALID;
    resp->result = (uint32_t)ret;
    resp->payload_size = 0;
}

/*=============================================================================
 * DISPATCH
 *===========================================================================*/

static void exe_net_dispatch(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));

    switch (req->func_id) {
        case EXEC_OP_PING:
            exe_net_handle_framework_ping(req, resp);
            EXEC_STAT_SUCCESS(g_ecb);
            break;

        case NET_OP_PING:
            exe_net_handle_ping(req, resp);
            if (resp->status == EXEC_OK) {
                EXEC_STAT_SUCCESS(g_ecb);
            } else {
                EXEC_STAT_FAILURE(g_ecb);
            }
            break;

        case NET_OP_GET_CONFIG:
            exe_net_handle_get_config(req, resp);
            EXEC_STAT_SUCCESS(g_ecb);
            break;

        case NET_OP_GET_STATUS:
            exe_net_handle_get_status(req, resp);
            EXEC_STAT_SUCCESS(g_ecb);
            break;

        case NET_OP_GET_PKT_LOG:
            exe_net_handle_get_pkt_log(req, resp);
            if (resp->status == EXEC_OK) {
                EXEC_STAT_SUCCESS(g_ecb);
            } else {
                EXEC_STAT_FAILURE(g_ecb);
            }
            break;

        case NET_OP_SOCK_CREATE:
            exe_net_handle_sock_create(req, resp);
            if (resp->status == EXEC_OK) EXEC_STAT_SUCCESS(g_ecb);
            else EXEC_STAT_FAILURE(g_ecb);
            break;

        case NET_OP_SOCK_CONNECT:
            exe_net_handle_sock_connect(req, resp);
            if (resp->status == EXEC_OK) EXEC_STAT_SUCCESS(g_ecb);
            else EXEC_STAT_FAILURE(g_ecb);
            break;

        case NET_OP_SOCK_SEND:
            exe_net_handle_sock_send(req, resp);
            if (resp->status == EXEC_OK) EXEC_STAT_SUCCESS(g_ecb);
            else EXEC_STAT_FAILURE(g_ecb);
            break;

        case NET_OP_SOCK_RECV:
            exe_net_handle_sock_recv(req, resp);
            if (resp->status == EXEC_OK) EXEC_STAT_SUCCESS(g_ecb);
            else EXEC_STAT_FAILURE(g_ecb);
            break;

        case NET_OP_SOCK_CLOSE:
            exe_net_handle_sock_close(req, resp);
            if (resp->status == EXEC_OK) EXEC_STAT_SUCCESS(g_ecb);
            else EXEC_STAT_FAILURE(g_ecb);
            break;

        case NET_OP_SOCK_SENDTO:
            exe_net_handle_sock_sendto(req, resp);
            if (resp->status == EXEC_OK) EXEC_STAT_SUCCESS(g_ecb);
            else EXEC_STAT_FAILURE(g_ecb);
            break;

        default:
            liblog_hex(LOG_WARN, "NETEXEC", "Unknown opcode:", req->func_id);
            resp->msg_id = req->msg_id;
            resp->status = EXEC_ERR_INVALID;
            resp->payload_size = 0;
            EXEC_STAT_FAILURE(g_ecb);
            break;
    }
}

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

static int exe_net_init(void) {
    liblog(LOG_INFO, "NETEXEC", "Initializing Network Executive...");

    /* Create SHM queues */
    g_req_queue_shm_id = exe_shm_create(sizeof(exec_request_queue_t));
    if (g_req_queue_shm_id < 0) {
        liblog(LOG_ERROR, "NETEXEC", "Failed to create request queue SHM");
        return -1;
    }

    g_req_queue = (exec_request_queue_t *)exe_shm_attach(g_req_queue_shm_id);
    if (!g_req_queue) {
        liblog(LOG_ERROR, "NETEXEC", "Failed to attach request queue SHM");
        return -1;
    }
    exe_request_queue_init(g_req_queue);
    liblog_hex(LOG_INFO, "NETEXEC", "Request queue SHM ID:", g_req_queue_shm_id);

    g_resp_queue_shm_id = exe_shm_create(sizeof(exec_response_queue_t));
    if (g_resp_queue_shm_id < 0) {
        liblog(LOG_ERROR, "NETEXEC", "Failed to create response queue SHM");
        return -1;
    }

    g_resp_queue = (exec_response_queue_t *)exe_shm_attach(g_resp_queue_shm_id);
    if (!g_resp_queue) {
        liblog(LOG_ERROR, "NETEXEC", "Failed to attach response queue SHM");
        return -1;
    }
    exe_response_queue_init(g_resp_queue);
    liblog_hex(LOG_INFO, "NETEXEC", "Response queue SHM ID:", g_resp_queue_shm_id);

    /* Set up control block */
    static exec_control_block_t local_ecb;
    g_ecb = &local_ecb;
    exe_str_copy(g_ecb->name, "network_executive", EXEC_NAME_MAX);
    g_ecb->exec_id = EXEC_ID_NETWORK;
    g_ecb->state = EXEC_STATE_STARTING;
    g_ecb->priority = PRIORITY_HIGH;
    g_ecb->request_queue_shm_id = g_req_queue_shm_id;
    g_ecb->response_queue_shm_id = g_resp_queue_shm_id;

    /* Write queue SHM IDs to cells for discovery by libnet */
    libcell_write("system.exec.net.req_shm", &g_req_queue_shm_id, sizeof(int));
    libcell_write("system.exec.net.resp_shm", &g_resp_queue_shm_id, sizeof(int));

    /* Also publish that network executive is ready */
    int ready = 1;
    libcell_write("system.net.ready", &ready, sizeof(int));

    liblog(LOG_INFO, "NETEXEC", "Network Executive initialized");
    return 0;
}

/*=============================================================================
 * MAIN LOOP
 *===========================================================================*/

void exe_net_main(void) {
    if (exe_net_init() != 0) {
        liblog(LOG_ERROR, "NETEXEC", "Initialization failed! Halting.");
        while (1) __asm__ volatile("hlt");
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_RUNNING);
    liblog(LOG_INFO, "NETEXEC", "Entering main loop...");

    exec_request_t req;
    exec_response_t resp;

    while (!EXEC_SHOULD_STOP(g_ecb)) {
        if (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            exe_net_dispatch(&req, &resp);
            exe_response_queue_push(g_resp_queue, &resp);
        } else {
            /* Sleep when idle so we don't starve the IO Executive
             * and other processes of CPU timeslices.  3 ticks ≈ 60ms
             * at 50 Hz — short enough to respond to requests promptly. */
            syscall1(SYS_SLEEP, 3);
        }
    }

    EXEC_SET_STATE(g_ecb, EXEC_STATE_STOPPED);
    liblog(LOG_WARN, "NETEXEC", "Stopped. Halting.");
    while (1) __asm__ volatile("hlt");
}
