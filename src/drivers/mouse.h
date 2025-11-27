#ifndef MOUSE_H
#define MOUSE_H

#include <stdint.h>

// Mouse button states
#define MOUSE_LEFT_BUTTON   0x01
#define MOUSE_RIGHT_BUTTON  0x02
#define MOUSE_MIDDLE_BUTTON 0x04

/**
 * Initialize PS/2 mouse
 * @return 1 on success, 0 on failure
 */
int mouse_init(void);

/**
 * Mouse IRQ handler (IRQ12)
 */
void mouse_handler(void);

/**
 * Get current mouse X position
 */
int mouse_get_x(void);

/**
 * Get current mouse Y position
 */
int mouse_get_y(void);

/**
 * Get mouse button state
 * @return Bitmap of button states (MOUSE_LEFT_BUTTON | MOUSE_RIGHT_BUTTON | MOUSE_MIDDLE_BUTTON)
 */
uint8_t mouse_get_buttons(void);

/**
 * Set screen bounds for mouse
 */
void mouse_set_bounds(int width, int height);

/**
 * Reset mouse position
 */
void mouse_reset_position(int x, int y);

#endif // MOUSE_H
