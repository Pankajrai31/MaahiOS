/**
 * MaahiOS Network Manager — Header
 *
 * Layer 5 (Kernel Manager). Ring 0.
 * Provides Ethernet framing, ARP, IPv4, and ICMP
 * on top of the E1000 device driver.
 *
 * The network manager is the ONLY code that directly
 * touches the NIC via kernel_device_read/write.
 */

#ifndef NETWORK_MANAGER_H
#define NETWORK_MANAGER_H

#include <stdint.h>

/* ═══════════════════════════════════════════
 * Network Configuration
 * ═══════════════════════════════════════════ */

/* QEMU SLiRP defaults */
#define NET_DEFAULT_IP       0x0A00020F  /* 10.0.2.15 */
#define NET_DEFAULT_GATEWAY  0x0A000202  /* 10.0.2.2  */
#define NET_DEFAULT_NETMASK  0xFFFFFF00  /* 255.255.255.0 */
#define NET_DEFAULT_DNS      0x0A000203  /* 10.0.2.3  */

/* ═══════════════════════════════════════════
 * Ethernet Constants
 * ═══════════════════════════════════════════ */
#define ETH_ALEN            6       /* MAC address length */
#define ETH_HLEN            14      /* Ethernet header length */
#define ETH_MTU             1500    /* Max payload */
#define ETH_FRAME_MAX       1518    /* Max frame (header+payload+CRC) */

#define ETH_TYPE_IPV4       0x0800
#define ETH_TYPE_ARP        0x0806

/* ═══════════════════════════════════════════
 * IP Protocol Numbers
 * ═══════════════════════════════════════════ */
#define IP_PROTO_ICMP       1
#define IP_PROTO_TCP        6
#define IP_PROTO_UDP        17

/* ═══════════════════════════════════════════
 * ICMP Types
 * ═══════════════════════════════════════════ */
#define ICMP_TYPE_ECHO_REPLY    0
#define ICMP_TYPE_ECHO_REQUEST  8

/* ═══════════════════════════════════════════
 * ARP Constants
 * ═══════════════════════════════════════════ */
#define ARP_HW_ETHERNET     1
#define ARP_OP_REQUEST      1
#define ARP_OP_REPLY        2

/* ═══════════════════════════════════════════
 * Protocol Structures (packed, network byte order)
 * ═══════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint8_t  dst[ETH_ALEN];
    uint8_t  src[ETH_ALEN];
    uint16_t ethertype;
} eth_header_t;

typedef struct __attribute__((packed)) {
    uint16_t hw_type;
    uint16_t proto_type;
    uint8_t  hw_len;
    uint8_t  proto_len;
    uint16_t opcode;
    uint8_t  sender_mac[ETH_ALEN];
    uint32_t sender_ip;
    uint8_t  target_mac[ETH_ALEN];
    uint32_t target_ip;
} arp_packet_t;

typedef struct __attribute__((packed)) {
    uint8_t  version_ihl;   /* version (4 bits) + IHL (4 bits) */
    uint8_t  tos;
    uint16_t total_length;
    uint16_t identification;
    uint16_t flags_fragment;
    uint8_t  ttl;
    uint8_t  protocol;
    uint16_t checksum;
    uint32_t src_ip;
    uint32_t dst_ip;
} ipv4_header_t;

typedef struct __attribute__((packed)) {
    uint8_t  type;
    uint8_t  code;
    uint16_t checksum;
    uint16_t identifier;
    uint16_t sequence;
} icmp_header_t;

/* ═══════════════════════════════════════════
 * Network Status / Config Structures
 * ═══════════════════════════════════════════ */

typedef struct {
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[ETH_ALEN];
    uint8_t  link_up;
    uint8_t  initialized;
} net_config_t;

typedef struct {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t tx_errors;
    uint32_t rx_errors;
    uint32_t arp_sent;
    uint32_t arp_received;
    uint32_t icmp_sent;
    uint32_t icmp_received;
} net_stats_t;

