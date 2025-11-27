#include "libgui.h"
#include <stddef.h>

/**
 * Window Management Functions
 */

// Simple memory allocation (static for now)
static GUI_Window window_pool[8];
static int window_count = 0;

GUI_Window* gui_create_window(int x, int y, int width, int height, const char *title, uint32_t bg_color) {
    if (window_count >= 8) return NULL;
    
    GUI_Window *win = &window_pool[window_count++];
    win->x = x;
    win->y = y;
    win->width = width;
    win->height = height;
    win->bg_color = bg_color;
    win->title_color = GUI_COLOR_NAVY;
    win->visible = 1;
    
    // Copy title
    int i;
    for (i = 0; i < 63 && title[i] != '\0'; i++) {
        win->title[i] = title[i];
    }
    win->title[i] = '\0';
    
    return win;
}

void gui_draw_window_title_bar(GUI_Window *win) {
    // Draw title bar background (navy blue like Windows 98)
    gui_draw_filled_rect(win->x, win->y, win->width, 25, 0xFF000080);
    
    // Draw title text (white, transparent background)
    gui_draw_text(win->x + 5, win->y + 5, win->title, 0xFFFFFFFF, 0);
    
    // Draw close button (red X button)
    int close_btn_x = win->x + win->width - 22;
    int close_btn_y = win->y + 3;
    gui_draw_filled_rect(close_btn_x, close_btn_y, 18, 18, 0xFFFF0000);
    gui_draw_text(close_btn_x + 4, close_btn_y + 2, "X", 0xFFFFFFFF, 0);
}

void gui_draw_window(GUI_Window *win) {
    if (!win || !win->visible) return;
    
    // Draw window background
    gui_draw_filled_rect(win->x, win->y + 25, win->width, win->height - 25, win->bg_color);
    
    // Draw title bar
    gui_draw_window_title_bar(win);
    
    // Draw window border (3D effect)
    gui_draw_rect(win->x, win->y, win->width, win->height, GUI_COLOR_BLACK);
    gui_draw_rect(win->x + 1, win->y + 1, win->width - 2, win->height - 2, GUI_COLOR_WHITE);
}

void gui_free_window(GUI_Window *win) {
    // Mark as invisible (simple approach)
    if (win) win->visible = 0;
}
