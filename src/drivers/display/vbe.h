/**
 * VBE (VESA BIOS Extensions) Framebuffer Driver Header
 *
 * Uses the framebuffer that GRUB set up via multiboot.
 * Works on any hypervisor/hardware that supports VESA
 * (Hyper-V Gen1, VirtualBox, real hardware, QEMU fallback).
 *
 * Internal header — use display.h for the public API.
 */

#ifndef VBE_H
#define VBE_H

#include <stdint.h>

/**
 * Check if GRUB left us a valid framebuffer.
 * Returns 1 if yes, 0 if not.
 */
int vbe_is_available(void);

/**
 * Initialize VBE driver using the framebuffer GRUB set up.
 * Returns 0 on success, -1 on failure.
 */
int vbe_init(void);

/* Query functions */
uint32_t vbe_get_framebuffer_addr(void);
uint16_t vbe_get_width(void);
uint16_t vbe_get_height(void);

/* Drawing functions (same API as bga_*) */
void vbe_clear(uint32_t color);
void vbe_fill_rect(int x, int y, int width, int height, uint32_t color);
void vbe_draw_rect(int x, int y, int width, int height, uint32_t color);
void vbe_print_at(int x, int y, const char *str, uint32_t fg, uint32_t bg);
void vbe_put_pixel(int x, int y, uint32_t color);
void vbe_put_pixel_alpha(int x, int y, uint32_t color, uint8_t alpha);

#endif /* VBE_H */
