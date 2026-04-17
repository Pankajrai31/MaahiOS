/**
 * MaahiOS Network Manager — Implementation
 *
 * Layer 5 (Kernel Manager). Ring 0.
 * Ethernet framing, ARP resolution, IPv4, ICMP echo (ping).
 *
 * Uses kernel_device_read/write(DEV_NETWORK) to talk to the E1000 driver.
 * Implements polling-based packet receive and ARP table.
 */

#include "network_manager.h"
#include "../device/device_manager.h"
#include "../klog/klog.h"
#include "../time/time_manager.h"
#include "../../drivers/network/e1000.h"

#define TAG "NET"

/* ═══════════════════════════════════════════
 * Byte Order Helpers (x86 is little-endian)
 * ═══════════════════════════════════════════ */
static inline uint16_t htons(uint16_t h) {
    return (h >> 8) | (h << 8);
}
static inline uint16_t ntohs(uint16_t n) {
    return htons(n);
}
static inline uint32_t htonl(uint32_t h) {
    return ((h >> 24) & 0xFF) |
           ((h >>  8) & 0xFF00) |
           ((h <<  8) & 0xFF0000) |
           ((h << 24) & 0xFF000000);
}
static inline uint32_t ntohl(uint32_t n) {
    return htonl(n);
}

/* ═══════════════════════════════════════════
 * Internal State
 * ═══════════════════════════════════════════ */

static int g_net_initialized = 0;

/* Network configuration (host byte order) */
static uint32_t g_ip_addr  = NET_DEFAULT_IP;
static uint32_t g_netmask  = NET_DEFAULT_NETMASK;
static uint32_t g_gateway  = NET_DEFAULT_GATEWAY;
static uint32_t g_dns      = NET_DEFAULT_DNS;
static uint8_t  g_mac[ETH_ALEN];

/* Statistics */
static net_stats_t g_stats;

/* ARP Table */
#define ARP_TABLE_SIZE  16
#define ARP_TIMEOUT_TICKS 500  /* ~10 seconds at 50 Hz */

typedef struct {
    uint32_t ip;
    uint8_t  mac[ETH_ALEN];
    uint8_t  valid;
    uint32_t timestamp;
} arp_entry_t;

static arp_entry_t g_arp_table[ARP_TABLE_SIZE];

/* ICMP Echo tracking */
static uint16_t g_icmp_id  = 0x4D41;   /* 'MA' for MaahiOS */
static uint16_t g_icmp_seq = 0;
static volatile int    g_ping_reply_received = 0;
static volatile uint32_t g_ping_reply_time = 0;
static volatile uint8_t  g_ping_reply_ttl  = 0;
static uint16_t g_ping_expect_seq = 0;

/* Packet buffer for receive processing */
#define PKT_BUF_SIZE 2048
static uint8_t g_pkt_buf[PKT_BUF_SIZE];

static const uint8_t BROADCAST_MAC[ETH_ALEN] = {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF};

/* ═══════════════════════════════════════════
 * Packet Log (ring buffer)
 * ═══════════════════════════════════════════ */

/* Forward declaration — defined below in Utility Functions section */
static uint32_t get_ticks(void);

static net_pkt_log_entry_t g_pkt_log[NET_PKT_LOG_SIZE];
static uint32_t g_pkt_log_head  = 0;   /* Next write index */
static uint32_t g_pkt_log_total = 0;   /* Total packets ever logged */

static void pkt_log_str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void pkt_log_str_append(char *dst, const char *src, int max) {
    int len = 0;
    while (dst[len]) len++;
    int i = 0;
    while (len < max - 1 && src[i]) { dst[len++] = src[i++]; }
    dst[len] = '\0';
}

static void pkt_log_append_ip(char *dst, uint32_t ip, int max) {
    char tmp[16];
    int pos = 0;
    for (int octet = 3; octet >= 0; octet--) {
        uint32_t val = (ip >> (octet * 8)) & 0xFF;
        if (val >= 100) tmp[pos++] = '0' + val / 100;
        if (val >= 10)  tmp[pos++] = '0' + (val / 10) % 10;
        tmp[pos++] = '0' + val % 10;
        if (octet > 0) tmp[pos++] = '.';
    }
    tmp[pos] = '\0';
    pkt_log_str_append(dst, tmp, max);
}

