/**
 * UIMan Library Implementation
 * Shared by all processes
 */

#include "uiman.h"
#include "uiman_internal.h"
#include "../syscalls/user_syscalls.h"

// Global registries (shared across all processes in Ring 3)
UIWindow g_windows[MAX_WINDOWS];
UIControl g_controls[MAX_CONTROLS];
EventQueue g_event_queues[MAX_PROCESSES];
volatile int g_next_window_id = 1;
volatile int g_next_control_id = 1;

/**
 * Initialize UIMan library
 */
void uiman_init(void) {
    static int initialized = 0;
    if (initialized) return;
    
    // Initialize windows
    for (int i = 0; i < MAX_WINDOWS; i++) {
        g_windows[i].active = 0;
    }
    
    // Initialize controls
    for (int i = 0; i < MAX_CONTROLS; i++) {
        g_controls[i].active = 0;
    }
    
    // Initialize event queues
    for (int i = 0; i < MAX_PROCESSES; i++) {
        g_event_queues[i].head = 0;
        g_event_queues[i].tail = 0;
        g_event_queues[i].count = 0;
    }
    
    initialized = 1;
}

/**
 * Find free window slot
 */
int uiman_find_free_window(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_windows[i].active) {
            return i;
        }
    }
    return -1;
}

/**
 * Find free control slot
 */
int uiman_find_free_control(void) {
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (!g_controls[i].active) {
            return i;
        }
    }
    return -1;
}

/**
 * Create a window
 */
int uiman_create_window(int x, int y, int w, int h, const char *title, int parent) {
    int slot = uiman_find_free_window();
    if (slot < 0) return -1;
    
    int window_id = g_next_window_id++;
    
    g_windows[slot].active = 1;
    g_windows[slot].id = window_id;
    g_windows[slot].parent_id = parent;
    g_windows[slot].owner_pid = syscall_get_current_pid();
    g_windows[slot].x = x;
    g_windows[slot].y = y;
    g_windows[slot].width = w;
    g_windows[slot].height = h;
    g_windows[slot].z_order = 0;
    g_windows[slot].visible = 1;
    g_windows[slot].focused = 0;
    
    // Copy title
    int i = 0;
    while (title && title[i] && i < 63) {
        g_windows[slot].title[i] = title[i];
        i++;
    }
    g_windows[slot].title[i] = '\0';
    
    return window_id;
}

/**
 * Create a button control
 */
int uiman_create_button(int window_id, int x, int y, int w, int h, const char *text) {
    int slot = uiman_find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_controls[slot].active = 1;
    g_controls[slot].id = control_id;
    g_controls[slot].window_id = window_id;
    g_controls[slot].owner_pid = syscall_get_current_pid();
    g_controls[slot].type = UIMAN_CONTROL_BUTTON;
    g_controls[slot].x = x;
    g_controls[slot].y = y;
    g_controls[slot].width = w;
    g_controls[slot].height = h;
    g_controls[slot].state = UIMAN_STATE_NORMAL;
    
    // Copy text
    int i = 0;
    while (text && text[i] && i < 127) {
        g_controls[slot].text[i] = text[i];
        i++;
    }
    g_controls[slot].text[i] = '\0';
    
    return control_id;
}

/**
 * Create a label control
 */
int uiman_create_label(int window_id, int x, int y, const char *text) {
    int slot = uiman_find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_controls[slot].active = 1;
    g_controls[slot].id = control_id;
    g_controls[slot].window_id = window_id;
    g_controls[slot].owner_pid = syscall_get_current_pid();
    g_controls[slot].type = UIMAN_CONTROL_LABEL;
    g_controls[slot].x = x;
    g_controls[slot].y = y;
    g_controls[slot].width = 0;  // Auto-size based on text
    g_controls[slot].height = 16;
    g_controls[slot].state = UIMAN_STATE_NORMAL;
    
    // Copy text
    int i = 0;
    while (text && text[i] && i < 127) {
        g_controls[slot].text[i] = text[i];
        i++;
    }
    g_controls[slot].text[i] = '\0';
    
    return control_id;
}

/**
 * Create a textbox control
 */
int uiman_create_textbox(int window_id, int x, int y, int w, int h) {
    int slot = uiman_find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_controls[slot].active = 1;
    g_controls[slot].id = control_id;
    g_controls[slot].window_id = window_id;
    g_controls[slot].owner_pid = syscall_get_current_pid();
    g_controls[slot].type = UIMAN_CONTROL_TEXTBOX;
    g_controls[slot].x = x;
    g_controls[slot].y = y;
    g_controls[slot].width = w;
    g_controls[slot].height = h;
    g_controls[slot].state = UIMAN_STATE_NORMAL;
    g_controls[slot].text[0] = '\0';
    
    return control_id;
}

