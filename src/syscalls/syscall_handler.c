#include <stdint.h>
#include "syscall_numbers.h"
#include "../managers/scheduler/scheduler.h"

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

/* Global graphics state - kernel manages colors for user programs */
uint32_t current_fg_color = 0xFFFFFFFF;  // White by default
uint32_t current_bg_color = 0x00000000;  // Black by default

/* Serial debug functions */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void serial_print(const char *str) {
    while (*str) {
        while ((inb(0x3FD) & 0x20) == 0);
        outb(0x3F8, *str++);
    }
}

static void serial_hex(unsigned char value) {
    char hex[] = "0123456789ABCDEF";
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[(value >> 4) & 0xF]);
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[value & 0xF]);
}

/* Forward declare process function */
extern int current_process_id(void);

/* Forward declare VGA functions from vga.c */
extern void vga_putchar(char c);
extern void vga_putint(int num);
extern void vga_clear();
extern void vga_set_color(unsigned char fg, unsigned char bg);
extern void vga_draw_rect(int x, int y, int width, int height, unsigned char color);
extern void vga_print_at(int x, int y, const char *s);
extern void vga_set_cursor(int x, int y);
extern void vga_draw_box(int x, int y, int width, int height);

/* Forward declare graphics functions from graphics.c */
extern void graphics_mode_13h(void);
extern void put_pixel(int x, int y, unsigned char color);
extern void clear_screen(unsigned char color);

/* Forward declare BGA graphics functions from bga.c */
extern void bga_clear(uint32_t color);
extern void bga_putpixel(int x, int y, uint32_t color);
extern void bga_fill_rect(int x, int y, int width, int height, uint32_t color);
extern void bga_draw_rect(int x, int y, int width, int height, uint32_t color);
extern void bga_print(const char *str, uint32_t fg, uint32_t bg);
extern void bga_print_at(int x, int y, const char *str, uint32_t fg, uint32_t bg);
extern void bga_set_cursor(int x, int y);
extern void bga_get_cursor(int *x, int *y);
extern uint16_t bga_get_width(void);
extern uint16_t bga_get_height(void);

/* Forward declare mouse driver functions from mouse.c */
extern int mouse_get_x(void);
extern int mouse_get_y(void);
extern uint8_t mouse_get_buttons(void);

/* Forward declare VMM functions (which wrap PMM) */
extern void *vmm_alloc_page();
extern void vmm_free_page(void *addr);

