/**
 * Button Renderer - Primary Button Style
 * Rounded corners, Segoe font, hover and click effects
 */

#ifndef BUTTON_RENDERER_H
#define BUTTON_RENDERER_H

#include <stdint.h>

/**
 * Render primary button
 * 
 * @param framebuffer Pointer to framebuffer
 * @param screen_width Screen width in pixels
 * @param x Button X position
 * @param y Button Y position
 * @param width Button width
 * @param height Button height
 * @param label Button label text
 * @param is_hovered Is mouse hovering over button
 * @param is_pressed Is button being clicked
 */
void button_render_primary(uint32_t *framebuffer, int screen_width,
                           int x, int y, int width, int height,
                           const char *label, int is_hovered, int is_pressed);

#endif /* BUTTON_RENDERER_H */