static void pkt_log_append_u16(char *dst, uint16_t val, int max) {
    char tmp[8];
    int pos = 0;
    if (val >= 10000) tmp[pos++] = '0' + val / 10000;
    if (val >= 1000)  tmp[pos++] = '0' + (val / 1000) % 10;
    if (val >= 100)   tmp[pos++] = '0' + (val / 100) % 10;
    if (val >= 10)    tmp[pos++] = '0' + (val / 10) % 10;
    tmp[pos++] = '0' + val % 10;
    tmp[pos] = '\0';
    pkt_log_str_append(dst, tmp, max);
}

static void pkt_log_add(uint8_t direction, uint8_t protocol,
                        uint32_t src_ip, uint32_t dst_ip,
                        uint16_t length, const char *summary) {
    net_pkt_log_entry_t *e = &g_pkt_log[g_pkt_log_head % NET_PKT_LOG_SIZE];
    e->timestamp = get_ticks();
    e->src_ip    = src_ip;
    e->dst_ip    = dst_ip;
    e->length    = length;
    e->direction = direction;
    e->protocol  = protocol;
    pkt_log_str_copy(e->summary, summary ? summary : "", NET_PKT_LOG_SUMMARY);
    g_pkt_log_head++;
    g_pkt_log_total++;
}

/* ═══════════════════════════════════════════
 * Utility Functions
 * ═══════════════════════════════════════════ */

static void mem_copy(void *dst, const void *src, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    const uint8_t *s = (const uint8_t *)src;
    for (uint32_t i = 0; i < n; i++) d[i] = s[i];
}

static void mem_set(void *dst, uint8_t val, uint32_t n) {
    uint8_t *d = (uint8_t *)dst;
    for (uint32_t i = 0; i < n; i++) d[i] = val;
}

static int mem_cmp(const void *a, const void *b, uint32_t n) {
    const uint8_t *pa = (const uint8_t *)a;
    const uint8_t *pb = (const uint8_t *)b;
    for (uint32_t i = 0; i < n; i++) {
        if (pa[i] != pb[i]) return pa[i] - pb[i];
    }
    return 0;
}

/* Internet checksum (RFC 1071) */
static uint16_t ip_checksum(const void *data, int len) {
    const uint16_t *p = (const uint16_t *)data;
    uint32_t sum = 0;
    while (len > 1) {
        sum += *p++;
        len -= 2;
    }
    if (len == 1) {
        sum += *(const uint8_t *)p;
    }
    while (sum >> 16) {
        sum = (sum & 0xFFFF) + (sum >> 16);
    }
    return (uint16_t)(~sum);
}

/* Format IP for logging: returns static buffer */
static uint32_t g_tick_freq = 0;

static uint32_t get_ticks(void) {
    return (uint32_t)kernel_time_get_ticks();
}

static uint32_t ticks_to_ms(uint32_t ticks) {
    if (g_tick_freq == 0) return ticks * 20; /* fallback: assume 50 Hz */
    return (ticks * 1000) / g_tick_freq;
}

/* ═══════════════════════════════════════════
 * ARP Table Operations
 * ═══════════════════════════════════════════ */

static arp_entry_t *arp_lookup(uint32_t ip) {
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (g_arp_table[i].valid && g_arp_table[i].ip == ip) {
            return &g_arp_table[i];
        }
    }
    return (arp_entry_t *)0;
}

static void arp_add(uint32_t ip, const uint8_t *mac) {
    /* Check if already exists → update */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (g_arp_table[i].valid && g_arp_table[i].ip == ip) {
            mem_copy(g_arp_table[i].mac, mac, ETH_ALEN);
            g_arp_table[i].timestamp = get_ticks();
            return;
        }
    }
    /* Find empty slot */
    for (int i = 0; i < ARP_TABLE_SIZE; i++) {
        if (!g_arp_table[i].valid) {
            g_arp_table[i].ip = ip;
            mem_copy(g_arp_table[i].mac, mac, ETH_ALEN);
            g_arp_table[i].valid = 1;
            g_arp_table[i].timestamp = get_ticks();
            KLOG_DEBUG(TAG, "ARP: learned %d.%d.%d.%d",
                       (ip >> 24) & 0xFF, (ip >> 16) & 0xFF,
                       (ip >> 8) & 0xFF, ip & 0xFF);
            return;
        }
    }
    /* Table full: overwrite oldest */
    int oldest = 0;
    for (int i = 1; i < ARP_TABLE_SIZE; i++) {
        if (g_arp_table[i].timestamp < g_arp_table[oldest].timestamp) {
            oldest = i;
        }
    }
    g_arp_table[oldest].ip = ip;
    mem_copy(g_arp_table[oldest].mac, mac, ETH_ALEN);
    g_arp_table[oldest].valid = 1;
    g_arp_table[oldest].timestamp = get_ticks();
}

