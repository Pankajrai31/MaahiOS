/**
 * Text Mode VGA Syscalls
 * Handles basic text output operations
 */

#include "../syscall_common.h"

/**
 * Handle text-mode VGA syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_text_vga(unsigned int syscall_num,
                                      unsigned int arg1,
                                      unsigned int arg2,
                                      unsigned int arg3) {
    switch(syscall_num) {
        case SYSCALL_PUTCHAR:
            // arg1 = character to print
            vga_putchar((char)arg1);
            break;
            
        case SYSCALL_PUTS:
            // arg1 = pointer to string
            {
                const char *str = (const char*)arg1;
                if (!str) {
                    vga_putchar('N');
                    vga_putchar('U');
                    vga_putchar('L');
                    vga_putchar('L');
                } else {
                    while (*str) {
                        vga_putchar(*str);
                        str++;
                    }
                }
            }
            break;
            
        case SYSCALL_PUTINT:
            // arg1 = integer to print
            vga_putint((int)arg1);
            break;
        
        case SYSCALL_CLEAR:
            // Clear screen
            vga_clear();
            break;
            
        case SYSCALL_SET_COLOR:
            // arg1 = foreground color, arg2 = background color
            vga_set_color((unsigned char)arg1, (unsigned char)arg2);
            break;
            
        case SYSCALL_SET_CURSOR:
            // arg1 = x, arg2 = y
            vga_set_cursor((int)arg1, (int)arg2);
            break;
            
        case SYSCALL_DRAW_RECT:
            // arg1 = x, arg2 = y, arg3 = packed (width, height, color)
            {
                int x = (int)arg1;
                int y = (int)arg2;
                int width = arg3 & 0xFF;
                int height = (arg3 >> 8) & 0xFF;
                unsigned char color = (arg3 >> 16) & 0xFF;
                vga_draw_rect(x, y, width, height, color);
            }
            break;
        
        case SYSCALL_PRINT_AT:
            // arg1 = x, arg2 = y, arg3 = string pointer
            {
                int x = (int)arg1;
                int y = (int)arg2;
                const char *str = (const char*)arg3;
                uint32_t fg = 0xFFFFFF;  // Pure white
                uint32_t bg = 0x000000;  // Black
                gfx_draw_string(x, y, str, fg, bg);
            }
            break;
            
        case SYSCALL_DRAW_BOX:
            // arg1 = x, arg2 = y, arg3 = packed (width, height)
            {
                int x = (int)arg1;
                int y = (int)arg2;
                int width = arg3 & 0xFFFF;
                int height = (arg3 >> 16) & 0xFFFF;
                vga_draw_box(x, y, width, height);
            }
            break;
            
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return 0;
}
