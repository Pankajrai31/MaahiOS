/**
 * UIManager Process - Window Server
 * - Owns the framebuffer (exclusive drawing rights)
 * - Reads mouse/keyboard events
 * - Routes events to application processes
 * - Renders all windows and controls from kernel-side state
 */

#include <stdint.h>
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

static inline void* syscall_alloc_memory(unsigned int size) {
    void* result;
    __asm__ volatile(
        "mov $50, %%eax\n"
        "mov %1, %%ebx\n"
        "int $0x80\n"
        : "=a"(result)
        : "r"(size)
        : "ebx"
    );
    return result;
}

// Atomic memcpy syscall - interrupt-safe buffer copy
static inline void syscall_atomic_memcpy(void* dest, void* src, unsigned int size_bytes) {
    __asm__ volatile(
        "mov $51, %%eax\n"
        "int $0x80\n"
        :
        : "b"(dest), "c"(src), "d"(size_bytes)
        : "eax"
    );
}

// Window states
#define WINDOW_STATE_NORMAL     0
#define WINDOW_STATE_MINIMIZED  1
#define WINDOW_STATE_MAXIMIZED  2
#define WINDOW_STATE_PENDING_CLOSE  3  // Marked for cleanup

// Window title bar constants
#define TITLE_BAR_HEIGHT 24
#define WINDOW_BUTTON_SIZE 20
#define WINDOW_BUTTON_MARGIN 2

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
    int state;  // WINDOW_STATE_NORMAL, MINIMIZED, MAXIMIZED
    int saved_x, saved_y, saved_width, saved_height;  // For restore from min/max
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

// Double buffering
static uint32_t *g_back_buffer = 0;      // Hidden buffer in RAM
static uint32_t *g_framebuffer = (uint32_t*)0xFD000000;  // BGA hardware buffer
static const int SCREEN_WIDTH = 1024;  // Must match BGA init in kernel.c
static const int SCREEN_HEIGHT = 768;  // Must match BGA init in kernel.c

// Track last state to detect changes
static int g_last_control_state[MAX_CONTROLS] = {0};

// Deferred cleanup queue - windows marked for safe cleanup
#define CLEANUP_QUEUE_SIZE 16
static int cleanup_queue[CLEANUP_QUEUE_SIZE];  // Window IDs to clean up
static int cleanup_queue_count = 0;

// Taskbar/Tray system
#define TASKBAR_HEIGHT 40
#define TASKBAR_BUTTON_WIDTH 120
#define TASKBAR_BUTTON_HEIGHT 30
#define TASKBAR_BUTTON_MARGIN 5
#define MAX_TASKBAR_ITEMS 16

typedef struct {
    int window_id;
    char title[32];
} TaskbarItem;

static TaskbarItem taskbar_items[MAX_TASKBAR_ITEMS];
static int taskbar_count = 0;

