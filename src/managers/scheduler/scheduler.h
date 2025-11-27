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

/**
 * Add a new process to the ready queue
 * Process will be started on next scheduler tick
 */
void scheduler_add_process(int pid, uint32_t entry_point, uint32_t user_stack, uint32_t kernel_stack);

#endif // SCHEDULER_H
