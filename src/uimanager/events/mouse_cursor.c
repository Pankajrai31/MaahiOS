/**
 * Mouse Cursor Handler for UIManager
 * Handles software cursor rendering with background save/restore
 */

#include "mouse_cursor.h"
#include "../../system/libraries/libgui/bmp_renderer.h"
#include "../../system/libraries/libgui/cursor_data.h"

// Cursor state
static uint32_t cursor_save_buffer[20 * 20]; // Save area under cursor
static int last_saved_x = -1;
static int last_saved_y = -1;
static int cursor_initialized = 0;

/**
 * Save area under cursor
 */
static void save_cursor_area(int x, int y, uint32_t* framebuffer, int screen_width, int screen_height) {
    int cursor_size = 20;
    for (int cy = 0; cy < cursor_size && (y + cy) < screen_height; cy++) {
        for (int cx = 0; cx < cursor_size && (x + cx) < screen_width; cx++) {
            if (x + cx >= 0 && y + cy >= 0) {
                cursor_save_buffer[cy * 20 + cx] = framebuffer[(y + cy) * screen_width + (x + cx)];
            }
        }
    }
    last_saved_x = x;
    last_saved_y = y;
}

/**
 * Restore area under cursor
 */
static void restore_cursor_area(int x, int y, uint32_t* framebuffer, int screen_width, int screen_height) {
    if (x < 0 || y < 0) return;
    int cursor_size = 20;
    for (int cy = 0; cy < cursor_size && (y + cy) < screen_height; cy++) {
        for (int cx = 0; cx < cursor_size && (x + cx) < screen_width; cx++) {
            if (x + cx >= 0 && y + cy >= 0) {
                framebuffer[(y + cy) * screen_width + (x + cx)] = cursor_save_buffer[cy * 20 + cx];
            }
        }
    }
}

/**
 * Initialize mouse cursor subsystem
 */
void mouse_cursor_init(void) {
    cursor_initialized = 1;
    last_saved_x = -1;
    last_saved_y = -1;
}

/**
 * Render mouse cursor at current position
 * Handles background save/restore automatically
 */
void mouse_cursor_render(int mx, int my, int last_mouse_x, int last_mouse_y,
                        uint32_t* framebuffer, uint32_t* back_buffer,
                        int screen_width, int screen_height) {
    // If mouse moved, update cursor
    if (mx != last_mouse_x || my != last_mouse_y) {
        // Restore old position if we saved one
        if (last_saved_x >= 0 && last_saved_y >= 0) {
            restore_cursor_area(last_saved_x, last_saved_y, framebuffer, screen_width, screen_height);
        }
        
        // Save new area
        save_cursor_area(mx, my, framebuffer, screen_width, screen_height);
        
        // Draw cursor at new position
        bmp_draw_transparent(mx, my, cursor_bmp_data, framebuffer, screen_width, screen_height);
    }
}
