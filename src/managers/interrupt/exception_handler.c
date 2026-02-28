/* MaahiOS Exception Handler
 * Handles CPU exceptions for both Ring 0 (kernel) and Ring 3 (user) modes
 * Ring 0 exceptions: BLACKHOLE screen, system halt
 * Ring 3 exceptions: Terminate faulting process, continue running
 */

#include "../klog/klog.h"

/* External functions */
extern void vga_print(const char *s);
extern void vga_clear(void);
extern void vga_set_color(unsigned char fg, unsigned char bg);
extern void vga_print_at(int x, int y, const char *s);
extern unsigned int sysman_entry_point;

/* Process management */
extern int scheduler_get_current_pid(void);
extern int process_terminate(int pid);
extern void scheduler_remove_process(int pid);

/* Serial output helpers - use proper x86 I/O port instructions */
static inline unsigned char exc_inb(unsigned short port) {
    unsigned char val;
    __asm__ volatile("inb %1, %0" : "=a"(val) : "Nd"(port));
    return val;
}
static inline void exc_outb(unsigned short port, unsigned char val) {
    __asm__ volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}
static void serial_putc(char c) {
    while ((exc_inb(0x3FD) & 0x20) == 0);  /* Wait for transmit buffer empty */
    exc_outb(0x3F8, c);
}

static void serial_puts(const char *s) {
    while (*s) serial_putc(*s++);
}

static void serial_hex(unsigned int val) {
    const char hex[] = "0123456789ABCDEF";
    serial_puts("0x");
    for (int i = 7; i >= 0; i--) {
        serial_putc(hex[(val >> (i * 4)) & 0xF]);
    }
}

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

/* Handle user mode exception - TERMINATE PROCESS, CONTINUE RUNNING */
static void handle_user_exception(unsigned int exception_num, unsigned int error_code,
                                   unsigned int eip, unsigned int cr2) {
    /* Get faulting process info */
    int pid = scheduler_get_current_pid();
    
    /* Log the exception using klog */
    serial_puts("\n");
    serial_puts("======================================================================\n");
    serial_puts("          USER MODE EXCEPTION - PROCESS TERMINATED                   \n");
    serial_puts("======================================================================\n");
    
    serial_puts("  Exception: ");
    serial_puts(get_exception_name(exception_num));
    serial_puts(" (#");
    serial_hex(exception_num);
    serial_puts(")\n");
    
    serial_puts("  Process:   PID ");
    serial_hex(pid);
    serial_puts("\n");
    
    serial_puts("  EIP:       ");
    serial_hex(eip);
    serial_puts("\n");
    
    serial_puts("  Error:     ");
    serial_hex(error_code);
    serial_puts("\n");
    
    if (exception_num == 14) {
        /* Page fault - show CR2 (faulting address) */
        serial_puts("  CR2:       ");
        serial_hex(cr2);
        serial_puts(" (Faulting Address)\n");
        
        /* Decode error code */
        serial_puts("  Cause:     ");
        if (error_code & 0x1) {
            serial_puts("Protection violation");
        } else {
            serial_puts("Page not present");
        }
        if (error_code & 0x2) {
            serial_puts(", Write access");
        } else {
            serial_puts(", Read access");
        }
        if (error_code & 0x4) {
            serial_puts(", User mode");
        }
        serial_puts("\n");
    }
    
    serial_puts("======================================================================\n\n");
    
    /* Log via klog for ring buffer storage */
    klog(LOG_ERROR, "EXCEPT", "User process crashed");
    klog_hex(LOG_ERROR, "EXCEPT", "  Exception #", exception_num);
    klog_hex(LOG_ERROR, "EXCEPT", "  PID: ", pid);
    klog_hex(LOG_ERROR, "EXCEPT", "  EIP: ", eip);
    if (exception_num == 14) {
        klog_hex(LOG_ERROR, "EXCEPT", "  CR2: ", cr2);
    }
    
    /* Terminate the faulting process */
    klog(LOG_INFO, "EXCEPT", "Terminating faulting process...");
    
    /* Remove from scheduler first */
    scheduler_remove_process(pid);
    klog(LOG_INFO, "EXCEPT", "Process removed from scheduler");
    
    /* Now terminate (free resources) */
    int result = process_terminate(pid);
    
    if (result == 0) {
        klog(LOG_INFO, "EXCEPT", "Process terminated successfully");
    } else {
        klog_hex(LOG_ERROR, "EXCEPT", "process_terminate returned: ", result);
    }
    
    /* Re-enable interrupts and wait for timer to schedule next process */
    klog(LOG_INFO, "EXCEPT", "Waiting for scheduler to pick next process...");
    
    /* Enable interrupts and halt - timer IRQ will wake us and schedule next process */
    while(1) {
        __asm__ volatile("sti; hlt");
    }
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
    
    /* *** DUMP EXCEPTION DETAILS TO SERIAL *** */
    serial_puts("\n\n======================================================================\n");
    serial_puts("          KERNEL MODE EXCEPTION - BLACKHOLE\n");
    serial_puts("======================================================================\n");
    serial_puts("  Exception: ");
    serial_puts(get_exception_name(exception_num));
    serial_puts(" (#");
    serial_hex(exception_num);
    serial_puts(")\n");
    serial_puts("  Error Code: ");
    serial_hex(error_code);
    serial_puts("\n");
    serial_puts("  EIP: ");
    serial_hex(eip);
    serial_puts("\n");
    serial_puts("  CR2: ");
    serial_hex(cr2);
    serial_puts("\n");
    serial_puts("  CR3: ");
    serial_hex(cr3);
    serial_puts("\n");
    serial_puts("  EAX: ");
    serial_hex(eax);
    serial_puts("  EBX: ");
    serial_hex(ebx);
    serial_puts("\n");
    serial_puts("  ECX: ");
    serial_hex(ecx);
    serial_puts("  EDX: ");
    serial_hex(edx);
    serial_puts("\n");
    if (exception_num == 14) {
        serial_puts("  Page Fault: ");
        if (error_code & 0x1) serial_puts("Protection");
        else serial_puts("Not-Present");
        if (error_code & 0x2) serial_puts(", Write");
        else serial_puts(", Read");
        if (error_code & 0x4) serial_puts(", User");
        else serial_puts(", Kernel");
        serial_puts("\n");
    }
    serial_puts("======================================================================\n");

    /* *** DUMP KLOG BEFORE HALTING *** */
    serial_puts("\n[EXCEPTION] Dumping kernel log before halt...\n");
    extern void klog_dump(void);
    klog_dump();  /* Dumps entire log buffer to serial */
    serial_puts("[EXCEPTION] Log dump complete. System halted.\n\n");
    
    /* Halt system */
    while(1) {
        __asm__ volatile("cli; hlt");
    }
}

