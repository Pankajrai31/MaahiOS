#include "windows_mgmt.h"

/* Global UI state - single source of truth for all UI elements */
static UIWindow g_kernel_windows[MAX_WINDOWS] = {0};
static UIControl g_kernel_controls[MAX_CONTROLS] = {0};
static EventQueue g_kernel_event_queues[MAX_PROCESSES] = {0};
static volatile int g_next_window_id = 1;
static volatile int g_next_control_id = 1;

/* Helper function - find free window slot */
static int find_free_window(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_kernel_windows[i].active) return i;
    }
    return -1;
}

/* Helper function - find free control slot */
static int find_free_control(void) {
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (!g_kernel_controls[i].active) return i;
    }
    return -1;
}

/* Helper function - safe string copy */
static void strcpy_safe(char *dest, const char *src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Initialize window management system
 */
void windows_mgmt_init(void) {
    // Already zeroed by static initialization, but explicitly clear for clarity
    for (int i = 0; i < MAX_WINDOWS; i++) {
        g_kernel_windows[i].active = 0;
    }
    for (int i = 0; i < MAX_CONTROLS; i++) {
        g_kernel_controls[i].active = 0;
    }
    for (int i = 0; i < MAX_PROCESSES; i++) {
        g_kernel_event_queues[i].head = 0;
        g_kernel_event_queues[i].tail = 0;
        g_kernel_event_queues[i].count = 0;
    }
    g_next_window_id = 1;
    g_next_control_id = 1;
}

/**
 * Create a new window
 */
int uiman_create_window_kernel(int x, int y, int w, int h, const char *title, int parent, int owner_pid) {
    int slot = find_free_window();
    if (slot < 0) return -1;
    
    int window_id = g_next_window_id++;
    
    g_kernel_windows[slot].active = 1;
    g_kernel_windows[slot].id = window_id;
    g_kernel_windows[slot].parent_id = parent;
    g_kernel_windows[slot].owner_pid = owner_pid;
    g_kernel_windows[slot].x = x;
    g_kernel_windows[slot].y = y;
    g_kernel_windows[slot].width = w;
    g_kernel_windows[slot].height = h;
    g_kernel_windows[slot].z_order = 0;
    g_kernel_windows[slot].visible = 1;
    g_kernel_windows[slot].focused = 0;
    g_kernel_windows[slot].state = WINDOW_STATE_NORMAL;
    g_kernel_windows[slot].icon_name[0] = '\0';  // No custom icon by default
    strcpy_safe(g_kernel_windows[slot].title, title, 64);
    
    return window_id;
}

/**
 * Set window icon by name (e.g., "folder_1" for folder_16.bmp)
 */
void uiman_set_window_icon_kernel(int window_id, const char *icon_name) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_kernel_windows[i].active && g_kernel_windows[i].id == window_id) {
            strcpy_safe(g_kernel_windows[i].icon_name, icon_name, 32);
            return;
        }
    }
}

/**
 * Create a button control
 */
int uiman_create_button_kernel(int window_id, int x, int y, int w, int h, const char *text, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_BUTTON;
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = w;
    g_kernel_controls[slot].height = h;
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    g_kernel_controls[slot].button_size = 0;  // Default size
    strcpy_safe(g_kernel_controls[slot].text, text, 128);
    
    // DEBUG: Print to serial log (for kernel debugging)
    extern void serial_print(const char *str);
    serial_print("[WINDOWS_MGMT] Button created: text='");
    serial_print(text);
    serial_print("' stored='");
    serial_print(g_kernel_controls[slot].text);
    serial_print("'\n");
    
    return control_id;
}

/**
 * Create a label control
 */
int uiman_create_label_kernel(int window_id, int x, int y, const char *text, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_LABEL;
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = 0;  // Auto-size
    g_kernel_controls[slot].height = 0;
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    g_kernel_controls[slot].button_size = 0;
    strcpy_safe(g_kernel_controls[slot].text, text, 128);
    
    return control_id;
}

/**
 * Create an icon control
 */
int uiman_create_icon_kernel(int window_id, int x, int y, const char *text, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_ICON;  // Type 6
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = 64;   // Icon width (48px icon + padding)
    g_kernel_controls[slot].height = 80;  // Icon height (48px + text below)
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    g_kernel_controls[slot].button_size = 0;  // Not used for icons
    strcpy_safe(g_kernel_controls[slot].text, text, 128);
    
    return control_id;
}

/**
 * Create a panel control (flat colored rectangle)
 */
