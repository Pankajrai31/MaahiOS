#include "pit.h"
#include <stdint.h>

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
 * PIT handler with context switching support
 * Called from irq0_stub with current ESP on stack
 * Returns new ESP (same process or switched process)
 */
uint32_t pit_handler_with_context(uint32_t current_esp) {
    pit_ticks++;
    
    /* For now, just call scheduler and return same ESP */
    /* TODO: Implement actual context save/restore */
    scheduler_tick();
    
    return current_esp;  /* Return same ESP for now */
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
