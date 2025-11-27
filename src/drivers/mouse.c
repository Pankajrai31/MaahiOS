/**
 * Simple Mouse - Just store IRQ12 data
 */

#include "mouse.h"
#include <stdint.h>

// Port to read mouse data
#define MOUSE_DATA_PORT 0x60
#define SERIAL_PORT 0x3F8

// Simple variables to store IRQ data (start at screen center)
// NOTE: NOT static so they're globally visible across syscall boundary
volatile int mouse_x;
volatile int mouse_y;
volatile int irq_count;
volatile int irq_total;  // Total IRQs received (even invalid ones)

// Packet assembly
static uint8_t packet[3];
static int packet_byte = 0;
static volatile int mouse_ready = 0;  // Set to 1 after init complete

// Port I/O
static inline uint8_t inb(uint16_t port) {
    uint8_t ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(uint16_t port, uint8_t val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

// Serial debug output
static void serial_putc(char c) {
    while ((inb(SERIAL_PORT + 5) & 0x20) == 0);
    outb(SERIAL_PORT, c);
}

static void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') serial_putc('\r');
        serial_putc(*str++);
    }
}

static void serial_hex(uint8_t val) {
    const char hex[] = "0123456789ABCDEF";
    serial_putc(hex[val >> 4]);
    serial_putc(hex[val & 0xF]);
}

// Helper: Wait for Input Buffer Empty flag to clear (IBF=0)
// Wait for input buffer to be clear (bit 1 = 0)
static inline void wait_input_clear(void) {
    for (int i = 0; i < 1000; i++) {
        if ((inb(0x64) & 0x02) == 0) return;
    }
}

// Wait for output buffer to be full (bit 0 = 1)
static inline void wait_output_full(void) {
    for (int i = 0; i < 1000; i++) {
        if (inb(0x64) & 0x01) return;
    }
}

// Flush any pending bytes in output buffer
static inline void flush_output(void) {
    for (int i = 0; i < 16; i++) {
        if (inb(0x64) & 0x01)
            (void)inb(0x60);
        else
            break;
    }
}

/**
 * Minimal clean PS/2 mouse init
 */
int mouse_init(void) {
    uint8_t cmd;
    
    // Initialize variables explicitly
    mouse_x = 320;
    mouse_y = 240;
    irq_count = 0;
    irq_total = 0;
    packet_byte = 0;
    mouse_ready = 0;
    
    // Disable both ports
    wait_input_clear();
    outb(0x64, 0xAD);  // Disable keyboard
    wait_input_clear();
    outb(0x64, 0xA7);  // Disable mouse
    
    // Flush any pending bytes
    flush_output();
    
    // Read command byte
    wait_input_clear();
    outb(0x64, 0x20);
    wait_output_full();
    cmd = inb(0x60);
    
    // Enable IRQs and mouse clock
    cmd |=  0x03;      // Enable keyboard IRQ (bit 0) and mouse IRQ (bit 1)
    cmd &= ~0x20;      // Enable mouse clock (bit 5 = 0)
    
    // Write command byte back
    wait_input_clear();
    outb(0x64, 0x60);
    wait_input_clear();
    outb(0x60, cmd);
    
    // Enable mouse port
    wait_input_clear();
    outb(0x64, 0xA8);
    
    // Enable keyboard port
    wait_input_clear();
    outb(0x64, 0xAE);
    
    // Send "enable data reporting" to mouse
    wait_input_clear();
    outb(0x64, 0xD4);  // Write to mouse
    wait_input_clear();
    outb(0x60, 0xF4);  // Enable data reporting
    
    // Read ACK - must be 0xFA
    wait_output_full();
    uint8_t ack = inb(0x60);
    
    serial_print("[MOUSE_INIT] Enable reporting ACK: ");
    serial_hex(ack);
    serial_print("\n");
    
    // Small delay for mouse to start generating data
    for (volatile int i = 0; i < 10000; i++);
    
    // Flush any initial bytes the mouse might send
    flush_output();
    
    // Now mouse is ready to send movement data
    mouse_ready = 1;
    
    return 1;
}

// Global variable to store timer mask during packet read
static uint8_t saved_timer_mask = 0;
static int timer_masked = 0;

/**
 * Simple IRQ12 handler - collect bytes one at a time
 * CRITICAL: Must ALWAYS read port 0x60 when IRQ12 fires, or PS/2 controller stops generating interrupts!
 * CRITICAL: Must read all 3 bytes atomically (disable timer IRQ during read to prevent desync)
 */
