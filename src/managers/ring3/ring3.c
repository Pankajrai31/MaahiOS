/* Switch to Ring 3 with specified entry point - NEVER RETURNS */
void ring3_switch(unsigned int entry_point) __attribute__((noreturn));

void ring3_switch(unsigned int entry_point) {
    unsigned int user_stack = 0x001FFFF0;  /* Ring 3 stack at 2MB - 16 bytes */
    
    __asm__ __volatile__(
        /* Push IRET frame: SS, ESP, EFLAGS, CS, EIP */
        "pushl $0x23\n\t"          /* User data segment (Ring 3) */
        "pushl %0\n\t"              /* User stack pointer */
        "pushf\n\t"                 /* Current EFLAGS */
        "popl %%eax\n\t"
        "andl $0xFFFFFDFF, %%eax\n\t"  /* Clear IF bit (disable interrupts in Ring 3) */
        "pushl %%eax\n\t"           /* Push modified EFLAGS */
        "pushl $0x1B\n\t"           /* User code segment (Ring 3) */
        "pushl %1\n\t"              /* Entry point */
        
        /* Set data segments to Ring 3 before IRET */
        "movw $0x23, %%ax\n\t"
        "movw %%ax, %%ds\n\t"
        "movw %%ax, %%es\n\t"
        "movw %%ax, %%fs\n\t"
        "movw %%ax, %%gs\n\t"
        
        /* Jump to Ring 3 - NEVER RETURNS */
        "iret\n\t"
        :
        : "r"(user_stack), "r"(entry_point)
        : "eax"
    );
    
    /* Should NEVER reach here - if we do, halt */
    while(1) {
        __asm__ volatile("hlt");
    }
}

