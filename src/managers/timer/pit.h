#ifndef PIT_H
#define PIT_H

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
