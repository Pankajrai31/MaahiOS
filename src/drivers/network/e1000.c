/**
 * Intel E1000 (82540EM) NIC Driver — MaahiOS
 *
 * Layer 6 (Hardware Driver). Ring 0 only.
 * Scanning PCI bus for vendor 0x8086 / device 0x100E,
 * mapping BAR0 via MMIO, setting up TX/RX descriptor rings,
 * and registering as DEV_NETWORK with Device Manager.
 *
 * Polling mode — no IRQ wiring required for initial implementation.
 */

#include "e1000.h"
#include "../../managers/device/device_manager.h"
#include "../../managers/klog/klog.h"
#include "../../managers/memory/paging.h"
#include "../../managers/memory/pmm.h"
#include "../../drivers/pci/pci.h"

#define TAG "E1000"

/* ═══════════════════════════════════════════
 * Internal State
 * ═══════════════════════════════════════════ */

/* PCI location */
static uint8_t  g_pci_bus  = 0;
static uint8_t  g_pci_slot = 0;
static uint8_t  g_pci_func = 0;

/* MMIO base (virtual == physical, identity-mapped by paging layer) */
static volatile uint32_t *g_mmio_base = 0;

/* MAC address */
static uint8_t g_mac[6];

/* TX ring */
static e1000_tx_desc_t *g_tx_descs  = 0;  /* physically-contiguous page */
static uint8_t         *g_tx_bufs   = 0;  /* TX packet buffers (one page per desc) */
static uint16_t         g_tx_tail   = 0;

/* RX ring */
static e1000_rx_desc_t *g_rx_descs  = 0;
static uint8_t         *g_rx_bufs   = 0;  /* RX packet buffers (E1000_RX_BUF_SIZE each) */
static uint16_t         g_rx_tail   = 0;

static int g_initialized = 0;

/* ═══════════════════════════════════════════
 * MMIO Helpers
 * ═══════════════════════════════════════════ */
static inline void e1000_write(uint32_t reg, uint32_t val) {
    g_mmio_base[reg / 4] = val;
}

static inline uint32_t e1000_read(uint32_t reg) {
    return g_mmio_base[reg / 4];
}

/* ═══════════════════════════════════════════
 * PCI Bus Scan — find the E1000
 * ═══════════════════════════════════════════ */
static int e1000_pci_find(void) {
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_config_read_word((uint8_t)bus, slot, 0, 0x00);
            if (vendor == 0xFFFF) continue;

            uint16_t device = pci_config_read_word((uint8_t)bus, slot, 0, 0x02);
            if (vendor == E1000_VENDOR_ID && device == E1000_DEVICE_ID) {
                g_pci_bus  = (uint8_t)bus;
                g_pci_slot = slot;
                g_pci_func = 0;
                KLOG_INFO(TAG, "Found E1000 at PCI %d:%d.0", (int)bus, (int)slot);
                return 0;
            }
        }
    }
    return -1;
}

/* ═══════════════════════════════════════════
 * Enable PCI Bus-Mastering + MMIO
 * ═══════════════════════════════════════════ */
static void e1000_pci_enable(void) {
    uint16_t cmd = pci_config_read_word(g_pci_bus, g_pci_slot, g_pci_func, 0x04);
    /* Bit 1 = Memory Space, Bit 2 = Bus Master */
    cmd |= (1 << 1) | (1 << 2);
    pci_config_write_word(g_pci_bus, g_pci_slot, g_pci_func, 0x04, cmd);
    KLOG_INFO(TAG, "PCI bus-master + MMIO enabled");
}

/* ═══════════════════════════════════════════
 * Map BAR0 into kernel address space
 * ═══════════════════════════════════════════ */
