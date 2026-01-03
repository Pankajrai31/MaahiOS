/**
 * UIManager Process - Window Server
 * - Owns the framebuffer (exclusive drawing rights)
 * - Reads mouse/keyboard events
 * - Routes events to application processes
 * - Renders all windows and controls from kernel-side state
 */

#include "../syscalls/user_syscalls.h"

// Syscall wrappers to get kernel UI state pointers
static inline void* syscall_get_windows_ptr(void) {
    void* result;
    __asm__ volatile(
        "mov $45, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
    );
    return result;
}

static inline void* syscall_get_controls_ptr(void) {
    void* result;
    __asm__ volatile(
        "mov $46, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
    );
    return result;
}

static inline void* syscall_get_events_ptr(void) {
    void* result;
    __asm__ volatile(
        "mov $47, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
    );
    return result;
}

// Control types
#define UIMAN_CONTROL_BUTTON  1
#define UIMAN_CONTROL_LABEL   2
#define UIMAN_CONTROL_TEXTBOX 3
#define UIMAN_CONTROL_TABLE   4
#define UIMAN_CONTROL_RADIO   5

// Control states
#define UIMAN_STATE_NORMAL    0
#define UIMAN_STATE_HOVER     1
#define UIMAN_STATE_PRESSED   2

// Event types
#define UIMAN_EVENT_NONE      0
#define UIMAN_EVENT_CLICK     1
#define UIMAN_EVENT_DBLCLICK  2
#define UIMAN_EVENT_HOVER     3

#define MAX_WINDOWS 32
#define MAX_CONTROLS 256
#define MAX_PROCESSES 64
#define EVENT_QUEUE_SIZE 32

typedef struct {
    int type;
    int control_id;
    int x, y;
} uiman_event_t;

typedef struct {
    int active;
    int id;
    int parent_id;
    int owner_pid;
    int x, y, width, height;
    int z_order;
    int visible;
    int focused;
    char title[64];
} UIWindow;

typedef struct {
    int active;
    int id;
    int window_id;
    int owner_pid;
    int type;
    int x, y, width, height;
    int state;
    char text[128];
} UIControl;

