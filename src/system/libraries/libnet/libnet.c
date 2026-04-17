/**
 * MaahiOS Network Library (libnet) - Implementation
 *
 * Layer 2 (Library). Ring 3.
 * Auto-initializes on first call (discovers SHM, attaches to Network Executive).
 * Sends requests via SHM queue, polls for responses.
 */

#include "libnet.h"
#include "../core/syscall_helpers.h"
#include "../libcell/libcell.h"
#include "../../executives/common/executive_queue.h"
#include "../../executives/networkexecutive/network_executive.h"

/*=============================================================================
 * LIBRARY STATE
 *===========================================================================*/

static exec_request_queue_t  *g_req_queue  = (void*)0;
static exec_response_queue_t *g_resp_queue = (void*)0;
static uint32_t g_msg_id      = 1;
static int      g_initialized = 0;
static uint32_t g_my_pid      = 0;

/* Heartbeat callback — called periodically during blocking polls */
static void (*g_heartbeat_fn)(void *ctx) = (void*)0;
static void  *g_heartbeat_ctx            = (void*)0;

/* Time-based heartbeat: call at most once per 75 ticks (~1.5 sec at 50 Hz).
 * This prevents flooding the WM queue when many fast _send_and_wait calls happen. */
static uint32_t g_hb_last_tick = 0;
#define HEARTBEAT_MIN_INTERVAL  75

static void _do_heartbeat(int iter) {
    (void)iter;
    if (!g_heartbeat_fn) return;
    uint32_t now = (uint32_t)syscall0(SYS_TIME_GET_TICKS);
    if (now - g_hb_last_tick < HEARTBEAT_MIN_INTERVAL) return;
    g_hb_last_tick = now;
    g_heartbeat_fn(g_heartbeat_ctx);
}

void libnet_set_heartbeat(void (*fn)(void *ctx), void *ctx) {
    g_heartbeat_fn  = fn;
    g_heartbeat_ctx = ctx;
}

/*=============================================================================
 * INTERNAL: AUTO-INIT (lazy, called on first use)
 *===========================================================================*/

static int _libnet_try_init(void) {
    if (g_initialized) return 0;

    /* Get our PID and seed msg_id to avoid collisions */
    if (g_my_pid == 0) {
        g_my_pid = (uint32_t)syscall0(SYS_GETPID);
        g_msg_id = (g_my_pid << 16) | 1;
    }

    /* Read Network Executive's request queue SHM ID from cell registry */
    int req_shm_id = -1;
    int result = libcell_read("system.exec.net.req_shm",
                              &req_shm_id, sizeof(int));
    if (result < 0 || req_shm_id < 0) {
        return -1;
    }

    /* Read Network Executive's response queue SHM ID */
    int resp_shm_id = -1;
    result = libcell_read("system.exec.net.resp_shm",
                          &resp_shm_id, sizeof(int));
    if (result < 0 || resp_shm_id < 0) {
        return -1;
    }

    /* Attach to request queue SHM */
    g_req_queue = (exec_request_queue_t *)syscall2(SYS_SHM_ATTACH, req_shm_id, 0);
    if (!g_req_queue || (uint32_t)g_req_queue == 0xFFFFFFFF) {
        g_req_queue = (void*)0;
        return -1;
    }

    /* Attach to response queue SHM */
    g_resp_queue = (exec_response_queue_t *)syscall2(SYS_SHM_ATTACH, resp_shm_id, 0);
    if (!g_resp_queue || (uint32_t)g_resp_queue == 0xFFFFFFFF) {
        g_resp_queue = (void*)0;
        return -1;
    }

    g_initialized = 1;
    return 0;
}

/**
 * Send request to Network Executive and wait for response.
 * Timeout = 200 polls (ping can take several seconds so we wait longer).
 */
static int _send_and_wait(exec_request_t *req, exec_response_t *resp) {
    if (!g_req_queue || !g_resp_queue) return EXEC_ERR_NOT_RUNNING;

    uint32_t my_id = g_msg_id++;
    req->msg_id     = my_id;
    req->sender_pid = g_my_pid;
    req->exec_id    = 0; /* Network executive */

    int push_result = exe_request_queue_push(g_req_queue, req);
    if (push_result != EXEC_OK) return push_result;

    /* Wait for matching response — 200 polls, sleep 1 tick between each */
    for (int i = 0; i < 200; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, resp);
        if (pop_result == EXEC_OK) {
            return EXEC_OK;
        }
        exe_poll_heartbeat();
        _do_heartbeat(i);
        syscall1(SYS_SLEEP, 1);
    }

    return EXEC_ERR_TIMEOUT;
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