static int e1000_map_bar0(void) {
    uint32_t bar0 = pci_config_read_dword(g_pci_bus, g_pci_slot, g_pci_func, 0x10);

    /* Bit 0 = 0 means MMIO */
    if (bar0 & 1) {
        KLOG_ERROR(TAG, "BAR0 is I/O-space, expected MMIO");
        return -1;
    }

    uint32_t phys = bar0 & 0xFFFFFFF0;
    KLOG_INFO_HEX(TAG, "BAR0 phys=", phys);

    /* Map 128 KB of MMIO (E1000 register space is ~128 KB) */
    paging_map_mmio_region(phys, 0x20000);
    g_mmio_base = (volatile uint32_t *)(uintptr_t)phys;

    KLOG_INFO(TAG, "MMIO mapped OK");
    return 0;
}

/* ═══════════════════════════════════════════
 * Read MAC from EEPROM word
 * ═══════════════════════════════════════════ */
static uint16_t e1000_eeprom_read(uint8_t addr) {
    e1000_write(E1000_EERD, ((uint32_t)addr << E1000_EERD_ADDR_SHIFT) | E1000_EERD_START);

    uint32_t val;
    for (int i = 0; i < 1000; i++) {
        val = e1000_read(E1000_EERD);
        if (val & E1000_EERD_DONE) {
            return (uint16_t)(val >> E1000_EERD_DATA_SHIFT);
        }
    }
    KLOG_WARN(TAG, "EEPROM read timeout");
    return 0;
}

static int e1000_read_mac_eeprom(void) {
    uint16_t w0 = e1000_eeprom_read(0);
    uint16_t w1 = e1000_eeprom_read(1);
    uint16_t w2 = e1000_eeprom_read(2);

    g_mac[0] = w0 & 0xFF;
    g_mac[1] = (w0 >> 8) & 0xFF;
    g_mac[2] = w1 & 0xFF;
    g_mac[3] = (w1 >> 8) & 0xFF;
    g_mac[4] = w2 & 0xFF;
    g_mac[5] = (w2 >> 8) & 0xFF;

    /* Sanity: all-zero or all-FF means EEPROM failed */
    if ((g_mac[0] | g_mac[1] | g_mac[2] | g_mac[3] | g_mac[4] | g_mac[5]) == 0) {
        return -1;
    }
    if (g_mac[0] == 0xFF && g_mac[1] == 0xFF && g_mac[2] == 0xFF) {
        return -1;
    }
    return 0;
}

static void e1000_read_mac_ral(void) {
    uint32_t ral = e1000_read(E1000_RAL0);
    uint32_t rah = e1000_read(E1000_RAH0);
    g_mac[0] = ral & 0xFF;
    g_mac[1] = (ral >> 8) & 0xFF;
    g_mac[2] = (ral >> 16) & 0xFF;
    g_mac[3] = (ral >> 24) & 0xFF;
    g_mac[4] = rah & 0xFF;
    g_mac[5] = (rah >> 8) & 0xFF;
}

static void e1000_read_mac(void) {
    if (e1000_read_mac_eeprom() != 0) {
        /* Fallback: read from RAL/RAH registers */
        KLOG_WARN(TAG, "EEPROM MAC failed, using RAL/RAH");
        e1000_read_mac_ral();
    }
    KLOG_INFO(TAG, "MAC=%x:%x:%x:%x:%x:%x",
              g_mac[0], g_mac[1], g_mac[2], g_mac[3], g_mac[4], g_mac[5]);
}

/* ═══════════════════════════════════════════
 * Reset the NIC
 * ═══════════════════════════════════════════ */
static void e1000_reset(void) {
    /* Disable interrupts first */
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    /* Global reset */
    uint32_t ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_RST;
    e1000_write(E1000_CTRL, ctrl);

    /* Wait for reset to complete (spin a bit) */
    for (volatile int i = 0; i < 100000; i++) { }

    /* Disable interrupts again after reset */
    e1000_write(E1000_IMC, 0xFFFFFFFF);

    /* Set link up */
    ctrl = e1000_read(E1000_CTRL);
    ctrl |= E1000_CTRL_SLU | E1000_CTRL_ASDE;
    ctrl &= ~E1000_CTRL_RST;
    e1000_write(E1000_CTRL, ctrl);

    KLOG_INFO(TAG, "NIC reset complete");
}

