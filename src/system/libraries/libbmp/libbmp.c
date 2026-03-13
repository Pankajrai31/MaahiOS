/**
 * MaahiOS BMP Image Decoder — Implementation
 *
 * Description:
 *   Parses Windows BITMAPINFOHEADER BMP files.  Supports:
 *   - 24-bit BGR (3 bytes/pixel)
 *   - 32-bit BGRA (4 bytes/pixel)
 *   - Bottom-up (positive height) and top-down (negative height)
 *   - No compression (BI_RGB = 0)
 *
 *   Output format: 0x00RRGGBB pixels, top-down row order.
 *   No malloc — writes directly to caller's buffer.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "libbmp.h"

/*=============================================================================
 * BMP HEADER STRUCTURES  (manually parsed to avoid struct packing issues)
 *===========================================================================*/

/* Read little-endian 16-bit from byte pointer */
static uint16_t read_u16(const uint8_t *p) {
    return (uint16_t)(p[0] | (p[1] << 8));
}

/* Read little-endian 32-bit from byte pointer */
static uint32_t read_u32(const uint8_t *p) {
    return (uint32_t)(p[0] | (p[1] << 8) | (p[2] << 16) | (p[3] << 24));
}

/* Read little-endian signed 32-bit from byte pointer */
static int32_t read_i32(const uint8_t *p) {
    return (int32_t)read_u32(p);
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

int libbmp_decode(const uint8_t *data, int data_size,
                  uint32_t *out_pixels, int max_pixels,
                  int *out_w, int *out_h) {
    /* Minimum size: 14 (BMP header) + 40 (DIB header) = 54 bytes */
    if (!data || data_size < 54 || !out_pixels) return -1;

    /* BMP file header (14 bytes) */
    if (data[0] != 'B' || data[1] != 'M') return -1;  /* Signature */
    uint32_t pixel_offset = read_u32(&data[10]);

    /* DIB header (BITMAPINFOHEADER — 40 bytes) */
    uint32_t dib_size = read_u32(&data[14]);
    if (dib_size < 40) return -1;  /* Only support BITMAPINFOHEADER+ */

    int32_t  width  = read_i32(&data[18]);
    int32_t  height = read_i32(&data[22]);
    uint16_t bpp    = read_u16(&data[28]);
    uint32_t compr  = read_u32(&data[30]);

    /* Validate */
    if (width <= 0 || width > 1024) return -1;
    if (height == 0 || height > 1024 || height < -1024) return -1;
    if (compr != 0) return -1;  /* Only uncompressed BI_RGB */
    if (bpp != 24 && bpp != 32) return -1;

    /* Determine actual height and row direction */
    int abs_height = height;
    int bottom_up  = 1;      /* BMP default: rows stored bottom-to-top */
    if (height < 0) {
        abs_height = -height;
        bottom_up  = 0;      /* Top-down BMP */
    }

    int total_pixels = width * abs_height;
    if (total_pixels > max_pixels) return -1;

    if (out_w) *out_w = width;
    if (out_h) *out_h = abs_height;

    /* Row stride: BMP rows are padded to 4-byte boundaries */
    int bytes_per_pixel = bpp / 8;
    int raw_row_bytes   = width * bytes_per_pixel;
    int row_stride      = (raw_row_bytes + 3) & ~3;  /* Align to 4 */

    /* Check data bounds */
    int needed = (int)pixel_offset + row_stride * abs_height;
    if (needed > data_size) return -1;

    /* Decode rows */
    const uint8_t *pixel_data = &data[pixel_offset];

    for (int row = 0; row < abs_height; row++) {
        /* Source row index depends on bottom-up vs top-down */
        int src_row = bottom_up ? (abs_height - 1 - row) : row;
        const uint8_t *src = &pixel_data[src_row * row_stride];

        /* Destination row is always top-down in output */
        uint32_t *dst = &out_pixels[row * width];

        if (bpp == 24) {
            /* BGR → 0x00RRGGBB */
            for (int x = 0; x < width; x++) {
                uint8_t b = src[x * 3 + 0];
                uint8_t g = src[x * 3 + 1];
                uint8_t r = src[x * 3 + 2];
                dst[x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        } else {
            /* BGRA → 0x00RRGGBB (alpha ignored for color-key transparency) */
            for (int x = 0; x < width; x++) {
                uint8_t b = src[x * 4 + 0];
                uint8_t g = src[x * 4 + 1];
                uint8_t r = src[x * 4 + 2];
                /* Alpha channel (src[x*4+3]) unused — we use color-key */
                dst[x] = ((uint32_t)r << 16) | ((uint32_t)g << 8) | (uint32_t)b;
            }
        }
    }

    return 0;
}
