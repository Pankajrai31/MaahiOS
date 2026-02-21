/*
 * Double Buffer Manager for UIManager
 * Provides flicker-free rendering by using an off-screen buffer
 */

#ifndef DOUBLE_BUFFER_H
#define DOUBLE_BUFFER_H

#include <stdint.h>

/**
 * Initialize double buffering system
 * @param width Screen width
 * @param height Screen height
 * @param framebuffer Physical framebuffer address
 * @return 0 on success, -1 on failure
 */
int double_buffer_init(int width, int height, uint32_t *framebuffer);

/**
 * Get pointer to back buffer for rendering
 * @return Pointer to back buffer
 */
uint32_t* double_buffer_get_back(void);

/**
 * Get pointer to front buffer (framebuffer)
 * @return Pointer to front buffer
 */
uint32_t* double_buffer_get_front(void);

/**
 * Swap buffers - copy back buffer to front buffer
 * This should be called once per frame after all rendering is complete
 */
void double_buffer_swap(void);

/**
 * Clear back buffer to specified color
 * @param color RGB color (0xRRGGBB)
 */
void double_buffer_clear(uint32_t color);

/**
 * Get screen dimensions
 */
int double_buffer_get_width(void);
int double_buffer_get_height(void);

#endif /* DOUBLE_BUFFER_H */
