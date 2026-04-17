/**
 * Network Syscall Handlers
 * Domain: 144-159 (net_get_config, net_ping, net_get_status, net_send, net_recv, sockets)
 *
 * Layer 4 (Syscall). Thin dispatch — validates args and calls Network Manager.
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../network/network_manager.h"
#include "../../network/tcp.h"
#include "../../scheduler/scheduler.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_net_get_config — Get network interface configuration
 * arg1 = pointer to net_config_t output struct (user buffer)
 */
static int sys_net_get_config(uint32_t config_ptr, uint32_t arg2, uint32_t arg3,
                              uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    if (!config_ptr) {
        KLOG_WARN("SYSCALL", "net_get_config: NULL config ptr");
        return SYSCALL_ERR_INVALID;
    }

    net_config_t *config = (net_config_t *)(uintptr_t)config_ptr;
    return kernel_net_get_config(config);
}

/**
 * sys_net_ping — Send ICMP echo request and wait for reply
 * arg1 = destination IP (host byte order)
 * arg2 = timeout in milliseconds
 * arg3 = pointer to ping_result_t output struct
 */
static int sys_net_ping(uint32_t dst_ip, uint32_t timeout_ms, uint32_t result_ptr,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;

    if (!dst_ip) {
        KLOG_WARN("SYSCALL", "net_ping: invalid dst_ip=0");
        return SYSCALL_ERR_INVALID;
    }
    if (!result_ptr) {
        KLOG_WARN("SYSCALL", "net_ping: NULL result ptr");
        return SYSCALL_ERR_INVALID;
    }

    ping_result_t *result = (ping_result_t *)(uintptr_t)result_ptr;

    KLOG_DEBUG("SYSCALL", "net_ping: ip=%d.%d.%d.%d timeout=%d",
               (dst_ip >> 24) & 0xFF, (dst_ip >> 16) & 0xFF,
               (dst_ip >> 8) & 0xFF, dst_ip & 0xFF,
               (int)timeout_ms);

    return kernel_net_ping(dst_ip, timeout_ms, result);
}

/**
 * sys_net_get_status — Get network statistics
 * arg1 = pointer to net_stats_t output struct
 */
static int sys_net_get_status(uint32_t stats_ptr, uint32_t arg2, uint32_t arg3,
                              uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    if (!stats_ptr) {
        KLOG_WARN("SYSCALL", "net_get_status: NULL stats ptr");
        return SYSCALL_ERR_INVALID;
    }

    net_stats_t *stats = (net_stats_t *)(uintptr_t)stats_ptr;
    return kernel_net_get_stats(stats);
}

/**
 * sys_net_send_packet — Send a raw Ethernet frame
 * arg1 = packet data pointer
 * arg2 = packet length
 */
static int sys_net_send_packet(uint32_t data_ptr, uint32_t length, uint32_t arg3,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    if (!data_ptr || length == 0 || length > 1518) {
        return SYSCALL_ERR_INVALID;
    }

    return kernel_net_send_raw((const void *)(uintptr_t)data_ptr, (uint16_t)length);
}

/**
 * sys_net_recv_packet — Receive a raw Ethernet frame (polling)
 * arg1 = buffer pointer
 * arg2 = buffer max length
 */
static int sys_net_recv_packet(uint32_t buf_ptr, uint32_t max_len, uint32_t arg3,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    if (!buf_ptr || max_len == 0) {
        return SYSCALL_ERR_INVALID;
    }

    return kernel_net_recv_raw((void *)(uintptr_t)buf_ptr, (uint16_t)max_len);
}

/**
 * sys_net_get_pkt_log — Get packet log snapshot
 * arg1 = pointer to net_pkt_log_t output struct (user buffer)
 */
static int sys_net_get_pkt_log(uint32_t log_ptr, uint32_t arg2, uint32_t arg3,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    if (!log_ptr) {
        KLOG_WARN("SYSCALL", "net_get_pkt_log: NULL log ptr");
        return SYSCALL_ERR_INVALID;
    }

    net_pkt_log_t *log = (net_pkt_log_t *)(uintptr_t)log_ptr;
    return kernel_net_get_pkt_log(log);
}

/* ===========================================================================
 * SOCKET HANDLERS (150-155)
 * =========================================================================== */

/**
 * sys_net_sock_create — Create a network socket
 * arg1 = type (1=UDP, 2=TCP)
 */
static int sys_net_sock_create(uint32_t type, uint32_t arg2, uint32_t arg3,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    int pid = scheduler_get_current_pid();
    return kernel_socket_create((int)type, pid);
}

