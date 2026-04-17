/**
 * MaahiOS Network Executive Header
 *
 * Description:
 *   Network Executive provides networking services to user-space:
 *   ping, network configuration, network status.
 *   Routes requests to kernel via SYS_NET_* syscalls.
 *
 *   PID 14 - loaded by sysman after FS Executive, before Orbit.
 *   Uses liblog for logging (auto-init)
 *   Uses libcell for cell registration (auto-init)
 *   Uses SYS_NET_* syscalls to talk to kernel Network Manager
 *   Dual SHM queues (request + response)
 *
 * Data Flow:
 *   App -> libnet -> SHM queue -> Network Executive -> SYS_NET_* syscalls
 *     -> Network Manager -> E1000 driver -> hardware
 */

#ifndef NETWORK_EXECUTIVE_H
#define NETWORK_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * NETWORK EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE = 16)
 *===========================================================================*/

#define NET_OP_PING             (EXEC_OP_CUSTOM_BASE + 0)   /* Send ICMP ping */
#define NET_OP_GET_CONFIG       (EXEC_OP_CUSTOM_BASE + 1)   /* Get network config */
#define NET_OP_GET_STATUS       (EXEC_OP_CUSTOM_BASE + 2)   /* Get network stats */
#define NET_OP_GET_PKT_LOG      (EXEC_OP_CUSTOM_BASE + 3)   /* Get packet log */

/* Socket operations */
#define NET_OP_SOCK_CREATE      (EXEC_OP_CUSTOM_BASE + 4)   /* Create socket */
#define NET_OP_SOCK_CONNECT     (EXEC_OP_CUSTOM_BASE + 5)   /* TCP connect */
#define NET_OP_SOCK_SEND        (EXEC_OP_CUSTOM_BASE + 6)   /* Send data */
#define NET_OP_SOCK_RECV        (EXEC_OP_CUSTOM_BASE + 7)   /* Recv data */
#define NET_OP_SOCK_CLOSE       (EXEC_OP_CUSTOM_BASE + 8)   /* Close socket */
#define NET_OP_SOCK_SENDTO      (EXEC_OP_CUSTOM_BASE + 9)   /* UDP sendto */

/*=============================================================================
 * NETWORK CONFIGURATION (user-space version)
 *===========================================================================*/

#define NET_MAC_LEN     6

typedef struct {
    uint32_t ip_addr;
    uint32_t netmask;
    uint32_t gateway;
    uint32_t dns;
    uint8_t  mac[NET_MAC_LEN];
    uint8_t  link_up;
    uint8_t  initialized;
} net_exec_config_t;

/*=============================================================================
 * NETWORK STATISTICS (user-space version)
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
} net_exec_stats_t;

/*=============================================================================
 * PING RESULT (user-space version)
 *===========================================================================*/

typedef struct {
    int      success;       /* 1 = reply received, 0 = timeout */
    uint32_t rtt_ms;        /* Round-trip time in milliseconds */
    uint8_t  ttl;           /* TTL from reply */
    uint8_t  _pad[3];
    uint16_t seq;           /* Sequence number */
    uint16_t _pad2;
    uint32_t dst_ip;        /* Target IP */
} net_exec_ping_result_t;

/*=============================================================================
 * REQUEST PAYLOADS
 *===========================================================================*/

/* PING request */
typedef struct {
    uint32_t dst_ip;        /* Target IP (host byte order) */
    uint32_t timeout_ms;    /* Timeout in milliseconds */
} net_ping_req_t;

/* GET_CONFIG request — no payload */
/* GET_STATUS request — no payload */

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* PING response — payload = net_exec_ping_result_t */
/* GET_CONFIG response — payload = net_exec_config_t */
/* GET_STATUS response — payload = net_exec_stats_t */
/* GET_PKT_LOG response — result = SHM ID of packet log data */

/*=============================================================================
 * PACKET LOG CONSTANTS (mirrors kernel-side)
 *===========================================================================*/

#define NET_PKT_LOG_SIZE        64
#define NET_PKT_LOG_SUMMARY     48

#define NET_PKT_DIR_TX  0
#define NET_PKT_DIR_RX  1

#define NET_PKT_PROTO_ARP   1
#define NET_PKT_PROTO_ICMP  2
#define NET_PKT_PROTO_IPV4  3
#define NET_PKT_PROTO_OTHER 4

typedef struct {
    uint32_t timestamp;
    uint32_t src_ip;
    uint32_t dst_ip;
    uint16_t length;
    uint8_t  direction;
    uint8_t  protocol;
    char     summary[NET_PKT_LOG_SUMMARY];
} net_exec_pkt_log_entry_t;

typedef struct {
    uint32_t count;
    uint32_t returned;
    net_exec_pkt_log_entry_t entries[NET_PKT_LOG_SIZE];
} net_exec_pkt_log_t;

/*=============================================================================
 * SOCKET REQUEST/RESPONSE PAYLOADS
 *===========================================================================*/

/* SOCK_CREATE request */
typedef struct {
    uint32_t type;              /* 1=UDP, 2=TCP */
} net_sock_create_req_t;

/* SOCK_CONNECT request */
typedef struct {
    int32_t  sock;              /* Socket handle (from create) */
    uint32_t remote_ip;         /* Host byte order */
    uint16_t remote_port;
    uint16_t _pad;
} net_sock_connect_req_t;

/* SOCK_SEND request
 * Data follows the header in payload[sizeof(net_sock_send_req_t)..] */
typedef struct {
    int32_t  sock;
    uint16_t len;               /* Byte count */
    uint16_t _pad;
    /* followed by `len` bytes of data */
} net_sock_send_req_t;

/* SOCK_RECV request */
typedef struct {
    int32_t  sock;
    uint16_t max_len;           /* Max bytes to receive */
    uint16_t _pad;
} net_sock_recv_req_t;

/* SOCK_CLOSE request */
typedef struct {
    int32_t  sock;
} net_sock_close_req_t;

/* SOCK_SENDTO request (UDP)
 * Data follows the header in payload[sizeof(net_sock_sendto_req_t)..] */
typedef struct {
    int32_t  sock;
    uint32_t dst_ip;            /* Host byte order */
    uint16_t dst_port;
    uint16_t len;
    /* followed by `len` bytes of data */
} net_sock_sendto_req_t;

/* Socket responses use resp->result for the return value.
 * SOCK_RECV puts received data in resp->payload, resp->payload_size = bytes. */

/*=============================================================================
 * EXECUTIVE ENTRY POINT
 *===========================================================================*/

void exe_net_main(void);

#endif /* NETWORK_EXECUTIVE_H */
