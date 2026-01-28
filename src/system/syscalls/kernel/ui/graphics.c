/**
 * Graphics Mode Syscalls
 * Handles BGA/VESA graphics operations
 */

#include "../syscall_common.h"

/**
 * Handle graphics-mode syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_graphics(unsigned int syscall_num,
                                      unsigned int arg1,
                                      unsigned int arg2,
                                      unsigned int arg3,
                                      unsigned int arg4_esi) {
    switch(syscall_num) {
        case SYSCALL_GRAPHICS_MODE:
        case SYSCALL_PUT_PIXEL:
            // Legacy Mode 13h - no longer supported (using BGA instead)
            break;
            
        case SYSCALL_CLEAR_GFX:
            // Clear graphics screen
            gfx_clear((uint32_t)arg1);
            break;
        
        case SYSCALL_GFX_SET_COLOR:
            // arg1 = fg color, arg2 = bg color
            current_fg_color = arg1;
            current_bg_color = arg2;
            break;
            
        case SYSCALL_GFX_FILL_RECT:
            // arg1=x, arg2=y, arg3=packed(w/h), arg4_esi=color
            {
                int x = (int)arg1;
                int y = (int)arg2;
                unsigned int packed = arg3;
                uint32_t color = arg4_esi;
                
                int width = (int)(packed & 0xFFFF);
                int height = (int)(packed >> 16);
                
                gfx_fill_rect(x, y, width, height, color);
            }
            break;
            
        case SYSCALL_GFX_PRINT_AT:
            // arg1 = x, arg2 = y, arg3 = str
            {
                int x = (int)arg1;
                int y = (int)arg2;
                const char *str = (const char *)arg3;
                
                uint32_t fg = 0xFFFFFF;  // Pure white
                uint32_t bg = 0x000000;
                
                gfx_draw_string(x, y, str, fg, bg);
            }
            break;
            
        case SYSCALL_GFX_CLEAR_COLOR:
            // arg1 = RGB color
            gfx_clear((uint32_t)arg1);
            break;
            
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return 0;
}
