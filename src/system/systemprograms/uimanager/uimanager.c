/**
 * UIManager - Generic UI Control Manager with Double Buffering
 * Architecture: polls input → dispatches to control managers → swaps buffers
 * UIManager is control-agnostic - all control-specific logic lives in control managers
 */

#include <stdint.h>
#include <stddef.h>
#include "../../../system/syscalls/user/user_syscalls.h"
#include "controls/button/button_manager.h"
#include "render/mouse/mouse_renderer.h"
#include "buffer/double_buffer.h"

// ============================================================================
// CONSTANTS
// ============================================================================
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768
#define BACKGROUND_COLOR 0xFFF5F7FA  /* Light gray background - ARGB format */

// ============================================================================
// GLOBALS
// ============================================================================
static uint32_t* g_framebuffer = (uint32_t*)0xFD000000;
static uint32_t* g_back_buffer = NULL;

// ============================================================================
// MAIN ENTRY POINT - Generic frame coordinator
// UIManager is control-agnostic - delegates to control managers
// ============================================================================
void uimanager_main_c(void) {
    syscall_puts("[UIMAN] UIManager started\n");
    
    /* Initialize double buffering */
    syscall_puts("[UIMAN] Initializing double buffer...\n");
    if (double_buffer_init(SCREEN_WIDTH, SCREEN_HEIGHT, g_framebuffer) != 0) {
        syscall_puts("[UIMAN] FATAL: Double buffer init failed\n");
        for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
            g_framebuffer[i] = 0xFF0000;  /* Red = error */
        }
        while(1);
    }
    
    /* Get back buffer pointer */
    g_back_buffer = double_buffer_get_back();
    syscall_puts("[UIMAN] Double buffer initialized\n");
    
    /* Clear back buffer and show initial frame */
    double_buffer_clear(BACKGROUND_COLOR);
    double_buffer_swap();
    syscall_puts("[UIMAN] Starting main loop\n");
    
    /* Main rendering loop - generic frame coordinator */
    while (1) {
        /* 1. Poll input */
        syscall_poll_mouse();
        int mouse_x = syscall_mouse_get_x();
        int mouse_y = syscall_mouse_get_y();
        int mouse_pressed = syscall_mouse_get_buttons() & 1;
        
        /* 2. Clear back buffer */
        double_buffer_clear(BACKGROUND_COLOR);
        
        /* 3. Dispatch to control managers (button, textbox, slider, etc.) */
        button_manager_render_all(g_back_buffer, SCREEN_WIDTH, mouse_x, mouse_y, mouse_pressed);
        
        /* 4. Render cursor */
        render_mouse_cursor(g_back_buffer, SCREEN_WIDTH, SCREEN_HEIGHT, mouse_x, mouse_y);
        
        /* 5. Show frame */
        double_buffer_swap();
        
        /* CPU delay */
        for (volatile int i = 0; i < 50000; i++);
    }
}
