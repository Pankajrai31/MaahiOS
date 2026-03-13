/**
 * MaahiOS Display Driver - Graphics Abstraction Layer
 * 
 * Provides hardware-independent graphics API.
 * Currently implements BGA (QEMU/Bochs/VirtualBox).
 */

#include "display.h"
#include "bga.h"
#include "vbe.h"
#include "../../managers/device/device_manager.h"
#include "../../managers/klog/klog.h"
#include "../mouse/mouse.h"

/* Forward: VMM allocator for back buffer */
extern void *vmm_alloc_size(uint32_t size_bytes);

/* ============================================
 * State
 * ============================================ */
static display_driver_t current_driver = DISPLAY_DRIVER_NONE;
static uint16_t g_width = 0;
static uint16_t g_height = 0;
static uint32_t *g_framebuffer = (uint32_t*)0;   /* HW MMIO framebuffer */
static uint32_t *g_backbuffer  = (uint32_t*)0;   /* RAM back buffer     */

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
    info->framebuffer_addr = (uint32_t)(g_backbuffer ? g_backbuffer : g_framebuffer);
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
            info->framebuffer_addr = (uint32_t)(g_backbuffer ? g_backbuffer : g_framebuffer);
            info->framebuffer_size = g_width * g_height * 4;
            info->driver = current_driver;
            return DEV_OK;
        }
        
        case DISPLAY_IOCTL_GET_FB:
            return (int)(g_backbuffer ? g_backbuffer : g_framebuffer);
        
        case DISPLAY_IOCTL_SET_MODE:
            KLOG_WARN("DISPLAY", "SET_MODE ioctl not yet implemented");
            return DEV_ERR_NOT_SUPPORTED;
        
        case DISPLAY_IOCTL_FLIP:
            gfx_flip();
            return DEV_OK;

        case DISPLAY_IOCTL_FLIP_RECT: {
            if (!arg) return DEV_ERR_INVALID;
            int *rect = (int *)arg;
            gfx_flip_rect(rect[0], rect[1], rect[2], rect[3]);
            return DEV_OK;
        }
        
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
    KLOG_INFO("DISPLAY", "Initializing graphics subsystem");
    
    /* Try BGA first (QEMU/Bochs/VirtualBox) */
    if (bga_is_available()) {
        if (bga_init(width, height, bpp) == 0) {
            current_driver = DISPLAY_DRIVER_BGA;
            g_width = bga_get_width();
            g_height = bga_get_height();
            g_framebuffer = (uint32_t *)bga_get_framebuffer_addr();
            
            KLOG_INFO("DISPLAY", "BGA driver active");

            /* Allocate RAM back buffer for double buffering */
            uint32_t fb_size = (uint32_t)g_width * (uint32_t)g_height * 4;
            g_backbuffer = (uint32_t *)vmm_alloc_size(fb_size);
            if (g_backbuffer) {
                /* Copy initial HW framebuffer content to back buffer */
                uint32_t px_count = (uint32_t)g_width * (uint32_t)g_height;
                for (uint32_t i = 0; i < px_count; i++)
                    g_backbuffer[i] = g_framebuffer[i];
                KLOG_INFO("DISPLAY", "Back buffer allocated (double buffering active)");
            } else {
                KLOG_WARN("DISPLAY", "Back buffer alloc failed, using direct HW fb");
            }
            return 0;
        }
    }
    
    /* Fallback: VBE framebuffer set up by GRUB (Hyper-V, real hardware) */
    if (vbe_is_available()) {
        if (vbe_init() == 0) {
            current_driver = DISPLAY_DRIVER_VBE;
            g_width = vbe_get_width();
            g_height = vbe_get_height();
            g_framebuffer = (uint32_t *)vbe_get_framebuffer_addr();
            
            KLOG_INFO("DISPLAY", "VBE driver active (GRUB framebuffer)");
            return 0;
        }
    }
    
    KLOG_ERROR("DISPLAY", "No graphics driver available");
    return -1;
}

/**
 * Register display with Device Manager.
 * Called from g_driver_table[] in device_manager.c.
 * BGA hardware must already be initialized (via gfx_init).
 */
