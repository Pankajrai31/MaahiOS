/**
 * MaahiOS - Graphics API
 */

#ifndef MAAHI_GRAPHICS_H
#define MAAHI_GRAPHICS_H

/**
 * Draw filled rectangle
 */
void maahi_fill_rect(int x, int y, int width, int height, unsigned int color);

/**
 * Draw rectangle outline
 */
void maahi_draw_rect(int x, int y, int width, int height, unsigned int color);

/**
 * Clear screen to color
 */
void maahi_clear_screen(unsigned int color);

#endif /* MAAHI_GRAPHICS_H */
