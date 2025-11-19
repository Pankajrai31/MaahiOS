#define IDT_ENTRIES 256

struct idt_entry {
    unsigned short offset_low;      /* Lower 16 bits of handler address */
    unsigned short selector;         /* Segment selector (0x08 = kernel code) */
    unsigned char zero;              /* Reserved, must be 0 */
    unsigned char type_attr;         /* Type and attributes */
    unsigned short offset_high;      /* Upper 16 bits of handler address */
} __attribute__((packed));

struct idt_ptr {
    unsigned short limit;            /* Size of IDT - 1 */
    unsigned int base;               /* Address of IDT */
} __attribute__((packed));

static struct idt_entry idt[IDT_ENTRIES];
static struct idt_ptr idt_pointer;

/* Set an IDT entry */
void idt_set_entry(int index, unsigned int handler, unsigned short selector, unsigned char type) {
    idt[index].offset_low = (handler & 0xFFFF);
    idt[index].offset_high = ((handler >> 16) & 0xFFFF);
    idt[index].selector = selector;
    idt[index].zero = 0;
    idt[index].type_attr = type;
}

/* Initialize IDT table */
int idt_init(void) {
    int i;
    
    /* Set up IDT pointer */
    idt_pointer.limit = (sizeof(struct idt_entry) * IDT_ENTRIES) - 1;
    idt_pointer.base = (unsigned int)&idt;
    
    /* Zero out all entries */
    for (i = 0; i < IDT_ENTRIES; i++) {
        idt_set_entry(i, 0, 0, 0);
    }
    
    return 1;  /* Success */
}

/* Load IDT into CPU */
int idt_load(void) {
    asm volatile("lidt %0" : : "m"(idt_pointer));
    return 1;  /* Success */
}

/* Register exception handlers and syscall handler */
extern void exception_stub_0(void);
extern void exception_stub_1(void);
extern void exception_stub_2(void);
extern void exception_stub_3(void);
extern void exception_stub_4(void);
extern void exception_stub_5(void);
extern void exception_stub_6(void);
extern void exception_stub_7(void);
extern void exception_stub_8(void);
extern void exception_stub_9(void);
extern void exception_stub_10(void);
extern void exception_stub_11(void);
extern void exception_stub_12(void);
extern void exception_stub_13(void);
extern void exception_stub_14(void);
extern void exception_stub_15(void);
extern void exception_stub_16(void);
extern void exception_stub_17(void);
extern void exception_stub_18(void);
extern void exception_stub_19(void);
extern void syscall_int(void);
extern void irq0_stub(void);  /* Timer IRQ */

int idt_install_exception_handlers(void) {
    /* Type: 0x8F = Present, Ring 0, Trap Gate (allows nested exceptions, doesn't disable interrupts) */
    idt_set_entry(0, (unsigned int)exception_stub_0, 0x08, 0x8F);
    idt_set_entry(1, (unsigned int)exception_stub_1, 0x08, 0x8F);
    idt_set_entry(2, (unsigned int)exception_stub_2, 0x08, 0x8F);
    idt_set_entry(3, (unsigned int)exception_stub_3, 0x08, 0x8F);
    idt_set_entry(4, (unsigned int)exception_stub_4, 0x08, 0x8F);
    idt_set_entry(5, (unsigned int)exception_stub_5, 0x08, 0x8F);
    idt_set_entry(6, (unsigned int)exception_stub_6, 0x08, 0x8F);
    idt_set_entry(7, (unsigned int)exception_stub_7, 0x08, 0x8F);
    idt_set_entry(8, (unsigned int)exception_stub_8, 0x08, 0x8F);
    idt_set_entry(9, (unsigned int)exception_stub_9, 0x08, 0x8F);
    idt_set_entry(10, (unsigned int)exception_stub_10, 0x08, 0x8F);
    idt_set_entry(11, (unsigned int)exception_stub_11, 0x08, 0x8F);
    idt_set_entry(12, (unsigned int)exception_stub_12, 0x08, 0x8F);
    idt_set_entry(13, (unsigned int)exception_stub_13, 0x08, 0x8F);
    idt_set_entry(14, (unsigned int)exception_stub_14, 0x08, 0x8F);
    idt_set_entry(15, (unsigned int)exception_stub_15, 0x08, 0x8F);
    idt_set_entry(16, (unsigned int)exception_stub_16, 0x08, 0x8F);
    idt_set_entry(17, (unsigned int)exception_stub_17, 0x08, 0x8F);
    idt_set_entry(18, (unsigned int)exception_stub_18, 0x08, 0x8F);
    idt_set_entry(19, (unsigned int)exception_stub_19, 0x08, 0x8F);
    
    /* Set up INT 0x80 (syscall) handler */
    /* 
     * Type: 0xEE = Present (1), DPL=3 (Ring 3 can call), Trap Gate (01110)
     * DPL=3 is critical! Without it, Ring 3 code cannot execute INT 0x80
     * Bit 7: Present = 1
     * Bit 6-5: DPL = 11 (Ring 3)
     * Bit 4-0: Type = 01110 (Trap Gate)
     */
    idt_set_entry(128, (unsigned int)syscall_int, 0x08, 0xEE);
    
    /* Set up IRQ 0 (PIT timer) handler - IRQ 0 is remapped to INT 32 */
    /* Type: 0x8E = Present (1), DPL=0 (kernel only), Interrupt Gate */
    idt_set_entry(32, (unsigned int)irq0_stub, 0x08, 0x8E);
    
    /* IRQ 15 (ATA) handler removed - now using AHCI */
    
    return 1;  /* Success */
}