int libnet_init(void) {
    return _libnet_try_init();
}

int libnet_ping(uint32_t dst_ip, uint32_t timeout_ms, libnet_ping_result_t *result) {
    if (!result) return -2;
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_PING;

    /* Build ping request payload */
    net_ping_req_t *ping_req = (net_ping_req_t *)req.payload;
    ping_req->dst_ip     = dst_ip;
    ping_req->timeout_ms = timeout_ms;
    req.payload_size = sizeof(net_ping_req_t);

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK) {
        return -2;
    }

    /* Copy result from response payload */
    if (resp.payload_size >= sizeof(net_exec_ping_result_t)) {
        const net_exec_ping_result_t *pr = (const net_exec_ping_result_t *)resp.payload;
        result->success = pr->success;
        result->rtt_ms  = pr->rtt_ms;
        result->ttl     = pr->ttl;
        result->seq     = pr->seq;
        result->dst_ip  = pr->dst_ip;
    }

    if (resp.status == EXEC_OK && resp.result) {
        return 0;  /* Success */
    } else if (resp.status == EXEC_ERR_TIMEOUT) {
        return -1; /* Timeout */
    }
    return -2; /* Error */
}

int libnet_get_config(libnet_config_t *config) {
    if (!config) return -1;
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -1;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_GET_CONFIG;
    req.payload_size = 0;

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK) return -1;
    if (resp.status != EXEC_OK) return -1;

    /* Copy config from response payload */
    if (resp.payload_size >= sizeof(net_exec_config_t)) {
        const net_exec_config_t *cfg = (const net_exec_config_t *)resp.payload;
        config->ip_addr     = cfg->ip_addr;
        config->netmask     = cfg->netmask;
        config->gateway     = cfg->gateway;
        config->dns         = cfg->dns;
        for (int i = 0; i < LIBNET_MAC_LEN; i++) {
            config->mac[i] = cfg->mac[i];
        }
        config->link_up     = cfg->link_up;
        config->initialized = cfg->initialized;
    }

    return 0;
}

int libnet_get_stats(libnet_stats_t *stats) {
    if (!stats) return -1;
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -1;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_GET_STATUS;
    req.payload_size = 0;

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK) return -1;
    if (resp.status != EXEC_OK) return -1;

    /* Copy stats from response payload */
    if (resp.payload_size >= sizeof(net_exec_stats_t)) {
        const net_exec_stats_t *st = (const net_exec_stats_t *)resp.payload;
        stats->tx_packets    = st->tx_packets;
        stats->rx_packets    = st->rx_packets;
        stats->tx_errors     = st->tx_errors;
        stats->rx_errors     = st->rx_errors;
        stats->arp_sent      = st->arp_sent;
        stats->arp_received  = st->arp_received;
        stats->icmp_sent     = st->icmp_sent;
        stats->icmp_received = st->icmp_received;
    }

    return 0;
}

int libnet_is_available(void) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return 0;

    /* Check cell: system.net.ready */
    int ready = 0;
    int ret = libcell_read("system.net.ready", &ready, sizeof(int));
    return (ret >= 0 && ready) ? 1 : 0;
}

/* Cached pkt_log SHM attachment */
static int g_pkt_log_shm_id = -1;
static void *g_pkt_log_ptr  = (void*)0;

