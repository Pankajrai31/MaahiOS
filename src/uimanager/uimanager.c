/**
 * UIManager - Centralized UI Control Registry and Event Manager
 * Maintains table of all UI controls across all applications
 * Handles hit testing and hover detection
 */

#include "../syscalls/user_syscalls.h"

#define MAX_CONTROLS 64

// Control types
#define CONTROL_TYPE_BUTTON 1

// Control states
#define CONTROL_STATE_NORMAL 0
#define CONTROL_STATE_HOVER  1
#define CONTROL_STATE_PRESSED 2

// UI Control structure
typedef struct {
    int active;           // Is this slot in use?
    int owner_pid;        // Which process owns this control
    int type;             // BUTTON, WINDOW, etc.
    int x, y, w, h;       // Position and size
    int state;            // NORMAL, HOVER, PRESSED
    char label[64];       // Button text/label
} UIControl;

// Global control registry
static UIControl controls[MAX_CONTROLS];
static int control_count = 0;

/**
 * Register a button control
 * Returns: Control ID (index) or -1 on failure
 */
int ui_register_button(int owner_pid, int x, int y, int width, int height, const char *label) {
    if (control_count >= MAX_CONTROLS) {
        return -1;
    }
    
    // Find free slot
    int id = -1;
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (!controls[i].active) {
            id = i;
            break;
        }
    }
    
    if (id == -1) return -1;
    
    // Register control
    controls[id].active = 1;
    controls[id].owner_pid = owner_pid;
    controls[id].type = CONTROL_TYPE_BUTTON;
    controls[id].x = x;
    controls[id].y = y;
    controls[id].w = width;
    controls[id].h = height;
    controls[id].state = CONTROL_STATE_NORMAL;
    
    // Copy label
    int i = 0;
    while (label[i] && i < 63) {
        controls[id].label[i] = label[i];
        i++;
    }
    controls[id].label[i] = '\0';
    
    control_count++;
    
    // Note: Button drawing will be done by the calling process
    // UIManager only tracks coordinates for hover detection
    
    return id;
}

/**
 * Check if point (x, y) is inside a control
 */
int is_point_inside(int px, int py, UIControl *ctrl) {
    return (px >= ctrl->x && px < ctrl->x + ctrl->w &&
            py >= ctrl->y && py < ctrl->y + ctrl->h);
}

/**
 * Update hover states based on mouse position
 */
void ui_update_hover(int mouse_x, int mouse_y) {
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (!controls[i].active) continue;
        
        int is_hovering = is_point_inside(mouse_x, mouse_y, &controls[i]);
        int old_state = controls[i].state;
        int new_state = is_hovering ? CONTROL_STATE_HOVER : CONTROL_STATE_NORMAL;
        
        // Only update state if it changed
        // (Drawing will be handled by sending events to owner process in future)
        if (old_state != new_state) {
            controls[i].state = new_state;
            // TODO: Send hover event to owner process
        }
    }
}

/**
 * UIManager main entry point
 */
void uimanager_main_c() {
    syscall_puts("[UIMANAGER] Started!\n");
    
    // Initialize all controls to inactive
    for (int i = 0; i < MAX_CONTROLS; i++) {
        controls[i].active = 0;
    }
    
    // Main event loop - yield to let other processes run
    int last_mouse_x = -1;
    int last_mouse_y = -1;
    
    while (1) {
        // Yield CPU to scheduler - let other processes run
        __asm__ volatile("hlt");
        
        // Get current mouse position
        int mx = syscall_mouse_get_x();
        int my = syscall_mouse_get_y();
        
        // Only check hover if mouse moved
        if (mx != last_mouse_x || my != last_mouse_y) {
            ui_update_hover(mx, my);
            last_mouse_x = mx;
            last_mouse_y = my;
        }
    }
}
