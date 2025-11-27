/**
 * Mouse Cursor Rendering for LibGUI
 */

#include "cursor.h"
#include "libgui.h"
#include "../syscalls/user_syscalls.h"

// Previous cursor position for erasing
static int prev_x = -1;
static int prev_y = -1;

// Cursor size
#define CURSOR_WIDTH 10
#define CURSOR_HEIGHT 16

// Simple arrow cursor bitmap (10x16)
static const unsigned char cursor_bitmap[16] = {
    0x80, // 1.......
    0xC0, // 11......
    0xE0, // 111.....
    0xF0, // 1111....
    0xF8, // 11111...
    0xFC, // 111111..
    0xFE, // 1111111.
    0xFF, // 11111111
    0xF8, // 11111...
    0xD8, // 11.11...
    0x8C, // 1...11..
    0x0C, // ....11..
    0x06, // .....11.
    0x06, // .....11.
    0x03, // ......11
    0x00, // ........
};

/**
 * Draw mouse cursor at current position (SIMPLIFIED - just a square for now)
 */
void gui_draw_cursor(void) {
    int x = syscall_mouse_get_x();
    int y = syscall_mouse_get_y();
    
    // Draw simple 10x16 white rectangle with black border
    // Black border
    syscall_fill_rect(x, y, 10, 16, 0x000000);
    // White fill
    syscall_fill_rect(x + 1, y + 1, 8, 14, 0xFFFFFF);
    
    prev_x = x;
    prev_y = y;
}

/**
 * Update cursor (no erasing - just redraw at new position)
 * Since we're redrawing the whole screen or background, 
 * we don't need to manually erase the old cursor
 */
void gui_update_cursor(void) {
    gui_draw_cursor();
}
