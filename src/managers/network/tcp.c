/**
 * MaahiOS TCP/UDP Socket Implementation
 *
 * Layer 5 (Kernel Manager). Ring 0.
 * Provides UDP sendto/recvfrom and TCP connect/send/recv/close.
 * Uses network_manager.c for IP-level send/recv and ARP resolution.
 *
 * Design:
 *   - 8 sockets max (simple array, no dynamic allocation)
 *   - Each socket has an 8 KB receive ring buffer
 *   - TCP: simplified single-flight (no sliding window)
 *   - TCP connect is blocking (polls until handshake completes or timeout)
 *   - TCP send is blocking (waits for ACK)
 *   - Recv is non-blocking (returns whatever is buffered)
 */

#include "tcp.h"
#include "network_manager.h"
#include "../klog/klog.h"
#include "../time/time_manager.h"

#define TAG "TCP"

/* ═══════════════════════════════════════════
 * External functions from network_manager.c
 * ═══════════════════════════════════════════ */
extern int ip_send_ext(uint32_t dst_ip, uint8_t proto,
                       const void *payload, uint16_t payload_len);
extern uint32_t net_get_local_ip(void);
extern uint16_t net_ip_checksum(const void *data, int len);

/* ═══════════════════════════════════════════
 * Byte order helpers (from network_manager.c)
 * ═══════════════════════════════════════════ */
static inline uint16_t _htons(uint16_t h) { return (h >> 8) | (h << 8); }
static inline uint16_t _ntohs(uint16_t n) { return _htons(n); }
static inline uint32_t _htonl(uint32_t h) {
    return ((h >> 24) & 0xFF) | ((h >> 8) & 0xFF00) |
           ((h << 8) & 0xFF0000) | ((h << 24) & 0xFF000000);
}
static inline uint32_t _ntohl(uint32_t n) { return _htonl(n); }

/* ═══════════════════════════════════════════
 * Internal helpers
 * ═══════════════════════════════════════════ */
static void _memcpy(void *d, const void *s, uint32_t n) {
    uint8_t *dd = (uint8_t*)d; const uint8_t *ss = (const uint8_t*)s;
    for (uint32_t i = 0; i < n; i++) dd[i] = ss[i];
}
static void _memset(void *d, uint8_t v, uint32_t n) {
    uint8_t *dd = (uint8_t*)d;
    for (uint32_t i = 0; i < n; i++) dd[i] = v;
}

static uint32_t _get_ticks(void) {
    return (uint32_t)kernel_time_get_ticks();
}

static uint32_t _tick_freq(void) {
    uint32_t f = kernel_time_get_tick_frequency();
    return f ? f : 50;
}

/* ═══════════════════════════════════════════
 * Socket Table
 * ═══════════════════════════════════════════ */

static net_socket_t g_sockets[MAX_SOCKETS];
static uint16_t g_next_ephemeral_port = 49152;
static int g_sockets_initialized = 0;

static void _init_sockets(void) {
    if (g_sockets_initialized) return;
    _memset(g_sockets, 0, sizeof(g_sockets));
    g_sockets_initialized = 1;
}

static uint16_t _alloc_port(void) {
    uint16_t port = g_next_ephemeral_port++;
    if (g_next_ephemeral_port > 65000) g_next_ephemeral_port = 49152;
    return port;
}

/* ═══════════════════════════════════════════
 * Receive Buffer Operations
 * ═══════════════════════════════════════════ */

static void _rbuf_push(net_socket_t *s, const uint8_t *data, uint16_t len) {
    for (uint16_t i = 0; i < len; i++) {
        if (s->recv_len >= SOCK_RECV_BUF_SIZE) break; /* Full — drop */
        s->recv_buf[s->recv_head] = data[i];
        s->recv_head = (s->recv_head + 1) % SOCK_RECV_BUF_SIZE;
        s->recv_len++;
    }
}

static uint16_t _rbuf_pop(net_socket_t *s, uint8_t *buf, uint16_t max) {
    uint16_t count = 0;
    while (count < max && s->recv_len > 0) {
        buf[count++] = s->recv_buf[s->recv_tail];
        s->recv_tail = (s->recv_tail + 1) % SOCK_RECV_BUF_SIZE;
        s->recv_len--;
    }
    return count;
}

