/**
 * Button Manager - Handles all button operations
 * Reads button data from cells and renders them
 */

#ifndef BUTTON_MANAGER_H
#define BUTTON_MANAGER_H

#include <stdint.h>

/**
 * Render all buttons from cell registry
 * 
 * @param framebuffer Pointer to framebuffer to render to
 * @param screen_width Screen width in pixels
 * @param mouse_x Current mouse X position
 * @param mouse_y Current mouse Y position
 * @param mouse_pressed Is mouse button pressed
 */
void button_manager_render_all(uint32_t *framebuffer, int screen_width,
                                int mouse_x, int mouse_y, int mouse_pressed);

#endif /* BUTTON_MANAGER_H */