/* ═══════════════════════════════════════════
 * Ethernet Frame Construction
 * ═══════════════════════════════════════════ */

static int eth_send(const uint8_t *dst_mac, uint16_t ethertype,
                    const void *payload, uint16_t payload_len) {
    uint8_t frame[ETH_FRAME_MAX];
    if (ETH_HLEN + payload_len > ETH_FRAME_MAX) return -1;

    /* Build Ethernet header */
    eth_header_t *eth = (eth_header_t *)frame;
    mem_copy(eth->dst, dst_mac, ETH_ALEN);
    mem_copy(eth->src, g_mac, ETH_ALEN);
    eth->ethertype = htons(ethertype);

    /* Copy payload */
    mem_copy(frame + ETH_HLEN, payload, payload_len);

    uint16_t total = ETH_HLEN + payload_len;
    /* Pad to minimum Ethernet frame size (60 bytes without CRC) */
    if (total < 60) {
        mem_set(frame + total, 0, 60 - total);
        total = 60;
    }

    int ret = e1000_send_packet(frame, total);
    if (ret == 0) {
        g_stats.tx_packets++;
    } else {
        g_stats.tx_errors++;
        KLOG_WARN_HEX(TAG, "eth_send failed ret=", (unsigned)ret);
    }

    /* Log TX packet */
    if (ret == 0) {
        uint8_t proto = NET_PKT_PROTO_OTHER;
        uint32_t sip = g_ip_addr, dip = 0;
        char summary[NET_PKT_LOG_SUMMARY];
        mem_set(summary, 0, sizeof(summary));

        if (ethertype == ETH_TYPE_ARP) {
            proto = NET_PKT_PROTO_ARP;
            sip = 0; dip = 0;
            if (payload_len >= sizeof(arp_packet_t)) {
                const arp_packet_t *a = (const arp_packet_t *)payload;
                uint16_t op = htons(a->opcode); /* already in net order, read */
                op = ntohs(a->opcode);
                dip = ntohl(a->target_ip);
                sip = ntohl(a->sender_ip);
                if (op == ARP_OP_REQUEST) {
                    pkt_log_str_copy(summary, "ARP Who has ", sizeof(summary));
                    pkt_log_append_ip(summary, dip, sizeof(summary));
                } else {
                    pkt_log_str_copy(summary, "ARP Reply ", sizeof(summary));
                    pkt_log_append_ip(summary, sip, sizeof(summary));
                }
            } else {
                pkt_log_str_copy(summary, "ARP", sizeof(summary));
            }
        } else if (ethertype == ETH_TYPE_IPV4 && payload_len >= sizeof(ipv4_header_t)) {
            const ipv4_header_t *ip = (const ipv4_header_t *)payload;
            dip = ntohl(ip->dst_ip);
            if (ip->protocol == IP_PROTO_ICMP) {
                proto = NET_PKT_PROTO_ICMP;
                /* Check ICMP type if enough data */
                uint16_t ihl = (ip->version_ihl & 0x0F) * 4;
                if (payload_len >= ihl + sizeof(icmp_header_t)) {
                    const icmp_header_t *ic = (const icmp_header_t *)(((const uint8_t*)payload) + ihl);
                    if (ic->type == ICMP_TYPE_ECHO_REQUEST) {
                        pkt_log_str_copy(summary, "ICMP Echo Req->", sizeof(summary));
                    } else if (ic->type == ICMP_TYPE_ECHO_REPLY) {
                        pkt_log_str_copy(summary, "ICMP Echo Reply->", sizeof(summary));
                    } else {
                        pkt_log_str_copy(summary, "ICMP->", sizeof(summary));
                    }
                    pkt_log_append_ip(summary, dip, sizeof(summary));
                    pkt_log_str_append(summary, " seq=", sizeof(summary));
                    pkt_log_append_u16(summary, ntohs(ic->sequence), sizeof(summary));
                } else {
                    pkt_log_str_copy(summary, "ICMP->", sizeof(summary));
                    pkt_log_append_ip(summary, dip, sizeof(summary));
                }
            } else {
                proto = NET_PKT_PROTO_IPV4;
                pkt_log_str_copy(summary, "IPv4->", sizeof(summary));
                pkt_log_append_ip(summary, dip, sizeof(summary));
            }
        }
        pkt_log_add(NET_PKT_DIR_TX, proto, sip, dip, total, summary);
    }

    return ret;
}

