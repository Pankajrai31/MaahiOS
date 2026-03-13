/**
 * MaahiOS Proportional Font Library — Implementation
 *
 * Description:
 *   Renders anti-aliased proportional glyphs from pre-generated data.
 *   Uses per-pixel alpha blending: reads background pixel from buffer,
 *   blends foreground color using glyph alpha, writes result back.
 *
 *   Generated glyph data is in segoe_font_data.h — produced by
 *   tools/generate_font.py from Segoe UI at 5 sizes.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "libfont.h"
#include "segoe_font_data.h"

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/** Look up the FontSize descriptor for a given size enum. */
static const FontSize *get_font(font_size_t size) {
    int idx = (int)size;
    if (idx < 0 || idx >= FONT_SIZE_COUNT) idx = 1; /* Default to BODY */
    return &font_sizes[idx];
}

/** Look up a glyph in the font.  Returns NULL for out-of-range chars. */
static const FontGlyph *get_glyph(const FontSize *fs, char c) {
    int code = (unsigned char)c;
    if (code < FONT_CHAR_START || code > FONT_CHAR_END) return (const FontGlyph *)0;
    return &fs->glyphs[code - FONT_CHAR_START];
}

/** Alpha-blend a single pixel: out = bg + (fg - bg) * alpha / 255 */
static inline uint32_t blend_pixel(uint32_t bg, uint32_t fg, uint8_t alpha) {
    if (alpha == 0)   return bg;
    if (alpha == 255) return fg;

    int bg_r = (bg >> 16) & 0xFF;
    int bg_g = (bg >>  8) & 0xFF;
    int bg_b =  bg        & 0xFF;

    int fg_r = (fg >> 16) & 0xFF;
    int fg_g = (fg >>  8) & 0xFF;
    int fg_b =  fg        & 0xFF;

    int a = (int)alpha;
    int r = bg_r + (fg_r - bg_r) * a / 255;
    int g = bg_g + (fg_g - bg_g) * a / 255;
    int b = bg_b + (fg_b - bg_b) * a / 255;

    return (uint32_t)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
}

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

int font_draw_char(uint32_t *pixels, int pitch_px, int buf_w, int buf_h,
                   int x, int y, char c, uint32_t fg, font_size_t size) {
    if (!pixels) return 0;

    const FontSize *fs = get_font(size);
    const FontGlyph *g = get_glyph(fs, c);
    if (!g) return fs->size / 3;   /* Fallback advance for unknown chars */

    /* Space character: just advance, nothing to draw */
    if (g->width <= 1 && g->height <= 1) return g->advance;

    const uint8_t *data = &fs->data[g->data_offset];

    /* Glyph drawing position: offset by left bearing and top offset.
     * 'y' is the top of the text line.  The glyph's top offset is
     * relative to the top of the bounding box from the font renderer.
     * We place the glyph at y + g->top (which positions it correctly
     * relative to the ascent). */
    int gx = x + g->left;
    int gy = y + g->top;

    uint32_t fg_rgb = fg & 0x00FFFFFF;

    for (int row = 0; row < g->height; row++) {
        int py = gy + row;
        if (py < 0 || py >= buf_h) {
            data += g->width;  /* Skip this row of glyph data */
            continue;
        }
        for (int col = 0; col < g->width; col++) {
            int px = gx + col;
            uint8_t alpha = *data++;
            if (alpha == 0) continue;         /* Fully transparent */
            if (px < 0 || px >= buf_w) continue;

            uint32_t *dst = &pixels[py * pitch_px + px];
            *dst = blend_pixel(*dst, fg_rgb, alpha);
        }
    }

    return g->advance;
}

int font_draw_string(uint32_t *pixels, int pitch_px, int buf_w, int buf_h,
                     int x, int y, const char *str, uint32_t fg,
                     font_size_t size) {
    if (!str) return 0;

    int start_x = x;
    while (*str) {
        x += font_draw_char(pixels, pitch_px, buf_w, buf_h,
                             x, y, *str, fg, size);
        str++;
    }
    return x - start_x;
}

int font_measure_string(const char *str, font_size_t size) {
    if (!str) return 0;

    const FontSize *fs = get_font(size);
    int total = 0;

    while (*str) {
        const FontGlyph *g = get_glyph(fs, *str);
        if (g) {
            total += g->advance;
        } else {
            total += fs->size / 3;  /* Fallback advance */
        }
        str++;
    }
    return total;
}

int font_line_height(font_size_t size) {
    const FontSize *fs = get_font(size);
    return fs->line_height;
}

int font_ascent(font_size_t size) {
    const FontSize *fs = get_font(size);
    return fs->ascent;
}
