/**
 * MaahiOS Display Driver - Graphics Abstraction Layer
 * 
 * Provides hardware-independent graphics API.
 * Currently implements BGA (QEMU/Bochs/VirtualBox).
 */

#include "display.h"
#include "bga.h"
#include "../../managers/device/device_manager.h"

/* ============================================
 * State
 * ============================================ */
static display_driver_t current_driver = DISPLAY_DRIVER_NONE;
static uint16_t g_width = 0;
static uint16_t g_height = 0;
static uint32_t *g_framebuffer = (uint32_t*)0;

/* ============================================
 * Device Manager Operations
 * ============================================ */
static int display_dev_open(int flags) {
    (void)flags;
    return 0;
}

static int display_dev_close(int handle) {
    (void)handle;
    return 0;
}

static int display_dev_read(int handle, void* buffer, size_t size) {
    (void)handle;
    
    /* Reading from display returns display info */
    if (!buffer || size < sizeof(display_info_t)) {
        return DEV_ERR_INVALID;
    }
    
    display_info_t* info = (display_info_t*)buffer;
    info->width = g_width;
    info->height = g_height;
    info->bpp = 32;
    info->framebuffer_addr = (uint32_t)g_framebuffer;
    info->framebuffer_size = g_width * g_height * 4;
    info->driver = current_driver;
    
    return sizeof(display_info_t);
}

static int display_dev_write(int handle, const void* buffer, size_t size) {
    (void)handle;
    (void)buffer;
    (void)size;
    /* Direct framebuffer writes not supported via this interface */
    return DEV_ERR_NOT_SUPPORTED;
}

static int display_dev_ioctl(int handle, int cmd, void* arg) {
    (void)handle;
    
    switch (cmd) {
        case DISPLAY_IOCTL_GET_INFO: {
            if (!arg) return DEV_ERR_INVALID;
            display_info_t* info = (display_info_t*)arg;
            info->width = g_width;
            info->height = g_height;
            info->bpp = 32;
            info->framebuffer_addr = (uint32_t)g_framebuffer;
            info->framebuffer_size = g_width * g_height * 4;
            info->driver = current_driver;
            return DEV_OK;
        }
        
        case DISPLAY_IOCTL_GET_FB:
            return (int)g_framebuffer;
        
        default:
            return DEV_ERR_INVALID;
    }
}

static int display_dev_poll(int handle) {
    (void)handle;
    return 1;  /* Display always ready */
}

static device_ops_t display_ops = {
    .open  = display_dev_open,
    .close = display_dev_close,
    .read  = display_dev_read,
    .write = display_dev_write,
    .ioctl = display_dev_ioctl,
    .poll  = display_dev_poll
};

/* ============================================
 * Initialization
 * ============================================ */

/**
 * Initialize graphics system
 */
int gfx_init(uint16_t width, uint16_t height, uint16_t bpp) {
    /* Try BGA first */
    if (bga_is_available()) {
        if (bga_init(width, height, bpp)) {
            current_driver = DISPLAY_DRIVER_BGA;
            g_width = bga_get_width();
            g_height = bga_get_height();
            g_framebuffer = (uint32_t *)bga_get_framebuffer_addr();
            
            /* Register with Device Manager */
            register_device(DEV_DISPLAY, "display", &display_ops);
            return 1;
        }
    }
    
    return 0;
}

/* ============================================
 * Query Functions
 * ============================================ */

display_driver_t gfx_get_driver(void) {
    return current_driver;
}

uint32_t* gfx_get_framebuffer(void) {
    return g_framebuffer;
}

uint16_t gfx_get_width(void) {
    return g_width;
}

uint16_t gfx_get_height(void) {
    return g_height;
}

/* ============================================
 * Drawing Functions
 * ============================================ */

void gfx_clear(uint32_t color) {
    switch (current_driver) {
        case DISPLAY_DRIVER_BGA:
            bga_clear(color);
            break;
        default:
            break;
    }
}

void gfx_fill_rect(int x, int y, int width, int height, uint32_t color) {
    switch (current_driver) {
        case DISPLAY_DRIVER_BGA:
            bga_fill_rect(x, y, width, height, color);
            break;
        default:
            break;
    }
}

void gfx_draw_rect(int x, int y, int width, int height, uint32_t color) {
    switch (current_driver) {
        case DISPLAY_DRIVER_BGA:
            bga_draw_rect(x, y, width, height, color);
            break;
        default:
            break;
    }
}

void gfx_draw_char(int x, int y, char c, uint32_t fg, uint32_t bg) {
    char str[2] = { c, '\0' };
    switch (current_driver) {
        case DISPLAY_DRIVER_BGA:
            bga_print_at(x, y, str, fg, bg);
            break;
        default:
            break;
    }
}

void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg) {
    switch (current_driver) {
        case DISPLAY_DRIVER_BGA:
            bga_print_at(x, y, str, fg, bg);
            break;
        default:
            break;
    }
}

uint32_t gfx_read_pixel(int x, int y) {
    if (!g_framebuffer) return 0;
    if (x < 0 || y < 0 || x >= g_width || y >= g_height) return 0;
    return g_framebuffer[y * g_width + x];
}

void gfx_put_pixel(int x, int y, uint32_t color) {
    if (!g_framebuffer) return;
    if (x < 0 || y < 0 || x >= g_width || y >= g_height) return;
    g_framebuffer[y * g_width + x] = color & 0x00FFFFFF;
}

void gfx_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha) {
    if (!g_framebuffer) return;
    if (x < 0 || y < 0 || x >= g_width || y >= g_height) return;
    
    if (alpha == 255) {
        g_framebuffer[y * g_width + x] = color & 0x00FFFFFF;
    } else if (alpha > 0) {
        uint32_t bg = g_framebuffer[y * g_width + x];
        
        uint8_t bg_r = (bg >> 16) & 0xFF;
        uint8_t bg_g = (bg >> 8) & 0xFF;
        uint8_t bg_b = bg & 0xFF;
        
        uint8_t fg_r = (color >> 16) & 0xFF;
        uint8_t fg_g = (color >> 8) & 0xFF;
        uint8_t fg_b = color & 0xFF;
        
        uint8_t r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
        uint8_t g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
        uint8_t b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;
        
        g_framebuffer[y * g_width + x] = (r << 16) | (g << 8) | b;
    }
}