/* ═══════════════════════════════════════════
 * TCP Pseudo-Header Checksum
 * ═══════════════════════════════════════════ */

typedef struct {
    uint32_t src_ip;
    uint32_t dst_ip;
    uint8_t  zero;
    uint8_t  protocol;
    uint16_t tcp_length;
} tcp_pseudo_hdr_t;

static uint16_t _tcp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const void *tcp_seg, uint16_t tcp_len) {
    /* Compute over pseudo-header + TCP segment */
    uint32_t sum = 0;

    /* Pseudo-header — use byte-level access to avoid packed alignment warning */
    tcp_pseudo_hdr_t ph;
    ph.src_ip     = _htonl(src_ip);
    ph.dst_ip     = _htonl(dst_ip);
    ph.zero       = 0;
    ph.protocol   = IP_PROTO_TCP;
    ph.tcp_length = _htons(tcp_len);

    const uint16_t *p = (const uint16_t *)&ph;
    for (int i = 0; i < (int)(sizeof(ph)/2); i++) sum += p[i];

    /* TCP segment */
    p = (const uint16_t *)tcp_seg;
    int len = tcp_len;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len == 1) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    return (uint16_t)(~sum);
}

static uint16_t _udp_checksum(uint32_t src_ip, uint32_t dst_ip,
                              const void *udp_seg, uint16_t udp_len) {
    uint32_t sum = 0;

    /* Pseudo-header — byte-level to avoid packed alignment warning */
    tcp_pseudo_hdr_t ph;
    ph.src_ip     = _htonl(src_ip);
    ph.dst_ip     = _htonl(dst_ip);
    ph.zero       = 0;
    ph.protocol   = IP_PROTO_UDP;
    ph.tcp_length = _htons(udp_len);

    const uint16_t *p = (const uint16_t *)&ph;
    for (int i = 0; i < (int)(sizeof(ph)/2); i++) sum += p[i];

    /* UDP segment */
    p = (const uint16_t *)udp_seg;
    int len = udp_len;
    while (len > 1) { sum += *p++; len -= 2; }
    if (len == 1) sum += *(const uint8_t *)p;

    while (sum >> 16) sum = (sum & 0xFFFF) + (sum >> 16);
    uint16_t result = (uint16_t)(~sum);
    return result == 0 ? 0xFFFF : result; /* UDP: 0 means no checksum */
}

/* ═══════════════════════════════════════════
 * TCP Segment Send
 * ═══════════════════════════════════════════ */

static int _tcp_send_segment(net_socket_t *s, uint8_t flags,
                             const void *data, uint16_t data_len) {
    uint8_t seg[sizeof(tcp_header_t) + 1460]; /* MSS */
    if (data_len > 1460) data_len = 1460;

    uint16_t tcp_len = sizeof(tcp_header_t) + data_len;

    tcp_header_t *tcp = (tcp_header_t *)seg;
    _memset(tcp, 0, sizeof(tcp_header_t));

    tcp->src_port    = _htons(s->local_port);
    tcp->dst_port    = _htons(s->remote_port);
    tcp->seq_num     = _htonl(s->snd_nxt);
    tcp->ack_num     = _htonl(s->rcv_nxt);
    tcp->data_offset = (5 << 4);   /* 20 bytes, no options */
    tcp->flags       = flags;
    tcp->window      = _htons(SOCK_RECV_BUF_SIZE - s->recv_len);
    tcp->checksum    = 0;
    tcp->urgent_ptr  = 0;

    if (data && data_len > 0) {
        _memcpy(seg + sizeof(tcp_header_t), data, data_len);
    }

    tcp->checksum = _tcp_checksum(s->local_ip, s->remote_ip, seg, tcp_len);

    int ret = ip_send_ext(s->remote_ip, IP_PROTO_TCP, seg, tcp_len);

    /* Advance send sequence */
    if (flags & TCP_SYN) s->snd_nxt++;
    if (flags & TCP_FIN) s->snd_nxt++;
    s->snd_nxt += data_len;

    s->last_activity = _get_ticks();
    return ret;
}

