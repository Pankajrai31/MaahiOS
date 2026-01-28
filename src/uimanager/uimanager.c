/**
 * UIManager - MINIMAL VERSION
 * Just show mouse cursor moving - nothing else
 */

#include <stdint.h>
#include "../system/syscalls/user/user_syscalls.h"
#include "events/mouse_cursor.h"

// ============================================================================
// CONSTANTS
// ============================================================================
#define SCREEN_WIDTH 1024
#define SCREEN_HEIGHT 768

// ============================================================================
// GLOBALS
// ============================================================================
static uint32_t* g_framebuffer = (uint32_t*)0xFD000000;

// ============================================================================
// MAIN ENTRY POINT
// ============================================================================
void uimanager_main_c(void) {
    // Clear screen to dark grey
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        g_framebuffer[i] = 0x2A2A2A;
    }
    
    // Initialize mouse cursor system
    mouse_cursor_init();
    
    int last_mouse_x = SCREEN_WIDTH / 2;
    int last_mouse_y = SCREEN_HEIGHT / 2;
    
    // Main loop - just render cursor
    while (1) {
        // Poll mouse position
        syscall_poll_mouse();
        
        int mx = syscall_mouse_get_x();
        int my = syscall_mouse_get_y();
        
        // Render cursor (handles save/restore internally)
        mouse_cursor_render(mx, my, last_mouse_x, last_mouse_y,
                           g_framebuffer, 0,
                           SCREEN_WIDTH, SCREEN_HEIGHT);
        
        last_mouse_x = mx;
        last_mouse_y = my;
        
        // Small delay to avoid consuming too much CPU
        for (volatile int i = 0; i < 50000; i++);
    }
}
