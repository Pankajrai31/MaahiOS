/* VGA printing */
extern void vga_print(const char *s);

static void print_hex(unsigned int val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[10];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[9 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[10] = '\0';
    vga_print(buf);
}

/* Exception handler called from assembly stubs */
void exception_handler(unsigned int exception_num, unsigned int error_code) {
    vga_print("\n\n!!! EXCEPTION !!!\n");
    vga_print("Raw exception_num param: ");
    print_hex(exception_num);
    vga_print("\n");
    vga_print("Raw error_code param: ");
    print_hex(error_code);
    vga_print("\n");
    
    vga_print("Exception #");
    print_hex(exception_num);
    vga_print(" Error Code: ");
    print_hex(error_code);
    vga_print("\n");
    
    /* Page fault (exception 14) - print CR2 (faulting address) */
    if (exception_num == 14) {
        unsigned int cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        vga_print("Page Fault Address: ");
        print_hex(cr2);
        vga_print("\n");
        
        vga_print("Fault Type: ");
        if (error_code & 0x1) vga_print("PROTECTION-VIOLATION ");
        else vga_print("NOT-PRESENT ");
        if (error_code & 0x2) vga_print("WRITE ");
        else vga_print("READ/EXEC ");
        if (error_code & 0x4) vga_print("USER ");
        else vga_print("KERNEL ");
        if (error_code & 0x10) vga_print("INSTR-FETCH");
        vga_print("\n");
    }
    
    /* Print register state */
    unsigned int esp, ebp;
    __asm__ volatile("mov %%esp, %0" : "=r"(esp));
    __asm__ volatile("mov %%ebp, %0" : "=r"(ebp));
    
    vga_print("ESP: ");
    print_hex(esp);
    vga_print(" EBP: ");
    print_hex(ebp);
    vga_print("\n");
    
    vga_print("\nSystem Halted.\n");
    
    /* Halt */
    while(1) {
        __asm__ volatile("hlt");
    }
}
