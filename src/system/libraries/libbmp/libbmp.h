/**
 * MaahiOS BMP Image Decoder — Header
 *
 * Description:
 *   Decodes 24-bit (BGR) and 32-bit (BGRA) Windows BMP files into
 *   0x00RRGGBB pixel arrays.  No dynamic allocation — caller provides
 *   the output buffer.  Supports bottom-up and top-down row order.
 *
 *   Used by Orbit to load desktop/taskbar icons from the ISO filesystem.
 *
 * Usage:
 *   uint32_t pixels[32 * 32];
 *   int w, h;
 *   if (libbmp_decode(file_data, file_size, pixels, 32*32, &w, &h) == 0)
 *       gui_blit_icon(x, y, pixels, w, h, 0x00000000);
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef LIBBMP_H
#define LIBBMP_H

#include <stdint.h>

/**
 * libbmp_decode - Decode a BMP file into a pixel buffer
 * @data:       Raw BMP file bytes
 * @data_size:  Number of bytes in data[]
 * @out_pixels: Caller-allocated output buffer (0x00RRGGBB per pixel)
 * @max_pixels: Maximum number of pixels out_pixels can hold
 * @out_w:      [out] Image width in pixels (may be NULL)
 * @out_h:      [out] Image height in pixels (may be NULL)
 *
 * Decodes 24-bit BGR and 32-bit BGRA uncompressed BMPs.
 * Converts bottom-up BMP row order to top-down.
 *
 * Returns: 0 on success, -1 on error (bad format, too large, etc.)
 */
int libbmp_decode(const uint8_t *data, int data_size,
                  uint32_t *out_pixels, int max_pixels,
                  int *out_w, int *out_h);

#endif /* LIBBMP_H */
