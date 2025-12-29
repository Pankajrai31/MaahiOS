/*
 * Simple Scheduler - Works with Process Manager
 * Round-robin scheduling between processes
 */

#include "scheduler.h"
#include "../process/process_manager.h"

/* Simple serial port output for debugging */
static void serial_print(const char *str) {
    while (*str) {
        while (!(*(volatile unsigned char*)0x3FD & 0x20));
        *(volatile unsigned char*)0x3F8 = *str++;
    }
}

static void serial_hex(unsigned char byte) {
    const char hex[] = "0123456789ABCDEF";
    while (!(*(volatile unsigned char*)0x3FD & 0x20));
    *(volatile unsigned char*)0x3F8 = hex[(byte >> 4) & 0xF];
    while (!(*(volatile unsigned char*)0x3FD & 0x20));
    *(volatile unsigned char*)0x3F8 = hex[byte & 0xF];
}

/* External process manager functions */
extern process_t* process_get_by_pid(int pid);

/* Current running process PID (0 = kernel idle) */
static int current_pid = 0;

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
    current_pid = 0;  /* 0 = kernel idle */
    scheduling_enabled = 0;
    serial_print("[SCHEDULER] Initialized\n");
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
        
        serial_print("[SCHEDULER] Starting PID ");
        serial_hex(proc->pid);
        serial_print(" from queue\n");
        
        /* Set TSS kernel stack for this process */
        extern void gdt_set_kernel_stack(unsigned int esp0_value);
        gdt_set_kernel_stack(proc->kernel_stack);
        
        /* Remove from queue */
        queue_head = (queue_head + 1) % MAX_QUEUED_PROCESSES;
        queue_count--;
        
        current_pid = proc->pid;
        
        /* Mark process as running */
        process_t *pcb = process_get_by_pid(proc->pid);
        if (pcb) {
            pcb->state = PROCESS_STATE_RUNNING;
            /* Initialize context for first run */
            pcb->eip = proc->entry_point;
            pcb->esp = proc->user_stack;
            pcb->cs = 0x1B;  /* Ring 3 code segment */
            pcb->ds = 0x23;  /* Ring 3 data segment */
            pcb->ss = 0x23;  /* Ring 3 stack segment */
            pcb->eflags = 0x202;  /* IF=1 (interrupts enabled) */
        }
        
        /* Jump to the new process in Ring 3 - DOES NOT RETURN */
        extern void ring3_switch_with_stack(uint32_t entry_point, uint32_t stack_top);
        ring3_switch_with_stack(proc->entry_point, proc->user_stack);
    }
    
    /* Context switch between running processes - not yet implemented */
    /* Will add this in next step after verifying basic scheduler works */
}

/**
 * Enable scheduling
 */
void scheduler_enable() {
    scheduling_enabled = 1;
    serial_print("[SCHEDULER] Enabled\n");
}

/**
 * Disable scheduling
 */
void scheduler_disable() {
    scheduling_enabled = 0;
    serial_print("[SCHEDULER] Disabled\n");
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
