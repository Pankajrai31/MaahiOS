/**
 * Button Renderer - Primary Button Implementation
 * Rounded corners, Segoe font, hover and click effects
 */

#include "button_renderer.h"
#include "../../render/font/font_manager.h"
#include <stdint.h>
#include <stddef.h>

#define COLOR_PRIMARY        0x2B5BB5
#define COLOR_PRIMARY_HOVER  0x1E4A9A
#define COLOR_PRIMARY_ACTIVE 0x174080
#define COLOR_TEXT_WHITE     0xFFFFFF
#define COLOR_BORDER_LIGHT   0x66BBFF
#define COLOR_BORDER_DARK    0x0077CC
#define BORDER_RADIUS 6

static uint32_t *g_fb = NULL;
static int g_screen_width = 0;

void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (!g_fb || x < 0 || y < 0 || x >= g_screen_width || y >= 768) return;
    
    if (alpha == 255) {
        g_fb[y * g_screen_width + x] = color;
    } else if (alpha > 0) {
        uint32_t bg = g_fb[y * g_screen_width + x];
        uint8_t r = (((color >> 16) & 0xFF) * alpha + ((bg >> 16) & 0xFF) * (255 - alpha)) / 255;
        uint8_t g = (((color >> 8) & 0xFF) * alpha + ((bg >> 8) & 0xFF) * (255 - alpha)) / 255;
        uint8_t b = ((color & 0xFF) * alpha + (bg & 0xFF) * (255 - alpha)) / 255;
        g_fb[y * g_screen_width + x] = (r << 16) | (g << 8) | b;
    }
}

static int is_in_rounded_rect(int px, int py, int x, int y, int w, int h, int r) {
    if (px < x + r && py < y + r) {
        int dx = (x + r) - px, dy = (y + r) - py;
        if (dx * dx + dy * dy > r * r) return 0;
    }
    if (px >= x + w - r && py < y + r) {
        int dx = px - (x + w - r - 1), dy = (y + r) - py;
        if (dx * dx + dy * dy > r * r) return 0;
    }
    if (px < x + r && py >= y + h - r) {
        int dx = (x + r) - px, dy = py - (y + h - r - 1);
        if (dx * dx + dy * dy > r * r) return 0;
    }
    if (px >= x + w - r && py >= y + h - r) {
        int dx = px - (x + w - r - 1), dy = py - (y + h - r - 1);
        if (dx * dx + dy * dy > r * r) return 0;
    }
    return 1;
}

static void draw_rounded_rect(uint32_t *fb, int sw, int x, int y, int w, int h, uint32_t c, int r) {
    for (int py = y; py < y + h; py++) {
        for (int px = x; px < x + w; px++) {
            if (px >= 0 && py >= 0 && px < sw && py < 768 && is_in_rounded_rect(px, py, x, y, w, h, r)) {
                fb[py * sw + px] = c;
            }
        }
    }
}

static void draw_rounded_border(uint32_t *fb, int sw, int x, int y, int w, int h, int r) {
    for (int px = x + r; px < x + w - r; px++) {
        if (px >= 0 && px < sw && y >= 0) {
            fb[y * sw + px] = COLOR_BORDER_LIGHT;
            if (y + 1 < 768) fb[(y + 1) * sw + px] = COLOR_BORDER_LIGHT;
        }
        if (px >= 0 && px < sw && y + h - 1 < 768) {
            fb[(y + h - 1) * sw + px] = COLOR_BORDER_DARK;
            if (y + h - 2 >= 0) fb[(y + h - 2) * sw + px] = COLOR_BORDER_DARK;
        }
    }
    for (int py = y + r; py < y + h - r; py++) {
        if (x >= 0 && x < sw && py >= 0 && py < 768) {
            fb[py * sw + x] = COLOR_BORDER_LIGHT;
            if (x + 1 < sw) fb[py * sw + (x + 1)] = COLOR_BORDER_LIGHT;
        }
        if (x + w - 1 < sw && py >= 0 && py < 768) {
            fb[py * sw + (x + w - 1)] = COLOR_BORDER_DARK;
            if (x + w - 2 >= 0) fb[py * sw + (x + w - 2)] = COLOR_BORDER_DARK;
        }
    }
}

void button_render_primary(uint32_t *fb, int sw, int x, int y, int w, int h, const char *l, int hov, int pr) {
    g_fb = fb;
    g_screen_width = sw;
    
    uint32_t bg = COLOR_PRIMARY;
    if (pr) { bg = COLOR_PRIMARY_ACTIVE; y += 2; }
    else if (hov) bg = COLOR_PRIMARY_HOVER;
    
    draw_rounded_rect(fb, sw, x, y, w, h, bg, BORDER_RADIUS);
    draw_rounded_border(fb, sw, x, y, w, h, BORDER_RADIUS);
    
    if (l && l[0]) {
        int tw = font_get_string_width(l, 14);
        font_draw_string(x + (w - tw) / 2, y + (h - 14) / 2, l, COLOR_TEXT_WHITE, 14, fb, sw);
    }
}
