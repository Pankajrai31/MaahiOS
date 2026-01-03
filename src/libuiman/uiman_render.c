/**
 * UIMan Rendering Functions
 * Used by UIManager process to draw controls
 */

#include "uiman_internal.h"
#include "../syscalls/user_syscalls.h"
#include "../libgui/libgui.h"

// Forward declarations
static int string_length(const char *str);

/**
 * Find window by ID
 */
static UIWindow* find_window(int window_id) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].id == window_id) {
            return &g_windows[i];
        }
    }
    return 0;
}

/**
 * Render a button control
 */
static void render_button(UIControl *ctrl) {
    UIWindow *win = find_window(ctrl->window_id);
    if (!win) return;
    
    // Calculate absolute position
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + ctrl->y;
    
    // Choose color based on state
    unsigned int bg_color, text_color;
    switch (ctrl->state) {
        case UIMAN_STATE_HOVER:
            bg_color = 0x555555;
            text_color = 0xFFFFFF;
            break;
        case UIMAN_STATE_PRESSED:
            bg_color = 0x333333;
            text_color = 0xFFFFFF;
            break;
        case UIMAN_STATE_DISABLED:
            bg_color = 0x222222;
            text_color = 0x888888;
            break;
        default: // NORMAL
            bg_color = 0x404040;
            text_color = 0xFFFFFF;
            break;
    }
    
    // Draw button background
    syscall_fill_rect(abs_x, abs_y, ctrl->width, ctrl->height, bg_color);
    
    // Draw border
    syscall_fill_rect(abs_x, abs_y, ctrl->width, 2, 0x606060); // Top
    syscall_fill_rect(abs_x, abs_y + ctrl->height - 2, ctrl->width, 2, 0x202020); // Bottom
    syscall_fill_rect(abs_x, abs_y, 2, ctrl->height, 0x606060); // Left
    syscall_fill_rect(abs_x + ctrl->width - 2, abs_y, 2, ctrl->height, 0x202020); // Right
    
    // Draw text centered
    int text_x = abs_x + (ctrl->width / 2) - (string_length(ctrl->text) * 8 / 2);
    int text_y = abs_y + (ctrl->height / 2) - 8;
    gui_draw_text(text_x, text_y, ctrl->text, text_color, 0);
}

/**
 * Render a label control
 */
static void render_label(UIControl *ctrl) {
    UIWindow *win = find_window(ctrl->window_id);
    if (!win) return;
    
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + ctrl->y;
    
    unsigned int text_color = (ctrl->state == UIMAN_STATE_DISABLED) ? 0x888888 : 0xFFFFFF;
    gui_draw_text(abs_x, abs_y, ctrl->text, text_color, 0);
}

/**
 * Render a textbox control
 */
static void render_textbox(UIControl *ctrl) {
    UIWindow *win = find_window(ctrl->window_id);
    if (!win) return;
    
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + ctrl->y;
    
    // Draw background
    unsigned int bg_color = (ctrl->state == UIMAN_STATE_DISABLED) ? 0x222222 : 0x303030;
    syscall_fill_rect(abs_x, abs_y, ctrl->width, ctrl->height, bg_color);
    
    // Draw border
    syscall_fill_rect(abs_x, abs_y, ctrl->width, 1, 0x505050);
    syscall_fill_rect(abs_x, abs_y + ctrl->height - 1, ctrl->width, 1, 0x505050);
    syscall_fill_rect(abs_x, abs_y, 1, ctrl->height, 0x505050);
    syscall_fill_rect(abs_x + ctrl->width - 1, abs_y, 1, ctrl->height, 0x505050);
    
    // Draw text
    gui_draw_text(abs_x + 4, abs_y + 4, ctrl->text, 0xFFFFFF, 0);
}

/**
 * Render a table control
 */
static void render_table(UIControl *ctrl) {
    UIWindow *win = find_window(ctrl->window_id);
    if (!win) return;
    
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + ctrl->y;
    
    // Draw background
    syscall_fill_rect(abs_x, abs_y, ctrl->width, ctrl->height, 0x303030);
    
    // Draw grid lines
    int cell_width = ctrl->width / ctrl->data.table.cols;
    int cell_height = ctrl->height / ctrl->data.table.rows;
    
    // Vertical lines
    for (int i = 0; i <= ctrl->data.table.cols; i++) {
        int line_x = abs_x + (i * cell_width);
        syscall_fill_rect(line_x, abs_y, 1, ctrl->height, 0x505050);
    }
    
    // Horizontal lines
    for (int i = 0; i <= ctrl->data.table.rows; i++) {
        int line_y = abs_y + (i * cell_height);
        syscall_fill_rect(abs_x, line_y, ctrl->width, 1, 0x505050);
    }
}

/**
 * Render a radio button control
 */
static void render_radio(UIControl *ctrl) {
    UIWindow *win = find_window(ctrl->window_id);
    if (!win) return;
    
    int abs_x = win->x + ctrl->x;
    int abs_y = win->y + ctrl->y;
    
    // Draw circle (simplified as square for now)
    unsigned int color = (ctrl->state == UIMAN_STATE_PRESSED) ? 0x00FF00 : 0x505050;
    syscall_fill_rect(abs_x, abs_y, 20, 20, color);
    
    // Draw text next to radio
    gui_draw_text(abs_x + 25, abs_y + 4, ctrl->text, 0xFFFFFF, 0);
}

/**
 * Render all windows and controls
 */
void uiman_render_all(void) {
    // Render all active windows (in z-order)
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_windows[i].active || !g_windows[i].visible) continue;
        
        UIWindow *win = &g_windows[i];
        
        // Draw window background (if not fullscreen desktop)
        if (win->parent_id != 0) {
            syscall_fill_rect(win->x, win->y, win->width, win->height, 0x202020);
        }
    }
    
    // Render all active controls
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (!g_controls[i].active) continue;
        
        UIControl *ctrl = &g_controls[i];
        UIWindow *win = find_window(ctrl->window_id);
        if (!win) continue;
        
        // DEBUG: Draw white dot at control position
        syscall_fill_rect(win->x + ctrl->x, win->y + ctrl->y, 4, 4, 0xFFFFFF);
        
        switch (ctrl->type) {
            case UIMAN_CONTROL_BUTTON:
                render_button(ctrl);
                break;
            case UIMAN_CONTROL_LABEL:
                render_label(ctrl);
                break;
            case UIMAN_CONTROL_TEXTBOX:
                render_textbox(ctrl);
                break;
            case UIMAN_CONTROL_TABLE:
                render_table(ctrl);
                break;
            case UIMAN_CONTROL_RADIO:
                render_radio(ctrl);
                break;
        }
    }
}

/**
 * Hit test - find control at screen coordinates
 */
int uiman_hit_test(int screen_x, int screen_y) {
    // Search in reverse order (topmost first)
    for (int i = MAX_CONTROLS - 1; i >= 0; i--) {
        if (!g_controls[i].active) continue;
        
        UIControl *ctrl = &g_controls[i];
        UIWindow *win = find_window(ctrl->window_id);
        if (!win || !win->visible) continue;
        
        // Calculate absolute position
        int abs_x = win->x + ctrl->x;
        int abs_y = win->y + ctrl->y;
        
        // Check if point is inside
        if (screen_x >= abs_x && screen_x < abs_x + ctrl->width &&
            screen_y >= abs_y && screen_y < abs_y + ctrl->height) {
            return i;  // Return slot index
        }
    }
    
    return -1;  // No hit
}

/**
 * Helper: Calculate string length
 */
int string_length(const char *str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}
