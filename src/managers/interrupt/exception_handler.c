/* External functions */
extern void vga_print(const char *s);
extern void vga_clear(void);
extern void ring3_switch(unsigned int entry_point);
extern unsigned int sysman_entry_point;

static void print_hex(unsigned int val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[9 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[10] = '\0';
    vga_print(buf);
}

static const char* get_exception_name(unsigned int num) {
    switch(num) {
        case 0: return "Divide by Zero";
        case 6: return "Invalid Opcode";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        default: return "Unknown Exception";
    }
}

/* Handle user mode exception - restart sysman */
static void handle_user_exception(unsigned int exception_num, unsigned int error_code) {
    vga_print("\n[RING3 EXCEPTION #");
    print_hex(exception_num);
    vga_print("] ");
    vga_print(get_exception_name(exception_num));
    vga_print(" - Error Code: ");
    print_hex(error_code);
    vga_print("\n[RING3 EXCEPTION] Restarting sysman...\n");
    
    ring3_switch(sysman_entry_point);
}

/* Handle kernel mode exception - fatal */
static void handle_kernel_exception(unsigned int exception_num, unsigned int error_code, unsigned int eip) {
    vga_print("\n\n======================================\n");
    vga_print("  KERNEL PANIC - FATAL ERROR\n");
    vga_print("======================================\n\n");
    
    vga_print("Exception: ");
    vga_print(get_exception_name(exception_num));
    vga_print(" (#");
    print_hex(exception_num);
    vga_print(")\n");
    
    vga_print("Error Code: ");
    print_hex(error_code);
    vga_print("\n");
    
    vga_print("EIP: ");
    print_hex(eip);
    vga_print("\n\n");
    
    if (exception_num == 14) {
        unsigned int cr2;
        __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
        vga_print("Fault Address: ");
        print_hex(cr2);
        vga_print("\n");
    }
    
    vga_print("\nSystem Halted.\n");
    
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}

/* Main exception handler */
void exception_handler(unsigned int exception_num, unsigned int error_code) {
    /* Stack layout when we enter:
     * Assembly stub pushed: eax,ebx,ecx,edx,esi,edi,ebp (28 bytes)
     * Before that: exception_num, error_code (8 bytes)
     * Before that, CPU pushed: EIP, CS, EFLAGS (and ESP,SS if privilege change)
     * 
     * Current ESP points to saved EAX
     * ESP+28 = exception_num
     * ESP+32 = error_code  
     * ESP+36 = EIP (pushed by CPU)
     * ESP+40 = CS (pushed by CPU) <- We need this!
     */
    
    unsigned int cs, eip;
    
    __asm__ volatile(
        "movl 36(%%esp), %0\n"
        "movl 40(%%esp), %1"
        : "=r"(eip), "=r"(cs)
    );
    
    /* Check CS lowest 2 bits for privilege level */
    if (cs & 0x3) {
        /* Ring 3 - user mode exception */
        handle_user_exception(exception_num, error_code);
    } else {
        /* Ring 0 - kernel mode exception */
        handle_kernel_exception(exception_num, error_code, eip);
    }
}