/* ═══════════════════════════════════════════
 * UDP Datagram Send
 * ═══════════════════════════════════════════ */

static int _udp_send(net_socket_t *s, uint32_t dst_ip, uint16_t dst_port,
                     const void *data, uint16_t data_len) {
    uint8_t pkt[sizeof(udp_header_t) + 1472]; /* Max UDP payload */
    if (data_len > 1472) data_len = 1472;

    uint16_t udp_len = sizeof(udp_header_t) + data_len;

    udp_header_t *udp = (udp_header_t *)pkt;
    udp->src_port = _htons(s->local_port);
    udp->dst_port = _htons(dst_port);
    udp->length   = _htons(udp_len);
    udp->checksum = 0;

    if (data && data_len > 0) {
        _memcpy(pkt + sizeof(udp_header_t), data, data_len);
    }

    udp->checksum = _udp_checksum(s->local_ip, dst_ip, pkt, udp_len);

    return ip_send_ext(dst_ip, IP_PROTO_UDP, pkt, udp_len);
}

/* ═══════════════════════════════════════════
 * Incoming Packet Handlers (called from network_manager poll)
 * ═══════════════════════════════════════════ */

/**
 * Handle incoming TCP segment.
 * Called from network_manager.c when an IPv4 packet with proto=TCP arrives.
 */
