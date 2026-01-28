/**
 * UI Controls Syscalls
 * Handles button, label, icon, list control creation and events
 */

#include "../syscall_common.h"

/**
 * Handle UI controls syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_controls(unsigned int syscall_num,
                                      unsigned int arg1,
                                      unsigned int arg2,
                                      unsigned int arg3,
                                      unsigned int arg4_esi,
                                      unsigned int arg5,
                                      unsigned int arg6) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        case SYSCALL_UI_CREATE_BUTTON:
            // arg1=window_id, arg2=x, arg3=y, arg4_esi=width, arg5=height, arg6=text
            serial_print("[SYSCALL] Button: arg5=");
            serial_hex((unsigned char)((arg5 >> 24) & 0xFF));
            serial_hex((unsigned char)((arg5 >> 16) & 0xFF));
            serial_hex((unsigned char)((arg5 >> 8) & 0xFF));
            serial_hex((unsigned char)(arg5 & 0xFF));
            serial_print(" arg6=");
            serial_hex((unsigned char)((arg6 >> 24) & 0xFF));
            serial_hex((unsigned char)((arg6 >> 16) & 0xFF));
            serial_hex((unsigned char)((arg6 >> 8) & 0xFF));
            serial_hex((unsigned char)(arg6 & 0xFF));
            serial_print("\n");
            
            return_value = (unsigned int)uiman_create_button_kernel(
                (int)arg1, (int)arg2, (int)arg3, (int)arg4_esi, (int)arg5,
                (const char*)arg6, scheduler_get_current_pid());
            break;
        
        case SYSCALL_UI_CREATE_ICON:
            // arg1=window_id, arg2=x, arg3=y, arg4_esi=text
            return_value = (unsigned int)uiman_create_icon_kernel(
                (int)arg1, (int)arg2, (int)arg3, (const char*)arg4_esi, scheduler_get_current_pid());
            break;
        
        case SYSCALL_UI_CREATE_LABEL:
            // arg1=window_id, arg2=x, arg3=y, arg4_esi=text
            serial_print("[SYSCALL] Label: window=");
            serial_hex((unsigned char)(arg1 & 0xFF));
            serial_print(" text_ptr=");
            serial_hex((unsigned char)((arg4_esi >> 24) & 0xFF));
            serial_hex((unsigned char)((arg4_esi >> 16) & 0xFF));
            serial_hex((unsigned char)((arg4_esi >> 8) & 0xFF));
            serial_hex((unsigned char)(arg4_esi & 0xFF));
            serial_print("\n");
            
            return_value = (unsigned int)uiman_create_label_kernel(
                (int)arg1, (int)arg2, (int)arg3, (const char*)arg4_esi, scheduler_get_current_pid());
            serial_print("[SYSCALL] Label created OK\n");
            break;
        
        case SYSCALL_UI_CREATE_LIST: {
            // arg1=window_id, arg2=x, arg3=y, arg4_esi=width, arg5=height, arg6=items
            int window_id = (int)arg1;
            int x = (int)arg2;
            int y = (int)arg3;
            int w = (int)arg4_esi;
            int h = (int)arg5;
            const char *items = (const char*)arg6;
            return_value = (unsigned int)uiman_create_list_kernel(
                window_id, x, y, w, h, items, scheduler_get_current_pid());
            break;
        }
        
        case SYSCALL_UI_POLL_EVENT:
            // arg1=pointer to uiman_event_t structure
            return_value = (unsigned int)uiman_poll_event_kernel((void*)arg1, scheduler_get_current_pid());
            break;
        
        case SYSCALL_UI_GET_WINDOWS_PTR:
            // Returns pointer to kernel windows array
            return_value = (unsigned int)uiman_get_kernel_windows();
            break;
        
        case SYSCALL_UI_GET_CONTROLS_PTR:
            // Returns pointer to kernel controls array
            return_value = (unsigned int)uiman_get_kernel_controls();
            break;
        
        case SYSCALL_UI_GET_EVENTS_PTR:
            // Returns pointer to kernel event queues
            return_value = (unsigned int)uiman_get_kernel_event_queues();
            break;
            
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
