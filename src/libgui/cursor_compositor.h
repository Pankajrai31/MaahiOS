/**
 * Cursor Compositor Header
 * Layered cursor rendering with save/restore
 */

#ifndef CURSOR_COMPOSITOR_H
#define CURSOR_COMPOSITOR_H

/**
 * Initialize cursor compositor
 * Call once at orbit startup
 */
void orbit_cursor_init(void);

/**
 * Draw cursor at position (x, y)
 * Automatically saves/restores background
 */
void orbit_draw_cursor(int x, int y);

#endif // CURSOR_COMPOSITOR_H
