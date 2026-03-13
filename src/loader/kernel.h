#ifndef KERNEL_H
#define KERNEL_H

#include <stdint.h>

/*=============================================================================
 * MaahiOS Kernel - Main Header
 * Declares all kernel subsystem functions used by kernel.c
 *===========================================================================*/

/* Multiboot structures */
struct multiboot_module {
    unsigned int mod_start;
    unsigned int mod_end;
    char *string;
    unsigned int reserved;
};

struct multiboot_info {
    unsigned int flags;
    unsigned int mem_lower;
    unsigned int mem_upper;
    unsigned int boot_device;
    unsigned int cmdline;
    unsigned int mods_count;
    unsigned int mods_addr;
    unsigned int pad[13];
    unsigned long long framebuffer_addr;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
} __attribute__((packed));

/* Serial port functions */
void serial_print(const char *str);
void serial_hex(unsigned char value);

/* VGA driver */
void vga_clear(void);
void vga_print(const char *s);
void vga_set_color(unsigned char fg, unsigned char bg);
void vga_print_at(int x, int y, const char *s);

/* BGA/Graphics driver */
int bga_is_available(void);
int bga_init(uint16_t width, uint16_t height, uint16_t bpp);
void bga_clear(uint32_t color);
void bga_print(const char *str, uint32_t fg, uint32_t bg);
void bga_fill_rect(int x, int y, int width, int height, uint32_t color);
uint32_t bga_get_framebuffer_addr(void);
uint32_t bga_get_framebuffer_size(void);
void bga_cursor_init(void);
void bga_cursor_enable(int enable);
int bga_cursor_is_supported(void);

/* VBE (VESA) driver — uses GRUB's framebuffer */
int vbe_is_available(void);
uint32_t vbe_get_framebuffer_addr(void);

/* Graphics abstraction layer */
int gfx_init(uint16_t width, uint16_t height, uint16_t bpp);
void gfx_clear(uint32_t color);
void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);
void gfx_draw_string(int x, int y, const char *str, uint32_t fg, uint32_t bg);

/* GDT manager */
int gdt_init(void);
int gdt_load(void);

/* IDT manager */
int idt_init(void);
int idt_load(void);
int idt_install_exception_handlers(void);
int idt_install_mouse_handler(void);

/* IRQ manager */
int irq_manager_init(void);
void irq_enable_mouse(void);
void irq_enable_timer(void);

/* Memory managers */
int pmm_init(struct multiboot_info *mbi);
int paging_init(struct multiboot_info *mbi);
void pmm_mark_region_used(uint32_t start, uint32_t end);
void identity_map_region(uint32_t *page_dir, uint32_t start, uint32_t end);
int kheap_init(void);

/* Kernel logger */
void klog_manager_init(void);

/* Process manager */
int process_manager_init(void);
int process_create(uint32_t entry_point);

/* Shared Memory manager */
int shm_manager_init(void);

/* Cell manager (key-value store) */
int cell_manager_init(void);

/* GRUB module manager */
int grub_module_manager_init(uint32_t mods_count, uint32_t mods_addr);

/* Time manager */
int time_manager_init(void);

/* Syscall manager */
int syscall_manager_init(void);

/* Scheduler */
int scheduler_init(void);
void scheduler_enable(void);

/* Timer */
int pit_init(unsigned int frequency);

/* Input drivers */
int mouse_init(void);
int keyboard_init(void);

/* Disk subsystem → Storage stack */
int disk_init(void);
int partdrive_init(void);
int voldrive_init(void);

/* Global variables for module addresses */
extern unsigned int orbit_module_address;
extern uint32_t *kernel_page_directory;

#endif /* KERNEL_H */
