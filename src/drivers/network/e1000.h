/**
 * Intel E1000 (82540EM) NIC Driver for MaahiOS
 * 
 * PCI-based Gigabit Ethernet controller.
 * QEMU emulates this by default with -device e1000.
 * 
 * Layer 7 (Hardware Driver) — Ring 0 only.
 * Registers with Device Manager as DEV_NETWORK.
 */

#ifndef E1000_H
#define E1000_H

#include <stdint.h>

/* ═══════════════════════════════════════════
 * PCI Identification
 * ═══════════════════════════════════════════ */
#define E1000_VENDOR_ID     0x8086
#define E1000_DEVICE_ID     0x100E  /* 82540EM — QEMU default */

/* ═══════════════════════════════════════════
 * E1000 Register Offsets (MMIO)
 * ═══════════════════════════════════════════ */
#define E1000_CTRL          0x0000  /* Device Control */
#define E1000_STATUS        0x0008  /* Device Status */
#define E1000_EECD          0x0010  /* EEPROM/Flash Control */
#define E1000_EERD          0x0014  /* EEPROM Read */
#define E1000_ICR           0x00C0  /* Interrupt Cause Read */
#define E1000_IMS           0x00D0  /* Interrupt Mask Set */
#define E1000_IMC           0x00D8  /* Interrupt Mask Clear */
#define E1000_RCTL          0x0100  /* Receive Control */
#define E1000_TCTL          0x0400  /* Transmit Control */
#define E1000_RDBAL         0x2800  /* RX Descriptor Base Low */
#define E1000_RDBAH         0x2804  /* RX Descriptor Base High */
#define E1000_RDLEN         0x2808  /* RX Descriptor Length */
#define E1000_RDH           0x2810  /* RX Descriptor Head */
#define E1000_RDT           0x2818  /* RX Descriptor Tail */
#define E1000_TDBAL         0x3800  /* TX Descriptor Base Low */
#define E1000_TDBAH         0x3804  /* TX Descriptor Base High */
#define E1000_TDLEN         0x3808  /* TX Descriptor Length */
#define E1000_TDH           0x3810  /* TX Descriptor Head */
#define E1000_TDT           0x3818  /* TX Descriptor Tail */
#define E1000_RAL0          0x5400  /* Receive Address Low */
#define E1000_RAH0          0x5404  /* Receive Address High */
#define E1000_MTA           0x5200  /* Multicast Table Array (128 entries) */

/* ═══════════════════════════════════════════
 * Control Register Bits
 * ═══════════════════════════════════════════ */
#define E1000_CTRL_RST      (1 << 26) /* Device Reset */
#define E1000_CTRL_SLU      (1 << 6)  /* Set Link Up */
#define E1000_CTRL_ASDE     (1 << 5)  /* Auto-Speed Detection Enable */

/* ═══════════════════════════════════════════
 * Receive Control (RCTL) Bits
 * ═══════════════════════════════════════════ */
#define E1000_RCTL_EN       (1 << 1)  /* Receiver Enable */
#define E1000_RCTL_SBP      (1 << 2)  /* Store Bad Packets */
#define E1000_RCTL_UPE      (1 << 3)  /* Unicast Promiscuous Enable */
#define E1000_RCTL_MPE      (1 << 4)  /* Multicast Promiscuous Enable */
#define E1000_RCTL_BAM      (1 << 15) /* Broadcast Accept Mode */
#define E1000_RCTL_BSIZE_2K (0 << 16) /* Buffer Size 2048 */
#define E1000_RCTL_SECRC    (1 << 26) /* Strip Ethernet CRC */

/* ═══════════════════════════════════════════
 * Transmit Control (TCTL) Bits
 * ═══════════════════════════════════════════ */
#define E1000_TCTL_EN       (1 << 1)  /* Transmitter Enable */
#define E1000_TCTL_PSP      (1 << 3)  /* Pad Short Packets */
#define E1000_TCTL_CT_SHIFT 4         /* Collision Threshold */
#define E1000_TCTL_COLD_SHIFT 12      /* Collision Distance */

/* ═══════════════════════════════════════════
 * TX Descriptor Command Bits
 * ═══════════════════════════════════════════ */
#define E1000_TXD_CMD_EOP   (1 << 0)  /* End of Packet */
#define E1000_TXD_CMD_IFCS  (1 << 1)  /* Insert FCS/CRC */
#define E1000_TXD_CMD_RS    (1 << 3)  /* Report Status */

/* ═══════════════════════════════════════════
 * RX Descriptor Status Bits
 * ═══════════════════════════════════════════ */
#define E1000_RXD_STAT_DD   (1 << 0)  /* Descriptor Done */
#define E1000_RXD_STAT_EOP  (1 << 1)  /* End of Packet */

/* TX Descriptor Status Bits */
#define E1000_TXD_STAT_DD   (1 << 0)  /* Descriptor Done */

/* ═══════════════════════════════════════════
 * EEPROM Read Register Bits
 * ═══════════════════════════════════════════ */
#define E1000_EERD_START    (1 << 0)
#define E1000_EERD_DONE     (1 << 4)
#define E1000_EERD_ADDR_SHIFT 8
#define E1000_EERD_DATA_SHIFT 16

/* ═══════════════════════════════════════════
 * Descriptor Ring Configuration
 * ═══════════════════════════════════════════ */
#define E1000_NUM_RX_DESC   32
#define E1000_NUM_TX_DESC   8
#define E1000_RX_BUF_SIZE   2048

/* ═══════════════════════════════════════════
 * Descriptor Structures (Hardware-defined)
 * ═══════════════════════════════════════════ */

/* Legacy TX Descriptor */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint8_t  cso;
    uint8_t  cmd;
    uint8_t  sta;
    uint8_t  css;
    uint16_t special;
} e1000_tx_desc_t;

/* Legacy RX Descriptor */
typedef struct __attribute__((packed)) {
    uint64_t addr;
    uint16_t length;
    uint16_t checksum;
    uint8_t  status;
    uint8_t  errors;
    uint16_t special;
} e1000_rx_desc_t;

/* ═══════════════════════════════════════════
 * Driver API
 * ═══════════════════════════════════════════ */

/**
 * Initialize the E1000 NIC driver.
 * Scans PCI bus, maps MMIO, sets up descriptor rings,
 * and registers with Device Manager as DEV_NETWORK.
 * Returns 0 on success, -1 if E1000 not found.
 */
int e1000_init(void);

/**
 * Send an Ethernet frame.
 * @param data   Pointer to complete Ethernet frame (dst+src+type+payload)
 * @param length Total frame length in bytes
 * @return 0 on success, negative on error
 */
int e1000_send_packet(const void *data, uint16_t length);

/**
 * Receive an Ethernet frame (polling mode).
 * @param buffer Output buffer for the received frame
 * @param max_len Maximum buffer size
 * @return Number of bytes received, 0 if no packet available, negative on error
 */
int e1000_recv_packet(void *buffer, uint16_t max_len);

/**
 * Get the MAC address of the NIC.
 * @param mac Output buffer (6 bytes)
 */
void e1000_get_mac(uint8_t *mac);

/**
 * Check if the E1000 link is up.
 * @return 1 if link up, 0 if down
 */
int e1000_link_up(void);

#endif /* E1000_H */
