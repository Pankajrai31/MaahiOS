/**
 * Advanced Control Framework Syscalls
 * Handles panel, table, textbox, and advanced control operations
 */

#include "../syscall_common.h"

/**
 * Handle control framework syscalls
 * Returns: syscall return value, or -1 if syscall not handled
 */
unsigned int syscall_handle_control_framework(unsigned int syscall_num,
                                                unsigned int arg1,
                                                unsigned int arg2,
                                                unsigned int arg3,
                                                unsigned int arg4_esi,
                                                unsigned int arg5) {
    unsigned int return_value = 0;
    
    switch(syscall_num) {
        case SYSCALL_CONTROL_CREATE: {
            // arg1 = window_id, arg2 = type
            int window_id = (int)arg1;
            uint8_t type = (uint8_t)arg2;
            return_value = control_create(window_id, type);
            break;
        }
        
        case SYSCALL_CONTROL_SET_POSITION: {
            // arg1 = control_id, arg2 = x, arg3 = y
            int control_id = (int)arg1;
            int x = (int)arg2;
            int y = (int)arg3;
            return_value = control_set_position(control_id, x, y);
            break;
        }
        
        case SYSCALL_CONTROL_SET_SIZE: {
            // arg1 = control_id, arg2 = width, arg3 = height
            int control_id = (int)arg1;
            int width = (int)arg2;
            int height = (int)arg3;
            return_value = control_set_size(control_id, width, height);
            break;
        }
        
        case SYSCALL_CONTROL_SET_TEXT: {
            // arg1 = control_id, arg2 = text pointer
            int control_id = (int)arg1;
            const char *text = (const char*)arg2;
            return_value = uiman_update_control_text_kernel(control_id, text);
            break;
        }
        
        case SYSCALL_CONTROL_SET_PARENT: {
            // arg1 = control_id, arg2 = parent_id
            int control_id = (int)arg1;
            int parent_id = (int)arg2;
            return_value = control_set_parent(control_id, parent_id);
            break;
        }
        
        case SYSCALL_CONTROL_SET_COLORS: {
            // arg1 = control_id, arg2 = bg, arg3 = fg, arg4_esi = border
            int control_id = (int)arg1;
            uint32_t bg = (uint32_t)arg2;
            uint32_t fg = (uint32_t)arg3;
            uint32_t border = (uint32_t)arg4_esi;
            return_value = control_set_colors(control_id, bg, fg, border);
            break;
        }
        
        case SYSCALL_CONTROL_SET_MARGINS: {
            // arg1 = control_id, arg2 = left, arg3 = top, arg4_esi = right, arg5 = bottom
            int control_id = (int)arg1;
            int left = (int)arg2;
            int top = (int)arg3;
            int right = (int)arg4_esi;
            int bottom = (int)arg5;
            return_value = control_set_margins(control_id, left, top, right, bottom);
            break;
        }
        
        case SYSCALL_CONTROL_RENDER: {
            // arg1 = control_id
            int control_id = (int)arg1;
            control_render(control_id);
            return_value = 0;
            break;
        }
        
        case SYSCALL_PANEL_ADD_CHILD: {
            // arg1 = panel_id, arg2 = child_id
            int panel_id = (int)arg1;
            int child_id = (int)arg2;
            return_value = panel_add_child(panel_id, child_id);
            break;
        }
        
        case SYSCALL_PANEL_SET_SCROLLABLE: {
            // arg1 = panel_id, arg2 = scrollable
            int panel_id = (int)arg1;
            uint8_t scrollable = (uint8_t)arg2;
            return_value = panel_set_scrollable(panel_id, scrollable);
            break;
        }
        
        case SYSCALL_TABLE_SET_DIMENSIONS: {
            // arg1 = table_id, arg2 = rows, arg3 = cols
            int table_id = (int)arg1;
            int rows = (int)arg2;
            int cols = (int)arg3;
            return_value = table_set_dimensions(table_id, rows, cols);
            break;
        }
        
        case SYSCALL_TABLE_SET_COLUMN_WIDTH: {
            // arg1 = table_id, arg2 = col, arg3 = width
            int table_id = (int)arg1;
            int col = (int)arg2;
            int width = (int)arg3;
            return_value = table_set_column_width(table_id, col, width);
            break;
        }
        
        case SYSCALL_TEXTBOX_SET_CONTENT: {
            // arg1 = textbox_id, arg5 = content pointer
            int textbox_id = (int)arg1;
            const char *content = (const char*)arg5;
            return_value = textbox_set_content(textbox_id, content);
            break;
        }
        
        case SYSCALL_TEXTBOX_GET_CONTENT: {
            // arg1 = textbox_id
            int textbox_id = (int)arg1;
            return_value = (int)textbox_get_content(textbox_id);
            break;
        }
        
        default:
            return (unsigned int)-1;  // Not handled
    }
    
    return return_value;
}
