#ifndef SYSCALL_COMMON_H
#define SYSCALL_COMMON_H

#include <stdint.h>
#include "../syscall_numbers.h"

/**
 * Common Syscall Header
 * Shared types, helpers, and extern declarations used across all syscall domains
 */

// Forward declare UI structures (defined in kernel.c)
typedef struct UIWindow UIWindow;
typedef struct UIControl UIControl;

/* Serial debug functions */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    __asm__ volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

void serial_print(const char *str);
void serial_hex(unsigned char value);

/* Global graphics state - kernel manages colors for user programs */
extern uint32_t current_fg_color;
extern uint32_t current_bg_color;

/* Forward declarations of external kernel functions */
extern int current_process_id(void);
extern int scheduler_get_current_pid(void);

// VGA functions
extern void vga_putchar(char c);
extern void vga_putint(int num);
extern void vga_clear(void);
extern void vga_set_color(unsigned char fg, unsigned char bg);
extern void vga_draw_rect(int x, int y, int width, int height, unsigned char color);
extern void vga_print_at(int x, int y, const char *s);
extern void vga_set_cursor(int x, int y);
extern void vga_draw_box(int x, int y, int width, int height);

// GFX abstraction layer
extern void gfx_clear(uint32_t color);
extern void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);
extern void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);

// Mouse driver
extern int mouse_get_x(void);
extern int mouse_get_y(void);
extern uint8_t mouse_get_buttons(void);
extern int mouse_get_irq_total(void);
extern void irq_enable_mouse(void);
extern void mouse_drain_buffer(void);
extern void mouse_handler(void);

// IRQ manager
extern unsigned int irq_get_pic_mask(void);

// VMM functions
extern void *vmm_alloc_page(void);
extern void vmm_free_page(void *addr);
extern void *vmm_alloc_size(unsigned int size_bytes);

// Process manager
extern int process_create(uint32_t entry_point);
extern int process_terminate(int pid);
extern int process_manager_get_count(void);

// Module addresses
extern uint32_t orbit_module_address;
extern uint32_t uimanager_module_address;

// Application launchers
extern int launch_file_manager(void);
extern int launch_disk_manager(void);

// UIManager kernel functions
extern int uiman_create_window_kernel(int x, int y, int w, int h, const char *title, int parent, int owner_pid);
extern int uiman_create_button_kernel(int window_id, int x, int y, int w, int h, const char *text, int owner_pid);
extern int uiman_create_icon_kernel(int window_id, int x, int y, const char *text, int owner_pid);
extern int uiman_create_label_kernel(int window_id, int x, int y, const char *text, int owner_pid);
extern int uiman_create_panel_kernel(int window_id, int x, int y, int w, int h, int color_style, const char *text, int owner_pid);
extern int uiman_create_list_kernel(int window_id, int x, int y, int w, int h, const char *items, int owner_pid);
extern int uiman_poll_event_kernel(void *event_ptr, int calling_pid);
extern void *uiman_get_kernel_windows(void);
extern void *uiman_get_kernel_controls(void);
extern void *uiman_get_kernel_event_queues(void);
extern int uiman_update_control_text_kernel(int control_id, const char *text);
extern void uiman_set_window_icon_kernel(int window_id, const char *icon_name);
extern int uiman_find_window_by_title(const char *title);
extern int uiman_get_window_state(int window_id);
extern int uiman_restore_window(int window_id);
extern int uiman_focus_window(int window_id);

// Control framework functions
extern int control_create(int window_id, uint8_t type);
extern int control_set_position(int control_id, int x, int y);
extern int control_set_size(int control_id, int width, int height);
extern int control_set_parent(int control_id, int parent_id);
extern int control_set_colors(int control_id, uint32_t bg, uint32_t fg, uint32_t border);
extern int control_set_margins(int control_id, int left, int top, int right, int bottom);
extern void control_render(int control_id);
extern int panel_add_child(int panel_id, int child_id);
extern int panel_set_scrollable(int panel_id, uint8_t scrollable);
extern int table_set_dimensions(int table_id, int rows, int cols);
extern int table_set_column_width(int table_id, int col, int width);
extern int textbox_set_content(int textbox_id, const char *content);
extern const char *textbox_get_content(int textbox_id);

// Disk subsystem
extern int disk_subsystem_get_count(void);
extern void *disk_subsystem_get_disk(uint8_t index);
extern int disk_subsystem_read_sector(uint8_t disk_index, uint32_t lba, void *buffer);

// ISO9660 filesystem
extern int iso9660_get_file_count(void);
extern int iso9660_list_root(void *entries, int max_entries);
extern int iso9660_list_boot(void *entries, int max_entries);
extern int iso9660_list_directory(uint32_t dir_lba, uint32_t dir_size, void *entries, int max_entries);
extern uint32_t iso9660_get_root_lba(void);
extern uint32_t iso9660_get_root_size(void);
extern int iso9660_read_file(uint32_t file_lba, uint32_t file_size, void *buffer, uint32_t max_size);
extern int iso9660_find_and_read_file(uint32_t dir_lba, uint32_t dir_size, const char *filename, void *buffer, uint32_t max_size);

// Hardware cursor
extern int bga_cursor_is_supported(void);

#endif // SYSCALL_COMMON_H