// Simple 8x8 font (ASCII 32-126) - minimal bitmap font
static const unsigned char font_8x8[95][8] = {
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00}, // Space
    {0x18,0x3C,0x3C,0x18,0x18,0x00,0x18,0x00}, // !
    {0x36,0x36,0x00,0x00,0x00,0x00,0x00,0x00}, // "
    {0x36,0x36,0x7F,0x36,0x7F,0x36,0x36,0x00}, // #
    {0x0C,0x3E,0x03,0x1E,0x30,0x1F,0x0C,0x00}, // $
    {0x00,0x63,0x33,0x18,0x0C,0x66,0x63,0x00}, // %
    {0x1C,0x36,0x1C,0x6E,0x3B,0x33,0x6E,0x00}, // &
    {0x06,0x06,0x03,0x00,0x00,0x00,0x00,0x00}, // '
    {0x18,0x0C,0x06,0x06,0x06,0x0C,0x18,0x00}, // (
    {0x06,0x0C,0x18,0x18,0x18,0x0C,0x06,0x00}, // )
    {0x00,0x66,0x3C,0xFF,0x3C,0x66,0x00,0x00}, // *
    {0x00,0x0C,0x0C,0x3F,0x0C,0x0C,0x00,0x00}, // +
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x06}, // ,
    {0x00,0x00,0x00,0x3F,0x00,0x00,0x00,0x00}, // -
    {0x00,0x00,0x00,0x00,0x00,0x0C,0x0C,0x00}, // .
    {0x60,0x30,0x18,0x0C,0x06,0x03,0x01,0x00}, // /
    {0x3E,0x63,0x73,0x7B,0x6F,0x67,0x3E,0x00}, // 0
    {0x0C,0x0E,0x0C,0x0C,0x0C,0x0C,0x3F,0x00}, // 1
    {0x1E,0x33,0x30,0x1C,0x06,0x33,0x3F,0x00}, // 2
    {0x1E,0x33,0x30,0x1C,0x30,0x33,0x1E,0x00}, // 3
    {0x38,0x3C,0x36,0x33,0x7F,0x30,0x78,0x00}, // 4
    {0x3F,0x03,0x1F,0x30,0x30,0x33,0x1E,0x00}, // 5
    {0x1C,0x06,0x03,0x1F,0x33,0x33,0x1E,0x00}, // 6
    {0x3F,0x33,0x30,0x18,0x0C,0x0C,0x0C,0x00}, // 7
    {0x1E,0x33,0x33,0x1E,0x33,0x33,0x1E,0x00}, // 8
    {0x1E,0x33,0x33,0x3E,0x30,0x18,0x0E,0x00}, // 9
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x00}, // :
    {0x00,0x0C,0x0C,0x00,0x00,0x0C,0x0C,0x06}, // ;
    {0x18,0x0C,0x06,0x03,0x06,0x0C,0x18,0x00}, // <
    {0x00,0x00,0x3F,0x00,0x00,0x3F,0x00,0x00}, // =
    {0x06,0x0C,0x18,0x30,0x18,0x0C,0x06,0x00}, // >
    {0x1E,0x33,0x30,0x18,0x0C,0x00,0x0C,0x00}, // ?
    {0x3E,0x63,0x7B,0x7B,0x7B,0x03,0x1E,0x00}, // @
    {0x0C,0x1E,0x33,0x33,0x3F,0x33,0x33,0x00}, // A
    {0x3F,0x66,0x66,0x3E,0x66,0x66,0x3F,0x00}, // B
    {0x3C,0x66,0x03,0x03,0x03,0x66,0x3C,0x00}, // C
    {0x1F,0x36,0x66,0x66,0x66,0x36,0x1F,0x00}, // D
    {0x7F,0x46,0x16,0x1E,0x16,0x46,0x7F,0x00}, // E
    {0x7F,0x46,0x16,0x1E,0x16,0x06,0x0F,0x00}, // F
    {0x3C,0x66,0x03,0x03,0x73,0x66,0x7C,0x00}, // G
    {0x33,0x33,0x33,0x3F,0x33,0x33,0x33,0x00}, // H
    {0x1E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // I
    {0x78,0x30,0x30,0x30,0x33,0x33,0x1E,0x00}, // J
    {0x67,0x66,0x36,0x1E,0x36,0x66,0x67,0x00}, // K
    {0x0F,0x06,0x06,0x06,0x46,0x66,0x7F,0x00}, // L
    {0x63,0x77,0x7F,0x7F,0x6B,0x63,0x63,0x00}, // M
    {0x63,0x67,0x6F,0x7B,0x73,0x63,0x63,0x00}, // N
    {0x1C,0x36,0x63,0x63,0x63,0x36,0x1C,0x00}, // O
    {0x3F,0x66,0x66,0x3E,0x06,0x06,0x0F,0x00}, // P
    {0x1E,0x33,0x33,0x33,0x3B,0x1E,0x38,0x00}, // Q
    {0x3F,0x66,0x66,0x3E,0x36,0x66,0x67,0x00}, // R
    {0x1E,0x33,0x07,0x0E,0x38,0x33,0x1E,0x00}, // S
    {0x3F,0x2D,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // T
    {0x33,0x33,0x33,0x33,0x33,0x33,0x3F,0x00}, // U
    {0x33,0x33,0x33,0x33,0x33,0x1E,0x0C,0x00}, // V
    {0x63,0x63,0x63,0x6B,0x7F,0x77,0x63,0x00}, // W
    {0x63,0x63,0x36,0x1C,0x1C,0x36,0x63,0x00}, // X
    {0x33,0x33,0x33,0x1E,0x0C,0x0C,0x1E,0x00}, // Y
    {0x7F,0x63,0x31,0x18,0x4C,0x66,0x7F,0x00}, // Z
    {0x1C,0x06,0x06,0x06,0x06,0x06,0x1C,0x00}, // [
    {0x03,0x06,0x0C,0x18,0x30,0x60,0x40,0x00}, // backslash
    {0x1C,0x18,0x18,0x18,0x18,0x18,0x1C,0x00}, // ]
    {0x08,0x1C,0x36,0x63,0x00,0x00,0x00,0x00}, // ^
    {0x00,0x00,0x00,0x00,0x00,0x00,0x00,0xFF}, // _
    {0x0C,0x0C,0x18,0x00,0x00,0x00,0x00,0x00}, // `
    {0x00,0x00,0x1E,0x30,0x3E,0x33,0x6E,0x00}, // a
    {0x07,0x06,0x06,0x3E,0x66,0x66,0x3B,0x00}, // b
    {0x00,0x00,0x1E,0x33,0x03,0x33,0x1E,0x00}, // c
    {0x38,0x30,0x30,0x3E,0x33,0x33,0x6E,0x00}, // d
    {0x00,0x00,0x1E,0x33,0x3F,0x03,0x1E,0x00}, // e
    {0x1C,0x36,0x06,0x0F,0x06,0x06,0x0F,0x00}, // f
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x1F}, // g
    {0x07,0x06,0x36,0x6E,0x66,0x66,0x67,0x00}, // h
    {0x0C,0x00,0x0E,0x0C,0x0C,0x0C,0x1E,0x00}, // i
    {0x30,0x00,0x30,0x30,0x30,0x33,0x33,0x1E}, // j
    {0x07,0x06,0x66,0x36,0x1E,0x36,0x67,0x00}, // k
    {0x0E,0x0C,0x0C,0x0C,0x0C,0x0C,0x1E,0x00}, // l
    {0x00,0x00,0x33,0x7F,0x7F,0x6B,0x63,0x00}, // m
    {0x00,0x00,0x1F,0x33,0x33,0x33,0x33,0x00}, // n
    {0x00,0x00,0x1E,0x33,0x33,0x33,0x1E,0x00}, // o
    {0x00,0x00,0x3B,0x66,0x66,0x3E,0x06,0x0F}, // p
    {0x00,0x00,0x6E,0x33,0x33,0x3E,0x30,0x78}, // q
    {0x00,0x00,0x3B,0x6E,0x66,0x06,0x0F,0x00}, // r
    {0x00,0x00,0x3E,0x03,0x1E,0x30,0x1F,0x00}, // s
    {0x08,0x0C,0x3E,0x0C,0x0C,0x2C,0x18,0x00}, // t
    {0x00,0x00,0x33,0x33,0x33,0x33,0x6E,0x00}, // u
    {0x00,0x00,0x33,0x33,0x33,0x1E,0x0C,0x00}, // v
    {0x00,0x00,0x63,0x6B,0x7F,0x7F,0x36,0x00}, // w
    {0x00,0x00,0x63,0x36,0x1C,0x36,0x63,0x00}, // x
    {0x00,0x00,0x33,0x33,0x33,0x3E,0x30,0x1F}, // y
    {0x00,0x00,0x3F,0x19,0x0C,0x26,0x3F,0x00}, // z
    {0x38,0x0C,0x0C,0x07,0x0C,0x0C,0x38,0x00}, // {
    {0x18,0x18,0x18,0x00,0x18,0x18,0x18,0x00}, // |
    {0x07,0x0C,0x0C,0x38,0x0C,0x0C,0x07,0x00}, // }
    {0x6E,0x3B,0x00,0x00,0x00,0x00,0x00,0x00}, // ~
};

