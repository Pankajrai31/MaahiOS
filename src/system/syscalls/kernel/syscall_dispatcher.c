/**
 * Main Syscall Dispatcher
 * Routes syscalls to appropriate domain handlers
 * 
 * Called from: src/managers/interrupt/interrupt_stubs.s (syscall_int stub)
 * Receives: eax=syscall_number, ebx=arg1, ecx=arg2, edx=arg3
 */

#include "syscall_common.h"
#include "../../../managers/scheduler/scheduler.h"

/* Global graphics state - kernel manages colors for user programs */
uint32_t current_fg_color = 0xFFFFFFFF;  // White by default
uint32_t current_bg_color = 0x00000000;  // Black by default

/* Domain-specific syscall handlers */
extern unsigned int syscall_handle_process(unsigned int syscall_num, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4_esi, unsigned int arg5, unsigned int arg6);
extern unsigned int syscall_handle_memory(unsigned int syscall_num, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4_esi, unsigned int arg5, unsigned int arg6);
extern unsigned int syscall_handle_ui(unsigned int syscall_num, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4_esi, unsigned int arg5, unsigned int arg6);
extern unsigned int syscall_handle_io(unsigned int syscall_num, unsigned int arg1, unsigned int arg2, unsigned int arg3, unsigned int arg4_esi, unsigned int arg5, unsigned int arg6);

/**
 * Kernel-side: exit implementation
 * Halts execution
 */
static void kernel_exit(int code) {
    (void)code;  // Unused parameter
    
    // Halt the CPU
    asm volatile("hlt");
    
    // Infinite loop (should never reach here)
    while(1) {
        asm volatile("hlt");
    }
}

/**
 * Main syscall dispatcher
 * 
 * Called from assembly stub with:
 *   EAX = syscall number
 *   EBX = argument 1
 *   ECX = argument 2
 *   EDX = argument 3
 *   ESI = argument 4
 *   User ESP = for stack arguments
 * 
 * This function is called after ALL general purpose registers
 * have been saved by the assembly stub
 * 
 * Return value in EAX will be passed back to userspace
 */
unsigned int syscall_dispatcher(unsigned int syscall_num,
                                unsigned int arg1,
                                unsigned int arg2,
                                unsigned int arg3,
                                unsigned int arg4_esi,
                                unsigned int user_esp) {
    // Get additional arguments from user stack if needed
    unsigned int *user_stack = (unsigned int*)user_esp;
    unsigned int arg5 = (user_esp > 0) ? user_stack[0] : 0;  // First arg on stack
    unsigned int arg6 = (user_esp > 0) ? user_stack[1] : 0;  // Second arg on stack
    
    // CRITICAL: Re-enable interrupts during syscall handling
    // INT 0x80 clears IF, but we need timer/mouse IRQs to work
    __asm__ volatile("sti");
    
    unsigned int return_value = 0;
    
    // Handle EXIT specially (doesn't return)
    if (syscall_num == SYSCALL_EXIT) {
        kernel_exit((int)arg1);
        return 0;  // Never reached
    }
    
    // Try process domain
    return_value = syscall_handle_process(syscall_num, arg1, arg2, arg3, arg4_esi, arg5, arg6);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try memory domain
    return_value = syscall_handle_memory(syscall_num, arg1, arg2, arg3, arg4_esi, arg5, arg6);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try UI domain
    return_value = syscall_handle_ui(syscall_num, arg1, arg2, arg3, arg4_esi, arg5, arg6);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try I/O domain
    return_value = syscall_handle_io(syscall_num, arg1, arg2, arg3, arg4_esi, arg5, arg6);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Unknown syscall
    vga_putchar('U');
    vga_putchar('n');
    vga_putchar('k');
    vga_putchar('n');
    vga_putchar('o');
    vga_putchar('w');
    vga_putchar('n');
    vga_putchar(' ');
    vga_putchar('s');
    vga_putchar('y');
    vga_putchar('s');
    vga_putchar('c');
    vga_putchar('a');
    vga_putchar('l');
    vga_putchar('l');
    vga_putchar(':');
    vga_putchar(' ');
    vga_putint(syscall_num);
    vga_putchar('\n');
    
    return 0;
}
