/**
 * Font Manager - Anti-aliased Font Rendering (14px only)
 * Uses pre-rendered Segoe UI 14px font data from segoe_font_data.h
 */

#include "font_manager.h"
#include "segoe_font_data.h"

void font_init(void) {
    /* Nothing to initialize - all data is static */
}

int font_get_line_height(int size) {
    return FONT_LINE_HEIGHT;
}

int font_draw_char(int x, int y, char c, uint32_t color, int size, uint32_t *fb, int sw) {
    /* Check character range */
    if (c < FONT_CHAR_START || c > FONT_CHAR_END) {
        return font_glyphs['I' - FONT_CHAR_START].advance;
    }
    
    int glyph_idx = c - FONT_CHAR_START;
    const FontGlyph* glyph = &font_glyphs[glyph_idx];
    
    /* Handle space */
    if (c == ' ') {
        return glyph->advance;
    }
    
    const uint8_t* pixels = font_data + glyph->data_offset;
    int draw_x = x + glyph->left;
    int draw_y = y + glyph->top;
    
    /* Draw with alpha blending */
    for (int row = 0; row < glyph->height; row++) {
        for (int col = 0; col < glyph->width; col++) {
            uint8_t alpha = pixels[row * glyph->width + col];
            if (alpha > 0 && draw_x + col >= 0 && draw_x + col < sw && draw_y + row >= 0 && draw_y + row < 768) {
                uint32_t dst = fb[(draw_y + row) * sw + (draw_x + col)];
                uint32_t r = (color >> 16) & 0xFF;
                uint32_t g = (color >> 8) & 0xFF;
                uint32_t b = color & 0xFF;
                uint32_t dr = (dst >> 16) & 0xFF;
                uint32_t dg = (dst >> 8) & 0xFF;
                uint32_t db = dst & 0xFF;
                r = (r * alpha + dr * (255 - alpha)) / 255;
                g = (g * alpha + dg * (255 - alpha)) / 255;
                b = (b * alpha + db * (255 - alpha)) / 255;
                fb[(draw_y + row) * sw + (draw_x + col)] = (r << 16) | (g << 8) | b;
            }
        }
    }
    
    return glyph->advance;
}

int font_draw_string(int x, int y, const char* text, uint32_t color, int size, uint32_t *fb, int sw) {
    int cursor = x;
    
    while (*text) {
        cursor += font_draw_char(cursor, y, *text, color, size, fb, sw);
        text++;
    }
    
    return cursor - x;
}

int font_get_char_width(char c, int size) {
    if (c < FONT_CHAR_START || c > FONT_CHAR_END) {
        return font_glyphs['I' - FONT_CHAR_START].advance;
    }
    
    return font_glyphs[c - FONT_CHAR_START].advance;
}

int font_get_string_width(const char* text, int size) {
    int width = 0;
    
    while (*text) {
        width += font_get_char_width(*text, size);
        text++;
    }
    
    return width;
}
