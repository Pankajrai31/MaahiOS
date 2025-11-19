/*
 * Process Manager - Simple version (Phase 1)
 * Manages process creation without scheduler integration yet
 */

#include "process_manager.h"

/* External functions */
extern void* kmalloc(uint32_t size);
extern void kfree(void* ptr);
extern void ring3_switch(uint32_t entry_point);

/* Process table (simple array for now) */
#define MAX_PROCESSES 64
static process_t *process_table[MAX_PROCESSES];
static int next_pid = 1;

/**
 * Initialize process manager
 */
void process_manager_init(void) {
    /* Clear process table */
    for (int i = 0; i < MAX_PROCESSES; i++) {
        process_table[i] = 0;
    }
    next_pid = 1;
}

/**
 * Create sysman process (PID 1) and start it immediately
 */
int process_create_sysman(uint32_t sysman_address) {
    process_t *pcb = (process_t *)kmalloc(sizeof(process_t));
    if (!pcb) {
        return -1;
    }
    
    pcb->pid = next_pid++;
    pcb->entry_point = sysman_address;
    pcb->state = PROCESS_STATE_RUNNING;
    
    process_table[pcb->pid - 1] = pcb;
    
    /* Jump to Ring 3 - NEVER RETURNS */
    ring3_switch(pcb->entry_point);
    
    return pcb->pid;
}

/**
 * Create a generic process (used by syscall)
 * Creates PCB and starts the process immediately
 */
int process_create(uint32_t entry_point) {
    if (next_pid >= MAX_PROCESSES) {
        return -1;
    }
    
    process_t *pcb = (process_t *)kmalloc(sizeof(process_t));
    if (!pcb) {
        return -1;
    }
    
    pcb->pid = next_pid++;
    pcb->entry_point = entry_point;
    pcb->state = PROCESS_STATE_RUNNING;
    
    process_table[pcb->pid - 1] = pcb;
    
    /* Jump to Ring 3 - NEVER RETURNS */
    ring3_switch(pcb->entry_point);
    
    return pcb->pid;
}

/**
 * Get process by PID
 */
process_t* process_get_by_pid(int pid) {
    if (pid < 1 || pid > MAX_PROCESSES) {
        return 0;
    }
    return process_table[pid - 1];
}

/**
 * Get total process count
 */
int process_manager_get_count(void) {
    int count = 0;
    for (int i = 0; i < MAX_PROCESSES; i++) {
        if (process_table[i] != 0) {
            count++;
        }
    }
    return count;
}


