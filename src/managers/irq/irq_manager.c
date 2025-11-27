/**
 * IRQ Manager - Centralized interrupt request management
 */

#include "irq_manager.h"

/* Port I/O functions */
static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void io_wait(void) {
    // Port 0x80 is used for 'checkpoints' during POST
    // Linux uses it for a brief delay
    outb(0x80, 0);
}

/* PIC ports */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/**
 * Remap PIC to avoid conflicts with CPU exceptions
 * IRQ 0-7 → INT 0x20-0x27
 * IRQ 8-15 → INT 0x28-0x2F
 */
static void pic_remap(void) {
    // Start initialization sequence (ICW1)
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    
    // Set vector offsets (ICW2)
    outb(PIC1_DATA, 0x20);  // Master PIC: IRQ 0-7 → INT 0x20-0x27
    io_wait();
    outb(PIC2_DATA, 0x28);  // Slave PIC: IRQ 8-15 → INT 0x28-0x2F
    io_wait();
    
    // Tell PICs about each other (ICW3)
    outb(PIC1_DATA, 0x04);  // Slave on IRQ2
    io_wait();
    outb(PIC2_DATA, 0x02);  // Cascade identity
    io_wait();
    
    // Set 8086 mode (ICW4)
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    
    // Start with all IRQs masked - we'll enable them selectively
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/**
 * Initialize IRQ manager
 */
void irq_manager_init(void) {
    pic_remap();
    // pic_remap already masks all IRQs - no need to do it again
}

// Simple serial output for debugging
static void serial_print(const char *str) {
    while (*str) {
        while ((inb(0x3FD) & 0x20) == 0);
        outb(0x3F8, *str++);
    }
}

static void serial_hex(unsigned char value) {
    char hex[] = "0123456789ABCDEF";
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[(value >> 4) & 0xF]);
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[value & 0xF]);
}

/**
 * Enable specific IRQ line (0-15)
 */
void irq_enable(int irq_number) {
    unsigned short port;
    unsigned char mask;
    int original_irq = irq_number;
    
    serial_print("\n[IRQ_ENABLE] IRQ ");
    serial_hex(original_irq);
    
    if (irq_number < 8) {
        // Master PIC (IRQ 0-7)
        port = PIC1_DATA;
        serial_print(" -> MASTER\n");
    } else {
        // Slave PIC (IRQ 8-15)
        port = PIC2_DATA;
        irq_number -= 8;
        serial_print(" -> SLAVE\n");
        
        // Also enable IRQ2 (cascade) on master - use separate variable
        unsigned char master_mask = inb(PIC1_DATA);
        serial_print("[IRQ2_CASCADE] Before: ");
        serial_hex(master_mask);
        
        unsigned char master_before = master_mask;
        master_mask &= ~(1 << 2);
        
        serial_print(" After calc: ");
        serial_hex(master_mask);
        
        outb(PIC1_DATA, master_mask);
        
        // Verify it was written
        unsigned char master_after = inb(PIC1_DATA);
        serial_print(" Readback: ");
        serial_hex(master_after);
        serial_print("\n");
        
        if (master_after != master_mask) {
            serial_print("[IRQ2_CASCADE] WRITE FAILED! Retrying...\n");
            // Write failed! Try again
            outb(PIC1_DATA, master_mask);
            io_wait();
            outb(PIC1_DATA, master_mask);
        }
    }
    
    // Clear the mask bit for this IRQ
    unsigned char before = inb(port);
    serial_print("[IRQ_MASK] Before: ");
    serial_hex(before);
    serial_print(" IRQ_BIT: ");
    serial_hex(irq_number);
    
    mask = before;
    // IMPORTANT: Cast to unsigned char FIRST to get correct 8-bit mask
    unsigned char bit_mask = (unsigned char)~((unsigned char)(1 << irq_number));
    serial_print(" ~(1<<n): ");
    serial_hex(bit_mask);
    
    mask &= bit_mask;
    
    serial_print(" After calc: ");
    serial_hex(mask);
    
    outb(port, mask);
    
    // Verify it was written
    unsigned char after = inb(port);
    serial_print(" Readback: ");
    serial_hex(after);
    serial_print("\n");
    
    if (after != mask) {
        serial_print("[IRQ_MASK] WRITE FAILED! Retrying...\n");
        // Write failed! Try multiple times with delays
        for (int i = 0; i < 5; i++) {
            outb(port, mask);
            io_wait();
        }
    }
}

/**
 * Disable specific IRQ line (0-15)
 */
void irq_disable(int irq_number) {
    unsigned short port;
    unsigned char mask;
    
    if (irq_number < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_number -= 8;
    }
    
    // Set the mask bit for this IRQ
    mask = inb(port);
    mask |= (1 << irq_number);
    outb(port, mask);
}

/**
 * Enable timer IRQ (IRQ 0)
 */
void irq_enable_timer(void) {
    irq_enable(0);
}

/**
 * Enable mouse IRQ (IRQ 12)
 */
void irq_enable_mouse(void) {
    irq_enable(12);
}

/**
 * Get PIC mask status for debugging
 * Returns: (slave_mask << 8) | master_mask
 */
unsigned int irq_get_pic_mask(void) {
    unsigned char master = inb(PIC1_DATA);
    unsigned char slave = inb(PIC2_DATA);
    return (slave << 8) | master;
}
