#include <stdint.h>

/* Multiboot header - Complete structure for module support */
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
    unsigned int pad[13];  // Skip to offset 88
    unsigned long long framebuffer_addr;
    unsigned int framebuffer_pitch;
    unsigned int framebuffer_width;
    unsigned int framebuffer_height;
    unsigned char framebuffer_bpp;
    unsigned char framebuffer_type;
} __attribute__((packed));

/* VGA driver functions */
void vga_clear(void);
void vga_print(const char *s);

/* BGA driver functions */
int bga_is_available(void);
int bga_init(uint16_t width, uint16_t height, uint16_t bpp);
void bga_clear(uint32_t color);
void bga_print(const char *str, uint32_t fg, uint32_t bg);
void bga_fill_rect(int x, int y, int width, int height, uint32_t color);
uint32_t bga_get_framebuffer_addr(void);
uint32_t bga_get_framebuffer_size(void);

/* GDT manager functions */
int gdt_init(void);
int gdt_load(void);

/* IDT manager functions */
int idt_init(void);
int idt_load(void);
int idt_install_exception_handlers(void);

/* PIC functions */
void pic_remap(void);

/* Ring 3 manager functions */
void ring3_switch(unsigned int entry_point);

/* Graphics functions */
void graphics_mode_13h(void);

/* VBE functions */
void vbe_init(void);
void vbe_clear(uint32_t color);
void vbe_print(const char *str, uint32_t fg, uint32_t bg);
uint32_t vbe_get_width(void);
uint32_t vbe_get_height(void);
uint32_t vbe_get_framebuffer_addr(void);
uint32_t vbe_get_framebuffer_size(void);

/* Global variable to pass orbit address to sysman */
unsigned int orbit_module_address = 0;

/* PMM functions */
int pmm_init(struct multiboot_info *mbi);

/* Paging functions */
int paging_init(struct multiboot_info *mbi);

/* PIT and Scheduler functions */
void pit_init(unsigned int frequency);
void scheduler_init();
int scheduler_create_task(void (*entry_point)(), const char *name);
void scheduler_enable();

/* VGA drawing functions */
extern void vga_set_color(unsigned char fg, unsigned char bg);
extern void vga_draw_box(int x, int y, int width, int height);
extern void vga_print_at(int x, int y, const char *s);

/* VMM functions */
void *vmm_alloc_page(void);
void vmm_free_page(void *addr);

unsigned int sysman_entry_point = 0;

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static void serial_print(const char *str) {
    while (*str) {
        while ((inb(0x3FD) & 0x20) == 0);
        outb(0x3F8, *str++);
    }
}

static void serial_hex(unsigned char value) {
    char hex[] = "0123456789ABCDEF";
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[(value >> 4) & 0xF]);
    while ((inb(0x3FD) & 0x20) == 0);
    outb(0x3F8, hex[value & 0xF]);
}

