/**
 * Mouse and Input Syscalls
 * Handles mouse position, buttons, IRQ debugging, and hardware cursor
 */

#include "../syscall_common.h"

/**
 * Handle mouse/input syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_mouse_input(unsigned int syscall_num) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        case SYSCALL_MOUSE_GET_X:
            // Return current mouse X position (atomic read)
            __asm__ volatile("cli");
            return_value = (unsigned int)mouse_get_x();
            __asm__ volatile("sti");
            break;
            
        case SYSCALL_MOUSE_GET_Y:
            // Return current mouse Y position (atomic read)
            __asm__ volatile("cli");
            return_value = (unsigned int)mouse_get_y();
            __asm__ volatile("sti");
            break;
            
        case SYSCALL_MOUSE_GET_BUTTONS:
            // Return button state bitmap (atomic read)
            __asm__ volatile("cli");
            return_value = (unsigned int)mouse_get_buttons();
            __asm__ volatile("sti");
            break;
        
        case SYSCALL_MOUSE_GET_IRQ_TOTAL:
            // Return total IRQ12 count for debugging (atomic read)
            __asm__ volatile("cli");
            return_value = (unsigned int)mouse_get_irq_total();
            __asm__ volatile("sti");
            break;
            
        case SYSCALL_GET_PIC_MASK:
            // Return PIC mask register status
            return_value = irq_get_pic_mask();
            break;
            
        case SYSCALL_RE_ENABLE_MOUSE:
            // Re-enable IRQ12 and drain PS/2 buffer
            mouse_drain_buffer();  // CRITICAL: drain buffer first
            irq_enable_mouse();
            break;
            
        case SYSCALL_POLL_MOUSE: {
            // Manually check 8042 for mouse data and process if available
            uint8_t status = inb(0x64);
            
            if ((status & 0x01) && (status & 0x20)) {  // Data available AND from mouse
                mouse_handler();  // Call handler directly
                return_value = 1;  // Indicate we found data
            } else {
                return_value = 0;  // No data available
            }
            break;
        }
        
        case SYSCALL_BGA_CURSOR_IS_SUPPORTED: {
            // Check hardware cursor support
            return_value = bga_cursor_is_supported();
            break;
        }
        
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
