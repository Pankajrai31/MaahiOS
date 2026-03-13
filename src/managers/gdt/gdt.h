/**
 * MaahiOS GDT (Global Descriptor Table) Manager
 * 
 * Sets up the GDT with kernel/user code+data segments and TSS.
 * Provides gdt_set_kernel_stack() for privilege-level switching.
 */

#ifndef GDT_H
#define GDT_H

/**
 * Set a GDT entry.
 * @param index     Entry index (0-5)
 * @param base      Segment base address
 * @param limit     Segment limit
 * @param access    Access byte
 * @param granularity Granularity byte
 */
void gdt_set_entry(int index, unsigned int base, unsigned int limit,
                   unsigned char access, unsigned char granularity);

/**
 * Set the TSS entry in the GDT.
 * @param index Entry index (typically 5)
 * @param base  TSS base address
 * @param limit TSS limit
 */
void gdt_set_tss_entry(int index, unsigned int base, unsigned int limit);

/**
 * Initialize the GDT with null, kernel, user, and TSS entries.
 * @return 0 on success, non-zero on failure
 */
int gdt_init(void);

/**
 * Load the GDT into the CPU and reload segment registers.
 * @return 0 on success, non-zero on failure
 */
int gdt_load(void);

/**
 * Update the TSS Ring 0 stack pointer.
 * Called during context switch to set the kernel stack for the next process.
 * @param esp0_value New Ring 0 stack pointer
 */
void gdt_set_kernel_stack(unsigned int esp0_value);

#endif /* GDT_H */
