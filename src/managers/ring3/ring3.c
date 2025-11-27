/* Serial debug */
static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static void serial_putc(char c) {
    while (!(inb(0x3F8 + 5) & 0x20));
    outb(0x3F8, c);
}

static void serial_print(const char *str) {
    while (*str) {
        if (*str == '\n') serial_putc('\r');
        serial_putc(*str++);
    }
}

/* Switch to Ring 3 with specified entry point and stack - NEVER RETURNS */
void ring3_switch_with_stack(unsigned int entry_point, unsigned int stack_top) __attribute__((noreturn));

void ring3_switch_with_stack(unsigned int entry_point, unsigned int stack_top) {
    serial_print("\n[RING3_SWITCH] Switching to Ring 3 now!\n");
    
    __asm__ __volatile__(
        /* Keep DS/ES/FS/GS as kernel segments (0x10) during transition */
        /* Let user-mode code set them to 0x23 after landing in Ring 3 */
        
        /* Push IRET frame: SS, ESP, EFLAGS, CS, EIP */
        "pushl $0x23\n\t"          /* User data segment (Ring 3) */
        "pushl %0\n\t"              /* User stack pointer */
        "pushf\n\t"                 /* Current EFLAGS */
        "popl %%eax\n\t"
        "orl $0x00000200, %%eax\n\t"  /* Set IF bit (interrupts enabled in Ring 3) */
        "pushl %%eax\n\t"           /* Push modified EFLAGS */
        "pushl $0x1B\n\t"           /* User code segment (Ring 3) */
        "pushl %1\n\t"              /* Entry point */
        
        /* Jump to Ring 3 - NEVER RETURNS */
        /* DS/ES/FS/GS remain 0x10 (kernel), user code should set them if needed */
        "iret\n\t"
        :
        : "r"(stack_top), "r"(entry_point)
        : "eax", "memory"          /* Added "memory" clobber as recommended */
    );
    
    /* Should NEVER reach here - if we do, halt */
    while(1) {
        __asm__ volatile("hlt");
    }
}

/* Backward compatibility wrapper */
void ring3_switch(unsigned int entry_point) __attribute__((noreturn));
void ring3_switch(unsigned int entry_point) {
    ring3_switch_with_stack(entry_point, 0x001FFFF0);
}