void mouse_handler(void) {
    irq_total++;
    
    // SAFETY CHECK: If timer is still masked after 100 interrupts, force unmask
    static int masked_irq_count = 0;
    if (timer_masked) {
        masked_irq_count++;
        if (masked_irq_count > 100) {
            serial_print("EMERGENCY_TIMER_UNMASK!\n");
            outb(0x21, saved_timer_mask);
            timer_masked = 0;
            masked_irq_count = 0;
        }
    } else {
        masked_irq_count = 0;
    }
    
    // Check status register
    uint8_t status = inb(0x64);
    
    if (!(status & 0x01)) {
        // This should NEVER happen in IRQ12 - but if it does, something is very wrong
        serial_print("MOUSE_IRQ12_NO_DATA!\n");
        // If timer was masked, restore it before returning
        if (timer_masked) {
            serial_print("RESTORING_TIMER_NO_DATA\n");
            outb(0x21, saved_timer_mask);
            timer_masked = 0;
        }
        return;
    }
    
    // CRITICAL: MUST read data port even if we're going to discard it
    // Otherwise PS/2 controller buffer fills and stops generating interrupts!
    uint8_t data = inb(MOUSE_DATA_PORT);
    
    if (!(status & 0x20)) {
        // Data was from keyboard, not mouse - discard
        serial_print("MOUSE_IRQ12_KBD_DATA!\n");
        return;
    }
    
    if (!mouse_ready) {
        // Mouse not initialized yet
        return;
    }
    
    irq_count++;
    
    // First byte must have bit 3 set
    if (packet_byte == 0) {
        if (!(data & 0x08)) {
            serial_print("MOUSE_INVALID_BYTE1\n");
            return;  // Invalid, wait for next
        }
        // TEST: Timer masking DISABLED to see if it's causing IRQ12 to stop
        // Friend's theory was desync, but IRQ12 count went DOWN after adding this
        // saved_timer_mask = inb(0x21);
        // outb(0x21, saved_timer_mask | 0x01);
        // timer_masked = 1;
    }
    
    packet[packet_byte++] = data;
    
    // When we have 3 bytes, update position
    if (packet_byte == 3) {
        int8_t dx = packet[1];
        int8_t dy = packet[2];
        
        // Increase sensitivity by 2x (since polling introduces delay)
        mouse_x += dx * 2;
        mouse_y -= dy * 2;
        
        // Clamp to screen size (1024x768)
        if (mouse_x < 0) mouse_x = 0;
        if (mouse_x > 1023) mouse_x = 1023;
        if (mouse_y < 0) mouse_y = 0;
        if (mouse_y > 767) mouse_y = 767;
        
        // Log updated value
        serial_print("HANDLER_UPDATE: dx=");
        serial_hex((uint8_t)dx);
        serial_print(" dy=");
        serial_hex((uint8_t)dy);
        serial_print(" -> mouse_x=");
        serial_hex(mouse_x >> 8);
        serial_hex((uint8_t)mouse_x);
        serial_print(" mouse_y=");
        serial_hex(mouse_y >> 8);
        serial_hex((uint8_t)mouse_y);
        serial_print("\n");
        
        packet_byte = 0;
        
        // TEST: Timer masking DISABLED
        // if (timer_masked) {
        //     outb(0x21, saved_timer_mask);
        //     timer_masked = 0;
        // }
    }
}

/**
 * Drain PS/2 controller buffer and FULLY re-initialize mouse
 * CRITICAL for re-enabling interrupts after Ring 3 switch
 */
void mouse_drain_buffer(void) {
    uint8_t status;
    uint8_t cmd_byte;
    
    // Read and discard all pending data from PS/2 controller
    for (int i = 0; i < 16; i++) {
        status = inb(0x64);
        if (status & 0x01) {  // Output buffer full
            inb(0x60);  // Read and discard
        } else {
            break;  // Buffer empty
        }
    }
    packet_byte = 0;  // Reset packet state
    
    // CRITICAL: Ensure 8042 controller's command byte has mouse IRQ enabled
    // Bit 5 of command byte must be set for mouse interrupts to work
    cmd_byte = 0x47;  // Safe default: keyboard IRQ + mouse IRQ + system flag + keyboard enabled
    outb(0x64, 0x20);  // Command: read command byte
    for (int i = 0; i < 10000; i++) {
        status = inb(0x64);
        if (status & 0x01) {
            cmd_byte = inb(0x60);
            break;
        }
    }
    
    serial_print("\n[8042_CMD_BYTE] Read: 0x");
    serial_hex(cmd_byte);
    serial_print(" (bit5=");
    serial_hex((cmd_byte >> 5) & 1);
    serial_print(")\n");
    
    // Enable mouse IRQ (bit 1) and ensure mouse clock enabled (bit 5 = 0)
    cmd_byte |=  0x02;   // Set bit 1: enable mouse IRQ
    cmd_byte &= ~0x20;   // Clear bit 5: enable mouse port clock (0=enabled!)
    
    serial_print("[8042_CMD_BYTE] After modify: 0x");
    serial_hex(cmd_byte);
    serial_print(" (bit5=");
    serial_hex((cmd_byte >> 5) & 1);
    serial_print(")\n");
    
    outb(0x64, 0x60);
    outb(0x60, cmd_byte);
    
    // Small delay
    for (volatile int i = 0; i < 10000; i++);
    
    // Read back to verify
    outb(0x64, 0x20);
    for (int i = 0; i < 10000; i++) {
        status = inb(0x64);
        if (status & 0x01) {
            uint8_t verify = inb(0x60);
            serial_print("[8042_CMD_BYTE] Verify: 0x");
            serial_hex(verify);
            serial_print(" (bit5=");
            serial_hex((verify & 0x20) >> 5);
            serial_print(")\n");
            break;
        }
    }
    
    // Re-enable mouse data reporting
    outb(0x64, 0xD4);
    outb(0x60, 0xF4);
    for (int i = 0; i < 10000; i++) {
        status = inb(0x64);
        if (status & 0x01) { inb(0x60); break; }
    }
    
    mouse_ready = 1;  // Mark mouse as ready again
}

/**
 * Get X - force memory read with barrier
 */
int mouse_get_x(void) {
    __asm__ volatile("" ::: "memory");  // Memory barrier
    return mouse_x;
}

/**
 * Get Y - force memory read with barrier
 */
int mouse_get_y(void) {
    __asm__ volatile("" ::: "memory");  // Memory barrier
    return mouse_y;
}

/**
 * Get total IRQ count (for debugging)
 */
int mouse_get_irq_total(void) {
    __asm__ volatile("" ::: "memory");  // Memory barrier
    return irq_total;
}

/**
 * Get buttons - return 0 for now
 */
uint8_t mouse_get_buttons(void) {
    return 0;
}
