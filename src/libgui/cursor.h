#ifndef LIBGUI_CURSOR_H
#define LIBGUI_CURSOR_H

/**
 * Cursor/Mouse Pointer Rendering
 */

/**
 * Draw mouse cursor at current mouse position
 */
void gui_draw_cursor(void);

/**
 * Update cursor (erase old, draw new)
 * Call this in your main loop
 */
void gui_update_cursor(void);

#endif // LIBGUI_CURSOR_H
