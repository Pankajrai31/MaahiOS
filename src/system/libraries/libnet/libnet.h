/**
 * MaahiOS Network Library - libnet.h
 *
 * Description:
 *   User library for network operations.
 *   Auto-initializes on first call (discovers SHM, attaches to Network Executive).
 *   
 *   Operations:
 *   - Ping a host (ICMP echo)
 *   - Get network configuration (IP, MAC, gateway)
 *   - Get network statistics
 *
 * Usage:
 *   #include "libnet.h"
 *   
 *   libnet_ping_result_t result;
 *   int ret = libnet_ping(0x0A000202, 3000, &result);  // ping 10.0.2.2
 *   
 *   libnet_config_t config;
 *   libnet_get_config(&config);
 *   
 *   No init() needed — handled automatically.
 *
 * Layer 2 (Library). Ring 3.
 * Talks to Network Executive via SHM queue.
 */

#ifndef LIBNET_H
#define LIBNET_H

#include <stdint.h>

/*=============================================================================
 * NETWORK CONFIGURATION
 *===========================================================================*/

#define LIBNET_MAC_LEN  6

typedef struct {
    uint32_t ip_addr;       /* Host byte order */
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[LIBNET_MAC_LEN];
    uint8_t  link_up;
    uint8_t  initialized;
} libnet_config_t;

/*=============================================================================
 * NETWORK STATISTICS
 *===========================================================================*/

typedef struct {
    uint32_t tx_packets;
    uint32_t rx_packets;
    uint32_t tx_errors;
    uint32_t rx_errors;
    uint32_t arp_sent;
    uint32_t arp_received;
    uint32_t icmp_sent;
    uint32_t icmp_received;
} libnet_stats_t;

/*=============================================================================
 * PING RESULT
 *===========================================================================*/

typedef struct {
    int      success;       /* 1 = reply received, 0 = timeout */
    uint32_t rtt_ms;        /* Round-trip time in milliseconds */
    uint8_t  ttl;           /* TTL from reply */
    uint8_t  _pad[3];
    uint16_t seq;           /* Sequence number */
    uint16_t _pad2;
    uint32_t dst_ip;        /* Target IP */
} libnet_ping_result_t;

/*=============================================================================
 * PACKET LOG
 *===========================================================================*/

#define LIBNET_PKT_LOG_SIZE     64
#define LIBNET_PKT_LOG_SUMMARY  48

#define LIBNET_PKT_DIR_TX   0
#define LIBNET_PKT_DIR_RX   1

#define LIBNET_PKT_PROTO_ARP    1
#define LIBNET_PKT_PROTO_ICMP   2
#define LIBNET_PKT_PROTO_IPV4   3
#define LIBNET_PKT_PROTO_OTHER  4

typedef struct {
    uint32_t timestamp;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t length;
    uint8_t  direction;
    uint8_t  protocol;
    char     summary[LIBNET_PKT_LOG_SUMMARY];
} libnet_pkt_log_entry_t;

typedef struct {
    uint32_t count;                    /* Total packets ever logged */
    uint32_t returned;                 /* Entries in this snapshot */
    libnet_pkt_log_entry_t entries[LIBNET_PKT_LOG_SIZE];
} libnet_pkt_log_t;

/*=============================================================================
 * IP ADDRESS HELPER
 *===========================================================================*/

/** Build IPv4 address from 4 octets: IP(10,0,2,2) = 0x0A000202 */
#define LIBNET_IP(a, b, c, d) \
    (((uint32_t)(a) << 24) | ((uint32_t)(b) << 16) | \
     ((uint32_t)(c) << 8)  | (uint32_t)(d))

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

/**
 * libnet_init — Explicitly initialize (optional, auto-called on first use)
 * @return 0 on success, -1 if Network Executive not available
 */
int libnet_init(void);

/**
 * libnet_ping — Send ICMP echo request and wait for reply
 * @param dst_ip      Target IP (host byte order) — use LIBNET_IP() macro
 * @param timeout_ms  Timeout in milliseconds (0 = default 5 sec)
 * @param result      Output ping result structure
 * @return 0 on success (reply received), -1 on timeout, -2 on error
 */
