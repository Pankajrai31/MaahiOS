#include "../../syscalls/user_syscalls.h"

/**
 * File Manager - MaahiOS File Browser Application
 * Demonstrates windowed application with title bar controls
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
    
    __asm__ volatile(
        "push %4\n"
        "mov $42, %%eax\n"
        "int $0x80\n"
        "add $4, %%esp\n"
        : "=a"(result)
        : "b"(_wid), "c"(_x), "d"(_y), "r"(text)
        : "memory"
    );
    return result;
}

/**
 * File Manager main entry point
 */
void file_manager_main_c() {
    syscall_puts("[FILE_MANAGER] Starting...\n");
    
    // Create main window (smaller, centered window)
    int window_id = ui_create_window(200, 100, 600, 400, "File Manager", 0);
    if (window_id < 0) {
        syscall_puts("[FILE_MANAGER] Failed to create window\n");
        while(1);
    }
    
    syscall_puts("[FILE_MANAGER] Window created\n");
    
    // Add some UI elements to test the window
    // Title label
    ui_create_label(window_id, 20, 10, "Current Directory: /");
    
    // Navigation buttons
    ui_create_button(window_id, 20, 40, 100, 40, "Up");
    ui_create_button(window_id, 130, 40, 100, 40, "Refresh");
    ui_create_button(window_id, 240, 40, 100, 40, "New Folder");
    
    // File list placeholders
    ui_create_label(window_id, 20, 100, "Documents/");
    ui_create_label(window_id, 20, 120, "Pictures/");
    ui_create_label(window_id, 20, 140, "Music/");
    ui_create_label(window_id, 20, 160, "Videos/");
    ui_create_label(window_id, 20, 180, "README.md");
    
    // Status bar at bottom
    ui_create_label(window_id, 20, 340, "5 items | 0 selected");
    
    syscall_puts("[FILE_MANAGER] UI created, entering event loop\n");
    
    // Event loop - wait for events (window close, button clicks, etc.)
    while (1) {
        // In a real implementation, we'd poll for events here
        // For now, just idle
        for (volatile int i = 0; i < 100000; i++);
    }
}