static int needs_full_redraw = 1;
static int use_double_buffer = 0;  // Track if double buffering is enabled

// Performance flags to avoid unnecessary work
static int cleanup_queue_empty = 1;

// Cursor management for direct framebuffer drawing
#define CURSOR_SIZE 7
static uint32_t cursor_save_buffer[CURSOR_SIZE * CURSOR_SIZE];
static int cursor_saved = 0;
static int cursor_last_x = -1;
static int cursor_last_y = -1;

// Forward declarations for title bar rendering
static void draw_to_back_buffer(int x, int y, int w, int h, unsigned int color);
static void draw_text_to_back_buffer(int x, int y, const char *text, unsigned int fg, unsigned int bg);

// Draw window title bar with close/minimize/maximize buttons
static void draw_window_title_bar(UIWindow *win) {
    if (!win || !win->active || !win->visible) return;
    
    // Title bar background (dark gray)
    draw_to_back_buffer(win->x, win->y, win->width, TITLE_BAR_HEIGHT, 0x303030);
    
    // Title text (centered vertically in title bar)
    draw_text_to_back_buffer(win->x + 8, win->y + (TITLE_BAR_HEIGHT / 2) - 4, win->title, 0xFFFFFF, 0);
    
    // Calculate button positions (right-aligned)
    int btn_y = win->y + WINDOW_BUTTON_MARGIN;
    int close_x = win->x + win->width - WINDOW_BUTTON_SIZE - WINDOW_BUTTON_MARGIN;
    int maximize_x = close_x - WINDOW_BUTTON_SIZE - WINDOW_BUTTON_MARGIN;
    int minimize_x = maximize_x - WINDOW_BUTTON_SIZE - WINDOW_BUTTON_MARGIN;
    
    // Close button (red X)
    draw_to_back_buffer(close_x, btn_y, WINDOW_BUTTON_SIZE, WINDOW_BUTTON_SIZE, 0xCC0000);
    draw_text_to_back_buffer(close_x + 6, btn_y + 6, "X", 0xFFFFFF, 0);
    
    // Maximize button (green square/restore)
    if (win->state == WINDOW_STATE_MAXIMIZED) {
        draw_to_back_buffer(maximize_x, btn_y, WINDOW_BUTTON_SIZE, WINDOW_BUTTON_SIZE, 0x008800);
        draw_text_to_back_buffer(maximize_x + 5, btn_y + 6, "=", 0xFFFFFF, 0);
    } else {
        draw_to_back_buffer(maximize_x, btn_y, WINDOW_BUTTON_SIZE, WINDOW_BUTTON_SIZE, 0x00CC00);
        draw_text_to_back_buffer(maximize_x + 5, btn_y + 6, "+", 0xFFFFFF, 0);
    }
    
    // Minimize button (yellow dash)
    draw_to_back_buffer(minimize_x, btn_y, WINDOW_BUTTON_SIZE, WINDOW_BUTTON_SIZE, 0xCCCC00);
    draw_text_to_back_buffer(minimize_x + 6, btn_y + 6, "-", 0xFFFFFF, 0);
}

// Check if click is on window title bar buttons
// Returns: 1=close, 2=minimize, 3=maximize, 0=none
static int hit_test_title_bar_button(UIWindow *win, int x, int y) {
    if (!win || !win->active || !win->visible) return 0;
    
    // Check if click is within title bar
    if (y < win->y || y >= win->y + TITLE_BAR_HEIGHT) return 0;
    if (x < win->x || x >= win->x + win->width) return 0;
    
    int btn_y = win->y + WINDOW_BUTTON_MARGIN;
    int close_x = win->x + win->width - WINDOW_BUTTON_SIZE - WINDOW_BUTTON_MARGIN;
    int maximize_x = close_x - WINDOW_BUTTON_SIZE - WINDOW_BUTTON_MARGIN;
    int minimize_x = maximize_x - WINDOW_BUTTON_SIZE - WINDOW_BUTTON_MARGIN;
    
    // Check close button
    if (x >= close_x && x < close_x + WINDOW_BUTTON_SIZE &&
        y >= btn_y && y < btn_y + WINDOW_BUTTON_SIZE) {
        return 1;  // Close
    }
    
    // Check maximize button
    if (x >= maximize_x && x < maximize_x + WINDOW_BUTTON_SIZE &&
        y >= btn_y && y < btn_y + WINDOW_BUTTON_SIZE) {
        return 3;  // Maximize
    }
    
    // Check minimize button
    if (x >= minimize_x && x < minimize_x + WINDOW_BUTTON_SIZE &&
        y >= btn_y && y < btn_y + WINDOW_BUTTON_SIZE) {
        return 2;  // Minimize
    }
    
    return 0;
}

