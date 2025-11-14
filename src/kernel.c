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
void gdt_init(void);
void gdt_load(void);

/* IDT manager functions */
void idt_init(void);
void idt_load(void);
void idt_install_exception_handlers(void);

/* Ring 3 manager functions */
void ring3_switch(unsigned int entry_point);

/* Graphics functions */
void graphics_mode_13h(void);

/* PMM functions */
void pmm_init(struct multiboot_info *mbi);

/* Paging functions */
void paging_init(struct multiboot_info *mbi);

/* VMM functions */
void *vmm_alloc_page(void);
void vmm_free_page(void *addr);

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

void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    vga_clear();
    vga_print("=== MaahiOS Kernel Starting ===\n\n");
    
    /* Setup GDT and IDT */
    gdt_init();
    gdt_load();
    idt_init();
    idt_install_exception_handlers();
    idt_load();
    
    /* Initialize Physical Memory Manager */
    pmm_init(mbi);
    
    /* Initialize Paging */
    paging_init(mbi);
    vga_print("\n");
    
    /* Switch to graphics mode before entering Ring 3 */
    vga_print("Switching to graphics mode...\n");
    graphics_mode_13h();
    
    /* Switch to Ring 3 and run sysman */
    if (mbi->flags & 0x8 && mbi->mods_count > 0) {
        struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
        ring3_switch(mod[0].mod_start);
    }
    
    /* Should never reach here */
    while(1) __asm__ volatile("hlt");
}