/* ═══════════════════════════════════════════
 * ARP Send
 * ═══════════════════════════════════════════ */

static int arp_send_request(uint32_t target_ip) {
    arp_packet_t arp;
    mem_set(&arp, 0, sizeof(arp));

    arp.hw_type    = htons(ARP_HW_ETHERNET);
    arp.proto_type = htons(ETH_TYPE_IPV4);
    arp.hw_len     = ETH_ALEN;
    arp.proto_len  = 4;
    arp.opcode     = htons(ARP_OP_REQUEST);

    mem_copy(arp.sender_mac, g_mac, ETH_ALEN);
    arp.sender_ip  = htonl(g_ip_addr);
    mem_set(arp.target_mac, 0, ETH_ALEN);
    arp.target_ip  = htonl(target_ip);

    g_stats.arp_sent++;
    KLOG_DEBUG(TAG, "ARP request for %d.%d.%d.%d",
               (target_ip >> 24) & 0xFF, (target_ip >> 16) & 0xFF,
               (target_ip >> 8) & 0xFF, target_ip & 0xFF);

    return eth_send(BROADCAST_MAC, ETH_TYPE_ARP, &arp, sizeof(arp));
}

static int arp_send_reply(const uint8_t *dst_mac, uint32_t dst_ip) {
    arp_packet_t arp;
    mem_set(&arp, 0, sizeof(arp));

    arp.hw_type    = htons(ARP_HW_ETHERNET);
    arp.proto_type = htons(ETH_TYPE_IPV4);
    arp.hw_len     = ETH_ALEN;
    arp.proto_len  = 4;
    arp.opcode     = htons(ARP_OP_REPLY);

    mem_copy(arp.sender_mac, g_mac, ETH_ALEN);
    arp.sender_ip  = htonl(g_ip_addr);
    mem_copy(arp.target_mac, dst_mac, ETH_ALEN);
    arp.target_ip  = htonl(dst_ip);

    return eth_send(dst_mac, ETH_TYPE_ARP, &arp, sizeof(arp));
}

/* ═══════════════════════════════════════════
 * ARP Resolution (blocking, with retries)
 * ═══════════════════════════════════════════ */

static int arp_resolve(uint32_t ip, uint8_t *mac_out) {
    /* Check if on our subnet → resolve directly; else resolve gateway */
    uint32_t target = ip;
    if ((ip & g_netmask) != (g_ip_addr & g_netmask)) {
        target = g_gateway;
    }

    /* Check ARP table first */
    arp_entry_t *entry = arp_lookup(target);
    if (entry) {
        mem_copy(mac_out, entry->mac, ETH_ALEN);
        return 0;
    }

    /* Send ARP request and poll for reply */
    for (int attempt = 0; attempt < 3; attempt++) {
        arp_send_request(target);

        /* Poll for ~1 second (50 ticks at 50 Hz) */
        uint32_t start = get_ticks();
        while ((get_ticks() - start) < 50) {
            kernel_net_poll(); /* Process incoming packets */
            entry = arp_lookup(target);
            if (entry) {
                mem_copy(mac_out, entry->mac, ETH_ALEN);
                KLOG_DEBUG(TAG, "ARP resolved after %d attempts", attempt + 1);
                return 0;
            }
        }
    }

    KLOG_WARN(TAG, "ARP resolve failed for %d.%d.%d.%d",
              (target >> 24) & 0xFF, (target >> 16) & 0xFF,
              (target >> 8) & 0xFF, target & 0xFF);
    return -1;
}

