# Network Driver (E1000)

## Files
- `drivers/network/e1000.c/.h` — Intel E1000 NIC driver

## Purpose
Ethernet NIC driver for Intel E1000 (emulated by QEMU).
Handles packet send/receive at the Ethernet frame level.

## Features
- PCI device discovery
- MMIO register access
- TX/RX descriptor rings
- IRQ-driven packet reception
- MAC address reading

## QEMU Configuration
```
-netdev user,id=net0 -device e1000,netdev=net0
```
QEMU provides DHCP, DNS, and NAT automatically with user-mode networking.

## Known Issues
*(Agents add issues here)*
