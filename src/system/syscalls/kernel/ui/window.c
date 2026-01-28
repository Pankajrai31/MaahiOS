/**
 * Window Management Syscalls
 * Handles window creation, state, focus, and icon operations
 */

#include "../syscall_common.h"

/**
 * Handle window management syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_window(unsigned int syscall_num,
                                    unsigned int arg1,
                                    unsigned int arg2,
                                    unsigned int arg3,
                                    unsigned int arg4_esi,
                                    unsigned int arg5,
                                    unsigned int arg6) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        case SYSCALL_UI_CREATE_WINDOW:
            // arg1=x, arg2=y, arg3=width, arg4_esi=height, arg5=title, arg6=parent
            return_value = (unsigned int)uiman_create_window_kernel(
                (int)arg1, (int)arg2, (int)arg3, (int)arg4_esi,
                (const char*)arg5, (int)arg6, scheduler_get_current_pid());
            break;
        
        case SYSCALL_FIND_WINDOW_BY_TITLE: {
            // arg1 = title string pointer
            const char *title = (const char*)arg1;
            return_value = uiman_find_window_by_title(title);
            break;
        }
        
        case SYSCALL_GET_WINDOW_STATE: {
            // arg1 = window_id
            int window_id = (int)arg1;
            return_value = uiman_get_window_state(window_id);
            break;
        }
        
        case SYSCALL_RESTORE_WINDOW: {
            // arg1 = window_id
            int window_id = (int)arg1;
            return_value = uiman_restore_window(window_id);
            break;
        }
        
        case SYSCALL_FOCUS_WINDOW: {
            // arg1 = window_id
            int window_id = (int)arg1;
            return_value = uiman_focus_window(window_id);
            break;
        }
        
        case SYSCALL_SET_WINDOW_ICON: {
            // arg1 = window_id, arg2 = icon_name pointer
            int window_id = (int)arg1;
            const char *icon_name = (const char*)arg2;
            uiman_set_window_icon_kernel(window_id, icon_name);
            break;
        }
        
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
