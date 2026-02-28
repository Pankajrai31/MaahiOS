/**
 * MaahiOS GUI Library (libgui) - Core Implementation
 * 
 * Description:
 *   Core initialization for the GUI library.
 *   Reads display info from cells published by the GUI Executive.
 *   Provides framebuffer pointer and screen dimensions to all
 *   sub-modules (printgui, fonts, keyboard).
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include "libgui.h"
#include "../core/syscall_helpers.h"

/*=============================================================================
 * LIBRARY STATE
 *===========================================================================*/

static uint32_t *g_framebuffer  = (uint32_t *)0;
static uint32_t  g_screen_width  = 0;
static uint32_t  g_screen_height = 0;
static int       g_initialized   = 0;

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

int gui_init(void) {
    if (g_initialized) return 0;

    /* Retry up to 50 times (~1 second total) waiting for GUI Executive
     * to publish ALL its cells. The executive writes framebuffer, width,
     * and height as separate cell_write calls — scheduler preemption can
     * cause us to see some but not all. Retry the entire read sequence. */
    uint32_t fb_addr = 0;
    uint32_t width   = 0;
    uint32_t height  = 0;

    for (int attempt = 0; attempt < 50; attempt++) {
        fb_addr = 0;
        width   = 0;
        height  = 0;

        syscall3(SYS_CELL_READ,
                 (uint32_t)"system.gui.framebuffer",
                 (uint32_t)&fb_addr, sizeof(uint32_t));

        syscall3(SYS_CELL_READ,
                 (uint32_t)"system.gui.width",
                 (uint32_t)&width, sizeof(uint32_t));

        syscall3(SYS_CELL_READ,
                 (uint32_t)"system.gui.height",
                 (uint32_t)&height, sizeof(uint32_t));

        if (fb_addr != 0 && width != 0 && height != 0) {
            break;  /* All cells found */
        }

        /* GUI Executive not ready yet, yield and sleep 1 tick */
        syscall0(SYS_YIELD);
        syscall1(SYS_SLEEP, 1);
    }

    if (fb_addr == 0 || width == 0 || height == 0) {
        return -1;
    }

    g_framebuffer   = (uint32_t *)fb_addr;
    g_screen_width  = width;
    g_screen_height = height;
    g_initialized   = 1;

    return 0;
}

int gui_is_initialized(void) {
    return g_initialized;
}

uint32_t *gui_get_framebuffer(void) {
    if (!g_initialized) gui_init();
    return g_framebuffer;
}

uint32_t gui_get_screen_width(void) {
    if (!g_initialized) gui_init();
    return g_screen_width;
}

uint32_t gui_get_screen_height(void) {
    if (!g_initialized) gui_init();
    return g_screen_height;
}