/* ═══════════════════════════════════════════
 * IPv4 Packet Construction & Sending
 * ═══════════════════════════════════════════ */

/* Forward declaration */
static int ip_send(uint32_t dst_ip, uint8_t proto,
                   const void *payload, uint16_t payload_len);

static uint16_t g_ip_id = 1;

/* Wrappers for tcp.c to call */
int ip_send_ext(uint32_t dst_ip, uint8_t proto,
                const void *payload, uint16_t payload_len) {
    return ip_send(dst_ip, proto, payload, payload_len);
}

uint32_t net_get_local_ip(void) {
    return g_ip_addr;
}

uint16_t net_ip_checksum(const void *data, int len) {
    return ip_checksum(data, len);
}

/* Forward declarations for TCP/UDP packet handlers */
extern void kernel_tcp_handle_rx(uint32_t src_ip, uint32_t dst_ip,
                                 const uint8_t *tcp_data, uint16_t tcp_len);
extern void kernel_udp_handle_rx(uint32_t src_ip, uint32_t dst_ip,
                                 const uint8_t *udp_data, uint16_t udp_len);

static int ip_send(uint32_t dst_ip, uint8_t proto,
                   const void *payload, uint16_t payload_len) {
    /* Resolve MAC */
    uint8_t dst_mac[ETH_ALEN];
    if (arp_resolve(dst_ip, dst_mac) != 0) {
        KLOG_WARN(TAG, "Cannot resolve MAC for IP send");
        return -1;
    }

    /* Build IP + payload buffer */
    uint8_t pkt[ETH_MTU];
    uint16_t ip_total = sizeof(ipv4_header_t) + payload_len;
    if (ip_total > ETH_MTU) return -2;

    ipv4_header_t *ip = (ipv4_header_t *)pkt;
    mem_set(ip, 0, sizeof(ipv4_header_t));

    ip->version_ihl    = 0x45;  /* IPv4, IHL=5 (20 bytes) */
    ip->tos            = 0;
    ip->total_length   = htons(ip_total);
    ip->identification = htons(g_ip_id++);
    ip->flags_fragment = 0;
    ip->ttl            = 64;
    ip->protocol       = proto;
    ip->checksum       = 0;
    ip->src_ip         = htonl(g_ip_addr);
    ip->dst_ip         = htonl(dst_ip);
    ip->checksum       = ip_checksum(ip, sizeof(ipv4_header_t));

    /* Copy payload after IP header */
    mem_copy(pkt + sizeof(ipv4_header_t), payload, payload_len);

    return eth_send(dst_mac, ETH_TYPE_IPV4, pkt, ip_total);
}

/* ═══════════════════════════════════════════
 * ICMP Echo Send
 * ═══════════════════════════════════════════ */

static int icmp_send_echo_request(uint32_t dst_ip, uint16_t seq) {
    /* ICMP echo request with 32 bytes of payload data */
    uint8_t icmp_buf[sizeof(icmp_header_t) + 32];

    icmp_header_t *icmp = (icmp_header_t *)icmp_buf;
    icmp->type       = ICMP_TYPE_ECHO_REQUEST;
    icmp->code       = 0;
    icmp->checksum   = 0;
    icmp->identifier = htons(g_icmp_id);
    icmp->sequence   = htons(seq);

    /* Fill payload with pattern */
    uint8_t *payload = icmp_buf + sizeof(icmp_header_t);
    for (int i = 0; i < 32; i++) {
        payload[i] = (uint8_t)(i + 0x30);
    }

    icmp->checksum = ip_checksum(icmp_buf, sizeof(icmp_buf));

    g_stats.icmp_sent++;
    return ip_send(dst_ip, IP_PROTO_ICMP, icmp_buf, sizeof(icmp_buf));
}

/* ═══════════════════════════════════════════
 * Packet Processing (RX)
 * ═══════════════════════════════════════════ */

