/**
 * MaahiOS GUI Library - Print/Drawing Implementation
 * 
 * Description:
 *   Framebuffer drawing primitives: rectangles, windows, text, cursors.
 *   Uses font data from fonts/font8x16.h.
 *   Gets framebuffer pointer from libgui core (auto-init).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "printgui.h"
#include "../fonts/font8x16.h"
#include "../fonts/libfont.h"

/* Forward declarations for libgui core accessors */
extern uint32_t *gui_get_framebuffer(void);
extern uint32_t  gui_get_screen_width(void);
extern uint32_t  gui_get_screen_height(void);

/*=============================================================================
 * RECTANGLE / FILL OPERATIONS
 *===========================================================================*/

void gui_fill_rect(int x, int y, int w, int h, uint32_t color) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb) return;

    uint32_t scr_w = gui_get_screen_width();
    uint32_t scr_h = gui_get_screen_height();
    uint32_t rgb = color & 0x00FFFFFF;

    /* Clip to screen bounds */
    int x0 = x;
    int y0 = y;
    int x1 = x + w;
    int y1 = y + h;
    if (x0 < 0) x0 = 0;
    if (y0 < 0) y0 = 0;
    if (x1 > (int)scr_w) x1 = (int)scr_w;
    if (y1 > (int)scr_h) y1 = (int)scr_h;
    int cw = x1 - x0;
    if (cw <= 0) return;

    /* Fast row-based fill: write one clipped row at a time */
    for (int row = y0; row < y1; row++) {
        uint32_t *dst = &fb[row * scr_w + x0];
        for (int i = 0; i < cw; i++) {
            dst[i] = rgb;
        }
    }
}

void gui_fill_screen(uint32_t color) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb) return;

    uint32_t scr_w = gui_get_screen_width();
    uint32_t scr_h = gui_get_screen_height();
    uint32_t rgb = color & 0x00FFFFFF;
    uint32_t total = scr_w * scr_h;

    for (uint32_t i = 0; i < total; i++) {
        fb[i] = rgb;
    }
}

void gui_scroll_rect_up(int x, int y, int w, int h,
                        int scroll_pixels, uint32_t bg_color) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb || scroll_pixels <= 0) return;

    uint32_t scr_w = gui_get_screen_width();
    uint32_t scr_h = gui_get_screen_height();
    uint32_t rgb = bg_color & 0x00FFFFFF;

    /* Clip horizontal span once */
    int x0 = x;
    int x1 = x + w;
    if (x0 < 0) x0 = 0;
    if (x1 > (int)scr_w) x1 = (int)scr_w;
    int cw = x1 - x0;
    if (cw <= 0) return;

    /* Move rows up by scroll_pixels within the region */
    int lines_to_copy = h - scroll_pixels;
    for (int row = 0; row < lines_to_copy; row++) {
        int src_y = y + scroll_pixels + row;
        int dst_y = y + row;
        if (src_y < 0 || src_y >= (int)scr_h) continue;
        if (dst_y < 0 || dst_y >= (int)scr_h) continue;

        uint32_t *src = &fb[src_y * scr_w + x0];
        uint32_t *dst = &fb[dst_y * scr_w + x0];
        for (int i = 0; i < cw; i++) {
            dst[i] = src[i];
        }
    }

    /* Clear the newly exposed bottom rows */
    int clear_y = y + h - scroll_pixels;
    if (clear_y < y) clear_y = y;
    for (int row = clear_y; row < y + h && row < (int)scr_h; row++) {
        if (row < 0) continue;
        uint32_t *dst = &fb[row * scr_w + x0];
        for (int i = 0; i < cw; i++) {
            dst[i] = rgb;
        }
    }
}

/*=============================================================================
 * WINDOW OPERATIONS
 *===========================================================================*/

void gui_create_window(int x, int y, int w, int h,
                       uint32_t bg_color, uint32_t border_color) {
    /* Draw 1px border */
    gui_fill_rect(x - 1, y - 1, w + 2, h + 2, border_color);
    /* Fill window background */
    gui_fill_rect(x, y, w, h, bg_color);
}

/*=============================================================================
 * TEXT RENDERING
 *===========================================================================*/

void gui_draw_char(int px, int py, char c, uint32_t fg, uint32_t bg) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb) return;

    uint32_t scr_w = gui_get_screen_width();
    uint32_t scr_h = gui_get_screen_height();

    uint8_t uc = (uint8_t)c;
    if (uc >= 128) uc = 0;
    const uint8_t *glyph = font_8x16[uc];

    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        int sy = py + row;
        if (sy < 0 || sy >= (int)scr_h) continue;
        uint8_t line = glyph[row];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            int sx = px + col;
            if (sx < 0 || sx >= (int)scr_w) continue;
            uint32_t clr = (line & (0x80 >> col)) ? fg : bg;
            fb[sy * scr_w + sx] = clr & 0x00FFFFFF;
        }
    }
}

void gui_draw_string(int px, int py, const char *str,
                     uint32_t fg, uint32_t bg) {
    while (*str) {
        gui_draw_char(px, py, *str, fg, bg);
        px += FONT_CHAR_WIDTH;
        str++;
    }
}

/*=============================================================================
 * CURSOR OPERATIONS
 *===========================================================================*/

void gui_draw_cursor(int px, int py, uint32_t color) {
    gui_fill_rect(px, py, FONT_CHAR_WIDTH, FONT_CHAR_HEIGHT, color);
}

void gui_erase_cursor(int px, int py, uint32_t bg_color) {
    gui_fill_rect(px, py, FONT_CHAR_WIDTH, FONT_CHAR_HEIGHT, bg_color);
}

/*=============================================================================
 * PROPORTIONAL TEXT (anti-aliased via libfont)
 *===========================================================================*/

void gui_draw_text(int x, int y, const char *str,
                   uint32_t fg, font_size_t size) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb || !str) return;

    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();

    font_draw_string(fb, scr_w, scr_w, scr_h, x, y, str, fg, size);
}

int gui_measure_text(const char *str, font_size_t size) {
    return font_measure_string(str, size);
}

int gui_text_height(font_size_t size) {
    return font_line_height(size);
}

/*=============================================================================
 * ICON BLIT (color-key transparency)
 *===========================================================================*/

void gui_blit_icon(int x, int y, const uint32_t *pixels,
                   int w, int h, uint32_t colorkey) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb || !pixels) return;

    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    uint32_t key = colorkey & 0x00FFFFFF;

    for (int row = 0; row < h; row++) {
        int sy = y + row;
        if (sy < 0 || sy >= scr_h) continue;
        for (int col = 0; col < w; col++) {
            int sx = x + col;
            if (sx < 0 || sx >= scr_w) continue;
            uint32_t px = pixels[row * w + col] & 0x00FFFFFF;
            if (px != key) {
                fb[sy * scr_w + sx] = px;
            }
        }
    }
}
