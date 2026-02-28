/**
 * MaahiOS IRQ Manager
 * 
 * Manages the 8259 PIC (Programmable Interrupt Controller).
 * - Remaps IRQs 0-15 to INT 0x20-0x2F (avoids CPU exception conflicts)
 * - Provides selective enable/disable of individual IRQ lines
 * - All IRQs masked by default, enabled selectively by drivers
 */

#include "irq_manager.h"
#include "../klog/klog.h"
#include "../../system/libraries/shared/io.h"

/* ============================================
 * PIC Ports
 * ============================================ */
#define PIC1_COMMAND 0x20
#define PIC1_DATA    0x21
#define PIC2_COMMAND 0xA0
#define PIC2_DATA    0xA1

/* ============================================
 * PIC Remapping
 * ============================================ */

/**
 * Remap PIC to avoid conflicts with CPU exceptions.
 * IRQ 0-7  → INT 0x20-0x27 (Master PIC)
 * IRQ 8-15 → INT 0x28-0x2F (Slave PIC)
 */
static void pic_remap(void) {
    /* ICW1: Start initialization sequence */
    outb(PIC1_COMMAND, 0x11);
    io_wait();
    outb(PIC2_COMMAND, 0x11);
    io_wait();
    
    /* ICW2: Set vector offsets */
    outb(PIC1_DATA, 0x20);  /* Master: IRQ 0-7 → INT 0x20-0x27 */
    io_wait();
    outb(PIC2_DATA, 0x28);  /* Slave: IRQ 8-15 → INT 0x28-0x2F */
    io_wait();
    
    /* ICW3: Tell PICs about each other */
    outb(PIC1_DATA, 0x04);  /* Master: Slave on IRQ2 */
    io_wait();
    outb(PIC2_DATA, 0x02);  /* Slave: Cascade identity */
    io_wait();
    
    /* ICW4: Set 8086 mode */
    outb(PIC1_DATA, 0x01);
    io_wait();
    outb(PIC2_DATA, 0x01);
    io_wait();
    
    /* Mask all IRQs - drivers enable them selectively */
    outb(PIC1_DATA, 0xFF);
    outb(PIC2_DATA, 0xFF);
}

/* ============================================
 * Initialization
 * ============================================ */

int irq_manager_init(void) {
    pic_remap();
    KLOG_INFO("IRQ", "PIC remapped (IRQ 0-15 -> INT 0x20-0x2F)");
    return 0;
}

/* ============================================
 * IRQ Enable/Disable
 * ============================================ */

void irq_enable(int irq_number) {
    unsigned short port;
    unsigned char mask;
    
    if (irq_number < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_number -= 8;
        
        /* Also enable IRQ2 (cascade) on master PIC */
        mask = inb(PIC1_DATA);
        mask &= ~(1 << 2);
        outb(PIC1_DATA, mask);
    }
    
    /* Clear the mask bit for this IRQ */
    mask = inb(port);
    mask &= ~(1 << irq_number);
    outb(port, mask);
    
    KLOG_DEBUG_HEX("IRQ", "Enabled IRQ", irq_number);
}

void irq_disable(int irq_number) {
    unsigned short port;
    unsigned char mask;
    
    if (irq_number < 8) {
        port = PIC1_DATA;
    } else {
        port = PIC2_DATA;
        irq_number -= 8;
    }
    
    mask = inb(port);
    mask |= (1 << irq_number);
    outb(port, mask);
    
    KLOG_DEBUG_HEX("IRQ", "Disabled IRQ", irq_number);
}

/* ============================================
 * Convenience Functions
 * ============================================ */

void irq_enable_timer(void) {
    irq_enable(0);
}

void irq_enable_keyboard(void) {
    irq_enable(1);
}

void irq_enable_mouse(void) {
    irq_enable(12);
}

unsigned int irq_get_pic_mask(void) {
    unsigned char master = inb(PIC1_DATA);
    unsigned char slave = inb(PIC2_DATA);
    return (slave << 8) | master;
}