int libnet_get_pkt_log(libnet_pkt_log_t *log) {
    if (!log) return -1;
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -1;

    /* Ask executive to refresh pkt log into its SHM */
    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));
    req.func_id = NET_OP_GET_PKT_LOG;
    req.payload_size = 0;

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK || resp.status != EXEC_OK) return -1;

    /* resp.result = SHM ID of packet log */
    int shm_id = (int)resp.result;

    /* Lazy-attach to the pkt_log SHM */
    if (g_pkt_log_shm_id != shm_id || !g_pkt_log_ptr) {
        if (g_pkt_log_ptr && g_pkt_log_shm_id != shm_id) {
            /* Detach old one */
            syscall1(SYS_SHM_DETACH, g_pkt_log_shm_id);
            g_pkt_log_ptr = (void*)0;
        }
        g_pkt_log_ptr = (void*)syscall2(SYS_SHM_ATTACH, shm_id, 0);
        if (!g_pkt_log_ptr || (uint32_t)g_pkt_log_ptr == 0xFFFFFFFF) {
            g_pkt_log_ptr = (void*)0;
            return -1;
        }
        g_pkt_log_shm_id = shm_id;
    }

    /* Copy from SHM to user buffer
     * Both structures (net_exec_pkt_log_t and libnet_pkt_log_t) have
     * the same layout — count, returned, entries[64] */
    const uint8_t *src = (const uint8_t *)g_pkt_log_ptr;
    uint8_t *dst = (uint8_t *)log;
    uint32_t sz = sizeof(libnet_pkt_log_t);
    if (sz > sizeof(libnet_pkt_log_t)) sz = sizeof(libnet_pkt_log_t);
    for (uint32_t i = 0; i < sz; i++) dst[i] = src[i];

    return 0;
}

/*=============================================================================
 * SOCKET API
 *===========================================================================*/

int libnet_socket_create(int type) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_SOCK_CREATE;
    net_sock_create_req_t *r = (net_sock_create_req_t *)req.payload;
    r->type = (uint32_t)type;
    req.payload_size = sizeof(net_sock_create_req_t);

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK || resp.status != EXEC_OK) return -1;
    return (int)resp.result;
}

int libnet_connect(int sock, uint32_t remote_ip, uint16_t remote_port) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_SOCK_CONNECT;
    net_sock_connect_req_t *r = (net_sock_connect_req_t *)req.payload;
    r->sock = sock;
    r->remote_ip = remote_ip;
    r->remote_port = remote_port;
    req.payload_size = sizeof(net_sock_connect_req_t);

    /* TCP connect can take several seconds — use extended timeout */
    if (!g_req_queue || !g_resp_queue) return -2;

    uint32_t my_id = g_msg_id++;
    req.msg_id     = my_id;
    req.sender_pid = g_my_pid;
    req.exec_id    = 0;

    int push_result = exe_request_queue_push(g_req_queue, &req);
    if (push_result != EXEC_OK) return -1;

    /* Wait up to 600 polls (~12 sec) for TCP handshake */
    for (int i = 0; i < 600; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, &resp);
        if (pop_result == EXEC_OK) {
            return (resp.status == EXEC_OK) ? 0 : -1;
        }
        _do_heartbeat(i);
        syscall1(SYS_SLEEP, 1);
    }
    return -1; /* timeout */
}

int libnet_send(int sock, const void *data, uint16_t len) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;
    if (!data || len == 0) return -1;

    /* Max data per request = EXEC_MSG_MAX_PAYLOAD - sizeof(net_sock_send_req_t) */
    uint16_t max_chunk = EXEC_MSG_MAX_PAYLOAD - sizeof(net_sock_send_req_t);
    uint16_t sent = 0;

    while (sent < len) {
        uint16_t chunk = len - sent;
        if (chunk > max_chunk) chunk = max_chunk;

        exec_request_t req;
        exec_response_t resp;
        exe_memset(&req, 0, sizeof(req));

        req.func_id = NET_OP_SOCK_SEND;
        net_sock_send_req_t *r = (net_sock_send_req_t *)req.payload;
        r->sock = sock;
        r->len = chunk;

        /* Copy data after the header */
        const uint8_t *src = (const uint8_t *)data + sent;
        uint8_t *dst = req.payload + sizeof(net_sock_send_req_t);
        for (uint16_t i = 0; i < chunk; i++) dst[i] = src[i];

        req.payload_size = sizeof(net_sock_send_req_t) + chunk;

        /* Use extended wait for send (TCP may block waiting for ACK) */
        if (!g_req_queue || !g_resp_queue) return -2;
        uint32_t my_id = g_msg_id++;
        req.msg_id = my_id;
        req.sender_pid = g_my_pid;
        req.exec_id = 0;

        int push_result = exe_request_queue_push(g_req_queue, &req);
        if (push_result != EXEC_OK) return (sent > 0) ? sent : -1;

        int got_resp = 0;
        for (int i = 0; i < 400; i++) {
            int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, &resp);
            if (pop_result == EXEC_OK) { got_resp = 1; break; }
            _do_heartbeat(i);
            syscall1(SYS_SLEEP, 1);
        }
        if (!got_resp) return (sent > 0) ? sent : -1;
        if (resp.status != EXEC_OK) return (sent > 0) ? sent : -1;

        sent += chunk;
    }

    return sent;
}