int display_register_device(void) {
    if (current_driver == DISPLAY_DRIVER_NONE) {
        KLOG_ERROR("DISPLAY", "Cannot register - no driver active");
        return -1;
    }
    register_device(DEV_DISPLAY, "display", &display_ops);
    KLOG_INFO("DISPLAY", "Registered as DEV_DISPLAY");
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
        case DISPLAY_DRIVER_VBE:
            vbe_clear(color);
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
        case DISPLAY_DRIVER_VBE:
            vbe_fill_rect(x, y, width, height, color);
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
        case DISPLAY_DRIVER_VBE:
            vbe_draw_rect(x, y, width, height, color);
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
        case DISPLAY_DRIVER_VBE:
            vbe_print_at(x, y, str, fg, bg);
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
        case DISPLAY_DRIVER_VBE:
            vbe_print_at(x, y, str, fg, bg);
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
    
    /* Use back buffer for pixel operations if available */
    uint32_t *target = g_backbuffer ? g_backbuffer : g_framebuffer;
    
    if (alpha == 255) {
        target[y * g_width + x] = color & 0x00FFFFFF;
    } else if (alpha > 0) {
        uint32_t bg = target[y * g_width + x];
        
        uint8_t bg_r = (bg >> 16) & 0xFF;
        uint8_t bg_g = (bg >> 8) & 0xFF;
        uint8_t bg_b = bg & 0xFF;
        
        uint8_t fg_r = (color >> 16) & 0xFF;
        uint8_t fg_g = (color >> 8) & 0xFF;
        uint8_t fg_b = color & 0xFF;
        
        uint8_t r = (fg_r * alpha + bg_r * (255 - alpha)) / 255;
        uint8_t g = (fg_g * alpha + bg_g * (255 - alpha)) / 255;
        uint8_t b = (fg_b * alpha + bg_b * (255 - alpha)) / 255;
        
        target[y * g_width + x] = (r << 16) | (g << 8) | b;
    }
}

/* ============================================
 * Back Buffer API
 * ============================================ */

uint32_t* gfx_get_backbuffer(void) {
    return g_backbuffer;
}

/**
 * gfx_flip - Copy RAM back buffer to HW framebuffer + redraw cursor.
 *
 * Flow:
 *   1. Fast memcpy back_buffer → hw_framebuffer (IRQs enabled)
 *   2. cli: refresh cursor on HW fb (save bg + draw sprite)
 *   3. sti
 *
 * The cursor only ever exists on the HW framebuffer.
 * The back buffer is cursor-free, so user draws never race with the cursor.
 */
void gfx_flip(void) {
    if (!g_backbuffer || !g_framebuffer) return;

    /* Atomically erase cursor from HW fb and suppress IRQ12 drawing.
     * This guarantees no cursor pixels are on the HW fb during the copy,
     * so the copy is a clean back-buffer → HW transfer. */
    __asm__ volatile("cli");
    mouse_erase_cursor();
    mouse_set_cursor_suppress(1);
    __asm__ volatile("sti");

    uint32_t count = (uint32_t)g_width * (uint32_t)g_height;

    /* Fast bulk copy using rep movsd (processor-optimised block transfer) */
    uint32_t *src = g_backbuffer;
    uint32_t *dst = g_framebuffer;
    __asm__ volatile(
        "rep movsl"
        : "+S"(src), "+D"(dst), "+c"(count)
        :
        : "memory"
    );

    /* Atomically refresh cursor on HW framebuffer after copy */
    __asm__ volatile("cli");
    mouse_refresh_cursor();
    mouse_set_cursor_suppress(0);
    __asm__ volatile("sti");
}

/**
 * gfx_flip_rect - Copy a rectangular region from back buffer to HW fb.
 *
 * Only copies pixels within (x, y, w, h), clamped to screen bounds.
 * For a 400×300 window this copies ~120K pixels instead of ~1M,
 * making it roughly 8× faster than a full flip.
 */
void gfx_flip_rect(int x, int y, int w, int h) {
    if (!g_backbuffer || !g_framebuffer) return;

    int sw = (int)g_width;
    int sh = (int)g_height;

    /* Clamp rectangle to screen bounds */
    if (x < 0) { w += x; x = 0; }
    if (y < 0) { h += y; y = 0; }
    if (x + w > sw) w = sw - x;
    if (y + h > sh) h = sh - y;
    if (w <= 0 || h <= 0) return;

    /* Atomically erase cursor from HW fb and suppress IRQ12 drawing.
     * This guarantees the old cursor is removed even if it's outside
     * the flipped rect — preventing ghost cursor artifacts. */
    __asm__ volatile("cli");
    mouse_erase_cursor();
    mouse_set_cursor_suppress(1);
    __asm__ volatile("sti");

    /* Copy only the dirty rows via rep movsd per scanline */
    for (int row = y; row < y + h; row++) {
        uint32_t *src = &g_backbuffer[row * sw + x];
        uint32_t *dst = &g_framebuffer[row * sw + x];
        uint32_t cnt  = (uint32_t)w;
        __asm__ volatile(
            "rep movsl"
            : "+S"(src), "+D"(dst), "+c"(cnt)
            :
            : "memory"
        );
    }

    /* Atomically refresh cursor on HW framebuffer */
    __asm__ volatile("cli");
    mouse_refresh_cursor();
    mouse_set_cursor_suppress(0);
    __asm__ volatile("sti");
}
