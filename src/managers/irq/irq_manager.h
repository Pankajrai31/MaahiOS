#ifndef IRQ_MANAGER_H
#define IRQ_MANAGER_H

/**
 * IRQ Manager - Centralized interrupt request management
 * Handles PIC configuration and IRQ enable/disable
 */

/**
 * Initialize IRQ manager and remap PIC
 */
void irq_manager_init(void);

/**
 * Enable specific IRQ line
 */
void irq_enable(int irq_number);

/**
 * Disable specific IRQ line
 */
void irq_disable(int irq_number);

/**
 * Enable timer IRQ (IRQ 0)
 */
void irq_enable_timer(void);

/**
 * Enable mouse IRQ (IRQ 12)
 */
void irq_enable_mouse(void);

/**
 * Get PIC mask status (for debugging)
 */
unsigned int irq_get_pic_mask(void);

#endif // IRQ_MANAGER_H