static void handle_arp(const uint8_t *pkt, uint16_t len) {
    if (len < sizeof(eth_header_t) + sizeof(arp_packet_t)) return;

    const arp_packet_t *arp = (const arp_packet_t *)(pkt + ETH_HLEN);
    uint16_t opcode = ntohs(arp->opcode);
    uint32_t sender_ip = ntohl(arp->sender_ip);
    uint32_t target_ip = ntohl(arp->target_ip);

    g_stats.arp_received++;

    /* Learn sender's MAC regardless of opcode */
    arp_add(sender_ip, arp->sender_mac);

    /* Log RX ARP */
    {
        char summary[NET_PKT_LOG_SUMMARY];
        mem_set(summary, 0, sizeof(summary));
        if (opcode == ARP_OP_REQUEST) {
            pkt_log_str_copy(summary, "ARP Who has ", sizeof(summary));
            pkt_log_append_ip(summary, target_ip, sizeof(summary));
        } else if (opcode == ARP_OP_REPLY) {
            pkt_log_str_copy(summary, "ARP Reply from ", sizeof(summary));
            pkt_log_append_ip(summary, sender_ip, sizeof(summary));
        } else {
            pkt_log_str_copy(summary, "ARP op=", sizeof(summary));
            pkt_log_append_u16(summary, opcode, sizeof(summary));
        }
        pkt_log_add(NET_PKT_DIR_RX, NET_PKT_PROTO_ARP, sender_ip, target_ip, len, summary);
    }

    if (opcode == ARP_OP_REQUEST && target_ip == g_ip_addr) {
        /* Someone is asking for our MAC → reply */
        KLOG_DEBUG(TAG, "ARP request for us from %d.%d.%d.%d",
                   (sender_ip >> 24) & 0xFF, (sender_ip >> 16) & 0xFF,
                   (sender_ip >> 8) & 0xFF, sender_ip & 0xFF);
        arp_send_reply(arp->sender_mac, sender_ip);
    }
}

static void handle_icmp(const ipv4_header_t *ip_hdr,
                        const uint8_t *icmp_data, uint16_t icmp_len) {
    if (icmp_len < sizeof(icmp_header_t)) return;

    const icmp_header_t *icmp = (const icmp_header_t *)icmp_data;

    if (icmp->type == ICMP_TYPE_ECHO_REPLY) {
        g_stats.icmp_received++;
        uint16_t id  = ntohs(icmp->identifier);
        uint16_t seq = ntohs(icmp->sequence);

        /* Log RX ICMP reply */
        {
            char summary[NET_PKT_LOG_SUMMARY];
            mem_set(summary, 0, sizeof(summary));
            pkt_log_str_copy(summary, "ICMP Echo Reply from ", sizeof(summary));
            pkt_log_append_ip(summary, ntohl(ip_hdr->src_ip), sizeof(summary));
            pkt_log_str_append(summary, " seq=", sizeof(summary));
            pkt_log_append_u16(summary, seq, sizeof(summary));
            pkt_log_add(NET_PKT_DIR_RX, NET_PKT_PROTO_ICMP,
                        ntohl(ip_hdr->src_ip), g_ip_addr, icmp_len, summary);
        }

        if (id == g_icmp_id && seq == g_ping_expect_seq) {
            g_ping_reply_received = 1;
            g_ping_reply_time = get_ticks();
            g_ping_reply_ttl = ip_hdr->ttl;
            KLOG_DEBUG(TAG, "ICMP echo reply seq=%d ttl=%d", (int)seq, (int)ip_hdr->ttl);
        }
    } else if (icmp->type == ICMP_TYPE_ECHO_REQUEST) {
        /* Reply to pings directed at us */
        uint32_t src_ip = ntohl(ip_hdr->src_ip);
        uint16_t payload_len = icmp_len; /* entire ICMP portion */

        /* Log RX ICMP request */
        {
            char summary[NET_PKT_LOG_SUMMARY];
            mem_set(summary, 0, sizeof(summary));
            pkt_log_str_copy(summary, "ICMP Echo Req from ", sizeof(summary));
            pkt_log_append_ip(summary, src_ip, sizeof(summary));
            pkt_log_add(NET_PKT_DIR_RX, NET_PKT_PROTO_ICMP,
                        src_ip, g_ip_addr, icmp_len, summary);
        }

        /* Build reply: copy entire ICMP payload, change type to reply */
        uint8_t reply_buf[512];
        if (icmp_len > sizeof(reply_buf)) return;

        mem_copy(reply_buf, icmp_data, icmp_len);
        icmp_header_t *reply = (icmp_header_t *)reply_buf;
        reply->type     = ICMP_TYPE_ECHO_REPLY;
        reply->checksum = 0;
        reply->checksum = ip_checksum(reply_buf, icmp_len);

        ip_send(src_ip, IP_PROTO_ICMP, reply_buf, payload_len);
        g_stats.icmp_sent++;
    }
}