void kernel_tcp_handle_rx(uint32_t src_ip, uint32_t dst_ip,
                          const uint8_t *tcp_data, uint16_t tcp_len) {
    if (tcp_len < sizeof(tcp_header_t)) return;

    const tcp_header_t *tcp = (const tcp_header_t *)tcp_data;
    uint16_t src_port = _ntohs(tcp->src_port);
    uint16_t dst_port = _ntohs(tcp->dst_port);
    uint32_t seq      = _ntohl(tcp->seq_num);
    uint32_t ack      = _ntohl(tcp->ack_num);
    uint8_t  flags    = tcp->flags;
    uint16_t window   = _ntohs(tcp->window);
    uint16_t hdr_len  = (tcp->data_offset >> 4) * 4;

    /* Find matching socket */
    net_socket_t *s = (net_socket_t *)0;
    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (g_sockets[i].in_use && g_sockets[i].type == SOCK_TYPE_TCP &&
            g_sockets[i].remote_ip == src_ip &&
            g_sockets[i].remote_port == src_port &&
            g_sockets[i].local_port == dst_port) {
            s = &g_sockets[i];
            break;
        }
    }

    if (!s) return; /* No matching socket — drop */

    s->last_activity = _get_ticks();
    s->snd_wnd = window;

    switch (s->tcp_state) {
        case TCP_STATE_SYN_SENT:
            /* Expecting SYN+ACK */
            if ((flags & (TCP_SYN | TCP_ACK)) == (TCP_SYN | TCP_ACK)) {
                if (ack == s->snd_nxt) {
                    s->rcv_nxt = seq + 1;
                    s->snd_una = ack;
                    s->tcp_state = TCP_STATE_ESTABLISHED;
                    s->connected = 1;

                    /* Send ACK */
                    _tcp_send_segment(s, TCP_ACK, (void*)0, 0);

                    KLOG_INFO(TAG, "TCP connected to %d.%d.%d.%d:%d",
                              (src_ip >> 24) & 0xFF, (src_ip >> 16) & 0xFF,
                              (src_ip >> 8) & 0xFF, src_ip & 0xFF, src_port);
                }
            } else if (flags & TCP_RST) {
                s->tcp_state = TCP_STATE_CLOSED;
                s->error = 1;
                KLOG_WARN(TAG, "TCP connection refused (RST)");
            }
            break;

        case TCP_STATE_ESTABLISHED:
            /* Update send window */
            if (flags & TCP_ACK) {
                s->snd_una = ack;
            }

            /* Handle incoming data */
            if (tcp_len > hdr_len) {
                const uint8_t *payload = tcp_data + hdr_len;
                uint16_t payload_len = tcp_len - hdr_len;

                if (seq == s->rcv_nxt) {
                    _rbuf_push(s, payload, payload_len);
                    s->rcv_nxt += payload_len;

                    /* ACK the data */
                    _tcp_send_segment(s, TCP_ACK, (void*)0, 0);
                }
                /* Out-of-order: drop (simplified — no reordering) */
            }

            /* Handle FIN */
            if (flags & TCP_FIN) {
                s->rcv_nxt = seq + 1;
                if (tcp_len > hdr_len) {
                    /* FIN after data — rcv_nxt already advanced by data */
                    s->rcv_nxt++;
                }
                s->peer_closed = 1;
                s->tcp_state = TCP_STATE_CLOSE_WAIT;

                /* ACK the FIN */
                _tcp_send_segment(s, TCP_ACK, (void*)0, 0);
                KLOG_DEBUG(TAG, "TCP peer FIN, entering CLOSE_WAIT");
            }

            if (flags & TCP_RST) {
                s->tcp_state = TCP_STATE_CLOSED;
                s->error = 1;
                s->peer_closed = 1;
                KLOG_WARN(TAG, "TCP RST received");
            }
            break;

        case TCP_STATE_FIN_WAIT_1:
            if ((flags & TCP_ACK) && ack == s->snd_nxt) {
                if (flags & TCP_FIN) {
                    /* Simultaneous close: FIN+ACK */
                    s->rcv_nxt = seq + 1;
                    s->tcp_state = TCP_STATE_TIME_WAIT;
                    _tcp_send_segment(s, TCP_ACK, (void*)0, 0);
                } else {
                    s->tcp_state = TCP_STATE_FIN_WAIT_2;
                }
                s->snd_una = ack;
            }
            break;

        case TCP_STATE_FIN_WAIT_2:
            if (flags & TCP_FIN) {
                s->rcv_nxt = seq + 1;
                s->tcp_state = TCP_STATE_TIME_WAIT;
                _tcp_send_segment(s, TCP_ACK, (void*)0, 0);
            }
            /* Still accept data before FIN */
            if (tcp_len > hdr_len && seq == s->rcv_nxt) {
                const uint8_t *payload = tcp_data + hdr_len;
                uint16_t payload_len = tcp_len - hdr_len;
                _rbuf_push(s, payload, payload_len);
                s->rcv_nxt += payload_len;
                _tcp_send_segment(s, TCP_ACK, (void*)0, 0);
            }
            break;

        case TCP_STATE_LAST_ACK:
            if ((flags & TCP_ACK) && ack == s->snd_nxt) {
                s->tcp_state = TCP_STATE_CLOSED;
                s->in_use = 0;
                KLOG_DEBUG(TAG, "TCP LAST_ACK -> CLOSED");
            }
            break;

        case TCP_STATE_TIME_WAIT:
            /* Just stay here; will be cleaned up on close */
            break;

        default:
            break;
    }
}

/**
 * Handle incoming UDP datagram.
 * Called from network_manager.c when an IPv4 packet with proto=UDP arrives.
 */
void kernel_udp_handle_rx(uint32_t src_ip, uint32_t dst_ip,
                          const uint8_t *udp_data, uint16_t udp_len) {
    if (udp_len < sizeof(udp_header_t)) return;

    const udp_header_t *udp = (const udp_header_t *)udp_data;
    uint16_t src_port = _ntohs(udp->src_port);
    uint16_t dst_port = _ntohs(udp->dst_port);
    uint16_t data_len = _ntohs(udp->length) - sizeof(udp_header_t);
    const uint8_t *payload = udp_data + sizeof(udp_header_t);

    /* Find matching socket */
    for (int i = 0; i < MAX_SOCKETS; i++) {
        net_socket_t *s = &g_sockets[i];
        if (s->in_use && s->type == SOCK_TYPE_UDP &&
            s->local_port == dst_port) {
            /* Store sender info for recvfrom pattern */
            s->remote_ip   = src_ip;
            s->remote_port = src_port;
            _rbuf_push(s, payload, data_len);
            s->last_activity = _get_ticks();
            return;
        }
    }
    /* No socket listening — drop */
}

