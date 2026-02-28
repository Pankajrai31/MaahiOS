/**
 * MaahiOS Scheduler
 * 
 * Round-robin scheduler for multitasking.
 * - Process queue for pending launches
 * - Running process list for round-robin switching
 * - Integrates with PIT timer for preemptive context switching
 */

#include "scheduler.h"
#include "../process/process_manager.h"
#include "../klog/klog.h"

/* External process manager functions */
extern process_t* process_get_by_pid(int pid);

/* Current running process PID (0 = kernel idle) */
static int current_pid = 0;

/* Flag to enable/disable scheduling */
static int scheduling_enabled = 0;

/* Flag to force immediate context switch (when current process dies) */
static int force_switch = 0;

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

/* List of all running processes for round-robin */
static int running_processes[MAX_QUEUED_PROCESSES];
static int running_count = 0;
static int current_index = -1;

/* Context switching control */
static int should_switch = 0;
static int next_switch_pid = 0;

/* ============================================
 * Initialization
 * ============================================ */

int scheduler_init(void) {
    current_pid = 0;
    scheduling_enabled = 0;
    KLOG_INFO("SCHED", "Initialized");
    return 0;
}

/* ============================================
 * Accessors
 * ============================================ */

int scheduler_get_current_pid(void) {
    return current_pid;
}

void scheduler_set_current_pid(int pid) {
    current_pid = pid;
}

int scheduler_should_switch(void) {
    return should_switch;
}

int scheduler_get_next_pid(void) {
    should_switch = 0;  /* Clear flag after reading */
    return next_switch_pid;
}

/* ============================================
 * Scheduler Tick (called from timer interrupt)
 * ============================================ */

void scheduler_tick(void) {
    if (!scheduling_enabled) {
        return;
    }
    
    /* If current process was killed, clear force flag and fall through */
    if (force_switch || current_pid == 0) {
        force_switch = 0;
    }
    
    /* Check if there's a queued process to start */
    if (queue_count > 0) {
        queued_process_t *proc = &process_queue[queue_head];
        
        /* NOTE: Do NOT call gdt_set_kernel_stack() here!
         * scheduler_tick() is called from both PIT and sys_yield().
         * Setting TSS.ESP0 here would corrupt the CURRENT process's
         * kernel stack on its next syscall. The PIT handler sets
         * TSS.ESP0 at the actual context switch point. */
        
        /* Remove from queue */
        queue_head = (queue_head + 1) % MAX_QUEUED_PROCESSES;
        queue_count--;
        
        /* Mark process as running */
        process_t *pcb = process_get_by_pid(proc->pid);
        if (!pcb) {
            KLOG_ERROR_HEX("SCHED", "NULL PCB for queued PID", proc->pid);
            /* Try next process on next tick */
            return;
        }
        
        pcb->state = PROCESS_STATE_RUNNING;
        
        /* Signal context switch to this process */
        should_switch = 1;
        next_switch_pid = proc->pid;
        
        /* Add to running processes list */
        if (running_count < MAX_QUEUED_PROCESSES) {
            running_processes[running_count++] = proc->pid;
            current_index = running_count - 1;
        }
        
        KLOG_INFO("SCHED", "Starting PID %d from queue", proc->pid);
        return;
    }
    
    /* Round-robin context switch between running processes */
    if (running_count > 1) {
        current_index = (current_index + 1) % running_count;
        int next_pid_val = running_processes[current_index];
        
        if (next_pid_val != current_pid) {
            should_switch = 1;
            next_switch_pid = next_pid_val;
        }
    }
}

/* ============================================
 * Enable/Disable
 * ============================================ */

void scheduler_enable() {
    scheduling_enabled = 1;
    KLOG_INFO("SCHED", "Enabled");
}

void scheduler_disable() {
    scheduling_enabled = 0;
    KLOG_INFO("SCHED", "Disabled");
}

/* ============================================
 * Process Management
 * ============================================ */

void scheduler_add_process(int pid, uint32_t entry_point, uint32_t user_stack, uint32_t kernel_stack) {
    if (queue_count >= MAX_QUEUED_PROCESSES) {
        KLOG_ERROR("SCHED", "Queue full, cannot add PID %d", pid);
        return;
    }
    
    process_queue[queue_tail].pid = pid;
    process_queue[queue_tail].entry_point = entry_point;
    process_queue[queue_tail].user_stack = user_stack;
    process_queue[queue_tail].kernel_stack = kernel_stack;
    
    queue_tail = (queue_tail + 1) % MAX_QUEUED_PROCESSES;
    queue_count++;
    
    KLOG_INFO("SCHED", "Queued PID %d (queue_count=%d)", pid, queue_count);
}

void scheduler_remove_process(int pid) {
    int was_current = 0;
    
    /* If it's the current process, mark for removal */
    if (current_pid == pid) {
        current_pid = 0;
        was_current = 1;
    }
    
    /* Remove from process queue (if queued but not running yet) */
    for (int i = 0; i < queue_count; i++) {
        int idx = (queue_head + i) % MAX_QUEUED_PROCESSES;
        if (process_queue[idx].pid == pid) {
            /* Shift remaining items */
            for (int j = i; j < queue_count - 1; j++) {
                int curr_idx = (queue_head + j) % MAX_QUEUED_PROCESSES;
                int next_idx = (queue_head + j + 1) % MAX_QUEUED_PROCESSES;
                process_queue[curr_idx] = process_queue[next_idx];
            }
            queue_count--;
            queue_tail = (queue_tail - 1 + MAX_QUEUED_PROCESSES) % MAX_QUEUED_PROCESSES;
            break;
        }
    }
    
    /* Remove from running_processes array */
    for (int i = 0; i < running_count; i++) {
        if (running_processes[i] == pid) {
            /* Shift remaining items */
            for (int j = i; j < running_count - 1; j++) {
                running_processes[j] = running_processes[j + 1];
            }
            running_count--;
            
            /* Adjust current_index */
            if (current_index >= i && current_index > 0) {
                current_index--;
            }
            if (current_index >= running_count && running_count > 0) {
                current_index = 0;
            }
            break;
        }
    }
    
    KLOG_INFO("SCHED", "Removed PID %d (running=%d, queued=%d)", pid, running_count, queue_count);
    
    /* If we killed the current process, force switch on next tick */
    if (was_current) {
        force_switch = 1;
    }
}
