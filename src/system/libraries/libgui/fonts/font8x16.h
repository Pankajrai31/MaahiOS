/**
 * MaahiOS GUI Library - 8x16 Bitmap Font Header
 * 
 * Description:
 *   Provides the standard 8x16 VGA bitmap font for text rendering.
 *   Covers printable ASCII characters (32-122).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef FONT8X16_H
#define FONT8X16_H

#include <stdint.h>

/*=============================================================================
 * FONT CONSTANTS
 *===========================================================================*/

#define FONT_CHAR_WIDTH     8
#define FONT_CHAR_HEIGHT    16

/*=============================================================================
 * FONT DATA
 *===========================================================================*/

/**
 * font_8x16 - 8×16 VGA bitmap font
 * 
 * Array of 128 glyphs, each 16 bytes (one byte per scanline).
 * Bit 7 = leftmost pixel, bit 0 = rightmost pixel.
 * Covers ASCII 32 (space) through 122 ('z').
 */
extern const uint8_t font_8x16[128][16];

#endif /* FONT8X16_H */