int libnet_ping(uint32_t dst_ip, uint32_t timeout_ms, libnet_ping_result_t *result);

/**
 * libnet_get_config — Get network interface configuration
 * @param config  Output configuration structure
 * @return 0 on success, negative on error
 */
int libnet_get_config(libnet_config_t *config);

/**
 * libnet_get_stats — Get network statistics
 * @param stats  Output statistics structure
 * @return 0 on success, negative on error
 */
int libnet_get_stats(libnet_stats_t *stats);

/**
 * libnet_get_pkt_log — Get snapshot of recent packet log
 * @param log  Output packet log structure
 * @return 0 on success, negative on error
 */
int libnet_get_pkt_log(libnet_pkt_log_t *log);

/**
 * libnet_is_available — Check if networking is available
 * @return 1 if network initialized, 0 otherwise
 */
int libnet_is_available(void);

/*=============================================================================
 * SOCKET API
 *===========================================================================*/

/** Socket types */
#define LIBNET_SOCK_UDP   1
#define LIBNET_SOCK_TCP   2

/**
 * libnet_socket_create — Create a network socket
 * @param type  LIBNET_SOCK_UDP or LIBNET_SOCK_TCP
 * @return socket handle (>=0) on success, negative on error
 */
int libnet_socket_create(int type);

/**
 * libnet_connect — TCP connect to remote host
 * @param sock        Socket handle from libnet_socket_create
 * @param remote_ip   Target IP (host byte order) — use LIBNET_IP()
 * @param remote_port Target port
 * @return 0 on success, negative on error
 */
int libnet_connect(int sock, uint32_t remote_ip, uint16_t remote_port);

/**
 * libnet_send — Send data on a connected socket
 * @param sock   Socket handle
 * @param data   Pointer to data to send
 * @param len    Number of bytes to send (max ~240 per call due to SHM payload limit)
 * @return bytes sent on success, negative on error
 */
int libnet_send(int sock, const void *data, uint16_t len);

/**
 * libnet_recv — Receive data from a socket (non-blocking)
 * @param sock      Socket handle
 * @param buf       Output buffer
 * @param max_len   Maximum bytes to receive (max ~256)
 * @return bytes received (>0), 0 if no data, negative on error
 */
int libnet_recv(int sock, void *buf, uint16_t max_len);

/**
 * libnet_recv_bulk — Direct-syscall bulk receive (bypasses executive IPC)
 * @param sock      Socket handle
 * @param buf       Output buffer
 * @param max_len   Maximum bytes to receive (up to 32768)
 * @return bytes received (>0), 0 if no data, negative on error
 */
int libnet_recv_bulk(int sock, void *buf, int max_len);

/**
 * libnet_close — Close a socket
 * @param sock  Socket handle
 * @return 0 on success, negative on error
 */
int libnet_close(int sock);

/**
 * libnet_udp_sendto — Send UDP datagram to a specific host
 * @param sock     Socket handle (must be UDP)
 * @param dst_ip   Destination IP (host byte order)
 * @param dst_port Destination port
 * @param data     Pointer to data
 * @param len      Number of bytes to send
 * @return bytes sent on success, negative on error
 */
int libnet_udp_sendto(int sock, uint32_t dst_ip, uint16_t dst_port,
                      const void *data, uint16_t len);

/*=============================================================================
 * DNS RESOLVER
 *===========================================================================*/

/**
 * libnet_dns_resolve — Resolve hostname to IP address via DNS
 * @param hostname  Null-terminated hostname (e.g. "example.com")
 * @param ip_out    Output IP address (host byte order)
 * @return 0 on success, negative on error (-1 = timeout, -2 = error)
 */
int libnet_dns_resolve(const char *hostname, uint32_t *ip_out);

/*=============================================================================
 * HEARTBEAT CALLBACK (keep-alive during long operations)
 *===========================================================================*/

/**
 * libnet_set_heartbeat — Register a callback invoked periodically during
 *                        blocking network operations (connect, recv, DNS, etc.).
 *                        GUI apps should use this to send WM heartbeats.
 * @param fn   Callback function (NULL to clear)
 * @param ctx  User context passed to fn
 */
void libnet_set_heartbeat(void (*fn)(void *ctx), void *ctx);

#endif /* LIBNET_H */
