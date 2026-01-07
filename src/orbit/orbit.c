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
    syscall_puts("[ORBIT] Starting desktop shell...\n");
    
    // Create desktop controls (window_id = 0 means desktop-level, no window)
    int btn1 = ui_create_button(0, 20, 20, 180, 50, "Process Manager");
    syscall_puts("[ORBIT] btn1 (Process Manager) ID=");
    syscall_putchar('0' + btn1);
    syscall_puts("\n");
    
    int btn2 = ui_create_button(0, 20, 90, 180, 50, "Disk Manager");
    syscall_puts("[ORBIT] btn2 (Disk Manager) ID=");
    syscall_putchar('0' + btn2);
    syscall_puts("\n");
    
    int btn3 = ui_create_button(0, 20, 160, 180, 50, "File Explorer");
    syscall_puts("[ORBIT] btn3 (File Explorer) ID=");
    syscall_putchar('0' + btn3);
    syscall_puts("\n");
    
    int btn4 = ui_create_button(0, 20, 230, 180, 50, "Notebook");
    syscall_puts("[ORBIT] btn4 (Notebook) ID=");
    syscall_putchar('0' + btn4);
    syscall_puts("\n");
    
    int btn5 = ui_create_button(0, 20, 300, 180, 50, "File Manager");
    syscall_puts("[ORBIT] btn5 (File Manager) ID=");
    syscall_putchar('0' + btn5);
    syscall_puts("\n");
    
    // Create a desktop label
    int label = ui_create_label(0, 300, 40, "MaahiOS Desktop - UIManager Active!");
    
    syscall_puts("[ORBIT] Desktop controls created\n");
    
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
                    } else if (event.control_id == btn5) {
                        syscall_puts("[ORBIT] File Manager clicked - launching...\n");
                        // Launch file_manager.bin as new process
                        int result;
                        __asm__ volatile(
                            "mov $52, %%eax\n"
                            "int $0x80\n"
                            : "=a"(result)
                            :
                            : "memory"
                        );
                        if (result >= 0) {
                            syscall_puts("[ORBIT] File Manager launched successfully, PID=");
                            syscall_putchar('0' + result);
                            syscall_puts("\n");
                        } else {
                            syscall_puts("[ORBIT] Failed to launch File Manager\n");
                        }
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
