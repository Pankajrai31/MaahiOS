#ifndef PIT_H
#define PIT_H

#include <stdint.h>

/* Port I/O functions */
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/**
 * Initialize Programmable Interval Timer
 * frequency: Timer interrupts per second (Hz)
 */
void pit_init(unsigned int frequency);

/**
 * PIT interrupt handler (called from interrupt stub)
 */
void pit_handler(void);

/**
 * PIT handler with context switching support
 * current_esp: Stack pointer of current process
 * Returns: Stack pointer to switch to (may be different process)
 */
uint32_t pit_handler_with_context(uint32_t current_esp);

/**
 * PIT IRQ handler (called from interrupt stub)
 */
void pit_handler();

/**
 * Get total ticks since boot
 */
unsigned int pit_get_ticks();

/**
 * Busy-wait for specified ticks
 */
void pit_wait(unsigned int ticks);

#endif // PIT_H
