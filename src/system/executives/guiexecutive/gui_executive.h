/**
 * MaahiOS GUI Executive Header
 * 
 * Description:
 *   GUI Executive provides display and framebuffer access services.
 *   Opens the display device, retrieves framebuffer address and screen
 *   dimensions, and publishes them to the cell registry for discovery.
 * 
 *   Future: window management, compositing, double-buffering.
 * 
 *   Uses liblog for logging (auto-init)
 *   Uses libcell for cell registration (auto-init)
 *   Uses SYS_DEV_* syscalls to talk to kernel Device Manager
 *   Dual SHM queues (request + response)
 * 
 * Data Flow:
 *   App -> libgui -> cell read (for FB address) -> direct framebuffer writes
 *   App -> libgui -> SHM queue -> GUI Executive (for future window ops)
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef GUI_EXECUTIVE_H
#define GUI_EXECUTIVE_H

#include "../common/executive_common.h"

/*=============================================================================
 * GUI EXECUTIVE OPCODES (starting at EXEC_OP_CUSTOM_BASE = 16)
 *===========================================================================*/

#define GUI_OP_GET_DISPLAY_INFO (EXEC_OP_CUSTOM_BASE + 0)   /* Get FB + dims */

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define GUI_SCREEN_WIDTH        1024
#define GUI_SCREEN_HEIGHT       768
#define GUI_SCREEN_BPP          32

/*=============================================================================
 * DISPLAY INFO STRUCTURE (returned to callers)
 *===========================================================================*/

typedef struct {
    uint32_t framebuffer;       /* Physical/virtual address of FB */
    uint32_t width;             /* Screen width in pixels */
    uint32_t height;            /* Screen height in pixels */
    uint32_t bpp;               /* Bits per pixel */
    uint32_t pitch;             /* Bytes per scanline (width * bpp/8) */
} gui_display_info_t;

/*=============================================================================
 * RESPONSE PAYLOADS
 *===========================================================================*/

/* GET_DISPLAY_INFO response — fits in payload */
typedef struct {
    gui_display_info_t info;
} gui_display_info_resp_t;

#endif /* GUI_EXECUTIVE_H */
