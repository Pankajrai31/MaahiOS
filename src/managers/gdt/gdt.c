#define GDT_ENTRIES 6

/* TSS (Task State Segment) structure - 104 bytes */
struct tss_entry {
    unsigned int prev_tss;
    unsigned int esp0;           /* Ring 0 ESP for privilege transition */
    unsigned int ss0;            /* Ring 0 SS for privilege transition */
    unsigned int esp1;           /* Ring 1 ESP (unused) */
    unsigned int ss1;            /* Ring 1 SS (unused) */
    unsigned int esp2;           /* Ring 2 ESP (unused) */
    unsigned int ss2;            /* Ring 2 SS (unused) */
    unsigned int cr3;
    unsigned int eip;
    unsigned int eflags;
    unsigned int eax;
    unsigned int ecx;
    unsigned int edx;
    unsigned int ebx;
    unsigned int esp;
    unsigned int ebp;
    unsigned int esi;
    unsigned int edi;
    unsigned int es;
    unsigned int cs;
    unsigned int ss;
    unsigned int ds;
    unsigned int fs;
    unsigned int gs;
    unsigned int ldt;
    unsigned short trap;
    unsigned short iomap_base;
} __attribute__((packed));

struct gdt_entry {
    unsigned short limit_low;
    unsigned short base_low;
    unsigned char base_mid;
    unsigned char access;
    unsigned char granularity;
    unsigned char base_high;
} __attribute__((packed));

struct gdt_ptr {
    unsigned short limit;
    unsigned int base;
} __attribute__((packed));

static struct gdt_entry gdt[GDT_ENTRIES];
static struct gdt_ptr gdt_pointer;
static struct tss_entry tss;  /* Global TSS instance */

void gdt_set_entry(int index, unsigned int base, unsigned int limit, unsigned char access, unsigned char granularity) {
    gdt[index].base_low = (base & 0xFFFF);
    gdt[index].base_mid = ((base >> 16) & 0xFF);
    gdt[index].base_high = ((base >> 24) & 0xFF);
    
    gdt[index].limit_low = (limit & 0xFFFF);
    gdt[index].granularity = ((limit >> 16) & 0x0F) | (granularity & 0xF0);
    
    gdt[index].access = access;
}

void gdt_set_tss_entry(int index, unsigned int base, unsigned int limit) {
    /* TSS descriptor: type 0x89 = Available TSS32, granularity 0x40 (no scaling) */
    gdt_set_entry(index, base, limit, 0x89, 0x40);
}

void gdt_init(void) {
    gdt_pointer.limit = (sizeof(struct gdt_entry) * GDT_ENTRIES) - 1;
    gdt_pointer.base = (unsigned int)&gdt;
    
    /* Entry 0: NULL (required) */
    gdt_set_entry(0, 0, 0, 0, 0);
    
    /* Entry 1: Kernel Code (Ring 0) */
    gdt_set_entry(1, 0, 0xFFFFFFFF, 0x9A, 0xCF);
    
    /* Entry 2: Kernel Data (Ring 0) */
    gdt_set_entry(2, 0, 0xFFFFFFFF, 0x92, 0xCF);
    
    /* Entry 3: User Code (Ring 3) */
    gdt_set_entry(3, 0, 0xFFFFFFFF, 0xFA, 0xCF);
    
    /* Entry 4: User Data (Ring 3) */
    gdt_set_entry(4, 0, 0xFFFFFFFF, 0xF3, 0xCF);
    
    /* Entry 5: TSS (Task State Segment) */
    gdt_set_tss_entry(5, (unsigned int)&tss, sizeof(struct tss_entry) - 1);
    
    /* Initialize TSS - set Ring 0 stack for privilege transitions */
    __builtin_memset(&tss, 0, sizeof(struct tss_entry));
    tss.ss0 = 0x10;              /* Ring 0 Data Segment */
    tss.esp0 = 0x00090000;       /* Ring 0 stack at 576KB (safe kernel area) */
    tss.iomap_base = sizeof(struct tss_entry);  /* No I/O port bitmap */
}

void gdt_load(void) {
    asm volatile("lgdt %0" : : "m"(gdt_pointer));
    
    /* Reload code segment */
    asm volatile("ljmp $0x08, $reload_cs\n"
                 "reload_cs:\n");
    
    /* Reload data segments */
    asm volatile("mov $0x10, %eax\n"
                 "mov %eax, %ds\n"
                 "mov %eax, %es\n"
                 "mov %eax, %fs\n"
                 "mov %eax, %gs\n"
                 "mov %eax, %ss\n");
    
    /* Load TSS - selector 0x28 = entry 5 * 8, no RPL bits needed for TSS */
    asm volatile("mov $0x28, %eax\n"
                 "ltr %ax");
}
