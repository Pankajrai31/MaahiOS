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

/* Global variables for module addresses */
unsigned int sysman_entry_point = 0;
unsigned int uimanager_module_address = 0;
unsigned int orbit_module_address = 0;

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

// ===== KERNEL-SIDE UI MANAGER STATE =====
// This is the single source of truth for all UI elements
// Both Orbit and UIManager access this through syscalls

#define MAX_WINDOWS 32
#define MAX_CONTROLS 256
#define MAX_PROCESSES 64
#define EVENT_QUEUE_SIZE 32

// Control types
#define UIMAN_CONTROL_BUTTON  1
#define UIMAN_CONTROL_LABEL   2
#define UIMAN_CONTROL_TEXTBOX 3
#define UIMAN_CONTROL_TABLE   4
#define UIMAN_CONTROL_RADIO   5

// Control states
#define UIMAN_STATE_NORMAL    0
#define UIMAN_STATE_HOVER     1
#define UIMAN_STATE_PRESSED   2

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

typedef struct {
    int active;
    int id;
    int parent_id;
    int owner_pid;
    int x, y, width, height;
    int z_order;
    int visible;
    int focused;
    char title[64];
} UIWindow;

typedef struct {
    int active;
    int id;
    int window_id;
    int owner_pid;
    int type;
    int x, y, width, height;
    int state;
    char text[128];
} UIControl;

typedef struct {
    uiman_event_t events[EVENT_QUEUE_SIZE];
    volatile int head;
    volatile int tail;
    volatile int count;
} EventQueue;

// Global UI state in kernel
static UIWindow g_kernel_windows[MAX_WINDOWS] = {0};
static UIControl g_kernel_controls[MAX_CONTROLS] = {0};
static EventQueue g_kernel_event_queues[MAX_PROCESSES] = {0};
static volatile int g_next_window_id = 1;
static volatile int g_next_control_id = 1;

// Kernel-side UI functions (called by syscalls)
static int find_free_window(void) {
    for (int i = 0; i < MAX_WINDOWS; i++) {
        if (!g_kernel_windows[i].active) return i;
    }
    return -1;
}

static int find_free_control(void) {
    for (int i = 0; i < MAX_CONTROLS; i++) {
        if (!g_kernel_controls[i].active) return i;
    }
    return -1;
}