/* ═══════════════════════════════════════════
 * Public Socket API
 * ═══════════════════════════════════════════ */

int kernel_socket_create(int type, int owner_pid) {
    _init_sockets();

    if (type != SOCK_TYPE_UDP && type != SOCK_TYPE_TCP) return -1;

    for (int i = 0; i < MAX_SOCKETS; i++) {
        if (!g_sockets[i].in_use) {
            _memset(&g_sockets[i], 0, sizeof(net_socket_t));
            g_sockets[i].type       = (uint8_t)type;
            g_sockets[i].in_use     = 1;
            g_sockets[i].local_ip   = net_get_local_ip();
            g_sockets[i].local_port = _alloc_port();
            g_sockets[i].rcv_wnd    = SOCK_RECV_BUF_SIZE;
            g_sockets[i].owner_pid  = owner_pid;
            g_sockets[i].tcp_state  = TCP_STATE_CLOSED;

            KLOG_DEBUG_HEX(TAG, "Socket created, handle:", i);
            return i;
        }
    }

    KLOG_WARN(TAG, "No free sockets");
    return -1;
}

int kernel_socket_connect(int sock, uint32_t remote_ip, uint16_t remote_port) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    net_socket_t *s = &g_sockets[sock];
    if (!s->in_use || s->type != SOCK_TYPE_TCP) return -1;

    s->remote_ip   = remote_ip;
    s->remote_port = remote_port;

    /* Initialize TCP sequence numbers */
    /* Use ticks as initial sequence (simple, not secure — fine for hobby OS) */
    s->snd_nxt    = _get_ticks() * 12345 + 1;
    s->snd_una    = s->snd_nxt;
    s->rcv_nxt    = 0;
    s->tcp_state  = TCP_STATE_SYN_SENT;
    s->connected  = 0;
    s->error      = 0;
    s->peer_closed = 0;

    KLOG_INFO(TAG, "TCP connecting to %d.%d.%d.%d:%d",
              (remote_ip >> 24) & 0xFF, (remote_ip >> 16) & 0xFF,
              (remote_ip >> 8) & 0xFF, remote_ip & 0xFF, remote_port);

    /* Send SYN */
    int ret = _tcp_send_segment(s, TCP_SYN, (void*)0, 0);
    if (ret != 0) {
        KLOG_ERROR(TAG, "TCP SYN send failed");
        s->tcp_state = TCP_STATE_CLOSED;
        return -2;
    }

    /* Poll until connected or timeout (10 seconds) */
    uint32_t deadline = _get_ticks() + _tick_freq() * 10;
    int retry = 0;

    while (_get_ticks() < deadline) {
        kernel_net_poll();

        if (s->connected) return 0;            /* Success! */
        if (s->error) return -3;               /* RST received */

        /* Retransmit SYN every ~2 seconds */
        if ((_get_ticks() - s->last_activity) > _tick_freq() * 2 && retry < 3) {
            s->snd_nxt = s->snd_una; /* Reset seq for retransmit */
            _tcp_send_segment(s, TCP_SYN, (void*)0, 0);
            retry++;
            KLOG_DEBUG(TAG, "TCP SYN retransmit #%d", retry);
        }
    }

    /* Timeout */
    s->tcp_state = TCP_STATE_CLOSED;
    KLOG_WARN(TAG, "TCP connect timeout");
    return -1;
}

int kernel_socket_send(int sock, const void *data, uint16_t len) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    net_socket_t *s = &g_sockets[sock];
    if (!s->in_use) return -1;

    if (s->type == SOCK_TYPE_TCP) {
        if (s->tcp_state != TCP_STATE_ESTABLISHED) return -1;

        /* Send in MSS-sized chunks */
        const uint8_t *ptr = (const uint8_t *)data;
        uint16_t sent = 0;

        while (sent < len) {
            uint16_t chunk = len - sent;
            if (chunk > 1460) chunk = 1460;

            int ret = _tcp_send_segment(s, TCP_ACK | TCP_PSH, ptr + sent, chunk);
            if (ret != 0) return (sent > 0) ? (int)sent : -1;

            /* Wait for ACK (simplified — poll up to 5 sec) */
            uint32_t ack_deadline = _get_ticks() + _tick_freq() * 5;
            while (_get_ticks() < ack_deadline) {
                kernel_net_poll();
                if (s->snd_una >= s->snd_nxt) break; /* ACKed */
                if (s->error) return -1;
            }

            if (s->snd_una < s->snd_nxt) {
                KLOG_WARN(TAG, "TCP send: ACK timeout");
                return (sent > 0) ? (int)sent : -1;
            }

            sent += chunk;
        }
        return (int)sent;

    } else if (s->type == SOCK_TYPE_UDP) {
        /* For UDP, must have remote set via connect or sendto */
        if (s->remote_ip == 0) return -1;
        return _udp_send(s, s->remote_ip, s->remote_port, data, len);
    }

    return -1;
}

