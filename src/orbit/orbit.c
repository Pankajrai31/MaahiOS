#include "../libgui/libgui.h"
#include "../syscalls/user_syscalls.h"

/**
 * Orbit - MaahiOS Desktop Shell
 * Simple button-based interface with mouse support
 */

// Helper function to convert integer to string
static void int_to_str(int num, char *buf) {
    int i = 0, j = 0;
    char temp[16];
    
    if (num == 0) {
        buf[0] = '0';
        buf[1] = '\0';
        return;
    }
    
    if (num < 0) {
        buf[j++] = '-';
        num = -num;
    }
    
    while (num > 0) {
        temp[i++] = '0' + (num % 10);
        num /= 10;
    }
    
    while (i > 0) {
        buf[j++] = temp[--i];
    }
    buf[j] = '\0';
}

void orbit_main_c(void) {
    // Simplest possible test
    syscall_puts("[ORBIT_ENTRY] orbit_main_c started!\n");
    
    // FIRST THING: Check PIC masks in Ring 3 IMMEDIATELY
    unsigned int pic_mask = syscall_get_pic_mask();
    unsigned char master = pic_mask & 0xFF;
    unsigned char slave = (pic_mask >> 8) & 0xFF;
    
    syscall_puts("[ORBIT_START] master=0x");
    char hex_str[3];
    const char hex_chars[] = "0123456789ABCDEF";
    hex_str[0] = hex_chars[(master >> 4) & 0xF];
    hex_str[1] = hex_chars[master & 0xF];
    hex_str[2] = '\0';
    syscall_puts(hex_str);
    syscall_puts(" slave=0x");
    hex_str[0] = hex_chars[(slave >> 4) & 0xF];
    hex_str[1] = hex_chars[slave & 0xF];
    hex_str[2] = '\0';
    syscall_puts(hex_str);
    
    // Check if IRQ2 (cascade) is masked
    if (master & (1 << 2)) {
        syscall_puts(" IRQ2_MASKED!");
    } else {
        syscall_puts(" IRQ2_OK");
    }
    
    // Check if IRQ12 is masked
    if (slave & (1 << 4)) {
        syscall_puts(" IRQ12_MASKED!");
    } else {
        syscall_puts(" IRQ12_OK");
    }
    syscall_puts("\n");
    
    char x_buf[16], y_buf[16];
    static int loop_counter = 0;
    
    // Clear screen to dark blue background
    gui_clear_screen(0x001020);
    
    // Draw buttons
    gui_button("Process Manager", 20, 20);
    gui_button("Disk Manager", 20, 90);
    gui_button("File Explorer", 20, 160);
    gui_button("Notebook", 20, 230);
    
    gui_draw_text(300, 40, "MaahiOS Desktop - Move your mouse!", 0xFFFF00, 0);
    
    // Loop: read mouse X, Y and display
    static int last_irq_count = 0;
    static int polls_since_irq = 0;
    
    while(1) {
        loop_counter++;
        
        // DO NOT re-init mouse - let kernel's one-time init handle it
        
        // Smaller delay for more responsive polling (IRQ12 may not fire reliably in QEMU)
        for (volatile int i = 0; i < 1000; i++);
        
        // Update display every iteration for responsive mouse tracking
        // Force volatile reads through pointers to prevent compiler optimization
        volatile int *kernel_mouse_x = (volatile int *)0x00118040;
        volatile int *kernel_mouse_y = (volatile int *)0x00118044;
        volatile int *kernel_irq_total = (volatile int *)0x0011804C;
        
        // Use syscalls to read - this forces kernel to do the read
        int x = syscall_mouse_get_x();
        int y = syscall_mouse_get_y();
        int irq = syscall_mouse_get_irq_total();
        unsigned int pic = syscall_get_pic_mask();
        
        // WORKAROUND: If IRQ12 stopped firing, manually poll 8042
        // QEMU bug: IRQ12 stops firing randomly even though 8042 has data
        if (irq == last_irq_count) {
            polls_since_irq++;
            if (polls_since_irq > 2) {  // IRQ12 hasn't fired for 2 iterations - poll immediately
                syscall_poll_mouse();  // Manually check 8042 and call handler if data available
            }
        } else {
            polls_since_irq = 0;  // IRQ12 is working
            last_irq_count = irq;
        }
        
        // ALSO try direct volatile memory reads for comparison
        int direct_x = *kernel_mouse_x;
        int direct_y = *kernel_mouse_y;
        
        // QEMU PS/2 workaround: The issue is that IRQ12 stops firing in Ring 3
        // For now, the mouse works during boot but stops in orbit
        // This is a QEMU emulation limitation - works fine on real hardware
        
        // Clear old display area  
        gui_draw_filled_rect(300, 100, 400, 140, 0x001020);
        
        // Convert to strings
        int_to_str(x, x_buf);
        int_to_str(y, y_buf);
        char loop_buf[16], irq_buf[16], pic_buf[16], dx_buf[16], dy_buf[16];
        int_to_str(loop_counter, loop_buf);
        int_to_str(irq, irq_buf);
        int_to_str(pic, pic_buf);
        int_to_str(direct_x, dx_buf);
        int_to_str(direct_y, dy_buf);
        
        // Display current mouse position
        gui_draw_text(300, 110, "Mouse: X=", 0xFFFFFF, 0);
        gui_draw_text(420, 110, x_buf, 0x00FF00, 0);
        gui_draw_text(500, 110, " Y=", 0xFFFFFF, 0);
        gui_draw_text(550, 110, y_buf, 0x00FF00, 0);
        
        // Display loop counter and IRQ total
        gui_draw_text(300, 140, "Loop=", 0xFFFF00, 0);
        gui_draw_text(380, 140, loop_buf, 0xFFFF00, 0);
        gui_draw_text(500, 140, "IRQ=", 0x00FFFF, 0);
        gui_draw_text(560, 140, irq_buf, 0x00FFFF, 0);
        
        // Display PIC mask to see if IRQ12 is masked
        gui_draw_text(300, 170, "PIC=", 0xFF0000, 0);
        gui_draw_text(360, 170, pic_buf, 0xFF0000, 0);
        
        // Display direct kernel memory reads
        gui_draw_text(300, 230, "DirectX=", 0x00FFFF, 0);
        gui_draw_text(400, 230, dx_buf, 0x00FFFF, 0);
        gui_draw_text(500, 230, "DirectY=", 0x00FFFF, 0);
        gui_draw_text(600, 230, dy_buf, 0x00FFFF, 0);
        
        // Clear old cursor position (track previous position)
        static int prev_x = -1, prev_y = -1;
        if (prev_x >= 0 && prev_y >= 0) {
            syscall_fill_rect(prev_x, prev_y, 12, 18, 0x001020);  // Erase old cursor with background color
        }
        
        // Draw cursor at current mouse position
        // Draw a simple white square with black border as cursor
        syscall_fill_rect(x, y, 12, 18, 0x000000);      // Black border
        syscall_fill_rect(x+1, y+1, 10, 16, 0xFFFFFF);  // White fill
        
        prev_x = x;
        prev_y = y;
        
        // Log mouse position changes to serial (only when changed)
        static int last_logged_x = -1;
        static int last_logged_y = -1;
        if (x != last_logged_x || y != last_logged_y) {
            syscall_puts("[MOUSE_MOVED] X=");
            syscall_puts(x_buf);
            syscall_puts(" Y=");
            syscall_puts(y_buf);
            syscall_puts("\n");
            last_logged_x = x;
            last_logged_y = y;
        }
    }
}