typedef struct {
    uiman_event_t events[EVENT_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
    volatile int count;
} EventQueue;

// Forward declarations
extern void gui_draw_text(int x, int y, const char *text, unsigned int fg, unsigned int bg);

// Pointers to kernel arrays
static UIWindow *g_windows = 0;
static UIControl *g_controls = 0;
static EventQueue *g_event_queues = 0;

// Track last state to detect changes
static int g_last_control_state[MAX_CONTROLS] = {0};

// Render a single control
static void render_control(int i) {
    if (!g_controls[i].active) return;
    
    UIControl *ctrl = &g_controls[i];
    
    // Find parent window
    UIWindow *win = 0;
    for (int j = 0; j < MAX_WINDOWS; j++) {
        if (g_windows[j].active && g_windows[j].id == ctrl->window_id) {
            win = &g_windows[j];
            break;
        }
    }
    if (!win) return;
    
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + ctrl->y;
    
    // Render based on type
    switch (ctrl->type) {
        case UIMAN_CONTROL_BUTTON: {
            // Draw button border
            unsigned int border_color = (ctrl->state == UIMAN_STATE_HOVER) ? 0x8080FF : 
                                       (ctrl->state == UIMAN_STATE_PRESSED) ? 0x00FF00 : 0x808080;
            
            // Draw 4-pixel border
            syscall_fill_rect(abs_x, abs_y, ctrl->width, 4, border_color);  // Top
            syscall_fill_rect(abs_x, abs_y + ctrl->height - 4, ctrl->width, 4, border_color);  // Bottom
            syscall_fill_rect(abs_x, abs_y, 4, ctrl->height, border_color);  // Left
            syscall_fill_rect(abs_x + ctrl->width - 4, abs_y, 4, ctrl->height, border_color);  // Right
            
            // Draw button background
            syscall_fill_rect(abs_x + 4, abs_y + 4, ctrl->width - 8, ctrl->height - 8, 0x404040);
            
            // Draw text
            gui_draw_text(abs_x + 10, abs_y + 15, ctrl->text, 0xFFFFFF, 0);
            break;
        }
        
        case UIMAN_CONTROL_LABEL: {
            // Just draw text (no background)
            gui_draw_text(abs_x, abs_y, ctrl->text, 0xFFFFFF, 0);
            break;
        }
        
        default:
            break;
    }
}

// Render all controls
static void render_all_controls(void) {
    for (int i = 0; i < MAX_CONTROLS; i++) {
        render_control(i);
    }
}

// Hit test - find control at coordinates
static int hit_test(int x, int y) {
    for (int i = MAX_CONTROLS - 1; i >= 0; i--) {
        if (!g_controls[i].active) continue;
        
        UIControl *ctrl = &g_controls[i];
        
        // Find parent window
        UIWindow *win = 0;
        for (int j = 0; j < MAX_WINDOWS; j++) {
            if (g_windows[j].active && g_windows[j].id == ctrl->window_id) {
                win = &g_windows[j];
                break;
            }
        }
        if (!win || !win->visible) continue;
        
        int abs_x = win->x + ctrl->x;
        int abs_y = win->y + ctrl->y;
        
        if (x >= abs_x && x < abs_x + ctrl->width &&
            y >= abs_y && y < abs_y + ctrl->height) {
            return i;
        }
    }
    return -1;
}

// Queue event to process
static void queue_event(int owner_pid, uiman_event_t *event) {
    if (owner_pid < 0 || owner_pid >= MAX_PROCESSES) return;
    
    EventQueue *queue = &g_event_queues[owner_pid];
    if (queue->count >= EVENT_QUEUE_SIZE) return;  // Queue full
    
    queue->events[queue->tail] = *event;
    queue->tail = (queue->tail + 1) % EVENT_QUEUE_SIZE;
    queue->count++;
}

/**
 * UIManager main entry point
 */
void uimanager_main_c() {
    syscall_puts("[UIMANAGER] Entry!\n");
    
    // Get pointers to kernel UI arrays via syscalls
    syscall_puts("[UIMANAGER] Getting kernel UI state pointers...\n");
    g_windows = (UIWindow*)syscall_get_windows_ptr();
    g_controls = (UIControl*)syscall_get_controls_ptr();
    g_event_queues = (EventQueue*)syscall_get_events_ptr();
    syscall_puts("[UIMANAGER] Got kernel state\n");
    
    // Clear screen to dark blue background
    syscall_puts("[UIMANAGER] Clearing screen...\n");
    syscall_fill_rect(0, 0, 800, 600, 0x001020);
    
    // Draw all controls initially
    syscall_puts("[UIMANAGER] Drawing initial controls...\n");
    render_all_controls();
    
    // Initialize state tracking
    for (int i = 0; i < MAX_CONTROLS; i++) {
        g_last_control_state[i] = g_controls[i].state;
    }
    
    syscall_puts("[UIMANAGER] Entering event loop\n");
    
    // Event loop state
    int last_mouse_x = -1;
    int last_mouse_y = -1;
    unsigned int last_buttons = 0;
    int hover_control = -1;
    static int last_click_time = 0;
    static int last_click_control = -1;
    int frame_count = 0;
    
    while (1) {
        frame_count++;
        
        // Get mouse state
        int mx = syscall_mouse_get_x();
        int my = syscall_mouse_get_y();
        unsigned int buttons = syscall_mouse_get_buttons();
        
        // Poll mouse periodically
        if (frame_count % 3 == 0) {
            syscall_poll_mouse();
        }
        
        // Hit test - which control is under cursor?
        int hit_control = hit_test(mx, my);
        
        // Update hover states
        if (hit_control != hover_control) {
            // Clear old hover
            if (hover_control >= 0 && g_controls[hover_control].active) {
                if (g_controls[hover_control].state == UIMAN_STATE_HOVER) {
                    g_controls[hover_control].state = UIMAN_STATE_NORMAL;
                }
                
                // Send hover exit event
                uiman_event_t event = {
                    .type = UIMAN_EVENT_HOVER,
                    .control_id = g_controls[hover_control].id,
                    .x = 0,
                    .y = 0
                };
                queue_event(g_controls[hover_control].owner_pid, &event);
            }
            
            // Set new hover
            if (hit_control >= 0 && g_controls[hit_control].active) {
                g_controls[hit_control].state = UIMAN_STATE_HOVER;
                
                // Send hover enter event
                uiman_event_t event = {
                    .type = UIMAN_EVENT_HOVER,
                    .control_id = g_controls[hit_control].id,
                    .x = mx,
                    .y = my
                };
                queue_event(g_controls[hit_control].owner_pid, &event);
            }
            
            hover_control = hit_control;
        }
        
        // Detect button press
        if ((buttons & 0x01) && !(last_buttons & 0x01)) {
            // Left button pressed
            if (hit_control >= 0 && g_controls[hit_control].active) {
                g_controls[hit_control].state = UIMAN_STATE_PRESSED;
                
                // Check for double click
                int is_double_click = 0;
                if (last_click_control == hit_control) {
                    int time_diff = frame_count - last_click_time;
                    if (time_diff < 30) {  // ~30 frames = ~500ms at 60fps
                        is_double_click = 1;
                    }
                }
                
                // Send click or double-click event
                uiman_event_t event = {
                    .type = is_double_click ? UIMAN_EVENT_DBLCLICK : UIMAN_EVENT_CLICK,
                    .control_id = g_controls[hit_control].id,
                    .x = mx,
                    .y = my
                };
                queue_event(g_controls[hit_control].owner_pid, &event);
                
                last_click_time = frame_count;
                last_click_control = hit_control;
            }
        }
        
        // Detect button release
        if (!(buttons & 0x01) && (last_buttons & 0x01)) {
            // Left button released
            if (hit_control >= 0 && g_controls[hit_control].active) {
                g_controls[hit_control].state = UIMAN_STATE_HOVER;
            }
        }
        
        last_buttons = buttons;
        
        // Erase old cursor position (draw background color)
        if (last_mouse_x >= 0 && last_mouse_y >= 0) {
            if (last_mouse_x != mx || last_mouse_y != my) {
                // Erase 11x16 area with background color
                syscall_fill_rect(last_mouse_x, last_mouse_y, 11, 16, 0x001020);
                
                // Redraw any control that was under the old cursor
                int old_hit = hit_test(last_mouse_x + 5, last_mouse_y + 8);
                if (old_hit >= 0) {
                    render_control(old_hit);
                }
            }
        }
        
        // Only redraw controls that changed state
        for (int i = 0; i < MAX_CONTROLS; i++) {
            if (g_controls[i].active && g_controls[i].state != g_last_control_state[i]) {
                render_control(i);
                g_last_control_state[i] = g_controls[i].state;
            }
        }
        
        // Draw arrow cursor on top using syscalls (pixel by pixel)
        if (mx >= 0 && mx < 800 && my >= 0 && my < 600) {
            // Classic arrow pointer (11x16 pixels)
            int arrow_data[16] = {
                0b10000000000,  // X
                0b11000000000,  // XX
                0b11100000000,  // XXX
                0b11110000000,  // XXXX
                0b11111000000,  // XXXXX
                0b11111100000,  // XXXXXX
                0b11111110000,  // XXXXXXX
                0b11111111000,  // XXXXXXXX
                0b11111111100,  // XXXXXXXXX
                0b11111100000,  // XXXXXX
                0b11011100000,  // XX XXX
                0b10001110000,  // X  XXX
                0b00001110000,  //    XXX
                0b00000111000,  //     XXX
                0b00000111000,  //     XXX
                0b00000011000   //      XX
            };
            
            // Draw each pixel of the arrow cursor using syscalls
            for (int dy = 0; dy < 16; dy++) {
                if (my + dy >= 600) break;
                for (int dx = 0; dx < 11; dx++) {
                    if (mx + dx >= 800) break;
                    if (arrow_data[dy] & (1 << (10 - dx))) {
                        syscall_fill_rect(mx + dx, my + dy, 1, 1, 0xFFFFFF);
                    }
                }
            }
        }
        
        last_mouse_x = mx;
        last_mouse_y = my;
    }
}
