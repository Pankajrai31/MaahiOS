#ifndef SCHEDULER_H
#define SCHEDULER_H

#include <stdint.h>

/**
 * Initialize the scheduler
 */
void scheduler_init(void);

/**
 * Called by timer interrupt - switches between processes
 */
void scheduler_tick(void);

/**
 * Enable/disable scheduler
 */
void scheduler_enable(void);
void scheduler_disable(void);

/**
 * Get current process PID
 */
int scheduler_get_current_pid(void);

#endif // SCHEDULER_H
