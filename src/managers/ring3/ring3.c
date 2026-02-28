/**
 * MaahiOS Ring 3 Manager
 * 
 * Provides the Ring 0 → Ring 3 transition via IRET.
 * Used for the initial switch to user mode.
 * 
 * NOTE: Once the scheduler is running, context switching happens
 * via PIT timer interrupt, not through these functions.
 */

#include "../klog/klog.h"

/**
 * Switch to Ring 3 with specified entry point and stack.
 * Builds an IRET frame and executes IRET to jump to user mode.
 * NEVER RETURNS.
 */
void ring3_switch_with_stack(unsigned int entry_point, unsigned int stack_top) __attribute__((noreturn));

void ring3_switch_with_stack(unsigned int entry_point, unsigned int stack_top) {
    KLOG_INFO("RING3", "Switching to Ring 3: EP=0x%x STK=0x%x", entry_point, stack_top);
    
    __asm__ __volatile__(
        /* Push IRET frame: SS, ESP, EFLAGS, CS, EIP */
        "pushl $0x23\n\t"          /* User data segment (Ring 3) */
        "pushl %0\n\t"              /* User stack pointer */
        "pushf\n\t"                 /* Current EFLAGS */
        "popl %%eax\n\t"
        "orl $0x00003200, %%eax\n\t"  /* Set IF (0x200) and IOPL=3 (0x3000) */
        "pushl %%eax\n\t"           /* Push modified EFLAGS */
        "pushl $0x1B\n\t"           /* User code segment (Ring 3) */
        "pushl %1\n\t"              /* Entry point */
        
        /* Jump to Ring 3 via IRET - NEVER RETURNS */
        "iret\n\t"
        :
        : "r"(stack_top), "r"(entry_point)
        : "eax", "memory"
    );
    
    /* Should NEVER reach here */
    while(1) {
        __asm__ volatile("hlt");
    }
}