/* ═══════════════════════════════════════════
 * Memory Helpers — alloc page-aligned DMA buffers
 * ═══════════════════════════════════════════ */
static void mem_zero(void *dst, uint32_t size) {
    uint8_t *p = (uint8_t *)dst;
    for (uint32_t i = 0; i < size; i++) p[i] = 0;
}

/* ═══════════════════════════════════════════
 * TX Ring Setup
 * ═══════════════════════════════════════════ */
static int e1000_tx_init(void) {
    /* Allocate descriptor ring: E1000_NUM_TX_DESC * 16 bytes = 128 bytes
       One PMM page (4096) is more than enough */
    g_tx_descs = (e1000_tx_desc_t *)pmm_alloc_page();
    if (!g_tx_descs) {
        KLOG_ERROR(TAG, "Failed to alloc TX desc ring");
        return -1;
    }
    mem_zero(g_tx_descs, PAGE_SIZE);

    /* Allocate TX packet buffers: E1000_NUM_TX_DESC pages */
    g_tx_bufs = (uint8_t *)pmm_alloc_size(E1000_NUM_TX_DESC * E1000_RX_BUF_SIZE);
    if (!g_tx_bufs) {
        KLOG_ERROR(TAG, "Failed to alloc TX buffers");
        return -1;
    }
    mem_zero(g_tx_bufs, E1000_NUM_TX_DESC * E1000_RX_BUF_SIZE);

    /* Set up each TX descriptor to point to its buffer */
    for (int i = 0; i < E1000_NUM_TX_DESC; i++) {
        g_tx_descs[i].addr   = (uint32_t)(uintptr_t)(g_tx_bufs + i * E1000_RX_BUF_SIZE);
        g_tx_descs[i].length = 0;
        g_tx_descs[i].cmd    = 0;
        g_tx_descs[i].sta    = E1000_TXD_STAT_DD; /* Mark as done (available) */
    }

    /* Point hardware to descriptor ring */
    e1000_write(E1000_TDBAL, (uint32_t)(uintptr_t)g_tx_descs);
    e1000_write(E1000_TDBAH, 0);
    e1000_write(E1000_TDLEN, E1000_NUM_TX_DESC * sizeof(e1000_tx_desc_t));
    e1000_write(E1000_TDH, 0);
    e1000_write(E1000_TDT, 0);

    g_tx_tail = 0;

    /* Enable transmitter */
    uint32_t tctl = E1000_TCTL_EN | E1000_TCTL_PSP |
                    (0x10 << E1000_TCTL_CT_SHIFT) |
                    (0x40 << E1000_TCTL_COLD_SHIFT);
    e1000_write(E1000_TCTL, tctl);

    KLOG_INFO(TAG, "TX ring initialized (%d descs)", E1000_NUM_TX_DESC);
    return 0;
}

/* ═══════════════════════════════════════════
 * RX Ring Setup
 * ═══════════════════════════════════════════ */
