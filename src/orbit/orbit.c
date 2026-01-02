#include "../libgui/libgui.h"
#include "../libgui/cursor_compositor.h"
#include "../syscalls/user_syscalls.h"
#include "../../libraries/icons/embedded_icons.h"

/**
 * Orbit - MaahiOS Desktop Shell
 * Simple button-based interface with mouse support
 */

void orbit_main_c(void) {
    // FIRST: Print debug message to see if we get here
    syscall_puts("[ORBIT] Entry!\n");
    
    // Initialize cursor compositor
    syscall_puts("[ORBIT] Cursor init...\n");
    orbit_cursor_init();
    syscall_puts("[ORBIT] Cursor OK\n");
    
    // Clear screen to dark blue background
    syscall_puts("[ORBIT] Clearing screen...\n");
    gui_clear_screen(0x001020);
    syscall_puts("[ORBIT] Screen cleared\n");
    
    // Draw buttons
    gui_button("Process Manager", 20, 20);
    gui_button("Disk Manager", 20, 90);
    gui_button("File Explorer", 20, 160);
    gui_button("Notebook", 20, 230);
    
    gui_draw_text(300, 40, "MaahiOS Desktop - Move your mouse!", 0xFFFF00, 0);
    
    // Draw file icon
    syscall_draw_bmp(200, 165, (unsigned int)icon_file_bmp);
    
    // Mouse event detection state
    static int last_irq_count = 0;
    static int polls_since_irq = 0;
    static unsigned int last_buttons = 0;
    static int last_click_time = 0;
    static int click_x = 0, click_y = 0;
    
    // Event status display areas
    char hover_text[64];
    char click_text[32] = "Click: None";
    char dblclick_text[32] = "Double Click: None";
    
    while(1) {
        // Get current mouse position and buttons (atomic reads)
        int x = syscall_mouse_get_x();
        int y = syscall_mouse_get_y();
        unsigned int buttons = syscall_mouse_get_buttons();
        int irq = syscall_mouse_get_irq_total();
        
        // Poll mouse if IRQ stopped
        if (irq == last_irq_count) {
            polls_since_irq++;
            if (polls_since_irq > 2) {
                syscall_poll_mouse();
            }
        } else {
            polls_since_irq = 0;
            last_irq_count = irq;
        }
        
        // Detect button press (transition from 0 to pressed)
        if (buttons != last_buttons) {
            if ((buttons & 0x01) && !(last_buttons & 0x01)) {
                // Left button pressed
                int current_time = irq;  // Use IRQ count as simple timer
                int time_diff = current_time - last_click_time;
                
                // Check for double click (within ~10 IRQs and near same position)
                int dx = x - click_x;
                int dy = y - click_y;
                if (dx < 0) dx = -dx;
                if (dy < 0) dy = -dy;
                
                if (time_diff < 10 && dx < 5 && dy < 5) {
                    // Double click detected
                    dblclick_text[0] = 'D'; dblclick_text[1] = 'o'; dblclick_text[2] = 'u';
                    dblclick_text[3] = 'b'; dblclick_text[4] = 'l'; dblclick_text[5] = 'e';
                    dblclick_text[6] = ' '; dblclick_text[7] = 'C'; dblclick_text[8] = 'l';
                    dblclick_text[9] = 'i'; dblclick_text[10] = 'c'; dblclick_text[11] = 'k';
                    dblclick_text[12] = ':'; dblclick_text[13] = ' '; dblclick_text[14] = 'Y';
                    dblclick_text[15] = 'E'; dblclick_text[16] = 'S'; dblclick_text[17] = '\0';
                } else {
                    // Single click
                    click_text[0] = 'C'; click_text[1] = 'l'; click_text[2] = 'i';
                    click_text[3] = 'c'; click_text[4] = 'k'; click_text[5] = ':';
                    click_text[6] = ' '; click_text[7] = 'Y'; click_text[8] = 'E';
                    click_text[9] = 'S'; click_text[10] = '\0';
                    
                    // Reset double click
                    dblclick_text[0] = 'D'; dblclick_text[1] = 'o'; dblclick_text[2] = 'u';
                    dblclick_text[3] = 'b'; dblclick_text[4] = 'l'; dblclick_text[5] = 'e';
                    dblclick_text[6] = ' '; dblclick_text[7] = 'C'; dblclick_text[8] = 'l';
                    dblclick_text[9] = 'i'; dblclick_text[10] = 'c'; dblclick_text[11] = 'k';
                    dblclick_text[12] = ':'; dblclick_text[13] = ' '; dblclick_text[14] = 'N';
                    dblclick_text[15] = 'o'; dblclick_text[16] = 'n'; dblclick_text[17] = 'e';
                    dblclick_text[18] = '\0';
                }
                
                last_click_time = current_time;
                click_x = x;
                click_y = y;
            }
            
            last_buttons = buttons;
        }
        
        // Build hover text with coordinates
        hover_text[0] = 'H'; hover_text[1] = 'o'; hover_text[2] = 'v';
        hover_text[3] = 'e'; hover_text[4] = 'r'; hover_text[5] = ':';
        hover_text[6] = ' '; hover_text[7] = 'x'; hover_text[8] = '=';
        
        // Convert x to string
        int pos = 9;
        int temp_x = x;
        int divisor = 100;
        int started = 0;
        while (divisor > 0) {
            int digit = temp_x / divisor;
            if (digit > 0 || started || divisor == 1) {
                hover_text[pos++] = '0' + digit;
                started = 1;
            }
            temp_x %= divisor;
            divisor /= 10;
        }
        
        hover_text[pos++] = ' ';
        hover_text[pos++] = 'y';
        hover_text[pos++] = '=';
        
        // Convert y to string
        int temp_y = y;
        divisor = 100;
        started = 0;
        while (divisor > 0) {
            int digit = temp_y / divisor;
            if (digit > 0 || started || divisor == 1) {
                hover_text[pos++] = '0' + digit;
                started = 1;
            }
            temp_y %= divisor;
            divisor /= 10;
        }
        hover_text[pos] = '\0';
        
        // Display event status (clear background first)
        syscall_fill_rect(300, 80, 400, 20, 0x001020);  // Clear click text area
        syscall_fill_rect(300, 110, 400, 20, 0x001020); // Clear double click area
        syscall_fill_rect(300, 140, 400, 20, 0x001020); // Clear hover area
        
        gui_draw_text(300, 80, click_text, 0x00FF00, 0);
        gui_draw_text(300, 110, dblclick_text, 0xFF8800, 0);
        gui_draw_text(300, 140, hover_text, 0x00FFFF, 0);
        
        // Update cursor
        orbit_draw_cursor(x, y);
    }
}