/**
 * sys_net_sock_connect — Connect TCP socket to remote host
 * arg1 = socket handle
 * arg2 = remote IP (host byte order)
 * arg3 = remote port
 */
static int sys_net_sock_connect(uint32_t sock, uint32_t remote_ip, uint32_t remote_port,
                                uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    return kernel_socket_connect((int)sock, remote_ip, (uint16_t)remote_port);
}

/**
 * sys_net_sock_send — Send data on a socket
 * arg1 = socket handle
 * arg2 = data pointer
 * arg3 = data length
 */
static int sys_net_sock_send(uint32_t sock, uint32_t data_ptr, uint32_t len,
                             uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    if (!data_ptr || len == 0) return SYSCALL_ERR_INVALID;
    return kernel_socket_send((int)sock, (const void *)(uintptr_t)data_ptr, (uint16_t)len);
}

/**
 * sys_net_sock_recv — Receive data from a socket (non-blocking)
 * arg1 = socket handle
 * arg2 = buffer pointer
 * arg3 = max buffer length
 */
static int sys_net_sock_recv(uint32_t sock, uint32_t buf_ptr, uint32_t max_len,
                             uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    if (!buf_ptr || max_len == 0) return SYSCALL_ERR_INVALID;
    return kernel_socket_recv((int)sock, (void *)(uintptr_t)buf_ptr, (uint16_t)max_len);
}

/**
 * sys_net_sock_close — Close a socket
 * arg1 = socket handle
 */
static int sys_net_sock_close(uint32_t sock, uint32_t arg2, uint32_t arg3,
                              uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return kernel_socket_close((int)sock);
}

/**
 * sys_net_sock_sendto — UDP sendto
 * arg1 = socket handle
 * arg2 = (dst_ip_high16 << 16) | dst_port  -- packed IP high + port
 * arg3 = data pointer
 * arg4 = data length
 * arg5 = dst_ip (host byte order)
 */
static int sys_net_sock_sendto(uint32_t sock, uint32_t dst_ip, uint32_t dst_port,
                               uint32_t data_ptr, uint32_t len_and_stuff) {
    /* Unpack: arg1=sock, arg2=dst_ip, arg3=dst_port, arg4=data_ptr, arg5=len */
    if (!data_ptr) return SYSCALL_ERR_INVALID;
    uint16_t port = (uint16_t)dst_port;
    uint16_t len = (uint16_t)len_and_stuff;
    return kernel_udp_sendto((int)sock, dst_ip, port,
                             (const void *)(uintptr_t)data_ptr, len);
}

/**
 * sys_net_sock_recv_bulk — Bulk receive, same as recv but allows larger max_len.
 * Bypasses executive IPC for direct kernel access. max_len up to full recv buffer.
 */
static int sys_net_sock_recv_bulk(uint32_t sock, uint32_t buf_ptr, uint32_t max_len,
                                  uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    if (!buf_ptr || max_len == 0) return SYSCALL_ERR_INVALID;
    if (max_len > 32768) max_len = 32768; /* Cap at SOCK_RECV_BUF_SIZE */
    return kernel_socket_recv((int)sock, (void *)(uintptr_t)buf_ptr, (uint16_t)max_len);
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_network_handlers(void) {
    /* Network management (144-149) */
    syscall_register(SYS_NET_GET_CONFIG,  sys_net_get_config);
    syscall_register(SYS_NET_PING,        sys_net_ping);
    syscall_register(SYS_NET_GET_STATUS,  sys_net_get_status);
    syscall_register(SYS_NET_SEND_PACKET, sys_net_send_packet);
    syscall_register(SYS_NET_RECV_PACKET, sys_net_recv_packet);
    syscall_register(SYS_NET_GET_PKT_LOG, sys_net_get_pkt_log);

    /* Socket API (150-156) */
    syscall_register(SYS_NET_SOCK_CREATE,  sys_net_sock_create);
    syscall_register(SYS_NET_SOCK_CONNECT, sys_net_sock_connect);
    syscall_register(SYS_NET_SOCK_SEND,    sys_net_sock_send);
    syscall_register(SYS_NET_SOCK_RECV,    sys_net_sock_recv);
    syscall_register(SYS_NET_SOCK_CLOSE,   sys_net_sock_close);
    syscall_register(SYS_NET_SOCK_SENDTO,  sys_net_sock_sendto);
    syscall_register(SYS_NET_SOCK_RECV_BULK, sys_net_sock_recv_bulk);

    KLOG_INFO("SYSCALL", "Network handlers registered (144-156)");
}