/**
 * Create a table control
 */
int uiman_create_table(int window_id, int x, int y, int w, int h, int rows, int cols) {
    int slot = uiman_find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_controls[slot].active = 1;
    g_controls[slot].id = control_id;
    g_controls[slot].window_id = window_id;
    g_controls[slot].owner_pid = syscall_get_current_pid();
    g_controls[slot].type = UIMAN_CONTROL_TABLE;
    g_controls[slot].x = x;
    g_controls[slot].y = y;
    g_controls[slot].width = w;
    g_controls[slot].height = h;
    g_controls[slot].state = UIMAN_STATE_NORMAL;
    g_controls[slot].data.table.rows = rows;
    g_controls[slot].data.table.cols = cols;
    
    return control_id;
}

/**
 * Create a radio button control
 */
int uiman_create_radio(int window_id, int x, int y, const char *text, int group) {
    int slot = uiman_find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_controls[slot].active = 1;
    g_controls[slot].id = control_id;
    g_controls[slot].window_id = window_id;
    g_controls[slot].owner_pid = syscall_get_current_pid();
    g_controls[slot].type = UIMAN_CONTROL_RADIO;
    g_controls[slot].x = x;
    g_controls[slot].y = y;
    g_controls[slot].width = 20;
    g_controls[slot].height = 20;
    g_controls[slot].state = UIMAN_STATE_NORMAL;
    g_controls[slot].data.radio.group = group;
    
    // Copy text
    int i = 0;
    while (text && text[i] && i < 127) {
        g_controls[slot].text[i] = text[i];
        i++;
    }
    g_controls[slot].text[i] = '\0';
    
    return control_id;
}

/**
 * Get event from queue (blocking)
 */
int uiman_get_event(uiman_event_t *event) {
    int my_pid = syscall_get_current_pid();
    EventQueue *q = &g_event_queues[my_pid];
    
    // Wait until event available (preemptive multitasking handles this)
    while (q->count == 0) {
        // Just spin - timer will switch to UIManager which fills queue
        __asm__ volatile("nop");
    }
    
    // Get event
    *event = q->events[q->head];
    q->head = (q->head + 1) % EVENT_QUEUE_SIZE;
    q->count--;
    
    return 1;
}

/**
 * Poll for event (non-blocking)
 */
int uiman_poll_event(uiman_event_t *event) {
    int my_pid = syscall_get_current_pid();
    EventQueue *q = &g_event_queues[my_pid];
    
    if (q->count == 0) {
        return 0;  // No events
    }
    
    // Get event
    *event = q->events[q->head];
    q->head = (q->head + 1) % EVENT_QUEUE_SIZE;
    q->count--;
    
    return 1;
}

/**
 * Set text of a control
 */
void uiman_set_text(int control_id, const char *text) {
    // Find control by ID
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (g_controls[i].active && g_controls[i].id == control_id) {
            int j = 0;
            while (text && text[j] && j < 127) {
                g_controls[i].text[j] = text[j];
                j++;
            }
            g_controls[i].text[j] = '\0';
            break;
        }
    }
}

/**
 * Set enabled state of a control
 */
void uiman_set_enabled(int control_id, int enabled) {
    // Find control by ID
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (g_controls[i].active && g_controls[i].id == control_id) {
            g_controls[i].state = enabled ? UIMAN_STATE_NORMAL : UIMAN_STATE_DISABLED;
            break;
        }
    }
}

/**
 * Queue event for a process
 */
void uiman_queue_event(int owner_pid, uiman_event_t *event) {
    if (owner_pid < 0 || owner_pid >= MAX_PROCESSES) return;
    
    EventQueue *q = &g_event_queues[owner_pid];
    
    // Don't overflow queue
    if (q->count >= EVENT_QUEUE_SIZE) {
        return;
    }
    
    q->events[q->tail] = *event;
    q->tail = (q->tail + 1) % EVENT_QUEUE_SIZE;
    q->count++;
}

// Stub implementations for functions not used by clients
void uiman_close_window(int window_id) {}
void uiman_show_window(int window_id) {}
void uiman_hide_window(int window_id) {}
void uiman_invalidate(int control_id) {}
