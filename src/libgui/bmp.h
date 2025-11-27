#ifndef BMP_H
#define BMP_H

#include <stdint.h>

/**
 * Simple BMP renderer for MaahiOS
 * Draws BMP files via syscall (kernel does the parsing)
 */

/**
 * Draw BMP from embedded data at position (x, y)
 * bmp_data: pointer to BMP file data in memory
 */
void bmp_draw_embedded(int x, int y, const uint8_t *bmp_data);

#endif // BMP_H
