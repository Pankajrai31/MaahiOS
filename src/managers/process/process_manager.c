/*
 * Process Manager - Simple version (Phase 1)
 * Manages process creation without scheduler integration yet
 */

#include "process_manager.h"

/* External functions */
extern void vga_puts(const char *str);
extern void vga_put_hex(uint32_t val);
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
    
    vga_puts("[PROCESS] Process manager initialized\n");
}

/**
 * Create sysman process (PID 1)
 */
int process_create_sysman(uint32_t sysman_address) {
    vga_puts("[PROCESS] Creating sysman process...\n");
    
    /* Allocate PCB from kernel heap */
    process_t *pcb = (process_t *)kmalloc(sizeof(process_t));
    if (!pcb) {
        vga_puts("[PROCESS] ERROR: Failed to allocate PCB!\n");
        return -1;
    }
    
    vga_puts("[PROCESS] PCB allocated at: 0x");
    vga_put_hex((uint32_t)pcb);
    vga_puts("\n");
    
    /* Initialize PCB */
    pcb->pid = next_pid++;
    pcb->entry_point = sysman_address;
    pcb->state = PROCESS_STATE_READY;
    
    /* Add to process table */
    process_table[pcb->pid - 1] = pcb;
    
    vga_puts("[PROCESS] Sysman process created (PID ");
    vga_put_hex(pcb->pid);
    vga_puts(", entry: 0x");
    vga_put_hex(pcb->entry_point);
    vga_puts(")\n");
    
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

/**
 * Start a process (jump to Ring 3)
 */
void process_start(process_t *pcb) {
    if (!pcb) {
        vga_puts("[PROCESS] ERROR: Invalid PCB!\n");
        return;
    }
    
    vga_puts("[PROCESS] Starting process PID ");
    vga_put_hex(pcb->pid);
    vga_puts(" at 0x");
    vga_put_hex(pcb->entry_point);
    vga_puts("\n");
    
    /* Mark as running */
    pcb->state = PROCESS_STATE_RUNNING;
    
    /* Jump to Ring 3 - NEVER RETURNS */
    ring3_switch(pcb->entry_point);
}
