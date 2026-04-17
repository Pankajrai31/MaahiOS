# Kernel Network Manager

## Files
- `managers/network/network_manager.c/.h` — High-level network ops
- `managers/network/tcp.c/.h` — TCP/IP protocol implementation

## Purpose
Implements the network protocol stack: IP, TCP, UDP, ICMP, ARP, DHCP.
Sits above the E1000 NIC driver. User-space accesses via SYS_NET_* syscalls.

## Syscalls (144–156)
- SYS_NET_GET_CONFIG (144) — get IP, gateway, DNS
- SYS_NET_PING (145) — ICMP ping
- SYS_NET_GET_STATUS (146) — link status
- SYS_NET_SEND_PACKET (147) — raw packet send
- SYS_NET_RECV_PACKET (148) — raw packet receive
- SYS_NET_GET_PKT_LOG (149) — packet statistics
- SYS_NET_SOCK_CREATE (150) — create TCP/UDP socket
- SYS_NET_SOCK_CONNECT (151) — TCP connect
- SYS_NET_SOCK_SEND (152) — send data on socket
- SYS_NET_SOCK_RECV (153) — receive data from socket
- SYS_NET_SOCK_CLOSE (154) — close socket
- SYS_NET_SOCK_SENDTO (155) — UDP sendto
- SYS_NET_SOCK_RECV_BULK (156) — bulk receive (performance)

## Known Issues
*(Agents add issues here)*