int libnet_recv(int sock, void *buf, uint16_t max_len) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;
    if (!buf || max_len == 0) return -1;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_SOCK_RECV;
    net_sock_recv_req_t *r = (net_sock_recv_req_t *)req.payload;
    r->sock = sock;
    r->max_len = max_len;
    req.payload_size = sizeof(net_sock_recv_req_t);

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK) return -1;
    if (resp.status != EXEC_OK) return (int)resp.result; /* error code */

    /* Copy received data from response payload */
    uint32_t got = resp.payload_size;
    if (got > max_len) got = max_len;
    if (got > 0) {
        const uint8_t *src = resp.payload;
        uint8_t *dst = (uint8_t *)buf;
        for (uint32_t i = 0; i < got; i++) dst[i] = src[i];
    }

    return (int)got;
}

int libnet_recv_bulk(int sock, void *buf, int max_len) {
    if (!buf || max_len <= 0) return -1;
    return syscall3(SYS_NET_SOCK_RECV_BULK, sock, (int)buf, max_len);
}

int libnet_close(int sock) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_SOCK_CLOSE;
    net_sock_close_req_t *r = (net_sock_close_req_t *)req.payload;
    r->sock = sock;
    req.payload_size = sizeof(net_sock_close_req_t);

    /* Close can take a while for TCP FIN exchange */
    if (!g_req_queue || !g_resp_queue) return -2;
    uint32_t my_id = g_msg_id++;
    req.msg_id = my_id;
    req.sender_pid = g_my_pid;
    req.exec_id = 0;

    int push_result = exe_request_queue_push(g_req_queue, &req);
    if (push_result != EXEC_OK) return -1;

    for (int i = 0; i < 300; i++) {
        int pop_result = exe_response_queue_pop_by_id(g_resp_queue, my_id, &resp);
        if (pop_result == EXEC_OK) {
            return (resp.status == EXEC_OK) ? 0 : -1;
        }
        _do_heartbeat(i);
        syscall1(SYS_SLEEP, 1);
    }
    return -1;
}

int libnet_udp_sendto(int sock, uint32_t dst_ip, uint16_t dst_port,
                      const void *data, uint16_t len) {
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;
    if (!data || len == 0) return -1;

    uint16_t max_data = EXEC_MSG_MAX_PAYLOAD - sizeof(net_sock_sendto_req_t);
    if (len > max_data) len = max_data;

    exec_request_t req;
    exec_response_t resp;
    exe_memset(&req, 0, sizeof(req));

    req.func_id = NET_OP_SOCK_SENDTO;
    net_sock_sendto_req_t *r = (net_sock_sendto_req_t *)req.payload;
    r->sock = sock;
    r->dst_ip = dst_ip;
    r->dst_port = dst_port;
    r->len = len;

    const uint8_t *src = (const uint8_t *)data;
    uint8_t *dst = req.payload + sizeof(net_sock_sendto_req_t);
    for (uint16_t i = 0; i < len; i++) dst[i] = src[i];

    req.payload_size = sizeof(net_sock_sendto_req_t) + len;

    int ret = _send_and_wait(&req, &resp);
    if (ret != EXEC_OK || resp.status != EXEC_OK) return -1;
    return (int)resp.result;
}

/*=============================================================================
 * DNS CACHE
 *===========================================================================*/

#define DNS_CACHE_SIZE  8

typedef struct {
    char     hostname[128];
    uint32_t ip;
    uint32_t timestamp;     /* Tick when cached */
} dns_cache_entry_t;

static dns_cache_entry_t g_dns_cache[DNS_CACHE_SIZE];
static int g_dns_cache_next = 0;  /* Round-robin insert index */

