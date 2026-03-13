/**
 * MaahiOS PS/2 Mouse Driver
 * 
 * Provides PS/2 mouse input handling via IRQ12.
 * Registers with Device Manager for unified device access.
 */

#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>
#include <stddef.h>

/* ============================================
 * Mouse Button Masks
 * ============================================ */
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

/* ============================================
 * Mouse State Structure (for device_read)
 * ============================================ */
typedef struct {
    int x;
    int y;
    uint8_t buttons;
} mouse_state_t;

/* ============================================
 * Driver API
 * ============================================ */

/**
 * Initialize PS/2 mouse driver.
 * Registers with Device Manager as DEV_MOUSE.
 * @return 1 on success, 0 on failure
 */
int mouse_init(void);

/**
 * Mouse IRQ12 handler.
 * Called by interrupt stub.
 */
void mouse_handler(void);

/**
 * Erase cursor from HW framebuffer (restore saved background).
 * Must be called with IRQs disabled (cli).
 * After this, saved_cx = -1 so no stale restore can occur.
 */
void mouse_erase_cursor(void);

/**
 * Refresh cursor on HW framebuffer after a flip.
 * Saves background at current position and redraws cursor sprite.
 * Called from gfx_flip() with IRQs disabled.
 */
void mouse_refresh_cursor(void);

/**
 * Suppress IRQ12 cursor drawing (e.g. during framebuffer flip).
 * 1 = suppress, 0 = allow.
 */
void mouse_set_cursor_suppress(int suppress);

#endif /* MOUSE_H */
