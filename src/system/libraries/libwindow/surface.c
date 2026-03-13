/**
 * MaahiOS Window Library - Surface Implementation
 * 
 * Description:
 *   Drawing primitives that operate on a surface pixel buffer.
 *   This is the rendering backend for all libwindow controls.
 * 
 *   Currently uses malloc for pixel buffers.
 *   Will migrate to SHM allocation when WM Executive is ready.
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "surface.h"
#include "../libgui/fonts/font8x16.h"
#include "../libgui/fonts/libfont.h"

/* Forward declaration — user-space malloc/free from libmem or syscalls */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

/** Clamp value to [lo, hi) */
static inline int clamp(int val, int lo, int hi) {
    if (val < lo) return lo;
    if (val >= hi) return hi;
    return val;
}

/** Get a pixel pointer at (x, y) in the surface. NULL if out of bounds. */
static inline uint32_t *pixel_at(surface_t *surf, int x, int y) {
    if (x < 0 || x >= surf->width || y < 0 || y >= surf->height)
        return (uint32_t *)0;
    return &surf->pixels[y * surf->width + x];
}

/*=============================================================================
 * LIFECYCLE
 *===========================================================================*/

surface_t surface_create(int width, int height) {
    surface_t surf;
    surf.width  = width;
    surf.height = height;
    surf.pitch  = width * (int)sizeof(uint32_t);
    surf.pixels = (uint32_t *)malloc((unsigned int)(width * height) * sizeof(uint32_t));
    if (surf.pixels) {
        /* Clear to white */
        for (int i = 0; i < width * height; i++) {
            surf.pixels[i] = 0x00FFFFFF;
        }
    }
    return surf;
}

void surface_destroy(surface_t *surf) {
    if (surf && surf->pixels) {
        free(surf->pixels);
        surf->pixels = (uint32_t *)0;
    }
}

/*=============================================================================
 * RECTANGLE OPERATIONS
 *===========================================================================*/

void surface_fill_rect(surface_t *surf, int x, int y, int w, int h,
                       uint32_t color) {
    if (!surf || !surf->pixels) return;

    uint32_t rgb = color & 0x00FFFFFF;

    /* Clip to surface bounds */
    int x0 = clamp(x, 0, surf->width);
    int y0 = clamp(y, 0, surf->height);
    int x1 = clamp(x + w, 0, surf->width);
    int y1 = clamp(y + h, 0, surf->height);

    for (int row = y0; row < y1; row++) {
        uint32_t *rowp = &surf->pixels[row * surf->width];
        for (int col = x0; col < x1; col++) {
            rowp[col] = rgb;
        }
    }
}

void surface_draw_hline(surface_t *surf, int x, int y, int w,
                        uint32_t color) {
    surface_fill_rect(surf, x, y, w, 1, color);
}

void surface_draw_vline(surface_t *surf, int x, int y, int h,
                        uint32_t color) {
    surface_fill_rect(surf, x, y, 1, h, color);
}

void surface_draw_rect(surface_t *surf, int x, int y, int w, int h,
                       uint32_t color, int thickness) {
    if (!surf || !surf->pixels || thickness <= 0) return;

    /* Top edge */
    surface_fill_rect(surf, x, y, w, thickness, color);
    /* Bottom edge */
    surface_fill_rect(surf, x, y + h - thickness, w, thickness, color);
    /* Left edge */
    surface_fill_rect(surf, x, y + thickness, thickness, h - 2 * thickness, color);
    /* Right edge */
    surface_fill_rect(surf, x + w - thickness, y + thickness, thickness,
                      h - 2 * thickness, color);
}

/*=============================================================================
 * TEXT RENDERING
 *===========================================================================*/

void surface_draw_char(surface_t *surf, int px, int py, char c,
                       uint32_t fg, uint32_t bg) {
    if (!surf || !surf->pixels) return;

    uint8_t uc = (uint8_t)c;
    if (uc >= 128) uc = 0;
    const uint8_t *glyph = font_8x16[uc];

    uint32_t fg_rgb = fg & 0x00FFFFFF;
    uint32_t bg_rgb = bg & 0x00FFFFFF;

    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        int sy = py + row;
        if (sy < 0 || sy >= surf->height) continue;
        uint8_t line = glyph[row];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            int sx = px + col;
            if (sx < 0 || sx >= surf->width) continue;
            uint32_t clr = (line & (0x80 >> col)) ? fg_rgb : bg_rgb;
            surf->pixels[sy * surf->width + sx] = clr;
        }
    }
}

