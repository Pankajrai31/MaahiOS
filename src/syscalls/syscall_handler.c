#include "syscall_numbers.h"

/**
 * Ring 0 Syscall Handler/Dispatcher
 * 
 * This code runs in Ring 0 (kernel mode) and is called from interrupt stub
 * when INT 0x80 is triggered from Ring 3
 * 
 * Called from: src/managers/interrupt/interrupt_stubs.s (syscall_int stub)
 * Receives: eax=syscall_number, ebx=arg1, ecx=arg2, edx=arg3
 * 
 * Dispatcher pattern:
 * 1. Read syscall number from EAX
 * 2. Switch on syscall number
 * 3. Call appropriate kernel function
 * 4. Return to Ring 3 via IRET
 */

/* Forward declare VGA functions from vga.c */
extern void vga_putchar(char c);
extern void vga_putint(int num);
extern void vga_clear();
extern void vga_set_color(unsigned char fg, unsigned char bg);
extern void vga_draw_rect(int x, int y, int width, int height, unsigned char color);

/* Forward declare graphics functions from graphics.c */
extern void graphics_mode_13h(void);
extern void put_pixel(int x, int y, unsigned char color);
extern void clear_screen(unsigned char color);

/* Forward declare VMM functions (which wrap PMM) */
extern void *vmm_alloc_page();
extern void vmm_free_page(void *addr);

/**
 * Kernel-side: putchar implementation
 * Can directly access VGA buffer (Ring 0 privilege)
 */
static void kernel_putchar(char c) {
    vga_putchar(c);
}

/**
 * Kernel-side: puts implementation
 * Can directly access VGA buffer (Ring 0 privilege)
 */
static void kernel_puts(const char* str) {
    // Safety check - make sure pointer looks valid
    if (!str) {
        vga_putchar('N');
        vga_putchar('U');
        vga_putchar('L');
        vga_putchar('L');
        return;
    }
    
    while (*str) {
        vga_putchar(*str);
        str++;
    }
}

/**
 * Kernel-side: putint implementation
 * Can directly access VGA buffer (Ring 0 privilege)
 */
static void kernel_putint(int num) {
    vga_putint(num);
}

/**
 * Kernel-side: exit implementation
 * Halts execution
 */
static void kernel_exit(int code) {
    // Unused parameter
    (void)code;
    
    // Halt the CPU
    asm volatile("hlt");
    
    // Infinite loop (should never reach here)
    while(1) {
        asm volatile("hlt");
    }
}

/**
 * Kernel-side: allocate page implementation
 * Returns address of allocated 4KB page (now via VMM)
 */
static unsigned int kernel_alloc_page() {
    void *page = vmm_alloc_page();
    return (unsigned int)page;
}

/**
 * Kernel-side: free page implementation
 * Frees a previously allocated page (now via VMM)
 */
static void kernel_free_page(unsigned int addr) {
    vmm_free_page((void*)addr);
}

/**
 * Kernel-side: clear screen implementation
 * Clears the VGA text buffer
 */
static void kernel_clear() {
    vga_clear();
}

/**
 * Main syscall dispatcher
 * 
 * Called from assembly stub with:
 *   EAX = syscall number
 *   EBX = argument 1
 *   ECX = argument 2
 *   EDX = argument 3
 * 
 * This function is called after ALL general purpose registers
 * have been saved by the assembly stub
 * 
 * Return value in EAX will be passed back to userspace
 */
unsigned int syscall_dispatcher(unsigned int syscall_num,
                                unsigned int arg1,
                                unsigned int arg2,
                                unsigned int arg3) {
    // Unused parameters
    (void)arg2;
    (void)arg3;
    
    unsigned int return_value = 0;
    
    // Dispatch based on syscall number
    switch(syscall_num) {
        case SYSCALL_PUTCHAR:
            // arg1 = character to print
            kernel_putchar((char)arg1);
            break;
            
        case SYSCALL_PUTS:
            // arg1 = pointer to string
            kernel_puts((const char*)arg1);
            break;
            
        case SYSCALL_PUTINT:
            // arg1 = integer to print
            kernel_putint((int)arg1);
            break;
            
        case SYSCALL_EXIT:
            // arg1 = exit code
            kernel_exit((int)arg1);
            break;
            
        case SYSCALL_ALLOC_PAGE:
            // Return address of allocated page
            return_value = kernel_alloc_page();
            break;
            
        case SYSCALL_FREE_PAGE:
            // arg1 = address to free
            kernel_free_page(arg1);
            break;
            
        case SYSCALL_CLEAR:
            // Clear screen
            kernel_clear();
            break;
            
        case SYSCALL_SET_COLOR:
            // arg1 = foreground color, arg2 = background color
            vga_set_color((unsigned char)arg1, (unsigned char)arg2);
            break;
            
        case SYSCALL_DRAW_RECT:
            // arg1 = x, arg2 = y, arg3 = width, EDX stack = height, ESI stack = color
            // For now, use arg3 as packed: low 16 bits = width+height, high byte = color
            {
                int x = (int)arg1;
                int y = (int)arg2;
                int width = arg3 & 0xFF;
                int height = (arg3 >> 8) & 0xFF;
                unsigned char color = (arg3 >> 16) & 0xFF;
                vga_draw_rect(x, y, width, height, color);
            }
            break;
            
        case SYSCALL_GRAPHICS_MODE:
            // Switch to 320x200 graphics mode
            graphics_mode_13h();
            break;
            
        case SYSCALL_PUT_PIXEL:
            // arg1 = x, arg2 = y, arg3 = color
            put_pixel((int)arg1, (int)arg2, (unsigned char)arg3);
            break;
            
        case SYSCALL_CLEAR_GFX:
            // arg1 = color
            clear_screen((unsigned char)arg1);
            break;
            
        default:
            // Unknown syscall - print error
            kernel_puts("Unknown syscall: ");
            kernel_putint(syscall_num);
            kernel_puts("\n");
            break;
    }
    
    return return_value;
}
