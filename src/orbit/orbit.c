/**
 * Orbit - MaahiOS Desktop Shell (Clean Theme Test)
 * 
 * Testing new themed UI components based on design system
 */

#include "../Libraries/maahi.h"
#include "../system/syscalls/user/user_syscalls.h"

void orbit_main_c(void) {
    maahi_print("[ORBIT] Starting desktop shell...\n");
    
    // Set white background for Orbit
    maahi_fill_rect(0, 0, 1024, 768, 0xFFFFFF);
    
    // Create ONE themed test button (centered on screen)
    // Theme: Primary button - #2B5BB5, 12x24px padding, 6px radius
    int btnTest = maahi_button_create(0, 400, 350, 200, 48, 
                                      "Click Me!", 
                                      MAAHI_BUTTON_PRIMARY, 
                                      MAAHI_BUTTON_MEDIUM);
    maahi_print("[ORBIT] Created themed test button\n");
    
    // Event loop - simple test
    maahi_print("[ORBIT] Entering event loop\n");
    
    while(1) {
        MaahiEvent event;
        if (maahi_poll_event(&event)) {
            if (event.type == MAAHI_EVENT_CLICK) {
                maahi_print("[ORBIT] Button clicked!\n");
            }
        }
        maahi_yield();
    }
}