void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    // Print startup message via VGA
    extern void vga_print(const char *str);
    vga_print("Starting MaahiOS...\n");
    
    // Check for BGA hardware
    if (!bga_is_available()) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Get framebuffer info (use hardcoded address to avoid slow PCI scan)
    uint32_t fb_addr = 0xFD000000;  // QEMU default BGA framebuffer
    uint32_t fb_size = 1024 * 768 * 4;
    
    // Initialize PMM
    if (!pmm_init(mbi)) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Reserve framebuffer in PMM
    extern void pmm_mark_region_used(uint32_t start, uint32_t end);
    pmm_mark_region_used(fb_addr, fb_addr + fb_size);
    
    // Initialize paging
    if (!paging_init(mbi)) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Map framebuffer
    extern void identity_map_region(uint32_t *page_dir, uint32_t start, uint32_t end);
    extern uint32_t *kernel_page_directory;
    identity_map_region(kernel_page_directory, fb_addr, fb_addr + fb_size);
    
    // Initialize GDT
    if (!gdt_init() || !gdt_load()) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Initialize IDT
    if (!idt_init() || !idt_load()) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Initialize IRQ manager (remaps PIC)
    extern void irq_manager_init(void);
    irq_manager_init();
    
    // Install exception handlers
    if (!idt_install_exception_handlers()) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Install mouse IRQ handler (IRQ12)
    extern int idt_install_mouse_handler(void);
    idt_install_mouse_handler();
    
    // Initialize kernel heap
    extern void kheap_init(void);
    kheap_init();
    
    // Initialize process manager
    extern void process_manager_init(void);
    process_manager_init();
    
    // Initialize scheduler
    extern void scheduler_init(void);
    scheduler_init();
    
    // Initialize PIT timer (1000Hz = 1ms ticks for faster scheduling)
    extern void pit_init(unsigned int frequency);
    pit_init(1000);
    
    // Enable interrupts NOW - kernel does this ONCE
    __asm__ volatile("sti");
    
    // NOTE: Don't enable timer IRQ yet - wait until after process creation
    // Otherwise scheduler_tick() will fire before processes exist!
    
    // Initialize BGA (switch to graphics mode)
    if (!bga_init(1024, 768, 32)) {
        while(1) __asm__ volatile("hlt");
    }
    
    // Draw beautiful loading screen (visible during QEMU display init)
    bga_clear(0x001020);  // Dark blue background
    
    // Draw centered loading box (500x250 at center of 1024x768)
    int box_x = (1024 - 500) / 2;  // 262
    int box_y = (768 - 250) / 2;   // 259
    
    // Draw gradient-like border (multiple layers for depth effect)
    bga_fill_rect(box_x - 8, box_y - 8, 516, 266, 0x0055AA);  // Outer blue
    bga_fill_rect(box_x - 6, box_y - 6, 512, 262, 0x0077CC);  // Mid blue
    bga_fill_rect(box_x - 4, box_y - 4, 508, 258, 0x0099EE);  // Light blue
    bga_fill_rect(box_x - 2, box_y - 2, 504, 254, 0x00BBFF);  // Lighter blue
    bga_fill_rect(box_x, box_y, 500, 250, 0x001040);  // Dark center
    
    // Print loading messages centered in the box
    extern void bga_print_at(int x, int y, const char *str, uint32_t fg, uint32_t bg);
    bga_print_at(box_x + 140, box_y + 50, "M a a h i O S", 0xFFFFFFFF, 0x00001040);
    bga_print_at(box_x + 120, box_y + 100, "Loading system...", 0xFF00BBFF, 0x00001040);
    bga_print_at(box_x + 120, box_y + 140, "Initializing components", 0xFF888888, 0x00001040);
    bga_print_at(box_x + 120, box_y + 180, "Please wait...", 0xFF666666, 0x00001040);
    
    // Debug: Print to serial to see if we got here
    extern void serial_print(const char *str);
    serial_print("[KERNEL] Finished drawing loading screen\n");
    
    // Initialize PS/2 mouse driver AFTER BGA
    serial_print("[KERNEL] About to enable mouse IRQ\n");
    extern void irq_enable_mouse(void);
    irq_enable_mouse();
    
    // Check BOTH PIC masks after mouse enable
    unsigned char m1 = inb(0x21);
    unsigned char s1 = inb(0xA1);
    serial_print("[KERNEL] After mouse enable: master=");
    serial_hex(m1);
    serial_print(" slave=");
    serial_hex(s1);
    serial_print("\n");
    
    serial_print("[KERNEL] About to call mouse_init\n");
    extern int mouse_init(void);
    mouse_init();
    serial_print("[KERNEL] Mouse init completed\n");
    
    // Start Ring 3 processes
    serial_print("[KERNEL] About to create sysman process\n");
    serial_print("[KERNEL] Module count: ");
    serial_hex(mbi->mods_count);
    serial_print("\n");
    
    if (mbi->mods_count >= 2) {
        serial_print("[KERNEL] Loading modules...\n");
        serial_print("[KERNEL] mods_addr: 0x");
        serial_hex((mbi->mods_addr >> 24) & 0xFF);
        serial_hex((mbi->mods_addr >> 16) & 0xFF);
        serial_hex((mbi->mods_addr >> 8) & 0xFF);
        serial_hex(mbi->mods_addr & 0xFF);
        serial_print("\n");
        
        struct multiboot_module *modules = (struct multiboot_module *)mbi->mods_addr;
        serial_print("[KERNEL] Getting sysman address...\n");
        uint32_t sysman_addr = modules[0].mod_start;
        serial_print("[KERNEL] sysman at 0x");
        serial_hex((sysman_addr >> 24) & 0xFF);
        serial_hex((sysman_addr >> 16) & 0xFF);
        serial_hex((sysman_addr >> 8) & 0xFF);
        serial_hex(sysman_addr & 0xFF);
        serial_print("\n");
        
        serial_print("[KERNEL] Getting orbit address...\n");
        uint32_t orbit_addr = modules[1].mod_start;
        uint32_t orbit_end = modules[1].mod_end;
        uint32_t orbit_size = orbit_end - orbit_addr;
        serial_print("[KERNEL] orbit at 0x");
        serial_hex((orbit_addr >> 24) & 0xFF);
        serial_hex((orbit_addr >> 16) & 0xFF);
        serial_hex((orbit_addr >> 8) & 0xFF);
        serial_hex(orbit_addr & 0xFF);
        serial_print(" size=");
        serial_hex((orbit_size >> 24) & 0xFF);
        serial_hex((orbit_size >> 16) & 0xFF);
        serial_hex((orbit_size >> 8) & 0xFF);
        serial_hex(orbit_size & 0xFF);
        serial_print("\n");
        
        // Copy orbit to its linked address (0x00300000)
        serial_print("[KERNEL] Copying orbit to 0x00300000...\n");
        uint8_t *src = (uint8_t *)orbit_addr;
        uint8_t *dst = (uint8_t *)0x00300000;
        for (uint32_t i = 0; i < orbit_size; i++) {
            dst[i] = src[i];
        }
        serial_print("[KERNEL] Orbit copied\n");
        
        extern unsigned int orbit_module_address;
        orbit_module_address = 0x00300000;  // Use the copied location
        
        // Disable interrupts before process creation to prevent timer from firing
        serial_print("[KERNEL] Disabling interrupts for process creation...\n");
        __asm__ volatile("cli");
        
        // Enable scheduler (doesn't need interrupts enabled)
        serial_print("[KERNEL] Enabling scheduler...\n");
        extern void scheduler_enable(void);
        scheduler_enable();
        
        // Enable timer IRQ in PIC (but interrupts are disabled so it won't fire yet)
        serial_print("[KERNEL] Enabling timer IRQ in PIC...\n");
        extern void irq_enable_timer(void);
        irq_enable_timer();
        
        // Check BOTH PIC masks after timer enable
        unsigned char m2 = inb(0x21);
        unsigned char s2 = inb(0xA1);
        serial_print("[KERNEL] After timer enable: master=");
        serial_hex(m2);
        serial_print(" slave=");
        serial_hex(s2);
        serial_print("\n");
        
        serial_print("[KERNEL] Calling process_create_sysman...\n");
        extern int process_create_sysman(unsigned int address);
        int result = process_create_sysman(sysman_addr);
        serial_print("[KERNEL] ERROR: process_create_sysman returned!\n");
    } else {
        serial_print("[KERNEL] ERROR: No modules loaded by bootloader!\n");
    }
    
    serial_print("[KERNEL] Entering idle loop\n");
    while(1) {
        asm volatile("hlt");
    }
}