/* Main exception handler */
void exception_handler(unsigned int dummy1, unsigned int dummy2) {
    /* NOTE: dummy1/dummy2 are NOT the real exception_num/error_code!
     * The assembly stub doesn't set up C calling convention properly.
     * We must read values directly from the stack using known offsets.
     * 
     * Stack layout (relative to EBP after compiler prologue):
     * [EBP+0]  = saved EBP (compiler prologue)
     * [EBP+4]  = return address (from call exception_handler)
     * [EBP+8]  = saved EBP (exception_common pusha)
     * [EBP+12] = saved EDI
     * [EBP+16] = saved ESI
     * [EBP+20] = saved EDX
     * [EBP+24] = saved ECX
     * [EBP+28] = saved EBX
     * [EBP+32] = saved EAX
     * [EBP+36] = exception_num (pushed by stub)
     * [EBP+40] = error_code (pushed by stub)
     * [EBP+44] = EIP (pushed by CPU)
     * [EBP+48] = CS (pushed by CPU)
     */
    (void)dummy1;
    (void)dummy2;
    
    unsigned int exception_num, error_code, cs, eip, cr2;
    
    __asm__ volatile(
        "movl 36(%%ebp), %0\n"
        "movl 40(%%ebp), %1\n"
        "movl 44(%%ebp), %2\n"
        "movl 48(%%ebp), %3\n"
        "movl %%cr2, %4"
        : "=r"(exception_num), "=r"(error_code), "=r"(eip), "=r"(cs), "=r"(cr2)
    );
    
    /* Check CS lowest 2 bits for privilege level */
    if (cs & 0x3) {
        /* Ring 3 - user mode exception */
        handle_user_exception(exception_num, error_code, eip, cr2);
    } else {
        /* Ring 0 - kernel mode exception */
        handle_kernel_exception(exception_num, error_code, eip);
    }
}
