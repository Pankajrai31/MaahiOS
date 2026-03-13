/**
 * MaahiOS GUI Library - Print/Drawing Header
 * 
 * Description:
 *   Provides framebuffer drawing primitives for windows, text,
 *   rectangles, and cursor rendering.
 * 
 *   All functions require gui_init() to have been called first
 *   (auto-called on first use).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef PRINTGUI_H
#define PRINTGUI_H

#include <stdint.h>
#include "../fonts/libfont.h"

/*=============================================================================
 * RECTANGLE / FILL OPERATIONS
 *===========================================================================*/

/**
 * gui_fill_rect - Fill a rectangle with a solid color
 * @x: Left edge (pixels)
 * @y: Top edge (pixels)
 * @w: Width (pixels)
 * @h: Height (pixels)
 * @color: 0x00RRGGBB color value
 */
void gui_fill_rect(int x, int y, int w, int h, uint32_t color);

/**
 * gui_fill_screen - Fill the entire screen with a solid color
 * @color: 0x00RRGGBB color value
 */
void gui_fill_screen(uint32_t color);

/**
 * gui_scroll_rect_up - Scroll a rectangular region up by a given number of pixel rows
 * @x: Left edge (pixels)
 * @y: Top edge (pixels)
 * @w: Width (pixels)
 * @h: Height (pixels)
 * @scroll_pixels: Number of pixel rows to scroll up
 * @bg_color: Color to fill the newly exposed bottom rows (0x00RRGGBB)
 */
void gui_scroll_rect_up(int x, int y, int w, int h,
                        int scroll_pixels, uint32_t bg_color);

/*=============================================================================
 * WINDOW OPERATIONS
 *===========================================================================*/

/**
 * gui_create_window - Draw a window rectangle with a 1px border
 * @x: Left edge (pixels)
 * @y: Top edge (pixels)
 * @w: Width (pixels)
 * @h: Height (pixels)
 * @bg_color: Background color (0x00RRGGBB)
 * @border_color: Border color (0x00RRGGBB)
 */
void gui_create_window(int x, int y, int w, int h,
                       uint32_t bg_color, uint32_t border_color);

/*=============================================================================
 * TEXT RENDERING
 *===========================================================================*/

/**
 * gui_draw_char - Draw a single character at pixel position
 * @px: X position (pixels)
 * @py: Y position (pixels)
 * @c: ASCII character to draw
 * @fg: Foreground color (0x00RRGGBB)
 * @bg: Background color (0x00RRGGBB)
 */
void gui_draw_char(int px, int py, char c, uint32_t fg, uint32_t bg);

/**
 * gui_draw_string - Draw a null-terminated string at pixel position
 * @px: X position (pixels)
 * @py: Y position (pixels)
 * @str: Null-terminated string
 * @fg: Foreground color (0x00RRGGBB)
 * @bg: Background color (0x00RRGGBB)
 */
void gui_draw_string(int px, int py, const char *str,
                     uint32_t fg, uint32_t bg);

/*=============================================================================
 * CURSOR OPERATIONS
 *===========================================================================*/

/**
 * gui_draw_cursor - Draw a text cursor (solid block) at pixel position
 * @px: X position (pixels)
 * @py: Y position (pixels)
 * @color: Cursor color (0x00RRGGBB)
 */
void gui_draw_cursor(int px, int py, uint32_t color);

/**
 * gui_erase_cursor - Erase cursor by filling with background color
 * @px: X position (pixels)
 * @py: Y position (pixels)
 * @bg_color: Background color to restore (0x00RRGGBB)
 */
void gui_erase_cursor(int px, int py, uint32_t bg_color);

/*=============================================================================
 * PROPORTIONAL TEXT (anti-aliased via libfont)
 *===========================================================================*/

/**
 * gui_draw_text - Draw a string using proportional anti-aliased font
 * @x:     Left edge (pixels)
 * @y:     Top edge (pixels, top of text line)
 * @str:   Null-terminated string
 * @fg:    Foreground color (0x00RRGGBB)
 * @size:  Font size (FONT_SMALL .. FONT_TITLE)
 *
 * Renders onto the global framebuffer (for Orbit and other direct-draw apps).
 */
void gui_draw_text(int x, int y, const char *str,
                   uint32_t fg, font_size_t size);

/**
 * gui_measure_text - Get pixel width of a string in proportional font
 * @str:  Null-terminated string
 * @size: Font size
 *
 * Returns: Width in pixels
 */
int gui_measure_text(const char *str, font_size_t size);

/**
 * gui_text_height - Get line height for a font size
 * @size: Font size
 *
 * Returns: Line height in pixels
 */
int gui_text_height(font_size_t size);

/*=============================================================================
 * ICON BLIT (color-key transparency)
 *===========================================================================*/

/**
 * gui_blit_icon - Blit a decoded icon onto the framebuffer
 * @x:         Destination X (pixels)
 * @y:         Destination Y (pixels)
 * @pixels:    Pre-decoded 0x00RRGGBB pixel data (top-down row order)
 * @w:         Icon width (pixels)
 * @h:         Icon height (pixels)
 * @colorkey:  Color treated as transparent (e.g. 0x00000000 = black)
 *
 * Pixels matching colorkey are skipped (not drawn).
 */
void gui_blit_icon(int x, int y, const uint32_t *pixels,
                   int w, int h, uint32_t colorkey);

#endif /* PRINTGUI_H */
