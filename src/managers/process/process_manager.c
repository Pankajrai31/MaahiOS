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

/* Stack allocator - each process gets unique stacks */
#define USER_STACK_BASE 0x00200000  /* Start at 2MB for user stacks */
#define USER_STACK_SIZE 0x00010000  /* 64KB per process */
static uint32_t next_stack_top = USER_STACK_BASE;

/* CRITICAL: Kernel interrupt stacks must NOT overlap with orbit (0x00300000)! */
#define KERNEL_INT_STACK_BASE 0x00280000  /* Start at 2.5MB for kernel interrupt stacks */
#define KERNEL_INT_STACK_SIZE 0x00004000  /* 16KB per process */
static uint32_t next_kernel_stack_top = KERNEL_INT_STACK_BASE;

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

/* Serial debug helper */
static void serial_print(const char *str) {
    while (*str) {
        while ((*(volatile unsigned char*)0x3FD & 0x20) == 0);
        *(volatile unsigned char*)0x3F8 = *str++;
    }
}

static void serial_hex32(uint32_t value) {
    char hex[] = "0123456789ABCDEF";
    for (int i = 28; i >= 0; i -= 4) {
        while ((*(volatile unsigned char*)0x3FD & 0x20) == 0);
        *(volatile unsigned char*)0x3F8 = hex[(value >> i) & 0xF];
    }
}

/**
 * Create sysman process (PID 1) and start it immediately
 */
int process_create_sysman(uint32_t sysman_address) {
    // ULTRA EARLY debug - before ANYTHING else
    *(volatile unsigned char*)0x3F8 = 'X';
    *(volatile unsigned char*)0x3F8 = '\n';
    
    serial_print("[PROCESS] Entered process_create_sysman\n");
    serial_print("[PROCESS] Creating sysman at 0x");
    serial_hex32(sysman_address);
    serial_print("\n");
    
    serial_print("[PROCESS] About to call kmalloc...\n");
    process_t *pcb = (process_t *)kmalloc(sizeof(process_t));
    serial_print("[PROCESS] kmalloc returned\n");
    if (!pcb) {
        serial_print("[PROCESS] ERROR: kmalloc failed!\n");
        return -1;
    }
    
    pcb->pid = next_pid++;
    pcb->entry_point = sysman_address;
    pcb->state = PROCESS_STATE_RUNNING;
    
    /* Allocate unique user stack */
    uint32_t stack_top = next_stack_top;
    next_stack_top += USER_STACK_SIZE;
    
    /* Allocate unique kernel interrupt stack */
    uint32_t kernel_stack_top = next_kernel_stack_top;
    next_kernel_stack_top += KERNEL_INT_STACK_SIZE;
    
    serial_print("[PROCESS] User stack: 0x");
    serial_hex32(stack_top);
    serial_print(" Kernel stack: 0x");
    serial_hex32(kernel_stack_top);
    serial_print("\n");
    
    process_table[pcb->pid - 1] = pcb;
    
    /* CRITICAL: Set TSS.esp0 to this process's kernel interrupt stack */
    extern void gdt_set_kernel_stack(unsigned int esp0_value);
    serial_print("[PROCESS] Setting TSS kernel stack\n");
    gdt_set_kernel_stack(kernel_stack_top);
    
    /* Enable interrupts before jumping to Ring 3 */
    serial_print("[PROCESS] Enabling interrupts\n");
    __asm__ volatile("sti");
    
    /* Jump to Ring 3 - NEVER RETURNS */
    serial_print("[PROCESS] Jumping to Ring 3...\n");
    extern void ring3_switch_with_stack(uint32_t entry_point, uint32_t stack_top);
    ring3_switch_with_stack(pcb->entry_point, stack_top);
    
    serial_print("[PROCESS] ERROR: Returned from ring3_switch!\n");
    return pcb->pid;
}

/**
 * Create a generic process (used by syscall)
 * Creates PCB, adds to ready queue, and RETURNS control to caller
 * The new process will be started by scheduler
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
    pcb->state = PROCESS_STATE_READY;  /* Mark as READY, not RUNNING */
    
    /* Allocate unique user stack */
    uint32_t stack_top = next_stack_top;
    next_stack_top += USER_STACK_SIZE;
    
    /* Allocate unique kernel interrupt stack */
    uint32_t kernel_stack_top = next_kernel_stack_top;
    next_kernel_stack_top += KERNEL_INT_STACK_SIZE;
    
    /* Store stack pointers in PCB for later use */
    pcb->user_stack_top = stack_top;
    pcb->kernel_stack_top = kernel_stack_top;
    
    /* Add to process table */
    process_table[pcb->pid - 1] = pcb;
    
    /* Add to scheduler's ready queue */
    extern void scheduler_add_process(int pid, uint32_t entry_point, uint32_t user_stack, uint32_t kernel_stack);
    scheduler_add_process(pcb->pid, pcb->entry_point, stack_top, kernel_stack_top);
    
    /* RETURN to caller - process will be started by scheduler */
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


