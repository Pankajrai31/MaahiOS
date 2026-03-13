/**
 * MaahiOS IDT (Interrupt Descriptor Table) Manager
 * 
 * Configures interrupt handlers: CPU exceptions, syscall gate, IRQs.
 */

#ifndef IDT_H
#define IDT_H

/**
 * Set an IDT entry.
 * @param index     Interrupt vector number (0-255)
 * @param handler   Handler function address
 * @param selector  Code segment selector (0x08 for kernel)
 * @param type      Gate type + DPL flags
 */
void idt_set_entry(int index, unsigned int handler,
                   unsigned short selector, unsigned char type);

/**
 * Initialize the IDT (zero all 256 entries).
 * @return 0 on success, non-zero on failure
 */
int idt_init(void);

/**
 * Load the IDT into the CPU.
 * @return 0 on success, non-zero on failure
 */
int idt_load(void);

/**
 * Install CPU exception handlers (0-19), syscall gate (0x80),
 * and IRQ stubs (PIT, keyboard).
 * @return 0 on success, non-zero on failure
 */
int idt_install_exception_handlers(void);

/**
 * Install IRQ12 handler for the PS/2 mouse.
 * @return 0 on success, non-zero on failure
 */
int idt_install_mouse_handler(void);

#endif /* IDT_H */
