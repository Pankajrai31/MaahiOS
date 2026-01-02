#include "pit.h"
#include <stdint.h>
#include "../process/process_manager.h"

/* External scheduler functions */
extern void scheduler_tick(void);

/* PIT frequency: 1.193182 MHz */
#define PIT_FREQUENCY 1193182

/* PIT command/data ports */
#define PIT_CHANNEL0 0x40
#define PIT_COMMAND  0x43

/* Global tick counter */
static volatile unsigned int pit_ticks = 0;

/**
 * PIT IRQ handler - Called from interrupt stub
 * Simple version: just ticks and calls scheduler
 */
void pit_handler(void) {
    pit_ticks++;
    
    /* Call scheduler - for now just checks if scheduling is needed */
    scheduler_tick();
}

/**
 * PIT handler with context switching support
 * Called from irq0_stub with current ESP on stack
 * Returns new ESP (same process or switched process)
 */
uint32_t pit_handler_with_context(uint32_t current_esp) {
    /* ABSOLUTE FIRST THING - inline assembly to avoid any C overhead */
    __asm__ volatile(
        "mov $0x3F8, %%dx\n"
        "mov $'P', %%al\n"
        "out %%al, %%dx\n"
        "mov $'H', %%al\n"
        "out %%al, %%dx\n"
        "mov $'\\n', %%al\n"
        "out %%al, %%dx\n"
        ::: "%eax", "%edx"
    );
    
    pit_ticks++;
    
    /* Debug: Print every 100 ticks to see if timer is working */
    static int debug_ticks = 0;
    if (++debug_ticks >= 100) {
        debug_ticks = 0;
        volatile unsigned char *serial = (volatile unsigned char *)0x3F8;
        serial[0] = '[';
        serial[0] = 'T';
        serial[0] = 'I';
        serial[0] = 'C';
        serial[0] = 'K';
        serial[0] = ']';
        serial[0] = '\n';
    }
    
    extern int scheduler_get_current_pid(void);
    extern int scheduler_should_switch(void);
    extern process_t* process_get_by_pid(int pid);
    extern int scheduler_get_next_pid(void);
    extern void gdt_set_kernel_stack(unsigned int esp0_value);
    
    int current_pid = scheduler_get_current_pid();
    
    /* Call scheduler to check if we should switch */
    scheduler_tick();
    
    /* If current_pid is 0 (kernel idle), check if scheduler wants to start a process */
    if (current_pid == 0) {
        int next_pid = scheduler_get_current_pid();
        if (next_pid > 0) {
            /* Scheduler started a new process, switch to it */
            process_t *next_pcb = process_get_by_pid(next_pid);
            if (next_pcb && next_pcb->esp != 0) {
                gdt_set_kernel_stack(next_pcb->kernel_stack_top);
                return next_pcb->esp;
            }
        }
        return current_esp;
    }
    
    /* Check if scheduler wants to switch processes */
    if (scheduler_should_switch()) {
        __asm__ volatile(
            "pushl %%eax\n"
            "pushl %%edx\n"
            "movl $0x3F8, %%edx\n"
            "movb $'C', %%al\n" "outb %%al, %%dx\n"
            "movb $'S', %%al\n" "outb %%al, %%dx\n"
            "movb $'!', %%al\n" "outb %%al, %%dx\n"
            "movb $'\\n', %%al\n" "outb %%al, %%dx\n"
            "popl %%edx\n"
            "popl %%eax\n"
            ::: "memory"
        );
        
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
                gdt_set_kernel_stack(next_pcb->kernel_stack_top);
                return next_pcb->esp;
            }
        }
    }
    
    return current_esp;
}

/**
 * Initialize PIT to fire at specified frequency (Hz)
 */
void pit_init(unsigned int frequency) {
    /* Calculate divisor for desired frequency */
    unsigned int divisor = PIT_FREQUENCY / frequency;
    
    /* Send command byte: Channel 0, lobyte/hibyte, rate generator */
    outb(PIT_COMMAND, 0x36);
    
    /* Send divisor (low byte, then high byte) */
    outb(PIT_CHANNEL0, divisor & 0xFF);
    outb(PIT_CHANNEL0, (divisor >> 8) & 0xFF);
}

/**
 * Get current tick count
 */
unsigned int pit_get_ticks() {
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