// Cursor drawing functions - direct to framebuffer for performance
static void save_cursor_background(int x, int y) {
    if (x < 3 || y < 3 || x >= SCREEN_WIDTH - 3 || y >= SCREEN_HEIGHT - 3) return;
    
    int idx = 0;
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            cursor_save_buffer[idx++] = g_framebuffer[(y + dy) * SCREEN_WIDTH + (x + dx)];
        }
    }
    cursor_saved = 1;
    cursor_last_x = x;
    cursor_last_y = y;
}

static void restore_cursor_background(void) {
    if (!cursor_saved) return;
    if (cursor_last_x < 3 || cursor_last_y < 3 || 
        cursor_last_x >= SCREEN_WIDTH - 3 || cursor_last_y >= SCREEN_HEIGHT - 3) return;
    
    int idx = 0;
    for (int dy = -3; dy <= 3; dy++) {
        for (int dx = -3; dx <= 3; dx++) {
            g_framebuffer[(cursor_last_y + dy) * SCREEN_WIDTH + (cursor_last_x + dx)] = cursor_save_buffer[idx++];
        }
    }
}

static void draw_cursor_direct(int x, int y) {
    if (x < 3 || y < 3 || x >= SCREEN_WIDTH - 3 || y >= SCREEN_HEIGHT - 3) return;
    
    // Draw plus sign cursor directly to framebuffer
    // Vertical line
    for (int i = -3; i <= 3; i++) {
        g_framebuffer[(y + i) * SCREEN_WIDTH + x] = 0xFFFFFFFF;
    }
    // Horizontal line
    for (int i = -3; i <= 3; i++) {
        g_framebuffer[y * SCREEN_WIDTH + (x + i)] = 0xFFFFFFFF;
    }
}

// Double buffering helper functions
static inline void copy_buffer_to_screen(void* src) {
    // Only copy if double buffering is enabled
    if (!use_double_buffer) return;
    if (!src || src == g_framebuffer) return;
    
    // Use atomic memcpy syscall to prevent scheduler interruption
    // This ensures the entire buffer is copied without tearing
    syscall_atomic_memcpy(g_framebuffer, src, SCREEN_WIDTH * SCREEN_HEIGHT * 4);
}

static inline void draw_to_back_buffer(int x, int y, int w, int h, unsigned int color) {
    if (!g_back_buffer) return;
    if (g_back_buffer == g_framebuffer) {
        // Fallback: use syscall if no back buffer
        syscall_fill_rect(x, y, w, h, color);
        return;
    }
    
    // Bounds check and clipping
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (w <= 0 || h <= 0) return;
    if (x >= SCREEN_WIDTH || y >= SCREEN_HEIGHT) return;
    if (x + w > SCREEN_WIDTH) w = SCREEN_WIDTH - x;
    if (y + h > SCREEN_HEIGHT) h = SCREEN_HEIGHT - y;
    
    // Draw rectangle to back buffer
    for (int row = 0; row < h; row++) {
        uint32_t *line = &g_back_buffer[(y + row) * SCREEN_WIDTH + x];
        for (int col = 0; col < w; col++) {
            line[col] = color;
        }
    }
}

// Draw text directly to back buffer using 8x8 font
static inline void draw_text_to_back_buffer(int x, int y, const char *text, unsigned int fg, unsigned int bg) {
    if (!g_back_buffer) return;
    
    int cur_x = x;
    int cur_y = y;
    
    for (int i = 0; text[i]; i++) {
        char c = text[i];
        if (c < 32 || c > 126) c = 32; // Default to space for unsupported chars
        
        const unsigned char *glyph = font_8x8[c - 32];
        
        // Draw 8x8 character
        for (int row = 0; row < 8; row++) {
            for (int col = 0; col < 8; col++) {
                if (cur_x + col >= SCREEN_WIDTH || cur_y + row >= SCREEN_HEIGHT) continue;
                if (cur_x + col < 0 || cur_y + row < 0) continue;
                
                // Read bits from LSB to MSB (bit 0 to bit 7)
                unsigned int color = (glyph[row] & (1 << col)) ? fg : bg;
                if (color == 0 && bg == 0) continue; // Skip transparent background
                
                g_back_buffer[(cur_y + row) * SCREEN_WIDTH + (cur_x + col)] = color;
            }
        }
        
        cur_x += 8; // Move to next character position
    }
}

// Render only the border of a button (for hover state changes)
static void render_button_border_only(int i) {
    if (!g_controls[i].active || g_controls[i].type != UIMAN_CONTROL_BUTTON) return;
    
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
    
    // Controls are positioned relative to window content area (below title bar)
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + TITLE_BAR_HEIGHT + ctrl->y;
    
    // Get border color based on state
    unsigned int border_color = (ctrl->state == UIMAN_STATE_HOVER) ? 0x8080FF : 
                               (ctrl->state == UIMAN_STATE_PRESSED) ? 0x00FF00 : 0x808080;
    
    // Draw only the 4-pixel border (don't touch background or text)
    syscall_fill_rect(abs_x, abs_y, ctrl->width, 4, border_color);  // Top
    syscall_fill_rect(abs_x, abs_y + ctrl->height - 4, ctrl->width, 4, border_color);  // Bottom
    syscall_fill_rect(abs_x, abs_y, 4, ctrl->height, border_color);  // Left
    syscall_fill_rect(abs_x + ctrl->width - 4, abs_y, 4, ctrl->height, border_color);  // Right
}

