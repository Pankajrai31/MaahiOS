/**
 * Mouse Cursor Renderer
 * Simple cursor drawing for double-buffered rendering
 */

#include "mouse_renderer.h"
#include "../../../../../Filetypes/bmp/bmp_renderer.h"
#include "../../../../../Filetypes/filedata/cursor_data.h"

/**
 * Render mouse cursor to buffer
 * Simple draw - double buffer prevents artifacts
 */
void render_mouse_cursor(uint32_t* buffer, int screen_width, int screen_height, int x, int y) {
    // Draw cursor using transparent BMP rendering
    bmp_draw_transparent(x, y, cursor_bmp_data, buffer, screen_width, screen_height);
}
