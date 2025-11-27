#include "pit.h"

/* External scheduler functions */
extern void scheduler_tick(void);

/* PIT frequency: 1.193182 MHz */
#define PIT_FREQUENCY 1193182

/* PIT command/data ports */
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

/* Global tick counter */
static volatile unsigned int pit_ticks = 0;

/**
 * PIT IRQ handler - Called from interrupt stub
 * Simple version: just ticks and calls scheduler
 */
void pit_handler(void) {
    pit_ticks++;
    
    /* Call scheduler - for now just checks if scheduling is needed */
    scheduler_tick();
}

/**
 * Initialize PIT to fire at specified frequency (Hz)
 */
void pit_init(unsigned int frequency) {
    /* Calculate divisor for desired frequency */
    unsigned int divisor = PIT_FREQUENCY / frequency;
    
    /* Send command byte: Channel 0, lobyte/hibyte, rate generator */
    outb(PIT_COMMAND, 0x36);
    
    /* Send divisor (low byte, then high byte) */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

/**
 * Get current tick count
 */
unsigned int pit_get_ticks() {
    return pit_ticks;
}

/**
 * Simple delay function (busy-wait)
 */
void pit_wait(unsigned int ticks) {
    unsigned int end_tick = pit_ticks + ticks;
    while (pit_ticks < end_tick) {
        asm volatile("pause");
    }
}