/* Forward declare ISO9660 functions */
extern int iso9660_list_files(void);

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
                                unsigned int arg3,
                                unsigned int arg4_esi,
                                unsigned int user_esp) {
    // Unused parameters
    (void)arg2;
    (void)arg3;
    (void)arg4_esi;
    
    // CRITICAL: Re-enable interrupts during syscall handling
    // INT 0x80 clears IF, but we need timer/mouse IRQs to work
    __asm__ volatile("sti");
    
    unsigned int return_value = 0;
    
    // Dispatch based on syscall number
    switch(syscall_num) {
        case SYSCALL_PUTCHAR:
            // arg1 = character to print
            kernel_putchar((char)arg1);
            break;
            
        case SYSCALL_PUTS:
            // arg1 = pointer to string
            serial_print("[SYSCALL_PUTS] str=");
            if (arg1 != 0) {
                serial_print((const char*)arg1);
            } else {
                serial_print("(null)\n");
            }
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
            
        case SYSCALL_PRINT_AT:
            // arg1 = x, arg2 = y, arg3 = string pointer, stack = fg, bg colors
            {
                int x = (int)arg1;
                int y = (int)arg2;
                const char *str = (const char*)arg3;
                // TEMPORARY: Force white color to verify it works
                uint32_t fg = 0xFFFFFF;  // Pure white
                uint32_t bg = 0x000000;  // Black (ignored by bga_putchar)
                bga_print_at(x, y, str, fg, bg);
            }
            break;
            
        case SYSCALL_SET_CURSOR:
            // arg1 = x, arg2 = y
            vga_set_cursor((int)arg1, (int)arg2);
            break;
            
        case SYSCALL_DRAW_BOX:
            // arg1 = x, arg2 = y, arg3 = width, EDX = height (packed)
            {
                int x = (int)arg1;
                int y = (int)arg2;
                int width = arg3 & 0xFFFF;
                int height = (arg3 >> 16) & 0xFFFF;
                vga_draw_box(x, y, width, height);
            }
            break;
            
        case SYSCALL_CREATE_PROCESS: {
            // Create new process
            extern int process_create(uint32_t entry_point);
            uint32_t entry_point = arg1;  // Entry point passed in arg1
            return_value = process_create(entry_point);
            break;
        }
        
        case SYSCALL_GET_ORBIT_ADDR:
            // Return orbit module address
            extern uint32_t orbit_module_address;
            return_value = orbit_module_address;
            break;
            
        case SYSCALL_GFX_PUTC:
            // arg1 = character
            {
                char str[2] = {(char)arg1, '\0'};
                extern uint32_t current_fg_color;
                extern uint32_t current_bg_color;
                bga_print(str, current_fg_color, current_bg_color);
            }
            break;
            
        case SYSCALL_GFX_PUTS:
            // arg1 = string pointer
            {
                extern uint32_t current_fg_color;
                extern uint32_t current_bg_color;
                bga_print((const char *)arg1, current_fg_color, current_bg_color);
            }
            break;
            
        case SYSCALL_GFX_CLEAR:
            // Clear to current background color
            bga_clear(current_bg_color);
            bga_set_cursor(0, 0);  // Reset cursor to top-left
            break;
            
        case SYSCALL_GFX_SET_COLOR:
            // arg1 = fg color, arg2 = bg color
            {
                extern uint32_t current_fg_color;
                extern uint32_t current_bg_color;
                current_fg_color = arg1;
                current_bg_color = arg2;
            }
            break;
            
        case SYSCALL_GFX_FILL_RECT:
            // SIMPLIFIED: arg1=x, arg2=y, arg3=packed(w/h), arg4_esi=color
            {
                int x = (int)arg1;
                int y = (int)arg2;
                unsigned int packed = arg3;
                uint32_t color = arg4_esi;
                
                int width = (int)(packed & 0xFFFF);
                int height = (int)(packed >> 16);
                
                bga_fill_rect(x, y, width, height, color);
            }
            break;
            
        case SYSCALL_GFX_DRAW_RECT:
            // arg1 = x, arg2 = y, arg3 = width, stack = height, color
            {
                int x = (int)arg1;
                int y = (int)arg2;
                int width = (int)arg3;
                
                uint32_t *stack_ptr = (uint32_t *)user_esp;
                int height = (int)stack_ptr[0];
                uint32_t color = stack_ptr[1];
                
                bga_draw_rect(x, y, width, height, color);
            }
            break;
            
        case SYSCALL_GFX_PRINT_AT:
            // arg1 = x, arg2 = y, arg3 = str, stack = fg, bg
            {
                int x = (int)arg1;
                int y = (int)arg2;
                const char *str = (const char *)arg3;
                
                // FORCE WHITE COLOR
                uint32_t fg = 0xFFFFFF;  // Pure white
                uint32_t bg = 0x000000;
                
                bga_print_at(x, y, str, fg, bg);
            }
            break;
            
        case SYSCALL_GFX_CLEAR_COLOR:
            // arg1 = RGB color
            bga_clear((uint32_t)arg1);
            bga_set_cursor(0, 0);
            break;
            
        case SYSCALL_GFX_DRAW_BMP:
            // arg1 = x, arg2 = y, arg3 = BMP data address
            extern void bga_draw_bmp(int x, int y, const uint8_t *bmp_data);
            bga_draw_bmp((int)arg1, (int)arg2, (const uint8_t *)arg3);
            break;
            
        case SYSCALL_MOUSE_GET_X:
            // Return current mouse X position
            return_value = (unsigned int)mouse_get_x();
            serial_print("[SYSCALL_X=");
            serial_hex((return_value >> 8) & 0xFF);
            serial_hex(return_value & 0xFF);
            serial_print("]\n");
            break;
            
        case SYSCALL_MOUSE_GET_Y:
            // Return current mouse Y position
            return_value = (unsigned int)mouse_get_y();
            serial_print("[SYSCALL_Y=");
            serial_hex((return_value >> 8) & 0xFF);
            serial_hex(return_value & 0xFF);
            serial_print("]\n");
            break;
            
        case SYSCALL_MOUSE_GET_BUTTONS:
            // Return button state bitmap
            return_value = (unsigned int)mouse_get_buttons();
            break;
            
        case SYSCALL_YIELD:
            // Yield CPU - trigger scheduler
            scheduler_tick();  // Trigger context switch
            break;
            
        case SYSCALL_MOUSE_GET_IRQ_TOTAL:
            // Return total IRQ12 count for debugging
            extern int mouse_get_irq_total(void);
            return_value = (unsigned int)mouse_get_irq_total();
            break;
            
        case SYSCALL_GET_PIC_MASK:
            // Return PIC mask register status
            extern unsigned int irq_get_pic_mask(void);
            return_value = irq_get_pic_mask();
            break;
            
        case SYSCALL_RE_ENABLE_MOUSE:
            // Re-enable IRQ12 and drain PS/2 buffer
            extern void irq_enable_mouse(void);
            extern void mouse_drain_buffer(void);
            mouse_drain_buffer();  // CRITICAL: drain buffer first
            irq_enable_mouse();
            break;
            
        case SYSCALL_POLL_MOUSE: {
            // Manually check 8042 for mouse data and process if available
            // This is a workaround for when IRQ12 stops firing
            extern void mouse_handler(void);
            uint8_t status = inb(0x64);
            uint8_t slave_pic = inb(0xA1);  // Check slave PIC mask
            
            // DEBUG: Log if IRQ12 is masked on slave PIC
            static int poll_count = 0;
            if (++poll_count % 100 == 0) {  // Log every 100 polls
                serial_print("[POLL] status=");
                serial_hex(status);
                serial_print(" slave_pic=");
                serial_hex(slave_pic);
                serial_print(" IRQ12_masked=");
                serial_hex((slave_pic & 0x10) ? 1 : 0);
                serial_print("\n");
            }
            
            if ((status & 0x01) && (status & 0x20)) {  // Data available AND it's from mouse (bit 5 set)
                mouse_handler();  // Call handler directly
                return_value = 1;  // Indicate we found data
            } else {
                return_value = 0;  // No data available
            }
            break;
        }
        
        case SYSCALL_READ_PIXEL:
            // arg1 = x, arg2 = y
            // Returns pixel color at (x, y)
            extern uint32_t bga_get_pixel(int x, int y);
            return_value = bga_get_pixel((int)arg1, (int)arg2);
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
