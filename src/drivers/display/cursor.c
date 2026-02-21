/**
 * MaahiOS Hardware Cursor Driver
 * Software cursor implementation (BGA hardware cursor not reliable)
 */

#include "display.h"
#include <stdint.h>

/* ============================================
 * Cursor State
 * ============================================ */
static int cursor_enabled = 0;
static int cursor_x = 0;
static int cursor_y = 0;
static int cursor_visible = 0;

/* Saved background under cursor */
#define CURSOR_SIZE 16
static uint32_t saved_background[CURSOR_SIZE * CURSOR_SIZE];

/* Simple arrow cursor (16x16) */
static const uint8_t cursor_mask[CURSOR_SIZE] = {
    0b10000000,
    0b11000000,
    0b11100000,
    0b11110000,
    0b11111000,
    0b11111100,
    0b11111110,
    0b11111111,
    0b11111100,
    0b11111000,
    0b11011000,
    0b10001100,
    0b00001100,
    0b00000110,
    0b00000110,
    0b00000000,
};

/* ============================================
 * Cursor Functions
 * ============================================ */

void bga_cursor_init(void) {
    cursor_enabled = 0;
    cursor_visible = 0;
    cursor_x = 512;
    cursor_y = 384;
}

int bga_cursor_is_supported(void) {
    /* Software cursor is always supported */
    return 1;
}

void bga_cursor_enable(int enable) {
    cursor_enabled = enable;
    if (!enable && cursor_visible) {
        /* Restore background when disabling */
        uint32_t *fb = gfx_get_framebuffer();
        uint16_t width = gfx_get_width();
        
        if (fb) {
            for (int row = 0; row < CURSOR_SIZE; row++) {
                for (int col = 0; col < CURSOR_SIZE; col++) {
                    int px = cursor_x + col;
                    int py = cursor_y + row;
                    if (px >= 0 && px < width && py >= 0 && py < gfx_get_height()) {
                        fb[py * width + px] = saved_background[row * CURSOR_SIZE + col];
                    }
                }
            }
        }
        cursor_visible = 0;
    }
}

void bga_cursor_set_position(int x, int y) {
    if (!cursor_enabled) {
        cursor_x = x;
        cursor_y = y;
        return;
    }
    
    uint32_t *fb = gfx_get_framebuffer();
    uint16_t width = gfx_get_width();
    uint16_t height = gfx_get_height();
    
    if (!fb) return;
    
    /* Restore old position */
    if (cursor_visible) {
        for (int row = 0; row < CURSOR_SIZE; row++) {
            for (int col = 0; col < CURSOR_SIZE; col++) {
                int px = cursor_x + col;
                int py = cursor_y + row;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    fb[py * width + px] = saved_background[row * CURSOR_SIZE + col];
                }
            }
        }
    }
    
    /* Update position */
    cursor_x = x;
    cursor_y = y;
    
    /* Save new background */
    for (int row = 0; row < CURSOR_SIZE; row++) {
        for (int col = 0; col < CURSOR_SIZE; col++) {
            int px = cursor_x + col;
            int py = cursor_y + row;
            if (px >= 0 && px < width && py >= 0 && py < height) {
                saved_background[row * CURSOR_SIZE + col] = fb[py * width + px];
            } else {
                saved_background[row * CURSOR_SIZE + col] = 0;
            }
        }
    }
    
    /* Draw cursor at new position */
    for (int row = 0; row < CURSOR_SIZE; row++) {
        uint8_t mask = cursor_mask[row];
        for (int col = 0; col < 8; col++) {
            if (mask & (0x80 >> col)) {
                int px = cursor_x + col;
                int py = cursor_y + row;
                if (px >= 0 && px < width && py >= 0 && py < height) {
                    /* White cursor with black outline effect */
                    fb[py * width + px] = 0xFFFFFF;
                }
            }
        }
    }
    
    cursor_visible = 1;
}
