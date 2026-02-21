/**
 * Mouse Cursor Handler for UIManager
 * Handles both hardware and software cursor rendering
 */

#ifndef MOUSE_CURSOR_H
#define MOUSE_CURSOR_H

#include <stdint.h>

/**
 * Initialize mouse cursor subsystem
 * Checks hardware cursor support and sets up rendering mode
 */
void mouse_cursor_init(void);

/**
 * Render mouse cursor at current position
 * Automatically uses hardware or software cursor based on availability
 * 
 * @param mx Current mouse X position
 * @param my Current mouse Y position
 * @param last_mouse_x Previous mouse X position
 * @param last_mouse_y Previous mouse Y position
 * @param framebuffer Pointer to framebuffer
 * @param back_buffer Pointer to back buffer
 * @param screen_width Screen width in pixels
 * @param screen_height Screen height in pixels
 */
void mouse_cursor_render(int mx, int my, int last_mouse_x, int last_mouse_y,
                        uint32_t* framebuffer, uint32_t* back_buffer,
                        int screen_width, int screen_height);

/**
 * Check if using software cursor fallback
 * @return 1 if using software cursor, 0 if using hardware cursor
 */
int mouse_cursor_using_software(void);

#endif // MOUSE_CURSOR_H
