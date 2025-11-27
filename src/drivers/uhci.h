#ifndef UHCI_H
#define UHCI_H

#include <stdint.h>

// UHCI Register Offsets
#define UHCI_REG_USBCMD     0x00  // USB Command
#define UHCI_REG_USBSTS     0x02  // USB Status
#define UHCI_REG_USBINTR    0x04  // USB Interrupt Enable
#define UHCI_REG_FRNUM      0x06  // Frame Number
#define UHCI_REG_FRBASEADD  0x08  // Frame List Base Address
#define UHCI_REG_SOFMOD     0x0C  // Start of Frame Modify
#define UHCI_REG_PORTSC1    0x10  // Port 1 Status/Control
#define UHCI_REG_PORTSC2    0x12  // Port 2 Status/Control

// UHCI Command Register Bits
#define UHCI_CMD_RS         (1 << 0)  // Run/Stop
#define UHCI_CMD_HCRESET    (1 << 1)  // Host Controller Reset
#define UHCI_CMD_GRESET     (1 << 2)  // Global Reset
#define UHCI_CMD_MAXP       (1 << 7)  // Max Packet (1=64, 0=32)

// UHCI Status Register Bits
#define UHCI_STS_USBINT     (1 << 0)  // USB Interrupt
#define UHCI_STS_ERROR      (1 << 1)  // USB Error Interrupt
#define UHCI_STS_RD         (1 << 2)  // Resume Detect
#define UHCI_STS_HSE        (1 << 3)  // Host System Error
#define UHCI_STS_HCPE       (1 << 4)  // Host Controller Process Error
#define UHCI_STS_HCH        (1 << 5)  // HC Halted

// UHCI Port Status/Control Bits
#define UHCI_PORT_CCS       (1 << 0)  // Current Connect Status
#define UHCI_PORT_CSC       (1 << 1)  // Connect Status Change
#define UHCI_PORT_PE        (1 << 2)  // Port Enable
#define UHCI_PORT_PEC       (1 << 3)  // Port Enable Change
#define UHCI_PORT_LSDA      (1 << 8)  // Low Speed Device Attached
#define UHCI_PORT_PR        (1 << 9)  // Port Reset
#define UHCI_PORT_SUSP      (1 << 12) // Suspend

// UHCI Transfer Descriptor (TD)
typedef struct {
    uint32_t link_ptr;
    uint32_t status;
    uint32_t token;
    uint32_t buffer;
    // Software use
    uint32_t reserved[4];
} __attribute__((aligned(16))) uhci_td_t;

// UHCI Queue Head (QH)
typedef struct {
    uint32_t head_link_ptr;
    uint32_t element_link_ptr;
    // Software use
    uint32_t reserved[2];
} __attribute__((aligned(16))) uhci_qh_t;

// UHCI Controller Structure
typedef struct {
    uint16_t io_base;
    uint8_t  irq;
    uint32_t *frame_list;
    uhci_qh_t *control_qh;
    uhci_td_t *setup_td;
    uhci_td_t *data_td;
    uhci_td_t *status_td;
} uhci_controller_t;

// UHCI Functions
int uhci_init(void);
int uhci_detect_controller(void);
int uhci_reset(uhci_controller_t *ctrl);
int uhci_control_transfer(uhci_controller_t *ctrl, uint8_t dev_addr,
                          uint8_t *setup, uint8_t *data, uint16_t length);

#endif // UHCI_H