static int _dns_str_eq(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static void _dns_str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

/** Look up hostname in DNS cache. Returns IP or 0 if not found / expired. */
static uint32_t _dns_cache_lookup(const char *hostname) {
    uint32_t now = (uint32_t)syscall0(SYS_TIME_GET_TICKS);
    uint32_t tick_freq = (uint32_t)syscall0(SYS_TIME_GET_TICK_FREQ);
    uint32_t ttl_ticks = tick_freq * 300; /* 5-minute TTL */

    for (int i = 0; i < DNS_CACHE_SIZE; i++) {
        if (g_dns_cache[i].ip != 0 &&
            _dns_str_eq(g_dns_cache[i].hostname, hostname)) {
            if (now - g_dns_cache[i].timestamp < ttl_ticks) {
                return g_dns_cache[i].ip;
            }
            /* Expired — clear entry */
            g_dns_cache[i].ip = 0;
            g_dns_cache[i].hostname[0] = '\0';
            return 0;
        }
    }
    return 0;
}

/** Store hostname→IP in DNS cache (round-robin eviction). */
static void _dns_cache_store(const char *hostname, uint32_t ip) {
    int slot = g_dns_cache_next;
    g_dns_cache_next = (g_dns_cache_next + 1) % DNS_CACHE_SIZE;
    _dns_str_copy(g_dns_cache[slot].hostname, hostname, 128);
    g_dns_cache[slot].ip = ip;
    g_dns_cache[slot].timestamp = (uint32_t)syscall0(SYS_TIME_GET_TICKS);
}

/*=============================================================================
 * DNS RESOLVER
 *===========================================================================*/

/** Simple string length */
static uint32_t _str_len(const char *s) {
    uint32_t n = 0;
    while (s[n]) n++;
    return n;
}

/**
 * Build DNS query packet for A record lookup.
 * Format: header (12 bytes) + QNAME + QTYPE(2) + QCLASS(2)
 * Returns total packet length.
 */
static int _dns_build_query(const char *hostname, uint8_t *buf, uint16_t buf_size,
                            uint16_t txn_id) {
    if (!hostname || !buf || buf_size < 64) return -1;

    /* DNS header: 12 bytes */
    exe_memset(buf, 0, buf_size);
    buf[0] = (uint8_t)(txn_id >> 8);   /* Transaction ID high */
    buf[1] = (uint8_t)(txn_id & 0xFF); /* Transaction ID low */
    buf[2] = 0x01; /* QR=0, OPCODE=0, RD=1 (recursion desired) */
    buf[3] = 0x00;
    buf[4] = 0x00; buf[5] = 0x01; /* QDCOUNT = 1 */
    /* ANCOUNT, NSCOUNT, ARCOUNT all 0 */

    /* Encode hostname as DNS QNAME (label-encoded)
     * e.g. "example.com" -> \x07example\x03com\x00 */
    int pos = 12;
    const char *p = hostname;
    while (*p) {
        /* Find next dot or end */
        const char *dot = p;
        while (*dot && *dot != '.') dot++;
        int label_len = (int)(dot - p);
        if (label_len <= 0 || label_len > 63) return -1;
        if (pos + label_len + 1 >= buf_size - 4) return -1;

        buf[pos++] = (uint8_t)label_len;
        for (int i = 0; i < label_len; i++) {
            buf[pos++] = (uint8_t)p[i];
        }
        p = dot;
        if (*p == '.') p++;
    }
    buf[pos++] = 0x00; /* End of QNAME */

    /* QTYPE = A (1) */
    buf[pos++] = 0x00;
    buf[pos++] = 0x01;

    /* QCLASS = IN (1) */
    buf[pos++] = 0x00;
    buf[pos++] = 0x01;

    return pos; /* total query length */
}

/**
 * Parse DNS response to extract first A record IP.
 * Returns 0 on success, -1 on error.
 */
static int _dns_parse_response(const uint8_t *buf, int len, uint16_t expected_txn,
                               uint32_t *ip_out) {
    if (!buf || len < 12 || !ip_out) return -1;

    /* Verify transaction ID */
    uint16_t txn = ((uint16_t)buf[0] << 8) | buf[1];
    if (txn != expected_txn) return -1;

    /* Check QR=1 (response), RCODE=0 (no error) */
    if (!(buf[2] & 0x80)) return -1;   /* Not a response */
    if ((buf[3] & 0x0F) != 0) return -1; /* Non-zero RCODE */

    uint16_t qdcount = ((uint16_t)buf[4] << 8) | buf[5];
    uint16_t ancount = ((uint16_t)buf[6] << 8) | buf[7];

    if (ancount == 0) return -1; /* No answers */

    /* Skip questions */
    int pos = 12;
    for (uint16_t q = 0; q < qdcount; q++) {
        /* Skip QNAME (labels) */
        while (pos < len) {
            uint8_t llen = buf[pos];
            if (llen == 0) { pos++; break; }
            if ((llen & 0xC0) == 0xC0) { pos += 2; break; } /* Pointer */
            pos += 1 + llen;
        }
        pos += 4; /* QTYPE + QCLASS */
    }

    /* Parse answers — find first A record */
    for (uint16_t a = 0; a < ancount; a++) {
        if (pos >= len) break;

        /* Skip NAME (may be pointer) */
        if ((buf[pos] & 0xC0) == 0xC0) {
            pos += 2; /* Compressed pointer */
        } else {
            while (pos < len) {
                uint8_t llen = buf[pos];
                if (llen == 0) { pos++; break; }
                if ((llen & 0xC0) == 0xC0) { pos += 2; break; }
                pos += 1 + llen;
            }
        }

        if (pos + 10 > len) break;

        uint16_t rtype  = ((uint16_t)buf[pos] << 8) | buf[pos+1];
        /* uint16_t rclass = ((uint16_t)buf[pos+2] << 8) | buf[pos+3]; */
        /* uint32_t ttl = ...; */
        uint16_t rdlen  = ((uint16_t)buf[pos+8] << 8) | buf[pos+9];
        pos += 10;

        if (rtype == 1 && rdlen == 4 && pos + 4 <= len) {
            /* A record — 4 bytes = IPv4 address (network byte order) */
            *ip_out = ((uint32_t)buf[pos] << 24) |
                      ((uint32_t)buf[pos+1] << 16) |
                      ((uint32_t)buf[pos+2] << 8) |
                      (uint32_t)buf[pos+3];
            return 0;
        }
        pos += rdlen;
    }

    return -1; /* No A record found */
}

int libnet_dns_resolve(const char *hostname, uint32_t *ip_out) {
    if (!hostname || !ip_out) return -2;
    if (!g_initialized) _libnet_try_init();
    if (!g_initialized) return -2;

    /* Check DNS cache first */
    uint32_t cached = _dns_cache_lookup(hostname);
    if (cached != 0) {
        *ip_out = cached;
        return 0;
    }

    /* Get DNS server IP from network config */
    libnet_config_t cfg;
    if (libnet_get_config(&cfg) != 0) return -2;
    uint32_t dns_ip = cfg.dns;
    if (dns_ip == 0) dns_ip = LIBNET_IP(10, 0, 2, 3); /* QEMU default */

    /* Create UDP socket for DNS */
    int sock = libnet_socket_create(LIBNET_SOCK_UDP);
    if (sock < 0) return -2;

    /* Build DNS query */
    uint8_t query_buf[256];
    uint16_t txn_id = (uint16_t)(g_msg_id & 0xFFFF);
    int query_len = _dns_build_query(hostname, query_buf, sizeof(query_buf), txn_id);
    if (query_len < 0) {
        libnet_close(sock);
        return -2;
    }

    /* Send to DNS server port 53 */
    int ret = libnet_udp_sendto(sock, dns_ip, 53, query_buf, (uint16_t)query_len);
    if (ret < 0) {
        libnet_close(sock);
        return -2;
    }

    /* Wait for response — poll for up to 5 seconds */
    uint8_t resp_buf[512];
    int resp_len = 0;
    for (int i = 0; i < 250; i++) {
        resp_len = libnet_recv(sock, resp_buf, sizeof(resp_buf));
        if (resp_len > 0) break;
        _do_heartbeat(i);
        syscall1(SYS_SLEEP, 1); /* ~20ms */
    }

    libnet_close(sock);

    if (resp_len <= 0) return -1; /* Timeout */

    /* Parse DNS response */
    int result = _dns_parse_response(resp_buf, resp_len, txn_id, ip_out);

    /* Cache successful resolution */
    if (result == 0 && *ip_out != 0) {
        _dns_cache_store(hostname, *ip_out);
    }

    return result;
}
