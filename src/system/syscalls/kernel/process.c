/**
 * Process Management Syscalls
 * Handles process creation, termination, and process information queries
 */

#include "syscall_common.h"

/**
 * Handle process-related syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_process(unsigned int syscall_num,
                                     unsigned int arg1,
                                     unsigned int arg2,
                                     unsigned int arg3,
                                     unsigned int arg4_esi,
                                     unsigned int arg5,
                                     unsigned int arg6) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        case SYSCALL_CREATE_PROCESS: {
            // Create new process
            // arg1 = entry_point
            uint32_t entry_point = arg1;
            return_value = process_create(entry_point);
            break;
        }
        
        case SYSCALL_GET_CURRENT_PID:
            // Returns current process PID
            return_value = (unsigned int)scheduler_get_current_pid();
            break;
        
        case SYSCALL_KILL_PROCESS: {
            // Terminate process
            // arg1 = pid
            int pid = arg1;
            return_value = process_terminate(pid);
            break;
        }
        
        case SYSCALL_YIELD:
            // Yield CPU - just pause briefly and let timer IRQ handle scheduling
            // DO NOT call scheduler_tick() here - it corrupts current_index
            // because the timer will call it again before we actually switch
            __asm__ volatile("pause");  // Brief pause, timer will handle context switch
            break;
        
        case SYSCALL_LAUNCH_FILE_MANAGER: {
            // Launch file_manager.bin
            return_value = launch_file_manager();
            break;
        }
        
        case SYSCALL_LAUNCH_DISK_MANAGER: {
            // Launch disk_manager.bin
            return_value = launch_disk_manager();
            break;
        }
        
        case SYSCALL_GET_ORBIT_ADDR:
            // Return orbit module address
            return_value = orbit_module_address;
            break;
            
        case SYSCALL_GET_UIMANAGER_ADDR:
            // Return UIManager module address
            return_value = uimanager_module_address;
            break;
        
        case SYSCALL_DEBUG_DUMP_RESOURCES: {
            // Dump resource usage to serial
            serial_print("\n========== RESOURCE DUMP ==========\n");
            
            // Count active windows
            void *windows_ptr = uiman_get_kernel_windows();
            int window_count = 0;
            int minimized_count = 0;
            int pending_close_count = 0;
            
            for (int i = 0; i < 32; i++) {  // MAX_WINDOWS
                char *window = (char*)windows_ptr + (i * 200);
                int *window_ints = (int*)window;
                int active = window_ints[0];
                if (active) {
                    int state = window_ints[15];
                    window_count++;
                    if (state == 1) minimized_count++;
                    if (state == 3) pending_close_count++;
                }
            }
            serial_print("Active Windows: ");
            serial_hex(window_count);
            serial_print(" (");
            serial_hex(minimized_count);
            serial_print(" minimized, ");
            serial_hex(pending_close_count);
            serial_print(" pending close)\n");
            
            // Count active controls
            void *controls_ptr = uiman_get_kernel_controls();
            int control_count = 0;
            for (int i = 0; i < 256; i++) {
                int *control = (int*)((char*)controls_ptr + (i * 180));
                if (control[0]) control_count++;
            }
            serial_print("Active Controls: ");
            serial_hex(control_count);
            serial_print("\n");
            
            // Count active processes
            int process_count = process_manager_get_count();
            serial_print("Active Processes: ");
            serial_hex(process_count);
            serial_print("\n");
            
            serial_print("===================================\n\n");
            break;
        }
        
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
