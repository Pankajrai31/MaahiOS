// USB Driver - Basic USB HID Tablet Support
// This is a simplified USB driver for QEMU's USB tablet emulation

#include "usb.h"
#include "uhci.h"
#include "../managers/memory/paging.h"

// Simple output functions (assuming these exist)
extern void vga_puts(const char *str);
extern void *kmalloc(uint32_t size);
extern void kfree(void *ptr);

// PCI access functions (assuming these exist)
extern uint16_t pci_config_read_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern uint32_t pci_config_read_dword(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset);
extern void pci_config_write_word(uint8_t bus, uint8_t slot, uint8_t func, uint8_t offset, uint16_t value);

// I/O port access
static inline void outb(uint16_t port, uint8_t value) {
    __asm__ volatile ("outb %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile ("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outw(uint16_t port, uint16_t value) {
    __asm__ volatile ("outw %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint16_t inw(uint16_t port) {
    uint16_t ret;
    __asm__ volatile ("inw %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outl(uint16_t port, uint32_t value) {
    __asm__ volatile ("outl %0, %1" : : "a"(value), "Nd"(port));
}

static inline uint32_t inl(uint16_t port) {
    uint32_t ret;
    __asm__ volatile ("inl %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Global USB state
static uhci_controller_t g_uhci_ctrl;
static usb_device_t g_tablet_device;
static int g_tablet_found = 0;
static uint8_t g_tablet_endpoint = 0;
static int32_t g_last_x = 512;  // Center of 1024x768
static int32_t g_last_y = 384;
static uint8_t g_last_buttons = 0;

// USB HID Boot Protocol Mouse Report (5 bytes)
typedef struct {
    uint8_t buttons;   // Button states
    int8_t  x;         // X movement
    int8_t  y;         // Y movement
    int8_t  wheel;     // Wheel movement
    int8_t  reserved;  // Reserved
} __attribute__((packed)) usb_hid_mouse_report_t;

// Simplified PCI scan for UHCI controller
static int find_uhci_controller(void) {
    // Scan PCI bus for UHCI controller (Class 0x0C, Subclass 0x03, Interface 0x00)
    for (uint16_t bus = 0; bus < 256; bus++) {
        for (uint8_t slot = 0; slot < 32; slot++) {
            uint16_t vendor = pci_config_read_word(bus, slot, 0, 0);
            if (vendor == 0xFFFF) continue;
            
            uint32_t class_code = pci_config_read_dword(bus, slot, 0, 0x08);
            uint8_t class = (class_code >> 24) & 0xFF;
            uint8_t subclass = (class_code >> 16) & 0xFF;
            uint8_t interface = (class_code >> 8) & 0xFF;
            
            // USB UHCI controller
            if (class == 0x0C && subclass == 0x03 && interface == 0x00) {
                // Read BAR4 (I/O base address)
                uint32_t bar4 = pci_config_read_dword(bus, slot, 0, 0x20);
                g_uhci_ctrl.io_base = bar4 & 0xFFFE;  // Clear bit 0 (I/O space indicator)
                
                // Enable bus mastering and I/O space
                uint16_t cmd = pci_config_read_word(bus, slot, 0, 0x04);
                cmd |= 0x05;  // Enable I/O space and bus mastering
                pci_config_write_word(bus, slot, 0, 0x04, cmd);
                
                vga_puts("USB: Found UHCI controller\n");
                return 1;
            }
        }
    }
    return 0;
}

// Reset UHCI controller
static void uhci_reset_controller(void) {
    uint16_t io_base = g_uhci_ctrl.io_base;
    
    // Stop the controller
    outw(io_base + UHCI_REG_USBCMD, 0);
    
    // Wait for halt
    int timeout = 1000;
    while (!(inw(io_base + UHCI_REG_USBSTS) & UHCI_STS_HCH) && timeout--) {
        // Small delay
        for (volatile int i = 0; i < 1000; i++);
    }
    
    // Reset the controller
    outw(io_base + UHCI_REG_USBCMD, UHCI_CMD_HCRESET);
    
    // Wait for reset to complete
    timeout = 1000;
    while ((inw(io_base + UHCI_REG_USBCMD) & UHCI_CMD_HCRESET) && timeout--) {
        for (volatile int i = 0; i < 1000; i++);
    }
    
    // Clear status register
    outw(io_base + UHCI_REG_USBSTS, 0xFFFF);
}

// Check for devices on UHCI ports
static int uhci_scan_ports(void) {
    uint16_t io_base = g_uhci_ctrl.io_base;
    
    // Check port 1
    uint16_t port1_status = inw(io_base + UHCI_REG_PORTSC1);
    if (port1_status & UHCI_PORT_CCS) {
        vga_puts("USB: Device detected on port 1\n");
        
        // Reset the port
        outw(io_base + UHCI_REG_PORTSC1, port1_status | UHCI_PORT_PR);
        for (volatile int i = 0; i < 100000; i++);  // Delay
        outw(io_base + UHCI_REG_PORTSC1, port1_status & ~UHCI_PORT_PR);
        
        // Enable the port
        port1_status = inw(io_base + UHCI_REG_PORTSC1);
        outw(io_base + UHCI_REG_PORTSC1, port1_status | UHCI_PORT_PE);
        
        return 1;
    }
    
    // Check port 2
    uint16_t port2_status = inw(io_base + UHCI_REG_PORTSC2);
    if (port2_status & UHCI_PORT_CCS) {
        vga_puts("USB: Device detected on port 2\n");
        
        // Reset the port
        outw(io_base + UHCI_REG_PORTSC2, port2_status | UHCI_PORT_PR);
        for (volatile int i = 0; i < 100000; i++);
        outw(io_base + UHCI_REG_PORTSC2, port2_status & ~UHCI_PORT_PR);
        
        // Enable the port
        port2_status = inw(io_base + UHCI_REG_PORTSC2);
        outw(io_base + UHCI_REG_PORTSC2, port2_status | UHCI_PORT_PE);
        
        return 1;
    }
    
    return 0;
}

// Initialize USB subsystem
void usb_init(void) {
    vga_puts("USB: Initializing...\n");
    
    // Find UHCI controller
    if (!find_uhci_controller()) {
        vga_puts("USB: No UHCI controller found\n");
        return;
    }
    
    // Reset controller
    uhci_reset_controller();
    
    // Scan for devices
    if (uhci_scan_ports()) {
        vga_puts("USB: Device enumeration not yet implemented\n");
        vga_puts("USB: Assuming tablet device is present\n");
        
        // For now, just mark tablet as present
        // Full enumeration would require USB transfers and descriptor parsing
        g_tablet_found = 1;
        g_tablet_device.address = 1;
        g_tablet_device.vendor_id = 0x0627;  // QEMU tablet vendor ID
        g_tablet_device.product_id = 0x0001;
    }
}

// Detect USB devices
int usb_detect_devices(void) {
    return g_tablet_found;
}

// Check if USB tablet is present
int usb_is_tablet_present(void) {
    return g_tablet_found;
}

// Get tablet report (simplified - would need actual USB transfers)
int usb_tablet_get_report(int32_t *x, int32_t *y, uint8_t *buttons) {
    if (!g_tablet_found) {
        return 0;
    }
    
    // TODO: Implement actual USB interrupt transfer to read HID report
    // For now, return last known position
    *x = g_last_x;
    *y = g_last_y;
    *buttons = g_last_buttons;
    
    return 1;
}
