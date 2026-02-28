/**
 * MaahiOS Display Driver - Header
 * 
 * Graphics abstraction layer supporting multiple backends.
 * Currently implements BGA (QEMU/Bochs/VirtualBox).
 */

#ifndef DISPLAY_H
#define DISPLAY_H

#include <stdint.h>
#include <stddef.h>

/* ============================================
 * Screen Defaults
 * ============================================ */
#define DISPLAY_DEFAULT_WIDTH   1024
#define DISPLAY_DEFAULT_HEIGHT  768
#define DISPLAY_DEFAULT_BPP     32

/* ============================================
 * Driver Types
 * ============================================ */
typedef enum {
    DISPLAY_DRIVER_NONE = 0,
    DISPLAY_DRIVER_BGA,      /* QEMU/Bochs/VirtualBox */
    DISPLAY_DRIVER_VMWARE,   /* VMware SVGA (future) */
    DISPLAY_DRIVER_VBE       /* Generic VESA (future) */
} display_driver_t;

/* ============================================
 * Display Info Structure (for IOCTL)
 * ============================================ */
typedef struct {
    uint16_t width;
    uint16_t height;
    uint16_t bpp;
    uint32_t framebuffer_addr;
    uint32_t framebuffer_size;
    display_driver_t driver;
} display_info_t;

/* ============================================
 * Graphics API
 * ============================================ */

/**
 * Initialize display system.
 * Does NOT register with device manager (called too early).
 */
int gfx_init(uint16_t width, uint16_t height, uint16_t bpp);

/**
 * Register display with Device Manager.
 * Called from g_driver_table[] after device_manager_init clears entries.
 * BGA hardware must already be initialized via gfx_init().
 */
int display_register_device(void);

/**
 * Get current driver type.
 */
display_driver_t gfx_get_driver(void);

/**
 * Get framebuffer address.
 */
uint32_t* gfx_get_framebuffer(void);

/**
 * Get screen dimensions.
 */
uint16_t gfx_get_width(void);
uint16_t gfx_get_height(void);

/**
 * Clear screen to color.
 */
void gfx_clear(uint32_t color);

/**
 * Draw filled rectangle.
 */
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);

/**
 * Draw rectangle outline.
 */
void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);

/**
 * Draw character at position.
 */
void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg);

/**
 * Draw string at position.
 */
void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);

/**
 * Read pixel value.
 */
uint32_t gfx_read_pixel(int x, int y);

/**
 * Write pixel.
 */
void gfx_put_pixel(int x, int y, uint32_t color);

/**
 * Write pixel with alpha blending.
 */
void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha);

/* ============================================
 * Hardware Cursor API
 * ============================================ */

/**
 * Initialize hardware cursor.
 */
void bga_cursor_init(void);

/**
 * Check if hardware cursor is supported.
 */
int bga_cursor_is_supported(void);

/**
 * Enable/disable hardware cursor.
 */
void bga_cursor_enable(int enable);

/**
 * Set cursor position.
 */
void bga_cursor_set_position(int x, int y);

#endif /* DISPLAY_H */
