/**
 * MaahiOS PIT (Programmable Interval Timer)
 * 
 * Drives the system timer at 50Hz for preemptive multitasking.
 * - pit_handler_with_context() is the real IRQ0 handler
 * - Returns new ESP for context switching between processes
 */

#include "pit.h"
#include <stdint.h>
#include "../process/process_manager.h"
#include "../klog/klog.h"

/* External scheduler functions */
extern void scheduler_tick(void);

/* PIT frequency: 1.193182 MHz */
#define PIT_FREQUENCY 1193182

/* PIT command/data ports */
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

/* Global tick counter (64-bit for long uptimes) */
static volatile uint64_t pit_ticks = 0;

/**
 * PIT handler with context switching support.
 * Called from irq0_stub with current ESP on stack.
 * Returns new ESP (same process or switched process).
 */
uint32_t pit_handler_with_context(uint32_t current_esp) {
    pit_ticks++;
    
    extern int scheduler_get_current_pid(void);
    extern int scheduler_should_switch(void);
    extern process_t* process_get_by_pid(int pid);
    extern int scheduler_get_next_pid(void);
    extern void gdt_set_kernel_stack(unsigned int esp0_value);
    extern void scheduler_set_current_pid(int pid);
    
    int current_pid = scheduler_get_current_pid();
    
    /* Call scheduler to check if we should switch */
    scheduler_tick();
    
    /* If kernel idle (PID 0), check if scheduler wants to start a process */
    if (current_pid == 0) {
        if (scheduler_should_switch()) {
            int next_pid = scheduler_get_next_pid();
            if (next_pid > 0) {
                process_t *next_pcb = process_get_by_pid(next_pid);
                if (next_pcb && next_pcb->esp != 0) {
                    /* Switch page directory if process has its own */
                    extern uint32_t *kernel_page_directory;
                    if (next_pcb->page_directory && next_pcb->page_directory != kernel_page_directory) {
                        asm volatile("mov %0, %%cr3" : : "r"(next_pcb->page_directory) : "memory");
                    }
                    gdt_set_kernel_stack(next_pcb->kernel_stack_top);
                    scheduler_set_current_pid(next_pid);
                    return next_pcb->esp;
                }
            }
        }
        return current_esp;
    }
    
    /* Check if scheduler wants to switch processes */
    if (scheduler_should_switch()) {
        int next_pid = scheduler_get_next_pid();
        
        if (next_pid > 0 && next_pid != current_pid) {
            /* Save current process context */
            process_t *current_pcb = process_get_by_pid(current_pid);
            if (current_pcb) {
                current_pcb->esp = current_esp;
            }
            
            /* Load next process context */
            process_t *next_pcb = process_get_by_pid(next_pid);
            if (next_pcb) {
                /* Switch page directory if switching between different address spaces */
                if (current_pcb && next_pcb->page_directory != current_pcb->page_directory) {
                    asm volatile("mov %0, %%cr3" : : "r"(next_pcb->page_directory) : "memory");
                }
                gdt_set_kernel_stack(next_pcb->kernel_stack_top);
                scheduler_set_current_pid(next_pid);
                return next_pcb->esp;
            }
        }
    }
    
    return current_esp;
}

/**
 * Initialize PIT to fire at specified frequency (Hz)
 */
int pit_init(unsigned int frequency) {
    unsigned int divisor = PIT_FREQUENCY / frequency;
    
    /* Channel 0, lobyte/hibyte, rate generator */
    outb(PIT_COMMAND, 0x36);
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
    
    KLOG_INFO("PIT", "Initialized at %u Hz (divisor=%u)", frequency, divisor);
    return 0;
}

/**
 * Get current tick count (32-bit, lower 32 bits of 64-bit counter)
 */
unsigned int pit_get_ticks(void) {
    return (unsigned int)pit_ticks;
}

/**
 * Get current tick count (64-bit, full counter)
 */
uint64_t pit_get_ticks64(void) {
    return pit_ticks;
}

/**
 * Simple delay function (busy-wait)
 */
void pit_wait(unsigned int ticks) {
    unsigned int end_tick = pit_ticks + ticks;
    while (pit_ticks < end_tick) {
        asm volatile("pause");
    }
}
