/**
 * GFX - Graphics Abstraction Layer
 * 
 * This provides a hardware-independent graphics API.
 * Currently implements BGA (QEMU/Bochs/VirtualBox).
 * Future: Add VMware SVGA, Hyper-V, VBE fallback.
 */

#ifndef GFX_H
#define GFX_H

#include <stdint.h>

/* Screen dimensions (set during init) */
#define GFX_DEFAULT_WIDTH   800
#define GFX_DEFAULT_HEIGHT  600
#define GFX_DEFAULT_BPP     32

/* Driver types for future expansion */
typedef enum {
    GFX_DRIVER_NONE = 0,
    GFX_DRIVER_BGA,      /* QEMU/Bochs/VirtualBox */
    GFX_DRIVER_VMWARE,   /* VMware SVGA (future) */
    GFX_DRIVER_HYPERV,   /* Hyper-V (future) */
    GFX_DRIVER_VBE       /* Generic VESA fallback (future) */
} gfx_driver_t;

/**
 * Initialize graphics system
 * @param width  Screen width in pixels
 * @param height Screen height in pixels
 * @param bpp    Bits per pixel (32 recommended)
 * @return 1 on success, 0 on failure
 */
int gfx_init(uint16_t width, uint16_t height, uint16_t bpp);

/**
 * Get current driver type
 */
gfx_driver_t gfx_get_driver(void);

/**
 * Get framebuffer address (for direct access by sysman)
 */
uint32_t* gfx_get_framebuffer(void);

/**
 * Get screen dimensions
 */
uint16_t gfx_get_width(void);
uint16_t gfx_get_height(void);

/**
 * Clear entire screen to a color
 */
void gfx_clear(uint32_t color);

/**
 * Draw a filled rectangle
 */
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);

/**
 * Draw a rectangle outline
 */
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);

/**
 * Draw a single character at pixel position
 */
void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);

/**
 * Draw a string at pixel position
 */
void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);

/**
 * Read a pixel value (for cursor compositing)
 */
uint32_t gfx_read_pixel(int x, int y);

/**
 * Write a single pixel
 */
void gfx_put_pixel(int x, int y, uint32_t color);

/**
 * Write a pixel with alpha blending (for anti-aliased fonts)
 * alpha: 0 = transparent, 255 = fully opaque
 */
void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha);

#endif /* GFX_H */
