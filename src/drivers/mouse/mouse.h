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

#endif /* MOUSE_H */
