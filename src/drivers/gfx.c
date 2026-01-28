/**
 * GFX - Graphics Abstraction Layer
 * 
 * Currently implements BGA driver for QEMU/Bochs/VirtualBox.
 * Future: Add detection for VMware SVGA, Hyper-V, VBE fallback.
 */

#include "gfx.h"
#include "bga.h"

/* Current driver in use */
static gfx_driver_t current_driver = GFX_DRIVER_NONE;

/* Cached screen info */
static uint16_t g_width = 0;
static uint16_t g_height = 0;
static uint32_t *g_framebuffer = 0;  /* Use 0 instead of NULL */

/**
 * Initialize graphics system
 * Detects available driver and initializes it
 */
int gfx_init(uint16_t width, uint16_t height, uint16_t bpp) {
    /* Try BGA first (QEMU/Bochs/VirtualBox) */
    if (bga_is_available()) {
        if (bga_init(width, height, bpp)) {
            current_driver = GFX_DRIVER_BGA;
            g_width = bga_get_width();
            g_height = bga_get_height();
            g_framebuffer = (uint32_t *)bga_get_framebuffer_addr();
            return 1;
        }
    }
    
    /* TODO: Try VMware SVGA */
    /* TODO: Try Hyper-V */
    /* TODO: Try generic VBE */
    
    return 0; /* No driver available */
}

/**
 * Get current driver type
 */
gfx_driver_t gfx_get_driver(void) {
    return current_driver;
}

/**
 * Get framebuffer address
 */
uint32_t* gfx_get_framebuffer(void) {
    return g_framebuffer;
}

/**
 * Get screen width
 */
uint16_t gfx_get_width(void) {
    return g_width;
}

/**
 * Get screen height
 */
uint16_t gfx_get_height(void) {
    return g_height;
}

/**
 * Clear screen to a color
 */
void gfx_clear(uint32_t color) {
    switch (current_driver) {
        case GFX_DRIVER_BGA:
            bga_clear(color);
            break;
        default:
            break;
    }
}

/**
 * Draw filled rectangle
 */
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color) {
    switch (current_driver) {
        case GFX_DRIVER_BGA:
            bga_fill_rect(x, y, width, height, color);
            break;
        default:
            break;
    }
}

/**
 * Draw rectangle outline
 */
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color) {
    switch (current_driver) {
        case GFX_DRIVER_BGA:
            bga_draw_rect(x, y, width, height, color);
            break;
        default:
            break;
    }
}

/**
 * Draw a single character
 */
void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    /* Use bga_print_at for single char (we need to create a string) */
    char str[2] = { c, '\0' };
    switch (current_driver) {
        case GFX_DRIVER_BGA:
            bga_print_at(x, y, str, fg, bg);
            break;
        default:
            break;
    }
}

/**
 * Draw a string
 */
void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    switch (current_driver) {
        case GFX_DRIVER_BGA:
            bga_print_at(x, y, str, fg, bg);
            break;
        default:
            break;
    }
}

/**
 * Read a pixel value
 */
uint32_t gfx_read_pixel(int x, int y) {
    if (!g_framebuffer) return 0;
    if (x < 0 || y < 0 || x >= g_width || y >= g_height) return 0;
    return g_framebuffer[y * g_width + x];
}

/**
 * Write a single pixel
 */
void gfx_put_pixel(int x, int y, uint32_t color) {
    if (!g_framebuffer) return;
    if (x < 0 || y < 0 || x >= g_width || y >= g_height) return;
    g_framebuffer[y * g_width + x] = color & 0x00FFFFFF;
}

/**
 * Write a pixel with alpha blending (for anti-aliased fonts)
 * alpha: 0 = transparent, 255 = fully opaque
 */
void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (!g_framebuffer) return;
    if (x < 0 || y < 0 || x >= g_width || y >= g_height) return;
    
    if (alpha == 255) {
        /* Fully opaque - just write */
        g_framebuffer[y * g_width + x] = color & 0x00FFFFFF;
    } else if (alpha > 0) {
        /* Blend with background */
        uint32_t bg = g_framebuffer[y * g_width + x];
        
        uint8_t bg_r = (bg >> 16) & 0xFF;
        uint8_t bg_g = (bg >> 8) & 0xFF;
        uint8_t bg_b = bg & 0xFF;
        
        uint8_t fg_r = (color >> 16) & 0xFF;
        uint8_t fg_g = (color >> 8) & 0xFF;
        uint8_t fg_b = color & 0xFF;
        
        /* Alpha blend: result = fg * alpha + bg * (255 - alpha) */
        uint8_t r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
        uint8_t g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
        uint8_t b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;
        
        g_framebuffer[y * g_width + x] = (r << 16) | (g << 8) | b;
    }
    /* alpha == 0: do nothing (fully transparent) */
}