static void handle_ipv4(const uint8_t *pkt, uint16_t len) {
    if (len < ETH_HLEN + sizeof(ipv4_header_t)) return;

    const ipv4_header_t *ip = (const ipv4_header_t *)(pkt + ETH_HLEN);

    /* Validate version */
    if ((ip->version_ihl >> 4) != 4) return;

    uint16_t ihl = (ip->version_ihl & 0x0F) * 4;
    uint16_t ip_total = ntohs(ip->total_length);

    /* Check destination is us or broadcast */
    uint32_t dst = ntohl(ip->dst_ip);
    if (dst != g_ip_addr && dst != 0xFFFFFFFF) return;

    const uint8_t *payload = pkt + ETH_HLEN + ihl;
    uint16_t payload_len = ip_total - ihl;

    switch (ip->protocol) {
        case IP_PROTO_ICMP:
            handle_icmp(ip, payload, payload_len);
            break;
        case IP_PROTO_TCP:
            kernel_tcp_handle_rx(ntohl(ip->src_ip), dst, payload, payload_len);
            break;
        case IP_PROTO_UDP:
            kernel_udp_handle_rx(ntohl(ip->src_ip), dst, payload, payload_len);
            break;
        default:
            break;
    }

    g_stats.rx_packets++;
}

/* ═══════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════ */

int network_manager_init(void) {
    KLOG_INFO(TAG, "Initializing network manager...");

    /* Zero out state */
    mem_set(&g_stats, 0, sizeof(g_stats));
    mem_set(g_arp_table, 0, sizeof(g_arp_table));

    /* Get tick frequency for timing */
    g_tick_freq = kernel_time_get_tick_frequency();
    KLOG_DEBUG_HEX(TAG, "tick_freq=", g_tick_freq);

    /* Check if E1000 (DEV_NETWORK) was registered */
    if (!device_exists(DEV_NETWORK)) {
        KLOG_WARN(TAG, "No network device found (DEV_NETWORK not registered)");
        g_net_initialized = 0;
        return -1;
    }

    /* Open network device */
    int ret = kernel_device_open(DEV_NETWORK, 0);
    if (ret < 0) {
        KLOG_ERROR_HEX(TAG, "Failed to open DEV_NETWORK ret=", (unsigned)ret);
        return -1;
    }

    /* Get MAC address from driver */
    ret = kernel_device_ioctl(DEV_NETWORK, 1 /* NET_IOCTL_GET_MAC */, g_mac);
    if (ret != 0) {
        KLOG_ERROR(TAG, "Failed to get MAC address");
        return -1;
    }

    KLOG_INFO(TAG, "IP=%d.%d.%d.%d",
              (g_ip_addr >> 24) & 0xFF, (g_ip_addr >> 16) & 0xFF,
              (g_ip_addr >> 8) & 0xFF, g_ip_addr & 0xFF);
    KLOG_INFO(TAG, "GW=%d.%d.%d.%d",
              (g_gateway >> 24) & 0xFF, (g_gateway >> 16) & 0xFF,
              (g_gateway >> 8) & 0xFF, g_gateway & 0xFF);

    g_net_initialized = 1;
    KLOG_INFO(TAG, "Network manager ready");
    return 0;
}

int kernel_net_get_config(net_config_t *config) {
    if (!config) return -1;
    config->ip_addr     = g_ip_addr;
    config->netmask     = g_netmask;
    config->gateway     = g_gateway;
    config->dns         = g_dns;
    mem_copy(config->mac, g_mac, ETH_ALEN);
    config->link_up     = g_net_initialized ? (uint8_t)e1000_link_up() : 0;
    config->initialized = (uint8_t)g_net_initialized;
    return 0;
}

int kernel_net_get_stats(net_stats_t *stats) {
    if (!stats) return -1;
    mem_copy(stats, &g_stats, sizeof(net_stats_t));
    return 0;
}

