/**
 * MaahiOS TCP/UDP Protocol Structures and Socket Table
 *
 * Layer 5 (Kernel Manager). Ring 0.
 * Used by network_manager.c for TCP/UDP state management.
 */

#ifndef NET_TCP_H
#define NET_TCP_H

#include <stdint.h>

/* ═══════════════════════════════════════════
 * UDP Header (RFC 768)
 * ═══════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint16_t length;        /* Header + data */
    uint16_t checksum;
} udp_header_t;

/* ═══════════════════════════════════════════
 * TCP Header (RFC 793)
 * ═══════════════════════════════════════════ */

typedef struct __attribute__((packed)) {
    uint16_t src_port;
    uint16_t dst_port;
    uint32_t seq_num;
    uint32_t ack_num;
    uint8_t  data_offset;   /* Upper 4 bits = header length / 4 */
    uint8_t  flags;
    uint16_t window;
    uint16_t checksum;
    uint16_t urgent_ptr;
} tcp_header_t;

/* TCP Flags */
#define TCP_FIN     0x01
#define TCP_SYN     0x02
#define TCP_RST     0x04
#define TCP_PSH     0x08
#define TCP_ACK     0x10
#define TCP_URG     0x20

/* TCP States */
typedef enum {
    TCP_STATE_CLOSED = 0,
    TCP_STATE_SYN_SENT,
    TCP_STATE_ESTABLISHED,
    TCP_STATE_FIN_WAIT_1,
    TCP_STATE_FIN_WAIT_2,
    TCP_STATE_CLOSE_WAIT,
    TCP_STATE_LAST_ACK,
    TCP_STATE_TIME_WAIT
} tcp_state_t;

/* ═══════════════════════════════════════════
 * Socket Table
 * ═══════════════════════════════════════════ */

#define MAX_SOCKETS         8
#define SOCK_RECV_BUF_SIZE  32768   /* 32 KB receive buffer per socket */

/* Socket type */
#define SOCK_TYPE_UNUSED    0
#define SOCK_TYPE_UDP       1
#define SOCK_TYPE_TCP       2

typedef struct {
    /* Identity */
    uint8_t  type;              /* SOCK_TYPE_* */
    uint8_t  in_use;

    /* Addresses */
    uint32_t local_ip;
    uint16_t local_port;
    uint32_t remote_ip;
    uint16_t remote_port;

    /* TCP state */
    tcp_state_t tcp_state;
    uint32_t    snd_nxt;        /* Next sequence number to send */
    uint32_t    snd_una;        /* Oldest unacknowledged seq */
    uint32_t    rcv_nxt;        /* Next expected receive seq */
    uint16_t    rcv_wnd;        /* Advertised receive window */
    uint16_t    snd_wnd;        /* Peer's advertised window */

    /* Receive circular buffer */
    uint8_t  recv_buf[SOCK_RECV_BUF_SIZE];
    uint16_t recv_head;         /* Write position */
    uint16_t recv_tail;         /* Read position */
    uint16_t recv_len;          /* Bytes available */

    /* Flags */
    uint8_t  connected;         /* 1 when connection fully established */
    uint8_t  peer_closed;       /* 1 when peer sent FIN */
    uint8_t  error;             /* Non-zero on error */

    /* Timeout */
    uint32_t last_activity;     /* Tick of last packet sent/received */

    /* Owner PID (for access control) */
    int      owner_pid;
} net_socket_t;

/* ═══════════════════════════════════════════
 * Socket API (Kernel Space)
 * ═══════════════════════════════════════════ */

/** Create a socket. Returns socket handle (0..MAX_SOCKETS-1) or -1. */
int kernel_socket_create(int type, int owner_pid);

/** Connect a TCP socket to remote host. Blocks for handshake. */
int kernel_socket_connect(int sock, uint32_t remote_ip, uint16_t remote_port);

/** Send data on connected socket. Returns bytes sent or -1. */
int kernel_socket_send(int sock, const void *data, uint16_t len);

/** Receive data from socket. Non-blocking: returns 0 if no data. */
int kernel_socket_recv(int sock, void *buf, uint16_t max_len);

/** Close a socket (TCP graceful close or immediate). */
int kernel_socket_close(int sock);

/** Send a UDP datagram (connectionless). */
int kernel_udp_sendto(int sock, uint32_t dst_ip, uint16_t dst_port,
                      const void *data, uint16_t len);

/** Poll network and dispatch to sockets. Called from exec or periodic. */
void kernel_net_poll_sockets(void);

#endif /* NET_TCP_H */