static int e1000_rx_init(void) {
    /* Allocate descriptor ring */
    g_rx_descs = (e1000_rx_desc_t *)pmm_alloc_page();
    if (!g_rx_descs) {
        KLOG_ERROR(TAG, "Failed to alloc RX desc ring");
        return -1;
    }
    mem_zero(g_rx_descs, PAGE_SIZE);

    /* Allocate RX packet buffers: one contiguous region */
    g_rx_bufs = (uint8_t *)pmm_alloc_size(E1000_NUM_RX_DESC * E1000_RX_BUF_SIZE);
    if (!g_rx_bufs) {
        KLOG_ERROR(TAG, "Failed to alloc RX buffers");
        return -1;
    }
    mem_zero(g_rx_bufs, E1000_NUM_RX_DESC * E1000_RX_BUF_SIZE);

    /* Set up each RX descriptor */
    for (int i = 0; i < E1000_NUM_RX_DESC; i++) {
        g_rx_descs[i].addr   = (uint32_t)(uintptr_t)(g_rx_bufs + i * E1000_RX_BUF_SIZE);
        g_rx_descs[i].status = 0;
    }

    /* Program RX register */
    e1000_write(E1000_RDBAL, (uint32_t)(uintptr_t)g_rx_descs);
    e1000_write(E1000_RDBAH, 0);
    e1000_write(E1000_RDLEN, E1000_NUM_RX_DESC * sizeof(e1000_rx_desc_t));
    e1000_write(E1000_RDH, 0);
    e1000_write(E1000_RDT, E1000_NUM_RX_DESC - 1);

    g_rx_tail = 0;

    /* Clear multicast table */
    for (int i = 0; i < 128; i++) {
        e1000_write(E1000_MTA + i * 4, 0);
    }

    /* Enable receiver */
    uint32_t rctl = E1000_RCTL_EN | E1000_RCTL_BAM |
                    E1000_RCTL_BSIZE_2K | E1000_RCTL_SECRC;
    e1000_write(E1000_RCTL, rctl);

    KLOG_INFO(TAG, "RX ring initialized (%d descs)", E1000_NUM_RX_DESC);
    return 0;
}

/* ═══════════════════════════════════════════
 * Device Ops (for Device Manager registration)
 * ═══════════════════════════════════════════ */

static int e1000_dev_open(int flags) {
    (void)flags;
    if (!g_initialized) return DEV_ERR_NOT_FOUND;
    return 0;
}

static int e1000_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int e1000_dev_read(int handle, void *buffer, size_t size) {
    (void)handle;
    return e1000_recv_packet(buffer, (uint16_t)size);
}

static int e1000_dev_write(int handle, const void *buffer, size_t size) {
    (void)handle;
    return e1000_send_packet(buffer, (uint16_t)size);
}

/* Network IOCTL commands */
#define NET_IOCTL_GET_MAC       1
#define NET_IOCTL_LINK_STATUS   2
#define NET_IOCTL_GET_STATS     3

static int e1000_dev_ioctl(int handle, int cmd, void *arg) {
    (void)handle;
    switch (cmd) {
        case NET_IOCTL_GET_MAC: {
            if (!arg) return DEV_ERR_INVALID;
            uint8_t *out = (uint8_t *)arg;
            for (int i = 0; i < 6; i++) out[i] = g_mac[i];
            return 0;
        }
        case NET_IOCTL_LINK_STATUS:
            return e1000_link_up();
        default:
            return DEV_ERR_NOT_SUPPORTED;
    }
}

static int e1000_dev_poll(int handle) {
    (void)handle;
    if (!g_initialized) return 0;
    /* Check if current RX descriptor has data */
    return (g_rx_descs[g_rx_tail].status & E1000_RXD_STAT_DD) ? 1 : 0;
}

static device_ops_t e1000_ops = {
    .open  = e1000_dev_open,
    .close = e1000_dev_close,
    .read  = e1000_dev_read,
    .write = e1000_dev_write,
    .ioctl = e1000_dev_ioctl,
    .poll  = e1000_dev_poll
};

/* ═══════════════════════════════════════════
 * Public API
 * ═══════════════════════════════════════════ */

