/*
 * BGA Hardware Cursor Support - Header
 */

#ifndef BGA_MOUSE_H
#define BGA_MOUSE_H

// Initialize hardware cursor (call once at boot)
void bga_cursor_init(void);

// Set cursor position (call on every mouse movement)
void bga_cursor_set_position(int x, int y);

// Enable/disable hardware cursor
void bga_cursor_enable(int enable);

// Check if hardware cursor is supported
int bga_cursor_is_supported(void);

// Get current cursor position
void bga_cursor_get_position(int *x, int *y);

#endif // BGA_MOUSE_H
