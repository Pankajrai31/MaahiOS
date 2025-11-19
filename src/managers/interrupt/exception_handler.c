/* External functions */
extern void vga_print(const char *s);
extern void vga_clear(void);
extern void vga_set_color(unsigned char fg, unsigned char bg);
extern void vga_print_at(int x, int y, const char *s);
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

static void print_hex_at(int x, int y, unsigned int val) {
    const char hex[] = "0123456789ABCDEF";
    char buf[11];
    buf[0] = '0';
    buf[1] = 'x';
    for (int i = 7; i >= 0; i--) {
        buf[9 - i] = hex[(val >> (i * 4)) & 0xF];
    }
    buf[10] = '\0';
    vga_print_at(x, y, buf);
}

static const char* get_exception_name(unsigned int num) {
    switch(num) {
        case 0: return "Divide by Zero";
        case 1: return "Debug Exception";
        case 2: return "Non-Maskable Interrupt";
        case 3: return "Breakpoint";
        case 4: return "Overflow";
        case 5: return "Bound Range Exceeded";
        case 6: return "Invalid Opcode";
        case 7: return "Device Not Available";
        case 8: return "Double Fault";
        case 9: return "Coprocessor Segment Overrun";
        case 10: return "Invalid TSS";
        case 11: return "Segment Not Present";
        case 12: return "Stack-Segment Fault";
        case 13: return "General Protection Fault";
        case 14: return "Page Fault";
        case 16: return "x87 FPU Error";
        case 17: return "Alignment Check";
        case 18: return "Machine Check";
        case 19: return "SIMD Floating-Point Exception";
        case 20: return "Virtualization Exception";
        case 30: return "Security Exception";
        default: return "Unknown Exception";
    }
}

static const char* get_exception_description(unsigned int num) {
    switch(num) {
        case 0: return "Attempt to divide by zero";
        case 6: return "CPU encountered invalid instruction";
        case 13: return "Segment violation or privilege error";
        case 14: return "Invalid memory access or page not present";
        default: return "Unknown error condition";
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

/* Handle kernel mode exception - fatal BLACKHOLE */
static void handle_kernel_exception(unsigned int exception_num, unsigned int error_code, unsigned int eip) {
    /* Get all CPU registers and state from stack */
    unsigned int *stack_ptr = (unsigned int *)__builtin_frame_address(0);
    unsigned int eax, ebx, ecx, edx, esi, edi, ebp, esp;
    unsigned int cr0, cr2, cr3;
    
    /* Read saved registers from stack (pushed by interrupt stub) */
    eax = stack_ptr[-7];  /* EAX saved first */
    ebx = stack_ptr[-6];
    ecx = stack_ptr[-5];
    edx = stack_ptr[-4];
    esi = stack_ptr[-3];
    edi = stack_ptr[-2];
    ebp = stack_ptr[-1];
    esp = (unsigned int)stack_ptr + 28;  /* ESP before push */
    
    /* Read control registers */
    __asm__ volatile("mov %%cr0, %0" : "=r"(cr0));
    __asm__ volatile("mov %%cr2, %0" : "=r"(cr2));
    __asm__ volatile("mov %%cr3, %0" : "=r"(cr3));
    
    /* Clear screen and setup BLACKHOLE display */
    vga_clear();
    
    /* Draw simple BLACKHOLE header */
    vga_set_color(7, 0);  /* Gray on Black */
    vga_print_at(0, 0, "================================================================================");
    vga_print_at(0, 1, "                                                                                ");
    vga_set_color(12, 0);  /* Light Red on Black */
    vga_print_at(32, 1, "  BLACKHOLE  ");
    vga_set_color(7, 0);
    vga_print_at(0, 2, "                                                                                ");
    vga_print_at(0, 3, "================================================================================");
    
    /* Exception Information Section */
    vga_set_color(14, 0);  /* Yellow on Black */
    vga_print_at(2, 5, "EXCEPTION INFORMATION:");
    
    vga_set_color(11, 0);  /* Cyan on Black */
    vga_print_at(4, 6, "Type:");
    vga_set_color(15, 0);  /* White on Black */
    vga_print_at(20, 6, get_exception_name(exception_num));
    
    vga_set_color(11, 0);
    vga_print_at(4, 7, "Number:");
    vga_set_color(15, 0);
    print_hex_at(20, 7, exception_num);
    
    vga_set_color(11, 0);
    vga_print_at(4, 8, "Error Code:");
    vga_set_color(15, 0);
    print_hex_at(20, 8, error_code);
    
    vga_set_color(11, 0);
    vga_print_at(4, 9, "Description:");
    vga_set_color(7, 0);  /* Gray on Black */
    vga_print_at(20, 9, get_exception_description(exception_num));
    
    /* CPU State Section */
    vga_set_color(14, 0);  /* Yellow on Black */
    vga_print_at(2, 11, "CPU STATE AT CRASH:");
    
    vga_set_color(10, 0);  /* Green on Black */
    vga_print_at(4, 12, "EIP:");
    print_hex_at(12, 12, eip);
    vga_print_at(26, 12, "EAX:");
    print_hex_at(34, 12, eax);
    vga_print_at(48, 12, "EBX:");
    print_hex_at(56, 12, ebx);
    
    vga_print_at(4, 13, "ECX:");
    print_hex_at(12, 13, ecx);
    vga_print_at(26, 13, "EDX:");
    print_hex_at(34, 13, edx);
    vga_print_at(48, 13, "ESI:");
    print_hex_at(56, 13, esi);
    
    vga_print_at(4, 14, "EDI:");
    print_hex_at(12, 14, edi);
    vga_print_at(26, 14, "EBP:");
    print_hex_at(34, 14, ebp);
    vga_print_at(48, 14, "ESP:");
    print_hex_at(56, 14, esp);
    
    /* Control Registers */
    vga_set_color(14, 0);
    vga_print_at(2, 16, "CONTROL REGISTERS:");
    vga_set_color(10, 0);
    vga_print_at(4, 17, "CR0:");
    print_hex_at(12, 17, cr0);
    vga_print_at(26, 17, "CR2:");
    print_hex_at(34, 17, cr2);
    vga_print_at(48, 17, "CR3:");
    print_hex_at(56, 17, cr3);
    
    /* Page Fault specific details */
    if (exception_num == 14) {
        vga_set_color(12, 0);  /* Light Red */
        vga_print_at(4, 18, "Page Fault Address:");
        vga_set_color(15, 0);
        print_hex_at(26, 18, cr2);
    }
    
    /* Footer */
    vga_set_color(12, 0);  /* Light Red */
    vga_print_at(2, 20, "The system has been halted to prevent data corruption.");
    vga_print_at(2, 21, "Please reboot your system.");
    
    /* Halt system */
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
