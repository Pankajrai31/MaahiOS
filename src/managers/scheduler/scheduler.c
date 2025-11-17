/*
 * Simple Scheduler - Works with Process Manager
 * Round-robin scheduling between processes
 */

#include "scheduler.h"

/* External VGA functions */
extern void vga_puts(const char *str);
extern void vga_put_hex(uint32_t val);

/* External process manager functions */
extern int process_manager_get_count(void);
extern void* process_manager_get_next_ready(int current_pid);
extern int process_manager_get_pid(void *pcb);

/* Current running process PID */
static int current_pid = -1;

/* Flag to enable/disable scheduling */
static int scheduling_enabled = 0;

/**
 * Initialize the scheduler
 */
void scheduler_init(void) {
    current_pid = -1;
    scheduling_enabled = 0;
    vga_puts("[SCHEDULER] Initialized\n");
}

/**
 * Get current process PID
 */
int scheduler_get_current_pid(void) {
    return current_pid;
}

/**
 * Called by timer interrupt - switches between processes
 * For now, just keeps running current process (sysman)
 * TODO: Implement context saving/switching when we have multiple processes
 */
void scheduler_tick(void) {
    if (!scheduling_enabled) {
        return;
    }
    
    /* Get process count from process manager */
    int process_count = process_manager_get_count();
    
    if (process_count == 0) {
        return;  /* No processes */
    }
    
    if (process_count == 1) {
        /* Only one process (sysman) - just keep running it */
        return;
    }
    
    /* TODO: Multiple processes - implement context switching */
    /* 1. Save current process context (registers, ESP) */
    /* 2. Get next ready process from process_manager */
    /* 3. Switch to new process (load its context, switch page directory if needed) */
}

/**
 * Enable scheduling
 */
void scheduler_enable(void) {
    scheduling_enabled = 1;
    vga_puts("[SCHEDULER] Enabled\n");
}

/**
 * Disable scheduling
 */
void scheduler_disable(void) {
    scheduling_enabled = 0;
    vga_puts("[SCHEDULER] Disabled\n");
}