int kernel_socket_recv(int sock, void *buf, uint16_t max_len) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    net_socket_t *s = &g_sockets[sock];
    if (!s->in_use) return -1;

    /* Poll to pick up any pending packets */
    kernel_net_poll();

    if (s->recv_len == 0) {
        if (s->peer_closed) return -2; /* EOF */
        return 0; /* No data yet */
    }

    uint16_t prev_len = s->recv_len;
    int got = (int)_rbuf_pop(s, (uint8_t *)buf, max_len);

    /* Send window-update ACK so the peer knows it can send more.
     * Only send when significant space freed (avoids silly-window). */
    if (got > 0 && s->type == SOCK_TYPE_TCP &&
        s->tcp_state == TCP_STATE_ESTABLISHED) {
        uint16_t prev_free = SOCK_RECV_BUF_SIZE - prev_len;
        uint16_t now_free  = SOCK_RECV_BUF_SIZE - s->recv_len;
        /* Send update if window was below MSS and now above, or large read */
        if (prev_free < 1460 && now_free >= 1460) {
            _tcp_send_segment(s, TCP_ACK, (void*)0, 0);
        }
    }

    return got;
}

int kernel_socket_close(int sock) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    net_socket_t *s = &g_sockets[sock];
    if (!s->in_use) return -1;

    if (s->type == SOCK_TYPE_TCP &&
        s->tcp_state == TCP_STATE_ESTABLISHED) {
        /* Send FIN */
        s->tcp_state = TCP_STATE_FIN_WAIT_1;
        _tcp_send_segment(s, TCP_FIN | TCP_ACK, (void*)0, 0);

        /* Wait briefly for FIN-ACK (2 seconds max) */
        uint32_t deadline = _get_ticks() + _tick_freq() * 2;
        while (_get_ticks() < deadline &&
               s->tcp_state != TCP_STATE_TIME_WAIT &&
               s->tcp_state != TCP_STATE_CLOSED) {
            kernel_net_poll();
        }
    } else if (s->type == SOCK_TYPE_TCP &&
               s->tcp_state == TCP_STATE_CLOSE_WAIT) {
        /* Peer already sent FIN — send our FIN */
        s->tcp_state = TCP_STATE_LAST_ACK;
        _tcp_send_segment(s, TCP_FIN | TCP_ACK, (void*)0, 0);

        uint32_t deadline = _get_ticks() + _tick_freq() * 2;
        while (_get_ticks() < deadline &&
               s->tcp_state != TCP_STATE_CLOSED) {
            kernel_net_poll();
        }
    }

    KLOG_DEBUG_HEX(TAG, "Socket closed, handle:", sock);
    _memset(s, 0, sizeof(net_socket_t));
    return 0;
}

int kernel_udp_sendto(int sock, uint32_t dst_ip, uint16_t dst_port,
                      const void *data, uint16_t len) {
    if (sock < 0 || sock >= MAX_SOCKETS) return -1;
    net_socket_t *s = &g_sockets[sock];
    if (!s->in_use || s->type != SOCK_TYPE_UDP) return -1;

    return _udp_send(s, dst_ip, dst_port, data, len);
}

void kernel_net_poll_sockets(void) {
    /* Just poll the network manager — TCP/UDP handlers are called
     * from handle_ipv4() in network_manager.c */
    kernel_net_poll();
}
