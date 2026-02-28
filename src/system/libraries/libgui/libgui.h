/**
 * MaahiOS GUI Library - libgui.h
 * 
 * Description:
 *   Main header for the MaahiOS GUI library.
 *   Provides framebuffer access, drawing primitives, font rendering,
 *   and keyboard input — all from a single #include.
 * 
 *   Auto-initializes on first use by reading display info from
 *   cells published by the GUI Executive.
 * 
 * Usage:
 *   #include "libgui.h"
 * 
 *   gui_init();
 *   gui_fill_rect(0, 0, 1024, 768, 0x002D3436);
 *   gui_draw_string(10, 10, "Hello", 0x00FFFFFF, 0x00000000);
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBGUI_H
#define LIBGUI_H

#include <stdint.h>

/* Include sub-module headers */
#include "printgui/printgui.h"
#include "fonts/font8x16.h"
#include "keyboard/keyboard.h"
#include "console/console.h"

/*=============================================================================
 * DISPLAY INFO (populated by gui_init)
 *===========================================================================*/

/** Get framebuffer pointer (NULL if not initialized) */
uint32_t *gui_get_framebuffer(void);

/** Get screen width in pixels */
uint32_t gui_get_screen_width(void);

/** Get screen height in pixels */
uint32_t gui_get_screen_height(void);

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

/**
 * gui_init - Initialize the GUI library
 * 
 * Reads display info (framebuffer address, width, height) from cells
 * published by the GUI Executive. Must be called before using any
 * drawing functions. Auto-called by drawing functions if needed.
 * 
 * Returns: 0 on success, -1 if GUI Executive cells not found
 */
int gui_init(void);

/**
 * gui_is_initialized - Check if library is initialized
 * 
 * Returns: 1 if initialized, 0 if not
 */
int gui_is_initialized(void);

#endif /* LIBGUI_H */