// Render a single control
static void render_control(int i) {
    if (!g_controls[i].active) return;
    
    UIControl *ctrl = &g_controls[i];
    
    int abs_x, abs_y;
    
    // Check if this is a desktop control (window_id = 0)
    if (ctrl->window_id == 0) {
        // Desktop control - render directly at specified position
        abs_x = ctrl->x;
        abs_y = ctrl->y;
    } else {
        // Window control - find parent window
        UIWindow *win = 0;
        for (int j = 0; j < MAX_WINDOWS; j++) {
            if (g_windows[j].active && g_windows[j].id == ctrl->window_id) {
                win = &g_windows[j];
                break;
            }
        }
        if (!win) return;
        
        // Controls are positioned relative to window content area (below title bar)
        abs_x = win->x + ctrl->x;
        abs_y = win->y + TITLE_BAR_HEIGHT + ctrl->y;
    }
    
    // Render based on type
    switch (ctrl->type) {
        case UIMAN_CONTROL_BUTTON: {
            // Draw button border (make normal state more visible)
            unsigned int border_color = (ctrl->state == UIMAN_STATE_HOVER) ? 0x8080FF : 
                                       (ctrl->state == UIMAN_STATE_PRESSED) ? 0x00FF00 : 0xC0C0C0;
            
            // Draw 4-pixel border
            draw_to_back_buffer(abs_x, abs_y, ctrl->width, 4, border_color);  // Top
            draw_to_back_buffer(abs_x, abs_y + ctrl->height - 4, ctrl->width, 4, border_color);  // Bottom
            draw_to_back_buffer(abs_x, abs_y, 4, ctrl->height, border_color);  // Left
            draw_to_back_buffer(abs_x + ctrl->width - 4, abs_y, 4, ctrl->height, border_color);  // Rightht
            
            // Draw button background
            draw_to_back_buffer(abs_x + 4, abs_y + 4, ctrl->width - 8, ctrl->height - 8, 0x404040);
            
            // Draw text - center it vertically in the button (8px font height)
            draw_text_to_back_buffer(abs_x + 8, abs_y + (ctrl->height / 2) - 4, ctrl->text, 0xFFFFFF, 0);
            break;
        }
        
        case UIMAN_CONTROL_LABEL: {
            // Just draw text (no background)
            draw_text_to_back_buffer(abs_x, abs_y, ctrl->text, 0xFFFFFF, 0);
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
        
        int abs_x, abs_y;
        
        // Check if this is a desktop control
        if (ctrl->window_id == 0) {
            // Desktop control - direct position
            abs_x = ctrl->x;
            abs_y = ctrl->y;
        } else {
            // Window control - find parent window
            UIWindow *win = 0;
            for (int j = 0; j < MAX_WINDOWS; j++) {
                if (g_windows[j].active && g_windows[j].id == ctrl->window_id) {
                    win = &g_windows[j];
                    break;
                }
            }
            if (!win || !win->visible) continue;
            
            // Controls are positioned below title bar
            abs_x = win->x + ctrl->x;
            abs_y = win->y + TITLE_BAR_HEIGHT + ctrl->y;
        }
        
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
 * Render taskbar at bottom of screen
 */
static void render_taskbar(void) {
    int taskbar_y = SCREEN_HEIGHT - TASKBAR_HEIGHT;
    
    // Draw taskbar background (dark gray)
    for (int y = taskbar_y; y < SCREEN_HEIGHT; y++) {
        for (int x = 0; x < SCREEN_WIDTH; x++) {
            g_back_buffer[y * SCREEN_WIDTH + x] = 0xFF2B2B2B;
        }
    }
    
    // Draw taskbar buttons for minimized windows
    int button_x = TASKBAR_BUTTON_MARGIN;
    for (int i = 0; i < taskbar_count; i++) {
        int button_y = taskbar_y + (TASKBAR_HEIGHT - TASKBAR_BUTTON_HEIGHT) / 2;
        
        // Draw button background (lighter gray)
        for (int y = button_y; y < button_y + TASKBAR_BUTTON_HEIGHT; y++) {
            for (int x = button_x; x < button_x + TASKBAR_BUTTON_WIDTH && x < SCREEN_WIDTH; x++) {
                g_back_buffer[y * SCREEN_WIDTH + x] = 0xFF4A4A4A;
            }
        }
        
        // Draw button border
        for (int x = button_x; x < button_x + TASKBAR_BUTTON_WIDTH && x < SCREEN_WIDTH; x++) {
            g_back_buffer[button_y * SCREEN_WIDTH + x] = 0xFF6A6A6A;
            g_back_buffer[(button_y + TASKBAR_BUTTON_HEIGHT - 1) * SCREEN_WIDTH + x] = 0xFF6A6A6A;
        }
        for (int y = button_y; y < button_y + TASKBAR_BUTTON_HEIGHT; y++) {
            g_back_buffer[y * SCREEN_WIDTH + button_x] = 0xFF6A6A6A;
            if (button_x + TASKBAR_BUTTON_WIDTH < SCREEN_WIDTH) {
                g_back_buffer[y * SCREEN_WIDTH + button_x + TASKBAR_BUTTON_WIDTH - 1] = 0xFF6A6A6A;
            }
        }
        
        // Draw title text
        draw_text_to_back_buffer(button_x + 5, button_y + 8, taskbar_items[i].title, 0xFFFFFFFF, 0);
        
        button_x += TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_MARGIN;
        if (button_x >= SCREEN_WIDTH) break;
    }
}

/**
 * Hit test taskbar - returns taskbar item index or -1
 */
static int hit_test_taskbar(int x, int y) {
    int taskbar_y = SCREEN_HEIGHT - TASKBAR_HEIGHT;
    
    // Check if click is in taskbar area
    if (y < taskbar_y) {
        return -1;
    }
    
    // Check each taskbar button
    int button_x = TASKBAR_BUTTON_MARGIN;
    for (int i = 0; i < taskbar_count; i++) {
        int button_y = taskbar_y + (TASKBAR_HEIGHT - TASKBAR_BUTTON_HEIGHT) / 2;
        
        if (x >= button_x && x < button_x + TASKBAR_BUTTON_WIDTH &&
            y >= button_y && y < button_y + TASKBAR_BUTTON_HEIGHT) {
            return i;
        }
        
        button_x += TASKBAR_BUTTON_WIDTH + TASKBAR_BUTTON_MARGIN;
        if (button_x >= SCREEN_WIDTH) break;
    }
    
    return -1;
}

/**
 * Process cleanup queue - safely clean up windows marked for closure
 * Called from main loop with interrupts disabled for atomicity
 */
static void process_cleanup_queue(void) {
    if (cleanup_queue_count == 0) {
        return;  // Nothing to clean up
    }
    
    syscall_puts("[UIMANAGER] Processing cleanup queue, count=");
    // syscall_putint(cleanup_queue_count);
    syscall_puts("\n");
    
    // Don't disable interrupts - syscalls handle their own atomicity
    // and we need timer interrupts to work for process termination
    
    for (int i = 0; i < cleanup_queue_count; i++) {
        int window_id = cleanup_queue[i];
        
        // Find the window
        int window_idx = -1;
        for (int j = 0; j < MAX_WINDOWS; j++) {
            if (g_windows[j].active && g_windows[j].id == window_id) {
                window_idx = j;
                break;
            }
        }
        
        if (window_idx == -1) {
            continue;  // Window already cleaned up
        }
        
        UIWindow *win = &g_windows[window_idx];
        
        // Only clean up if still pending close
        if (win->state != WINDOW_STATE_PENDING_CLOSE) {
            continue;
        }
        
        syscall_puts("[UIMANAGER] Cleaning up window: ");
        syscall_puts(win->title);
        syscall_puts("\n");
        
        int owner_pid = win->owner_pid;
        
        // Deactivate all controls belonging to this window
        for (int k = 0; k < MAX_CONTROLS; k++) {
            if (g_controls[k].active && g_controls[k].window_id == window_id) {
                g_controls[k].active = 0;
            }
        }
        
        // Deactivate the window
        win->active = 0;
        win->visible = 0;
        
        // Remove from taskbar if present
        for (int t = 0; t < taskbar_count; t++) {
            if (taskbar_items[t].window_id == window_id) {
                // Shift remaining items
                for (int s = t; s < taskbar_count - 1; s++) {
                    taskbar_items[s] = taskbar_items[s + 1];
                }
                taskbar_count--;
                break;
            }
        }
        
        // Kill the process that owns this window
        if (owner_pid > 0) {
            syscall_puts("[UIMANAGER] Terminating process PID=");
            // syscall_putint(owner_pid);
            syscall_puts("\n");
            
            // Call kill_process syscall
            int result;
            __asm__ volatile(
                "int $0x80"
                : "=a"(result)
                : "a"(53), "b"(owner_pid)  // syscall 53 = kill_process
                : "memory"
            );
            
            syscall_puts("[UIMANAGER] Process terminated, result=");
            // syscall_putint(result);
            syscall_puts("\n");
        }
        
        syscall_puts("[UIMANAGER] Cleanup complete\n");
    }
    
    // Clear cleanup queue
    cleanup_queue_count = 0;
    cleanup_queue_empty = 1;  // Set flag to avoid checking empty queue
    
    needs_full_redraw = 1;
}

/**
 * UIManager main entry point
 */
void uimanager_main_c() {
    syscall_puts("[UIMANAGER] Starting...\n");
    
    // Get pointers to kernel UI arrays via syscalls
    g_windows = (UIWindow*)syscall_get_windows_ptr();
    g_controls = (UIControl*)syscall_get_controls_ptr();
    g_event_queues = (EventQueue*)syscall_get_events_ptr();
    
    syscall_puts("[UIMANAGER] Got kernel pointers\n");
    
    // Allocate back buffer for double buffering
    g_back_buffer = (uint32_t*)syscall_alloc_memory(SCREEN_WIDTH * SCREEN_HEIGHT * 4);
    
    syscall_puts("[UIMANAGER] Back buffer allocated: 0x");
    // Check if allocation succeeded - fall back to direct rendering if it failed
    if (!g_back_buffer || (unsigned int)g_back_buffer < 0x100000) {
        syscall_puts("FAILED - using direct rendering\n");
        g_back_buffer = g_framebuffer;  // Direct rendering fallback
        use_double_buffer = 0;
    } else {
        syscall_puts("OK\n");
        use_double_buffer = 1;
    }
    
    syscall_puts("[UIMANAGER] Entering main loop\n");
    
    // Initialize state tracking (all controls start as "changed" so they render on first frame)
    for (int i = 0; i < MAX_CONTROLS; i++) {
        g_last_control_state[i] = -1;  // Force render on first frame
    }
    
    // Initialize taskbar
    taskbar_count = 0;
    for (int i = 0; i < MAX_TASKBAR_ITEMS; i++) {
        taskbar_items[i].window_id = 0;
        for (int j = 0; j < 32; j++) {
            taskbar_items[i].title[j] = '\0';
        }
    }
    
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
        
        // Only process cleanup queue if it has items
        if (!cleanup_queue_empty) {
            process_cleanup_queue();
        }
        
        // Poll mouse less frequently to reduce overhead
        if (frame_count % 5 == 0) {
            syscall_poll_mouse();
        }
        
        // Get mouse state
        int mx = syscall_mouse_get_x();
        int my = syscall_mouse_get_y();
        unsigned int buttons = syscall_mouse_get_buttons();
        
        // Separate mouse movement from full scene redraw
        int mouse_moved = (mx != last_mouse_x) || (my != last_mouse_y);
        
        // Redraw on mouse movement or explicit request
        int need_redraw = needs_full_redraw || mouse_moved;
        
        // Always hit test to ensure we have valid data
        int hit_control = hit_test(mx, my);
        
        // Update hover states only when hit control changes
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
            
            // First, check if clicking on taskbar
            int taskbar_item = hit_test_taskbar(mx, my);
            if (taskbar_item >= 0) {
                // Restore window from taskbar
                int window_id = taskbar_items[taskbar_item].window_id;
                
                // Find and restore the window
                for (int j = 0; j < MAX_WINDOWS; j++) {
                    if (g_windows[j].active && g_windows[j].id == window_id) {
                        // Restore window
                        g_windows[j].x = g_windows[j].saved_x;
                        g_windows[j].y = g_windows[j].saved_y;
                        g_windows[j].width = g_windows[j].saved_width;
                        g_windows[j].height = g_windows[j].saved_height;
                        g_windows[j].visible = 1;
                        g_windows[j].state = WINDOW_STATE_NORMAL;
                        
                        // Restore visibility of all child controls
                        for (int k = 0; k < MAX_CONTROLS; k++) {
                            if (g_controls[k].window_id == window_id && g_controls[k].id != 0) {
                                g_controls[k].active = 1;
                            }
                        }
                        
                        // Remove from taskbar
                        for (int t = taskbar_item; t < taskbar_count - 1; t++) {
                            taskbar_items[t] = taskbar_items[t + 1];
                        }
                        taskbar_count--;
                        
                        need_redraw = 1;
                        needs_full_redraw = 1;
                        break;
                    }
                }
                
                // Skip other click handling
                last_buttons = buttons;
                last_mouse_x = mx;
                last_mouse_y = my;
                continue;
            }
            
            // Second, check if clicking on window title bar buttons
            int window_button_clicked = 0;
            for (int j = 0; j < MAX_WINDOWS; j++) {
                if (!g_windows[j].active || !g_windows[j].visible) continue;
                if (g_windows[j].state == WINDOW_STATE_MINIMIZED) continue;  // Skip minimized windows
                
                int btn = hit_test_title_bar_button(&g_windows[j], mx, my);
                if (btn > 0) {
                    window_button_clicked = 1;
                    
                    if (btn == 1) {
                        // Close button - mark window for deferred cleanup
                        int window_id = g_windows[j].id;
                        
                        // Hide window immediately
                        g_windows[j].visible = 0;
                        g_windows[j].state = WINDOW_STATE_PENDING_CLOSE;
                        
                        // Add to cleanup queue
                        if (cleanup_queue_count < CLEANUP_QUEUE_SIZE) {
                            cleanup_queue[cleanup_queue_count++] = window_id;
                            cleanup_queue_empty = 0;  // Mark queue as non-empty
                            syscall_puts("[UIMANAGER] Window marked for cleanup: ");
                            syscall_puts(g_windows[j].title);
                            syscall_puts("\n");
                        }
                        
                        need_redraw = 1;
                        needs_full_redraw = 1;
                        
                    } else if (btn == 2) {
                        // Minimize button - hide window and add to taskbar
                        if (g_windows[j].state != WINDOW_STATE_MINIMIZED) {
                            // Save current position/size
                            g_windows[j].saved_x = g_windows[j].x;
                            g_windows[j].saved_y = g_windows[j].y;
                            g_windows[j].saved_width = g_windows[j].width;
                            g_windows[j].saved_height = g_windows[j].height;
                            
                            // Hide window
                            g_windows[j].visible = 0;
                            g_windows[j].state = WINDOW_STATE_MINIMIZED;
                            
                            // Hide all child controls
                            int window_id = g_windows[j].id;
                            for (int k = 0; k < MAX_CONTROLS; k++) {
                                if (g_controls[k].active && g_controls[k].window_id == window_id) {
                                    g_controls[k].active = 0;  // Deactivate the control
                                    g_controls[k].state = UIMAN_STATE_NORMAL;  // Reset state
                                }
                            }
                            
                            // Add to taskbar if not already there
                            int already_in_taskbar = 0;
                            for (int t = 0; t < taskbar_count; t++) {
                                if (taskbar_items[t].window_id == g_windows[j].id) {
                                    already_in_taskbar = 1;
                                    break;
                                }
                            }
                            
                            if (!already_in_taskbar && taskbar_count < MAX_TASKBAR_ITEMS) {
                                taskbar_items[taskbar_count].window_id = g_windows[j].id;
                                // Copy title (truncate if needed)
                                int k;
                                for (k = 0; k < 31 && g_windows[j].title[k]; k++) {
                                    taskbar_items[taskbar_count].title[k] = g_windows[j].title[k];
                                }
                                taskbar_items[taskbar_count].title[k] = '\0';
                                
                                // Debug: verify title was copied
                                syscall_puts("[UIMAN] Added to taskbar: '");
                                syscall_puts(taskbar_items[taskbar_count].title);
                                syscall_puts("'\n");
                                
                                taskbar_count++;
                            }
                            
                            need_redraw = 1;
                            needs_full_redraw = 1;
                        }
                        
                    } else if (btn == 3) {
                        // Maximize button - toggle between normal (original) and full-screen
                        if (g_windows[j].state != WINDOW_STATE_MAXIMIZED) {
                            // Save current position/size
                            g_windows[j].saved_x = g_windows[j].x;
                            g_windows[j].saved_y = g_windows[j].y;
                            g_windows[j].saved_width = g_windows[j].width;
                            g_windows[j].saved_height = g_windows[j].height;
                            
                            // Maximize to full screen (leave room for taskbar)
                            g_windows[j].x = 0;
                            g_windows[j].y = 0;
                            g_windows[j].width = SCREEN_WIDTH;
                            g_windows[j].height = SCREEN_HEIGHT - TASKBAR_HEIGHT;
                            g_windows[j].state = WINDOW_STATE_MAXIMIZED;
                            need_redraw = 1;
                            needs_full_redraw = 1;
                        } else {
                            // Restore to original size
                            g_windows[j].x = g_windows[j].saved_x;
                            g_windows[j].y = g_windows[j].saved_y;
                            g_windows[j].width = g_windows[j].saved_width;
                            g_windows[j].height = g_windows[j].saved_height;
                            g_windows[j].state = WINDOW_STATE_NORMAL;
                            need_redraw = 1;
                            needs_full_redraw = 1;
                        }
                    }
                    break;  // Only process one window button per click
                }
            }
            
            // If not clicking window buttons, check control clicks
            if (!window_button_clicked && hit_control >= 0 && g_controls[hit_control].active) {
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
        
        // Check if any control state changed (before button processing)
        for (int i = 0; i < MAX_CONTROLS; i++) {
            if (g_controls[i].active && g_controls[i].state != g_last_control_state[i]) {
                need_redraw = 1;
                g_last_control_state[i] = g_controls[i].state;
            }
        }
        
        last_buttons = buttons;
        
        // Only redraw if needed
        if (need_redraw) {
            // Clear back buffer (desktop background)
            draw_to_back_buffer(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x001020);
            
            // First pass: Render normal windows (not maximized)
            for (int j = 0; j < MAX_WINDOWS; j++) {
                if (!g_windows[j].active || !g_windows[j].visible) continue;
                if (g_windows[j].state == WINDOW_STATE_MAXIMIZED) continue;  // Skip maximized, render later
                
                UIWindow *win = &g_windows[j];
                
                // Draw window background (content area below title bar)
                draw_to_back_buffer(win->x, win->y + TITLE_BAR_HEIGHT, 
                                   win->width, win->height - TITLE_BAR_HEIGHT, 0x202020);
                
                // Draw window border (2px)
                draw_to_back_buffer(win->x, win->y, win->width, 2, 0x606060);  // Top
                draw_to_back_buffer(win->x, win->y + win->height - 2, win->width, 2, 0x606060);  // Bottom
                draw_to_back_buffer(win->x, win->y, 2, win->height, 0x606060);  // Left
                draw_to_back_buffer(win->x + win->width - 2, win->y, 2, win->height, 0x606060);  // Right
                
                // Draw title bar with buttons
                draw_window_title_bar(win);
            }
            
            // Render desktop controls (Orbit buttons, etc.)
            for (int i = 0; i < MAX_CONTROLS; i++) {
                if (g_controls[i].active && g_controls[i].window_id == 0) {
                    render_control(i);
                }
            }
            
            // Second pass: Render maximized windows (on top of desktop controls)
            for (int j = 0; j < MAX_WINDOWS; j++) {
                if (!g_windows[j].active || !g_windows[j].visible) continue;
                if (g_windows[j].state != WINDOW_STATE_MAXIMIZED) continue;  // Only maximized
                
                UIWindow *win = &g_windows[j];
                
                // Draw window background (content area below title bar)
                draw_to_back_buffer(win->x, win->y + TITLE_BAR_HEIGHT, 
                                   win->width, win->height - TITLE_BAR_HEIGHT, 0x202020);
                
                // Draw window border (2px)
                draw_to_back_buffer(win->x, win->y, win->width, 2, 0x606060);  // Top
                draw_to_back_buffer(win->x, win->y + win->height - 2, win->width, 2, 0x606060);  // Bottom
                draw_to_back_buffer(win->x, win->y, 2, win->height, 0x606060);  // Left
                draw_to_back_buffer(win->x + win->width - 2, win->y, 2, win->height, 0x606060);  // Right
                
                // Draw title bar with buttons
                draw_window_title_bar(win);
            }
            
            // Render window controls (controls inside windows)
            for (int i = 0; i < MAX_CONTROLS; i++) {
                if (g_controls[i].active && g_controls[i].window_id != 0) {
                    render_control(i);
                }
            }
            
            // Render taskbar
            render_taskbar();
            
            // Draw cursor in back buffer (simple approach)
            if (mx >= 3 && mx < SCREEN_WIDTH - 3 && my >= 3 && my < SCREEN_HEIGHT - 3) {
                // Vertical line
                for (int i = -3; i <= 3; i++) {
                    draw_to_back_buffer(mx, my + i, 1, 1, 0xFFFFFFFF);
                }
                // Horizontal line
                for (int i = -3; i <= 3; i++) {
                    draw_to_back_buffer(mx + i, my, 1, 1, 0xFFFFFFFF);
                }
            }
            
            // Copy back buffer to screen (SWAP)
            copy_buffer_to_screen(g_back_buffer);
            needs_full_redraw = 0;
        }
        
        last_mouse_x = mx;
        last_mouse_y = my;
    }
}