static void strcpy_safe(char *dest, const char *src, int max_len) {
    int i = 0;
    while (i < max_len - 1 && src[i] != '\0') {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

int uiman_create_window_kernel(int x, int y, int w, int h, const char *title, int parent, int owner_pid) {
    int slot = find_free_window();
    if (slot < 0) return -1;
    
    int window_id = g_next_window_id++;
    
    g_kernel_windows[slot].active = 1;
    g_kernel_windows[slot].id = window_id;
    g_kernel_windows[slot].parent_id = parent;
    g_kernel_windows[slot].owner_pid = owner_pid;
    g_kernel_windows[slot].x = x;
    g_kernel_windows[slot].y = y;
    g_kernel_windows[slot].width = w;
    g_kernel_windows[slot].height = h;
    g_kernel_windows[slot].z_order = 0;
    g_kernel_windows[slot].visible = 1;
    g_kernel_windows[slot].focused = 0;
    strcpy_safe(g_kernel_windows[slot].title, title, 64);
    
    return window_id;
}

int uiman_create_button_kernel(int window_id, int x, int y, int w, int h, const char *text, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_BUTTON;
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = w;
    g_kernel_controls[slot].height = h;
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    strcpy_safe(g_kernel_controls[slot].text, text, 128);
    
    // DEBUG: Print what text was stored
    serial_print("[KERNEL] Button created: text='");
    serial_print(text);
    serial_print("' stored='");
    serial_print(g_kernel_controls[slot].text);
    serial_print("'\n");
    
    return control_id;
}

int uiman_create_label_kernel(int window_id, int x, int y, const char *text, int owner_pid) {
    int slot = find_free_control();
    if (slot < 0) return -1;
    
    int control_id = g_next_control_id++;
    
    g_kernel_controls[slot].active = 1;
    g_kernel_controls[slot].id = control_id;
    g_kernel_controls[slot].window_id = window_id;
    g_kernel_controls[slot].owner_pid = owner_pid;
    g_kernel_controls[slot].type = UIMAN_CONTROL_LABEL;
    g_kernel_controls[slot].x = x;
    g_kernel_controls[slot].y = y;
    g_kernel_controls[slot].width = 0;  // Auto-size
    g_kernel_controls[slot].height = 0;
    g_kernel_controls[slot].state = UIMAN_STATE_NORMAL;
    strcpy_safe(g_kernel_controls[slot].text, text, 128);
    
    return control_id;
}

int uiman_poll_event_kernel(void *event_ptr, int calling_pid) {
    if (calling_pid < 0 || calling_pid >= MAX_PROCESSES) return 0;
    
    EventQueue *queue = &g_kernel_event_queues[calling_pid];
    if (queue->count == 0) return 0;
    
    // Copy event to user space
    uiman_event_t *dest = (uiman_event_t*)event_ptr;
    *dest = queue->events[queue->head];
    
    queue->head = (queue->head + 1) % EVENT_QUEUE_SIZE;
    queue->count--;
    
    return 1;
}

UIWindow* uiman_get_kernel_windows(void) {
    return g_kernel_windows;
}

UIControl* uiman_get_kernel_controls(void) {
    return g_kernel_controls;
}

EventQueue* uiman_get_kernel_event_queues(void) {
    return g_kernel_event_queues;
}

// ===== END KERNEL UI MANAGER =====

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
    
    // Initialize PIT timer (50Hz = 20ms ticks for responsive but not aggressive preemption)
    // Lower frequency allows graphics operations to complete without constant interruption
    extern void pit_init(unsigned int frequency);
    pit_init(50);
    
    // DON'T enable interrupts yet - wait until after process creation
    // NOTE: Interrupts will be enabled after process_create() is called
    
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
    
    if (mbi->mods_count >= 3) {
        serial_print("[KERNEL] Loading modules...\n");
        serial_print("[KERNEL] mods_addr: 0x");
        serial_hex((mbi->mods_addr >> 24) & 0xFF);
        serial_hex((mbi->mods_addr >> 16) & 0xFF);
        serial_hex((mbi->mods_addr >> 8) & 0xFF);
        serial_hex(mbi->mods_addr & 0xFF);
        serial_print("\n");
        
        struct multiboot_module *modules = (struct multiboot_module *)mbi->mods_addr;
        serial_print("[KERNEL] Getting sysman address...\n");
        
        // WORKAROUND: GRUB's mod_start has a 0x33 byte offset for some reason
        // Subtract it to get the actual module start
        volatile uint32_t sysman_addr = (modules[0].mod_start & 0xFFFFF000);
        
        serial_print("[KERNEL] sysman module at 0x");
        serial_hex((sysman_addr >> 24) & 0xFF);
        serial_hex((sysman_addr >> 16) & 0xFF);
        serial_hex((sysman_addr >> 8) & 0xFF);
        serial_hex(sysman_addr & 0xFF);
        serial_print("\n");
        
        serial_print("[KERNEL] Getting uimanager address...\n");
        uint32_t uimanager_addr = modules[1].mod_start;
        uint32_t uimanager_end = modules[1].mod_end;
        uint32_t uimanager_size = uimanager_end - uimanager_addr;
        serial_print("[KERNEL] uimanager at 0x");
        serial_hex((uimanager_addr >> 24) & 0xFF);
        serial_hex((uimanager_addr >> 16) & 0xFF);
        serial_hex((uimanager_addr >> 8) & 0xFF);
        serial_hex(uimanager_addr & 0xFF);
        serial_print(" size=");
        serial_hex((uimanager_size >> 24) & 0xFF);
        serial_hex((uimanager_size >> 16) & 0xFF);
        serial_hex((uimanager_size >> 8) & 0xFF);
        serial_hex(uimanager_size & 0xFF);
        serial_print("\n");
        
        // Copy uimanager to its linked address (0x00280000)
        serial_print("[KERNEL] Copying uimanager to 0x00280000...\n");
        uint8_t *src_ui = (uint8_t *)uimanager_addr;
        uint8_t *dst_ui = (uint8_t *)0x00280000;
        for (uint32_t i = 0; i < uimanager_size; i++) {
            dst_ui[i] = src_ui[i];
        }
        serial_print("[KERNEL] UIManager copied\n");
        
        extern unsigned int uimanager_module_address;
        uimanager_module_address = 0x00280000;  // Use the copied location
        
        serial_print("[KERNEL] Getting orbit address...\n");
        uint32_t orbit_addr = modules[2].mod_start;
        uint32_t orbit_end = modules[2].mod_end;
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
        
        // Disable interrupts before process creation
        serial_print("[KERNEL] Disabling interrupts for process creation...\n");
        __asm__ volatile("cli");
        
        // Enable scheduler
        serial_print("[KERNEL] Enabling scheduler...\n");
        extern void scheduler_enable(void);
        scheduler_enable();
        
        serial_print("[KERNEL] Creating sysman process (PID 1)...\n");
        
        extern int process_create(uint32_t entry_point);
        int sysman_pid = process_create(sysman_addr);  // sysman at masked address
        
        serial_print("[KERNEL] process_create() returned PID: ");
        serial_hex(sysman_pid);
        serial_print("\n");
        
        if (sysman_pid < 0) {
            serial_print("[KERNEL] ERROR: Failed to create sysman!\n");
            while(1) asm volatile("hlt");
        }
        
        // NOW enable timer IRQ after process is created
        serial_print("[KERNEL] Enabling timer IRQ in PIC...\n");
        extern void irq_enable_timer(void);
        irq_enable_timer();
        
        serial_print("[KERNEL] Enabling interrupts (timer will start multitasking)...\n");
        __asm__ volatile("sti");
        
        serial_print("[KERNEL] Entering idle loop (scheduler controls execution)\n");
    } else {
        serial_print("[KERNEL] ERROR: No modules loaded by bootloader!\n");
    }
    
    serial_print("[KERNEL] Entering idle loop (PID 0)\n");
    while(1) {
        asm volatile("hlt");  /* Wait for interrupts, scheduler runs processes */
    }
}