int uiman_create_panel_kernel(int window_id, int x, int y, int w, int h, int color_style, const char *text, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_PANEL;  // Type 7
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = w;
    g_kernel_controls[slot].height = h;
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    g_kernel_controls[slot].button_size = color_style;  // 0=blue, 1=green, 2=gray, 3=purple
    strcpy_safe(g_kernel_controls[slot].text, text, 128);
    
    return control_id;
}

/**
 * Create a list control (scrollable list of items)
 */
int uiman_create_list_kernel(int window_id, int x, int y, int w, int h, const char *items, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_LIST;  // Type 8
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = w;
    g_kernel_controls[slot].height = h;
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    g_kernel_controls[slot].button_size = 0;
    strcpy_safe(g_kernel_controls[slot].text, items, 128);  // Items are newline-separated
    
    return control_id;
}

/**
 * Update the text of an existing control
 */
int uiman_update_control_text_kernel(int control_id, const char *text) {
    // Find control by ID in kernel controls array
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (g_kernel_controls[i].active && g_kernel_controls[i].id == control_id) {
            // Found it - update the text
            strcpy_safe(g_kernel_controls[i].text, text, 128);
            return 0;  // Success
        }
    }
    return -1;  // Control not found
}

/**
 * Poll for UI events for a specific process
 */
int uiman_poll_event_kernel(void *event_ptr, int calling_pid) {
    if (calling_pid < 0 || calling_pid >= MAX_PROCESSES) return 0;
    
    EventQueue *queue = &g_kernel_event_queues[calling_pid];
    if (queue->count == 0) return 0;
    
    // Copy event to user space
    uiman_event_t *dest = (uiman_event_t*)event_ptr;
    *dest = queue->events[queue->head];
    
    queue->head = (queue->head + 1) % EVENT_QUEUE_SIZE;
    queue->count--;
    
    return 1;
}

/**
 * Get access to kernel windows array (for UIManager rendering)
 */
UIWindow* uiman_get_kernel_windows(void) {
    return g_kernel_windows;
}

/**
 * Get access to kernel controls array (for UIManager rendering)
 */
UIControl* uiman_get_kernel_controls(void) {
    return g_kernel_controls;
}

/**
 * Get access to kernel event queues (for UIManager event injection)
 */
EventQueue* uiman_get_kernel_event_queues(void) {
    return g_kernel_event_queues;
}

/**
 * Find window by title
 * Returns window_id on success, -1 if not found
 */
int uiman_find_window_by_title(const char *title) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_kernel_windows[i].active) {
            // Compare titles
            const char *a = g_kernel_windows[i].title;
            const char *b = title;
            int match = 1;
            while (*a && *b) {
                if (*a != *b) {
                    match = 0;
                    break;
                }
                a++;
                b++;
            }
            if (match && *a == *b) {  // Both should be at null terminator
                return g_kernel_windows[i].id;
            }
        }
    }
    return -1;
}

/**
 * Get window state
 * Returns: 0=normal, 1=minimized, 3=pending_close, -1=error
 */
int uiman_get_window_state(int window_id) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_kernel_windows[i].active && g_kernel_windows[i].id == window_id) {
            return g_kernel_windows[i].state;
        }
    }
    return -1;  // Window not found
}

/**
 * Restore minimized window
 * Returns: 0 on success, -1 on error
 */
int uiman_restore_window(int window_id) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_kernel_windows[i].active && g_kernel_windows[i].id == window_id) {
            // Restore position and size from saved values
            g_kernel_windows[i].x = g_kernel_windows[i].saved_x;
            g_kernel_windows[i].y = g_kernel_windows[i].saved_y;
            g_kernel_windows[i].width = g_kernel_windows[i].saved_width;
            g_kernel_windows[i].height = g_kernel_windows[i].saved_height;
            g_kernel_windows[i].visible = 1;
            g_kernel_windows[i].state = WINDOW_STATE_NORMAL;
            
            // Restore all child controls
            for (int k = 0; k < MAX_CONTROLS; k++) {
                if (g_kernel_controls[k].window_id == window_id && g_kernel_controls[k].id != 0) {
                    g_kernel_controls[k].active = 1;
                }
            }
            
            return 0;  // Success
        }
    }
    return -1;  // Window not found
}

/**
 * Focus window (bring to front)
 * Returns: 0 on success, -1 on error
 */
int uiman_focus_window(int window_id) {
    int found = -1;
    
    // Find the window
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_kernel_windows[i].active && g_kernel_windows[i].id == window_id) {
            found = i;
            break;
        }
    }
    
    if (found < 0) return -1;
    
    // Unfocus all windows
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_kernel_windows[i].active) {
            g_kernel_windows[i].focused = 0;
        }
    }
    
    // Focus this window
    g_kernel_windows[found].focused = 1;
    
    return 0;  // Success
}
