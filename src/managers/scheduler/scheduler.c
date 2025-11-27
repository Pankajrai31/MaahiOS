/*
 * Simple Scheduler - Works with Process Manager
 * Round-robin scheduling between processes
 */

#include "scheduler.h"

/* External VBE functions */
extern void vbe_print(const char *str, uint32_t fg, uint32_t bg);

/* External process manager functions */
extern int process_manager_get_count(void);
extern void* process_manager_get_next_ready(int current_pid);
extern int process_manager_get_pid(void *pcb);

/* Current running process PID */
static int current_pid = -1;

/* Flag to enable/disable scheduling */
static int scheduling_enabled = 0;

/* Process queue for starting new processes */
#define MAX_QUEUED_PROCESSES 16
typedef struct {
    int pid;
    uint32_t entry_point;
    uint32_t user_stack;
    uint32_t kernel_stack;
} queued_process_t;

static queued_process_t process_queue[MAX_QUEUED_PROCESSES];
static int queue_head = 0;
static int queue_tail = 0;
static int queue_count = 0;

/**
 * Initialize the scheduler
 */
void scheduler_init(void) {
    current_pid = -1;
    scheduling_enabled = 0;
    vbe_print("[SCHEDULER] Initialized\n", 0xFF00FF00, 0xFF001020);
}

/**
 * Get current process PID
 */
int scheduler_get_current_pid(void) {
    return current_pid;
}

/**
 * Called by timer interrupt - switches between processes
 * Starts queued processes on first tick after they're added
 */
void scheduler_tick(void) {
    if (!scheduling_enabled) {
        return;
    }
    
    /* Check if there's a queued process to start */
    if (queue_count > 0) {
        queued_process_t *proc = &process_queue[queue_head];
        
        /* Set TSS kernel stack for this process */
        extern void gdt_set_kernel_stack(unsigned int esp0_value);
        gdt_set_kernel_stack(proc->kernel_stack);
        
        /* Remove from queue */
        queue_head = (queue_head + 1) % MAX_QUEUED_PROCESSES;
        queue_count--;
        
        current_pid = proc->pid;
        
        /* Jump to the new process in Ring 3 - DOES NOT RETURN */
        extern void ring3_switch_with_stack(uint32_t entry_point, uint32_t stack_top);
        ring3_switch_with_stack(proc->entry_point, proc->user_stack);
    }
    
    /* If no queued processes, continue running current process */
    /* TODO: Implement context switching between running processes */
}

/**
 * Enable scheduling
 */
void scheduler_enable() {
    scheduling_enabled = 1;
    vbe_print("[SCHEDULER] Enabled\n", 0xFF00FF00, 0xFF001020);
}

/**
 * Disable scheduling
 */
void scheduler_disable() {
    scheduling_enabled = 0;
    vbe_print("[SCHEDULER] Disabled\n", 0xFFFFFF00, 0xFF001020);
}

/**
 * Add a new process to the ready queue
 * It will be started on the next scheduler tick
 */
void scheduler_add_process(int pid, uint32_t entry_point, uint32_t user_stack, uint32_t kernel_stack) {
    if (queue_count >= MAX_QUEUED_PROCESSES) {
        return;  /* Queue full */
    }
    
    process_queue[queue_tail].pid = pid;
    process_queue[queue_tail].entry_point = entry_point;
    process_queue[queue_tail].user_stack = user_stack;
    process_queue[queue_tail].kernel_stack = kernel_stack;
    
    queue_tail = (queue_tail + 1) % MAX_QUEUED_PROCESSES;
    queue_count++;
}
