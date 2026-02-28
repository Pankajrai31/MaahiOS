#ifndef PIT_H
#define PIT_H

#include <stdint.h>
#include "../../system/libraries/shared/io.h"

/**
 * Initialize Programmable Interval Timer
 * frequency: Timer interrupts per second (Hz)
 * Returns: 0 on success
 */
int pit_init(unsigned int frequency);

/**
 * PIT handler with context switching support
 * current_esp: Stack pointer of current process
 * Returns: Stack pointer to switch to (may be different process)
 */
uint32_t pit_handler_with_context(uint32_t current_esp);

/**
 * Get total ticks since boot (32-bit)
 */
unsigned int pit_get_ticks(void);

/**
 * Get total ticks since boot (64-bit)
 */
uint64_t pit_get_ticks64(void);

/**
 * Busy-wait for specified ticks
 */
void pit_wait(unsigned int ticks);

#endif // PIT_H
