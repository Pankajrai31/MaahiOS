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

    for (int row = y; row < y + h && row < (int)scr_h; row++) {
        if (row < 0) continue;
        for (int col = x; col < x + w && col < (int)scr_w; col++) {
            if (col < 0) continue;
            fb[row * scr_w + col] = rgb;
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

    /* Move rows up by scroll_pixels within the region */
    int lines_to_copy = h - scroll_pixels;
    for (int row = 0; row < lines_to_copy; row++) {
        int src_y = y + scroll_pixels + row;
        int dst_y = y + row;
        if (src_y < 0 || src_y >= (int)scr_h) continue;
        if (dst_y < 0 || dst_y >= (int)scr_h) continue;

        for (int col = x; col < x + w && col < (int)scr_w; col++) {
            if (col < 0) continue;
            fb[dst_y * scr_w + col] = fb[src_y * scr_w + col];
        }
    }

    /* Clear the newly exposed bottom rows */
    int clear_y = y + h - scroll_pixels;
    if (clear_y < y) clear_y = y;
    for (int row = clear_y; row < y + h && row < (int)scr_h; row++) {
        if (row < 0) continue;
        for (int col = x; col < x + w && col < (int)scr_w; col++) {
            if (col < 0) continue;
            fb[row * scr_w + col] = rgb;
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