/* Ping result */
typedef struct {
    int      success;       /* 1 = reply received, 0 = timeout */
    uint32_t rtt_ms;        /* Round-trip time in milliseconds */
    uint8_t  ttl;           /* TTL from reply */
    uint16_t seq;           /* Sequence number */
    uint32_t dst_ip;        /* Target IP */
} ping_result_t;

/* ═══════════════════════════════════════════
 * Packet Log (ring buffer of recent packets)
 * ═══════════════════════════════════════════ */

#define NET_PKT_LOG_SIZE     64     /* Ring buffer capacity */
#define NET_PKT_LOG_SUMMARY  48     /* Bytes of summary text per entry */

/* Direction */
#define NET_PKT_DIR_TX  0
#define NET_PKT_DIR_RX  1

/* Protocol type tag */
#define NET_PKT_PROTO_ARP   1
#define NET_PKT_PROTO_ICMP  2
#define NET_PKT_PROTO_IPV4  3
#define NET_PKT_PROTO_OTHER 4

typedef struct {
    uint32_t timestamp;                /* Tick count when logged */
    uint32_t src_ip;                   /* Source IP (host order, 0 for ARP) */
    uint32_t dst_ip;                   /* Dest IP (host order, 0 for ARP) */
    uint16_t length;                   /* Frame length in bytes */
    uint8_t  direction;                /* NET_PKT_DIR_TX or NET_PKT_DIR_RX */
    uint8_t  protocol;                 /* NET_PKT_PROTO_* */
    char     summary[NET_PKT_LOG_SUMMARY]; /* Human-readable summary */
} net_pkt_log_entry_t;

typedef struct {
    uint32_t count;                    /* Total packets ever logged */
    uint32_t returned;                 /* Entries returned in this call */
    net_pkt_log_entry_t entries[NET_PKT_LOG_SIZE];
} net_pkt_log_t;

/* ═══════════════════════════════════════════
 * Manager API (Kernel Space)
 * ═══════════════════════════════════════════ */

/**
 * Initialize the network manager.
 * Call AFTER device_manager_init() so E1000 is available.
 * @return 0 on success, -1 if NIC not available
 */
int network_manager_init(void);

/**
 * Get current network configuration.
 */
int kernel_net_get_config(net_config_t *config);

/**
 * Get network statistics.
 */
int kernel_net_get_stats(net_stats_t *stats);

/**
 * Send an ICMP echo request (ping) and wait for reply.
 * @param dst_ip    Target IP address (host byte order)
 * @param timeout_ms Timeout in milliseconds
 * @param result    Output ping result
 * @return 0 on success (reply received), -1 on timeout, -2 on error
 */
int kernel_net_ping(uint32_t dst_ip, uint32_t timeout_ms, ping_result_t *result);

/**
 * Send a raw Ethernet frame.
 * @param data   Complete frame data
 * @param length Frame length
 * @return 0 on success, negative on error
 */
int kernel_net_send_raw(const void *data, uint16_t length);

/**
 * Receive a raw Ethernet frame (polling).
 * @param buffer Output buffer
 * @param max_len Buffer size
 * @return Bytes received, 0 if none, negative on error
 */
int kernel_net_recv_raw(void *buffer, uint16_t max_len);

/**
 * Process pending incoming packets (call periodically).
 * Handles ARP replies, ICMP, etc.
 */
void kernel_net_poll(void);

/**
 * Check if network is available.
 * @return 1 if NIC is initialized and link up, 0 otherwise
 */
int kernel_net_is_available(void);

/**
 * Get recent packet log entries.
 * @param log    Output buffer for packet log (ring buffer snapshot)
 * @return 0 on success, negative on error
 */
int kernel_net_get_pkt_log(net_pkt_log_t *log);

#endif /* NETWORK_MANAGER_H */