void surface_draw_char_transparent(surface_t *surf, int px, int py, char c,
                                   uint32_t fg) {
    if (!surf || !surf->pixels) return;

    uint8_t uc = (uint8_t)c;
    if (uc >= 128) uc = 0;
    const uint8_t *glyph = font_8x16[uc];

    uint32_t fg_rgb = fg & 0x00FFFFFF;

    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        int sy = py + row;
        if (sy < 0 || sy >= surf->height) continue;
        uint8_t line = glyph[row];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            int sx = px + col;
            if (sx < 0 || sx >= surf->width) continue;
            if (line & (0x80 >> col)) {
                surf->pixels[sy * surf->width + sx] = fg_rgb;
            }
            /* Skip background pixels — keep existing surface content */
        }
    }
}

void surface_draw_string(surface_t *surf, int px, int py, const char *str,
                         uint32_t fg, uint32_t bg) {
    if (!str) return;
    while (*str) {
        surface_draw_char(surf, px, py, *str, fg, bg);
        px += FONT_CHAR_WIDTH;
        str++;
    }
}

void surface_draw_string_transparent(surface_t *surf, int px, int py,
                                     const char *str, uint32_t fg) {
    if (!str) return;
    while (*str) {
        surface_draw_char_transparent(surf, px, py, *str, fg);
        px += FONT_CHAR_WIDTH;
        str++;
    }
}

int surface_measure_string(const char *str) {
    if (!str) return 0;
    int len = 0;
    while (str[len]) len++;
    return len * FONT_CHAR_WIDTH;
}

/*=============================================================================
 * CIRCLE (for window traffic-light buttons)
 *===========================================================================*/

void surface_fill_circle(surface_t *surf, int cx, int cy, int r,
                         uint32_t color) {
    if (!surf || !surf->pixels || r <= 0) return;

    uint32_t rgb = color & 0x00FFFFFF;
    int r2 = r * r;

    /* Scan the bounding box, test each pixel with r^2 */
    int y0 = clamp(cy - r, 0, surf->height);
    int y1 = clamp(cy + r + 1, 0, surf->height);
    int x0 = clamp(cx - r, 0, surf->width);
    int x1 = clamp(cx + r + 1, 0, surf->width);

    for (int y = y0; y < y1; y++) {
        int dy = y - cy;
        for (int x = x0; x < x1; x++) {
            int dx = x - cx;
            if (dx * dx + dy * dy <= r2) {
                surf->pixels[y * surf->width + x] = rgb;
            }
        }
    }
}

/*=============================================================================
 * BLIT (surface-to-surface copy)
 *===========================================================================*/

void surface_blit(surface_t *dst, int dx, int dy,
                  const surface_t *src, int sx, int sy, int w, int h) {
    if (!dst || !dst->pixels || !src || !src->pixels) return;

    for (int row = 0; row < h; row++) {
        int src_y = sy + row;
        int dst_y = dy + row;
        if (src_y < 0 || src_y >= src->height) continue;
        if (dst_y < 0 || dst_y >= dst->height) continue;

        for (int col = 0; col < w; col++) {
            int src_x = sx + col;
            int dst_x = dx + col;
            if (src_x < 0 || src_x >= src->width) continue;
            if (dst_x < 0 || dst_x >= dst->width) continue;

            dst->pixels[dst_y * dst->width + dst_x] =
                src->pixels[src_y * src->width + src_x];
        }
    }
}

/*=============================================================================
 * PROPORTIONAL TEXT (anti-aliased via libfont)
 *===========================================================================*/

void surface_draw_text(surface_t *surf, int x, int y, const char *str,
                       font_size_t size, uint32_t color) {
    if (!surf || !surf->pixels || !str) return;
    font_draw_string(surf->pixels, surf->width, surf->width, surf->height,
                     x, y, str, color, size);
}

int surface_measure_text(const char *str, font_size_t size) {
    return font_measure_string(str, size);
}

int surface_text_height(font_size_t size) {
    return font_line_height(size);
}

/*=============================================================================
 * BLIT WITH COLOR KEY (transparency)
 *===========================================================================*/

void surface_blit_colorkey(surface_t *dst, int dx, int dy,
                           const surface_t *src, int sx, int sy,
                           int w, int h, uint32_t colorkey) {
    if (!dst || !dst->pixels || !src || !src->pixels) return;

    uint32_t key = colorkey & 0x00FFFFFF;

    for (int row = 0; row < h; row++) {
        int src_y = sy + row;
        int dst_y = dy + row;
        if (src_y < 0 || src_y >= src->height) continue;
        if (dst_y < 0 || dst_y >= dst->height) continue;

        for (int col = 0; col < w; col++) {
            int src_x = sx + col;
            int dst_x = dx + col;
            if (src_x < 0 || src_x >= src->width) continue;
            if (dst_x < 0 || dst_x >= dst->width) continue;

            uint32_t px = src->pixels[src_y * src->width + src_x] & 0x00FFFFFF;
            if (px != key) {
                dst->pixels[dst_y * dst->width + dst_x] = px;
            }
        }
    }
}
