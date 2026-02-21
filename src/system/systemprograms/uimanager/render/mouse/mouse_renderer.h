/**
 * Mouse Cursor Renderer
 * Simple cursor drawing - no save/restore (double buffer handles it)
 */

#ifndef MOUSE_RENDERER_H
#define MOUSE_RENDERER_H

#include <stdint.h>

/**
 * Render mouse cursor to buffer
 * Just draws cursor at position - double buffer prevents artifacts
 * 
 * @param buffer Buffer to draw to (back buffer)
 * @param screen_width Screen width
 * @param screen_height Screen height
 * @param x Mouse X position
 * @param y Mouse Y position
 */
void render_mouse_cursor(uint32_t* buffer, int screen_width, int screen_height, int x, int y);

#endif // MOUSE_RENDERER_H
