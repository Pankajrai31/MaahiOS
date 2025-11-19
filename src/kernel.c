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
};

/* VGA driver functions */
void vga_clear(void);
void vga_print(const char *s);

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

/* Global variable for exception handler to restart sysman */
unsigned int sysman_entry_point = 0;

/* Helper function to print hex */
static void print_hex(unsigned int val) {
    char hex[9];
    hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        unsigned int digit = val & 0xF;
        hex[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        val >>= 4;
    }
    vga_print(hex);
}

/* Port I/O functions */
static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

/* Remap PIC to avoid conflict with CPU exceptions */
void pic_remap(void) {
    /* Save masks */
    unsigned char mask1 = inb(0x21);
    unsigned char mask2 = inb(0xA1);
    
    /* Start initialization */
    outb(0x20, 0x11);  /* ICW1: Init + ICW4 */
    outb(0xA0, 0x11);
    
    /* ICW2: Vector offsets (remap IRQ 0-7 to INT 32-39, IRQ 8-15 to INT 40-47) */
    outb(0x21, 0x20);  /* Master PIC starts at 32 */
    outb(0xA1, 0x28);  /* Slave PIC starts at 40 */
    
    /* ICW3: Cascading */
    outb(0x21, 0x04);  /* Master: Slave on IRQ2 */
    outb(0xA1, 0x02);  /* Slave: Cascade identity */
    
    /* ICW4: 8086 mode */
    outb(0x21, 0x01);
    outb(0xA1, 0x01);
    
    /* Restore masks */
    outb(0x21, mask1);
    outb(0xA1, mask2);
}

void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    vga_clear();
    vga_print("=== MaahiOS Kernel v1.1 ===\n\n");
    
    /* Initialize GDT */
    if (!gdt_init()) {
        vga_print("[FAIL] GDT initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    if (!gdt_load()) {
        vga_print("[FAIL] GDT load failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize IDT */
    if (!idt_init()) {
        vga_print("[FAIL] IDT initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    if (!idt_install_exception_handlers()) {
        vga_print("[FAIL] Exception handler installation failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    if (!idt_load()) {
        vga_print("[FAIL] IDT load failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Remap PIC to avoid conflict with exceptions */
    pic_remap();
    
    /* Initialize Physical Memory Manager */
    if (!pmm_init(mbi)) {
        vga_print("[FAIL] PMM initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize Paging */
    if (!paging_init(mbi)) {
        vga_print("[FAIL] Paging initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize kernel heap */
    extern void kheap_init(void);
    kheap_init();
    
    /* Initialize process manager */
    extern void process_manager_init(void);
    process_manager_init();
    
    /* Initialize Scheduler (stub for now) */
    extern void scheduler_init(void);
    scheduler_init();
    
    /* Initialize PIT at 100 Hz (10ms intervals - reduces overhead) */
    extern void pit_init(unsigned int frequency);
    pit_init(100);
    
    /* Enable interrupts */
    asm volatile("sti");
    
    /* Enable timer interrupts (unmask IRQ 0) */
    unsigned char mask = inb(0x21);
    mask &= ~0x01;  /* Unmask IRQ 0 */
    outb(0x21, mask);
    
    /* Enable scheduler (even with one process, to test multitasking infrastructure) */
    extern void scheduler_enable(void);
    scheduler_enable();
    
    /* Start sysman and orbit (Ring 3 processes) */
    vga_print("Kernel ready. Starting processes...\n\n");
    
    /* Create processes from GRUB modules */
    if (mbi->mods_count >= 2) {
        struct multiboot_module *modules = (struct multiboot_module *)mbi->mods_addr;
        
        /* Module 0: sysman.bin */
        uint32_t sysman_addr = modules[0].mod_start;
        
        /* Module 1: orbit.bin - pass address to sysman */
        uint32_t orbit_addr = modules[1].mod_start;
        
        /* Store orbit address for sysman to access */
        extern unsigned int orbit_module_address;
        orbit_module_address = orbit_addr;
        
        /* Create sysman process via process manager */
        extern int process_create_sysman(unsigned int address);
        
        /* Create and start sysman (PID 1) - NEVER RETURNS */
        process_create_sysman(sysman_addr);
        
        /* Should never reach here */
        vga_print("[ERROR] Impossible - sysman returned!\n");
    } else {
        vga_print("[FAIL] Need at least 2 modules (sysman + orbit)!\n");
    }
    
    /* Kernel idle loop */
    while(1) {
        asm volatile("hlt");
    }
}
