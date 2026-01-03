#include "../syscalls/user_syscalls.h"

/**
 * Orbit - MaahiOS Desktop Shell
 * Calls syscalls to create UI elements (no local state)
 */

// Syscall wrappers for UI operations
static int ui_create_window(int x, int y, int w, int h, const char *title, int parent) {
    int result;
    register int _x __asm__("ebx") = x;
    register int _y __asm__("ecx") = y;
    register int _w __asm__("edx") = w;
    register int _h __asm__("esi") = h;
    
    __asm__ volatile(
        "push %6\n"
        "push %5\n"
        "mov $40, %%eax\n"
        "int $0x80\n"
        "add $8, %%esp\n"
        : "=a"(result)
        : "b"(_x), "c"(_y), "d"(_w), "S"(_h), "r"(title), "r"(parent)
        : "memory"
    );
    return result;
}

static int ui_create_button(int window_id, int x, int y, int w, int h, const char *text) {
    int result;
    register int _wid __asm__("ebx") = window_id;
    register int _x __asm__("ecx") = x;
    register int _y __asm__("edx") = y;
    register int _w __asm__("esi") = w;
    
    __asm__ volatile(
        "push %6\n"
        "push %5\n"
        "mov $41, %%eax\n"
        "int $0x80\n"
        "add $8, %%esp\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "S"(_w), "r"(h), "r"(text)
        : "memory"
    );
    return result;
}

static int ui_create_label(int window_id, int x, int y, const char *text) {
    int result;
    register int _wid __asm__("ebx") = window_id;
    register int _x __asm__("ecx") = x;
    register int _y __asm__("edx") = y;
    register const char *_text __asm__("esi") = text;
    
    __asm__ volatile(
        "mov $42, %%eax\n"
        "int $0x80\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "S"(_text)
    );
    return result;
}

// Event types
#define UIMAN_EVENT_NONE      0
#define UIMAN_EVENT_CLICK     1
#define UIMAN_EVENT_DBLCLICK  2
#define UIMAN_EVENT_HOVER     3

typedef struct {
    int type;
    int control_id;
    int x, y;
} uiman_event_t;

static inline int ui_poll_event(uiman_event_t *event) {
    int result;
    __asm__ volatile(
        "mov $43, %%eax\n"  // SYSCALL_UI_POLL_EVENT
        "mov %1, %%ebx\n"
        "int $0x80\n"
        "mov %%eax, %0\n"
        : "=r"(result)
        : "r"(event)
        : "eax", "ebx", "memory"
    );
    return result;
}

void orbit_main_c(void) {
    syscall_puts("[ORBIT] Entry!\n");
    
    // NOTE: Sysman should clear screen before starting Orbit
    // Orbit doesn't touch framebuffer directly - only through UIManager syscalls
    
    // Create desktop window (fullscreen, no parent)
    syscall_puts("[ORBIT] Creating desktop window...\n");
    int desktop = ui_create_window(0, 0, 800, 600, "Desktop", 0);
    if (desktop < 0) {
        syscall_puts("[ORBIT] ERROR: Failed to create desktop window\n");
        while(1) __asm__ volatile("hlt");
    }
    syscall_puts("[ORBIT] Desktop window created, ID=");
    syscall_putchar('0' + desktop);
    syscall_puts("\n");
    
    // Create buttons using UIManager syscalls
    syscall_puts("[ORBIT] Creating buttons...\n");
    int btn1 = ui_create_button(desktop, 20, 20, 180, 50, "Process Manager");
    int btn2 = ui_create_button(desktop, 20, 90, 180, 50, "Disk Manager");
    int btn3 = ui_create_button(desktop, 20, 160, 180, 50, "File Explorer");
    int btn4 = ui_create_button(desktop, 20, 230, 180, 50, "Notebook");
    
    if (btn1 < 0 || btn2 < 0 || btn3 < 0 || btn4 < 0) {
        syscall_puts("[ORBIT] ERROR: Failed to create buttons\n");
    } else {
        syscall_puts("[ORBIT] Buttons created successfully\n");
        syscall_puts("[ORBIT] btn1="); syscall_putchar('0' + btn1); syscall_puts("\n");
        syscall_puts("[ORBIT] btn2="); syscall_putchar('0' + btn2); syscall_puts("\n");
        syscall_puts("[ORBIT] btn3="); syscall_putchar('0' + btn3); syscall_puts("\n");
        syscall_puts("[ORBIT] btn4="); syscall_putchar('0' + btn4); syscall_puts("\n");
    }
    
    // Create a label
    int label = ui_create_label(desktop, 300, 40, "MaahiOS Desktop - UIManager Active!");
    if (label < 0) {
        syscall_puts("[ORBIT] ERROR: Failed to create label\n");
    } else {
        syscall_puts("[ORBIT] Label created, ID=");
        syscall_putchar('0' + label);
        syscall_puts("\n");
    }
    
    syscall_puts("[ORBIT] Entering event loop...\n");
    
    // Event loop - process UI events from UIManager
    while(1) {
        // Poll for events (non-blocking)
        uiman_event_t event;
        if (ui_poll_event(&event)) {
            // Handle events
            switch (event.type) {
                case UIMAN_EVENT_CLICK:
                    syscall_puts("[ORBIT] Button clicked: ID=");
                    syscall_putchar('0' + event.control_id);
                    syscall_puts("\n");
                    
                    // Handle specific buttons
                    if (event.control_id == btn1) {
                        syscall_puts("[ORBIT] Process Manager clicked!\n");
                    } else if (event.control_id == btn2) {
                        syscall_puts("[ORBIT] Disk Manager clicked!\n");
                    } else if (event.control_id == btn3) {
                        syscall_puts("[ORBIT] File Explorer clicked!\n");
                    } else if (event.control_id == btn4) {
                        syscall_puts("[ORBIT] Notebook clicked!\n");
                    }
                    break;
                    
                case UIMAN_EVENT_DBLCLICK:
                    syscall_puts("[ORBIT] Double-click on control ");
                    syscall_putchar('0' + event.control_id);
                    syscall_puts("\n");
                    break;
                    
                case UIMAN_EVENT_HOVER:
                    // Hover events are frequent, don't spam output
                    break;
                    
                default:
                    break;
            }
        }
        
        // Small delay to prevent busy-waiting
        for (volatile int i = 0; i < 1000; i++);
    }
}
