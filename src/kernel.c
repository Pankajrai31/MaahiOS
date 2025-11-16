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

/* Ring 3 manager functions */
void ring3_switch(unsigned int entry_point);

/* Graphics functions */
void graphics_mode_13h(void);

/* PMM functions */
int pmm_init(struct multiboot_info *mbi);

/* Paging functions */
int paging_init(struct multiboot_info *mbi);

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

void kernel_main(unsigned int magic, struct multiboot_info *mbi) {
    vga_clear();
    vga_print("=== MaahiOS Kernel v1.1 ===\n\n");
    
    /* Initialize GDT */
    if (gdt_init()) {
        vga_print("[OK] GDT initialized\n");
    } else {
        vga_print("[FAIL] GDT initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    if (gdt_load()) {
        vga_print("[OK] GDT loaded\n");
    } else {
        vga_print("[FAIL] GDT load failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize IDT */
    if (idt_init()) {
        vga_print("[OK] IDT initialized\n");
    } else {
        vga_print("[FAIL] IDT initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    if (idt_install_exception_handlers()) {
        vga_print("[OK] Exception handlers installed\n");
    } else {
        vga_print("[FAIL] Exception handler installation failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    if (idt_load()) {
        vga_print("[OK] IDT loaded\n");
    } else {
        vga_print("[FAIL] IDT load failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize Physical Memory Manager */
    if (pmm_init(mbi)) {
        vga_print("[OK] PMM initialized\n");
    } else {
        vga_print("[FAIL] PMM initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    /* Initialize Paging */
    if (paging_init(mbi)) {
        vga_print("[OK] Paging enabled\n");
    } else {
        vga_print("[FAIL] Paging initialization failed\n");
        while(1) __asm__ volatile("hlt");
    }
    
    vga_print("\n[INFO] Kernel initialization complete.\n");
    vga_print("[INFO] Starting sysman (Ring 3)...\n");
    
    /* Switch to Ring 3 and run sysman */
    if (mbi->flags & 0x8 && mbi->mods_count > 0) {
        struct multiboot_module *mod = (struct multiboot_module *)mbi->mods_addr;
        sysman_entry_point = mod[0].mod_start;
        
        vga_print("[DEBUG] Sysman loaded at: 0x");
        print_hex(sysman_entry_point);
        vga_print("\n[DEBUG] Sysman size: ");
        print_hex(mod[0].mod_end - mod[0].mod_start);
        vga_print(" bytes\n");
        vga_print("[DEBUG] About to switch to Ring 3...\n");
        
        ring3_switch(sysman_entry_point);
        
        vga_print("[ERROR] Ring 3 switch returned - should never happen!\n");
    }
    
    /* Should never reach here */
    while(1) __asm__ volatile("hlt");
}