int kernel_net_ping(uint32_t dst_ip, uint32_t timeout_ms, ping_result_t *result) {
    if (!g_net_initialized) {
        KLOG_ERROR(TAG, "ping: network not initialized");
        return -2;
    }
    if (!result) return -2;

    mem_set(result, 0, sizeof(ping_result_t));
    result->dst_ip = dst_ip;

    /* Increment sequence */
    g_icmp_seq++;
    g_ping_expect_seq   = g_icmp_seq;
    g_ping_reply_received = 0;

    /* Send ICMP echo request */
    uint32_t send_tick = get_ticks();
    int ret = icmp_send_echo_request(dst_ip, g_icmp_seq);
    if (ret != 0) {
        KLOG_ERROR_HEX(TAG, "ping: send failed ret=", (unsigned)ret);
        return -2;
    }

    KLOG_DEBUG(TAG, "ping: sent seq=%d to %d.%d.%d.%d",
               (int)g_icmp_seq,
               (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
               (dst_ip >> 8) & 0xFF, dst_ip & 0xFF);

    /* Calculate timeout in ticks */
    uint32_t timeout_ticks = (timeout_ms * g_tick_freq) / 1000;
    if (timeout_ticks == 0) timeout_ticks = g_tick_freq * 5; /* default 5 sec */

    /* Poll for reply */
    while ((get_ticks() - send_tick) < timeout_ticks) {
        kernel_net_poll();

        if (g_ping_reply_received) {
            result->success = 1;
            result->rtt_ms  = ticks_to_ms(g_ping_reply_time - send_tick);
            result->ttl     = g_ping_reply_ttl;
            result->seq     = g_icmp_seq;
            KLOG_INFO(TAG, "ping: reply seq=%d rtt=%dms ttl=%d",
                      (int)result->seq, (int)result->rtt_ms, (int)result->ttl);
            return 0;
        }
    }

    /* Timeout */
    result->success = 0;
    result->seq     = g_icmp_seq;
    KLOG_WARN(TAG, "ping: timeout for seq=%d", (int)g_icmp_seq);
    return -1;
}

int kernel_net_send_raw(const void *data, uint16_t length) {
    if (!g_net_initialized) return -1;
    return e1000_send_packet(data, length);
}

int kernel_net_recv_raw(void *buffer, uint16_t max_len) {
    if (!g_net_initialized) return -1;
    return e1000_recv_packet(buffer, max_len);
}

void kernel_net_poll(void) {
    if (!g_net_initialized) return;

    /* Process up to 8 packets per poll call */
    for (int i = 0; i < 8; i++) {
        int len = e1000_recv_packet(g_pkt_buf, PKT_BUF_SIZE);
        if (len <= 0) break;

        if ((uint16_t)len < ETH_HLEN) continue;

        const eth_header_t *eth = (const eth_header_t *)g_pkt_buf;
        uint16_t ethertype = ntohs(eth->ethertype);

        switch (ethertype) {
            case ETH_TYPE_ARP:
                handle_arp(g_pkt_buf, (uint16_t)len);
                break;
            case ETH_TYPE_IPV4:
                handle_ipv4(g_pkt_buf, (uint16_t)len);
                break;
            default:
                /* Ignore other protocols */
                break;
        }
    }
}

int kernel_net_is_available(void) {
    return g_net_initialized;
}

int kernel_net_get_pkt_log(net_pkt_log_t *log) {
    if (!log) return -1;
    mem_set(log, 0, sizeof(net_pkt_log_t));
    log->count = g_pkt_log_total;

    /* Copy ring buffer entries in chronological order */
    uint32_t n = (g_pkt_log_total < NET_PKT_LOG_SIZE)
                 ? g_pkt_log_total : NET_PKT_LOG_SIZE;
    log->returned = n;

    uint32_t start = (g_pkt_log_total <= NET_PKT_LOG_SIZE)
                     ? 0 : (g_pkt_log_head % NET_PKT_LOG_SIZE);
    for (uint32_t i = 0; i < n; i++) {
        uint32_t idx = (start + i) % NET_PKT_LOG_SIZE;
        mem_copy(&log->entries[i], &g_pkt_log[idx], sizeof(net_pkt_log_entry_t));
    }
    return 0;
}
