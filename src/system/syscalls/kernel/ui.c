/**
 * UI/Window Management Syscalls - Router
 * Routes UI syscalls to appropriate component handlers
 */

#include "syscall_common.h"
#include "ui/ui_handlers.h"

/**
 * Handle UI-related syscalls by routing to component handlers
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_ui(unsigned int syscall_num,
                                unsigned int arg1,
                                unsigned int arg2,
                                unsigned int arg3,
                                unsigned int arg4_esi,
                                unsigned int arg5,
                                unsigned int arg6) {
    unsigned int return_value;
    
    // Try text VGA handler
    return_value = syscall_handle_text_vga(syscall_num, arg1, arg2, arg3);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try graphics handler
    return_value = syscall_handle_graphics(syscall_num, arg1, arg2, arg3, arg4_esi);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try window handler
    return_value = syscall_handle_window(syscall_num, arg1, arg2, arg3, arg4_esi, arg5, arg6);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try controls handler
    return_value = syscall_handle_controls(syscall_num, arg1, arg2, arg3, arg4_esi, arg5, arg6);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try control framework handler
    return_value = syscall_handle_control_framework(syscall_num, arg1, arg2, arg3, arg4_esi, arg5);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Try mouse/input handler
    return_value = syscall_handle_mouse_input(syscall_num);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    // Try mouse/input handler
    return_value = syscall_handle_mouse_input(syscall_num);
    if (return_value != (unsigned int)-1) {
        return return_value;
    }
    
    // Not handled by any UI component
    return (unsigned int)-1;
}

