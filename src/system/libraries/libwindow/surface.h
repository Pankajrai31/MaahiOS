/**
 * MaahiOS Window Library - Surface Header
 * 
 * Description:
 *   A surface is a pixel buffer (will be backed by SHM in the future).
 *   Controls draw into a surface, and the Window Manager composites
 *   surfaces onto the framebuffer.
 * 
 *   This is the ONLY place that touches pixels in libwindow.
 *   Controls call surface_fill_rect / surface_draw_char / etc.
 *   They NEVER touch the framebuffer directly.
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef SURFACE_H
#define SURFACE_H

#include <stdint.h>
#include "../libgui/fonts/libfont.h"

/*=============================================================================
 * SURFACE STRUCTURE
 *===========================================================================*/

/**
 * surface_t - A rectangular pixel buffer
 * 
 * @pixels: Pointer to pixel data (uint32_t, 0x00RRGGBB per pixel)
 * @width:  Width in pixels
 * @height: Height in pixels
 * @pitch:  Bytes per row (width * 4 for 32bpp, but kept separate for alignment)
 */
typedef struct surface {
    uint32_t *pixels;
    int       width;
    int       height;
    int       pitch;       /* bytes per row (width * sizeof(uint32_t)) */
} surface_t;

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

/**
 * surface_create - Allocate a new surface
 * @width:  Width in pixels
 * @height: Height in pixels
 * 
 * Returns: Initialized surface, or surface with NULL pixels on failure
 */
surface_t surface_create(int width, int height);

/**
 * surface_destroy - Free a surface's pixel buffer
 * @surf: Surface to destroy
 */
void surface_destroy(surface_t *surf);

/*=============================================================================
 * DRAWING PRIMITIVES
 *===========================================================================*/

/**
 * surface_fill_rect - Fill a rectangle with a solid color
 * @surf:  Target surface
 * @x:     Left edge
 * @y:     Top edge
 * @w:     Width
 * @h:     Height
 * @color: 0x00RRGGBB color
 */
void surface_fill_rect(surface_t *surf, int x, int y, int w, int h,
                       uint32_t color);

/**
 * surface_draw_hline - Draw a horizontal line
 * @surf:  Target surface
 * @x:     Start X
 * @y:     Y position
 * @w:     Width (length)
 * @color: 0x00RRGGBB color
 */
void surface_draw_hline(surface_t *surf, int x, int y, int w, uint32_t color);

/**
 * surface_draw_vline - Draw a vertical line
 * @surf:  Target surface
 * @x:     X position
 * @y:     Start Y
 * @h:     Height (length)
 * @color: 0x00RRGGBB color
 */
void surface_draw_vline(surface_t *surf, int x, int y, int h, uint32_t color);

/**
 * surface_draw_rect - Draw a rectangle outline (border only, no fill)
 * @surf:      Target surface
 * @x:         Left edge
 * @y:         Top edge
 * @w:         Width
 * @h:         Height
 * @color:     Border color
 * @thickness: Border width in pixels
 */
void surface_draw_rect(surface_t *surf, int x, int y, int w, int h,
                       uint32_t color, int thickness);

/**
 * surface_draw_char - Draw a single 8x16 character
 * @surf: Target surface
 * @px:   X position (pixels)
 * @py:   Y position (pixels)
 * @c:    ASCII character
 * @fg:   Foreground color
 * @bg:   Background color
 */
void surface_draw_char(surface_t *surf, int px, int py, char c,
                       uint32_t fg, uint32_t bg);

/**
 * surface_draw_char_transparent - Draw a char without filling background
 * @surf: Target surface
 * @px:   X position (pixels)
 * @py:   Y position (pixels)
 * @c:    ASCII character
 * @fg:   Foreground color (only foreground pixels drawn)
 */
void surface_draw_char_transparent(surface_t *surf, int px, int py, char c,
                                   uint32_t fg);

/**
 * surface_draw_string - Draw a null-terminated string
 * @surf: Target surface
 * @px:   X position (pixels)
 * @py:   Y position (pixels)
 * @str:  Null-terminated string
 * @fg:   Foreground color
 * @bg:   Background color
 */
void surface_draw_string(surface_t *surf, int px, int py, const char *str,
                         uint32_t fg, uint32_t bg);

/**
 * surface_draw_string_transparent - Draw string without filling background
 * @surf: Target surface
 * @px:   X position (pixels)
 * @py:   Y position (pixels)
 * @str:  Null-terminated string
 * @fg:   Foreground color
 */
void surface_draw_string_transparent(surface_t *surf, int px, int py,
                                     const char *str, uint32_t fg);

/**
 * surface_measure_string - Get pixel width of a string
 * @str: Null-terminated string
 * 
 * Returns: Width in pixels (strlen * FONT_CHAR_WIDTH)
 */
int surface_measure_string(const char *str);

/**
 * surface_fill_circle - Fill a circle with a solid color
 * @surf:  Target surface
 * @cx:    Center X
 * @cy:    Center Y
 * @r:     Radius
 * @color: Fill color
 */
void surface_fill_circle(surface_t *surf, int cx, int cy, int r,
                         uint32_t color);

/**
 * surface_blit - Copy a rectangular region from one surface to another
 * @dst:    Destination surface
 * @dx, dy: Destination position
 * @src:    Source surface
 * @sx, sy: Source position
 * @w, h:   Region size
 */
void surface_blit(surface_t *dst, int dx, int dy,
                  const surface_t *src, int sx, int sy, int w, int h);

/*=============================================================================
 * PROPORTIONAL TEXT (anti-aliased via libfont)
 *===========================================================================*/

/**
 * surface_draw_text - Draw a string using proportional anti-aliased font
 * @surf:  Target surface
 * @x:     Left edge (pixels)
 * @y:     Top edge (pixels)
 * @str:   Null-terminated string
 * @size:  Font size (FONT_SMALL .. FONT_TITLE)
 * @color: Text color (0x00RRGGBB)
 *
 * Alpha-blends text onto existing surface content.
 */
void surface_draw_text(surface_t *surf, int x, int y, const char *str,
                       font_size_t size, uint32_t color);

/**
 * surface_measure_text - Get pixel width of a string in proportional font
 * @str:  Null-terminated string
 * @size: Font size
 *
 * Returns: Width in pixels
 */
int surface_measure_text(const char *str, font_size_t size);

/**
 * surface_text_height - Get line height for a font size
 * @size: Font size
 *
 * Returns: Line height in pixels
 */
int surface_text_height(font_size_t size);

/**
 * surface_blit_colorkey - Copy pixels, skipping a transparent color key
 * @dst:      Destination surface
 * @dx, dy:   Destination position
 * @src:      Source surface
 * @sx, sy:   Source position
 * @w, h:     Region size
 * @colorkey: Color to treat as transparent (pixels matching this are skipped)
 */
void surface_blit_colorkey(surface_t *dst, int dx, int dy,
                           const surface_t *src, int sx, int sy,
                           int w, int h, uint32_t colorkey);

#endif /* SURFACE_H */
