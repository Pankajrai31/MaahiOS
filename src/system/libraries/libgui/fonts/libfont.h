/**
 * MaahiOS Proportional Font Library — Header
 *
 * Description:
 *   Anti-aliased, proportional font rendering from pre-generated
 *   glyph data (Segoe UI at 12/14/16/18/24 px).  Renders glyphs
 *   with per-pixel alpha blending onto a pixel buffer.
 *
 *   This library operates on raw uint32_t* pixel buffers.
 *   Higher-level wrappers (surface_draw_text, gui_draw_text) are
 *   provided by libwindow and libgui respectively.
 *
 * Usage:
 *   #include "libfont.h"
 *
 *   int adv = font_draw_string(pixels, pitch_px, buf_w, buf_h,
 *                              10, 5, "Hello", 0x001A1A2E, FONT_BODY);
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef LIBFONT_H
#define LIBFONT_H

#include <stdint.h>

/*=============================================================================
 * FONT SIZES — semantic names for the 5 pre-generated sizes
 *===========================================================================*/

typedef enum {
    FONT_SMALL  = 0,    /* 12 px — captions, footnotes              */
    FONT_BODY   = 1,    /* 14 px — default body / UI labels         */
    FONT_H3     = 2,    /* 16 px — sub-heading                      */
    FONT_H2     = 3,    /* 18 px — heading                          */
    FONT_TITLE  = 4,    /* 24 px — window titles, large headings    */
} font_size_t;

#define FONT_SIZE_COUNT  5

/*=============================================================================
 * CORE API
 *===========================================================================*/

/**
 * font_draw_char - Render one glyph with alpha blending
 * @pixels:   Destination pixel buffer (0x00RRGGBB per pixel)
 * @pitch_px: Pixels per row (width of buffer)
 * @buf_w:    Buffer width (for clipping)
 * @buf_h:    Buffer height (for clipping)
 * @x:        X position to start drawing
 * @y:        Y baseline position (top of line, not typographic baseline)
 * @c:        ASCII character to render
 * @fg:       Foreground color (0x00RRGGBB)
 * @size:     Font size enum
 *
 * Returns: Advance width in pixels (how far to move cursor)
 */
int font_draw_char(uint32_t *pixels, int pitch_px, int buf_w, int buf_h,
                   int x, int y, char c, uint32_t fg, font_size_t size);

/**
 * font_draw_string - Render a null-terminated string
 * @pixels:   Destination pixel buffer
 * @pitch_px: Pixels per row
 * @buf_w:    Buffer width (for clipping)
 * @buf_h:    Buffer height (for clipping)
 * @x:        X start position
 * @y:        Y position (top of text line)
 * @str:      Null-terminated string
 * @fg:       Foreground color (0x00RRGGBB)
 * @size:     Font size enum
 *
 * Returns: Total advance width in pixels
 */
int font_draw_string(uint32_t *pixels, int pitch_px, int buf_w, int buf_h,
                     int x, int y, const char *str, uint32_t fg,
                     font_size_t size);

/**
 * font_measure_string - Measure pixel width of a string (no drawing)
 * @str:  Null-terminated string
 * @size: Font size enum
 *
 * Returns: Total advance width in pixels
 */
int font_measure_string(const char *str, font_size_t size);

/**
 * font_line_height - Get line height for a font size
 * @size: Font size enum
 *
 * Returns: Line height in pixels
 */
int font_line_height(font_size_t size);

/**
 * font_ascent - Get ascent (baseline distance from top) for a font size
 * @size: Font size enum
 *
 * Returns: Ascent in pixels
 */
int font_ascent(font_size_t size);

#endif /* LIBFONT_H */