int e1000_init(void) {
    KLOG_INFO(TAG, "Initializing E1000 NIC driver...");

    /* Step 1: Find E1000 on PCI bus */
    if (e1000_pci_find() != 0) {
        KLOG_WARN(TAG, "E1000 not found on PCI bus");
        return -1;
    }

    /* Step 2: Enable bus-mastering and MMIO */
    e1000_pci_enable();

    /* Step 3: Map BAR0 MMIO region */
    if (e1000_map_bar0() != 0) {
        KLOG_ERROR(TAG, "Failed to map BAR0");
        return -1;
    }

    /* Step 4: Reset NIC */
    e1000_reset();

    /* Step 5: Read MAC address */
    e1000_read_mac();

    /* Step 6: Initialize TX ring */
    if (e1000_tx_init() != 0) {
        KLOG_ERROR(TAG, "TX ring init failed");
        return -1;
    }

    /* Step 7: Initialize RX ring */
    if (e1000_rx_init() != 0) {
        KLOG_ERROR(TAG, "RX ring init failed");
        return -1;
    }

    /* Step 8: Register with Device Manager */
    int ret = register_device(DEV_NETWORK, "e1000", &e1000_ops);
    if (ret != 0) {
        KLOG_ERROR_HEX(TAG, "register_device failed ret=", (unsigned)ret);
        return -1;
    }

    g_initialized = 1;
    KLOG_INFO(TAG, "E1000 driver ready (link=%d)", e1000_link_up());
    return 0;
}

int e1000_send_packet(const void *data, uint16_t length) {
    if (!g_initialized) return -1;
    if (!data || length == 0 || length > 1518) return -2;

    /* Wait for current TX descriptor to be available */
    e1000_tx_desc_t *txd = &g_tx_descs[g_tx_tail];

    /* Polling: wait until DD bit is set (descriptor done) */
    int timeout = 10000;
    while (!(txd->sta & E1000_TXD_STAT_DD) && --timeout > 0) {
        /* spin */
    }
    if (timeout == 0) {
        KLOG_WARN(TAG, "TX timeout");
        return -3;
    }

    /* Copy packet data into TX buffer */
    uint8_t *buf = g_tx_bufs + g_tx_tail * E1000_RX_BUF_SIZE;
    const uint8_t *src = (const uint8_t *)data;
    for (uint16_t i = 0; i < length; i++) {
        buf[i] = src[i];
    }

    /* Configure descriptor */
    txd->addr   = (uint32_t)(uintptr_t)buf;
    txd->length = length;
    txd->cmd    = E1000_TXD_CMD_EOP | E1000_TXD_CMD_IFCS | E1000_TXD_CMD_RS;
    txd->sta    = 0;

    /* Advance tail — tells hardware to send */
    g_tx_tail = (g_tx_tail + 1) % E1000_NUM_TX_DESC;
    e1000_write(E1000_TDT, g_tx_tail);

    return 0;
}

int e1000_recv_packet(void *buffer, uint16_t max_len) {
    if (!g_initialized) return -1;
    if (!buffer || max_len == 0) return -2;

    e1000_rx_desc_t *rxd = &g_rx_descs[g_rx_tail];

    /* Check if descriptor has received data */
    if (!(rxd->status & E1000_RXD_STAT_DD)) {
        return 0; /* No packet available */
    }

    uint16_t pkt_len = rxd->length;
    if (pkt_len > max_len) pkt_len = max_len;

    /* Copy data out */
    uint8_t *src = g_rx_bufs + g_rx_tail * E1000_RX_BUF_SIZE;
    uint8_t *dst = (uint8_t *)buffer;
    for (uint16_t i = 0; i < pkt_len; i++) {
        dst[i] = src[i];
    }

    /* Reset descriptor for reuse */
    rxd->status = 0;

    /* Advance tail — tell hardware this descriptor is available again */
    uint16_t old_tail = g_rx_tail;
    g_rx_tail = (g_rx_tail + 1) % E1000_NUM_RX_DESC;
    e1000_write(E1000_RDT, old_tail);

    return pkt_len;
}

void e1000_get_mac(uint8_t *mac) {
    if (!mac) return;
    for (int i = 0; i < 6; i++) mac[i] = g_mac[i];
}

int e1000_link_up(void) {
    if (!g_mmio_base) return 0;
    uint32_t status = e1000_read(E1000_STATUS);
    return (status & (1 << 1)) ? 1 : 0; /* Bit 1 = Link Up */
}
