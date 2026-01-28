#ifndef BMP_RENDERER_H
#define BMP_RENDERER_H

#include <stdint.h>

/**
 * BMP Renderer API
 * Converts 24-bit BGR BMP images to 32-bit RGB framebuffer
 */

/**
 * Draw BMP image to back buffer
 * 
 * @param x X position on screen
 * @param y Y position on screen  
 * @param bmp_data Pointer to BMP file data (starts with 'BM' signature)
 * @param back_buffer Pointer to 32-bit RGB framebuffer
 * @param screen_width Screen width in pixels
 * @param screen_height Screen height in pixels
 */
void bmp_draw_to_buffer(int x, int y, const unsigned char *bmp_data,
                        uint32_t *back_buffer, int screen_width, int screen_height);

/**
 * Draw BMP with transparency (magenta 0xFF00FF = transparent)
 * Useful for cursors and icons
 */
void bmp_draw_transparent(int x, int y, const unsigned char *bmp_data,
                          uint32_t *back_buffer, int screen_width, int screen_height);

/**
 * Get BMP dimensions without drawing
 * @return 1 if valid BMP, 0 otherwise
 */
int bmp_get_dimensions(const unsigned char *bmp_data, int *width, int *height);

#endif // BMP_RENDERER_H
