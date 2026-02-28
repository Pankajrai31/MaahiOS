/**
 * Syscall Handlers Header
 * Declares registration functions for handler domains
 * 
 * ONLY domains with existing managers!
 */

#ifndef SYSCALL_HANDLERS_H
#define SYSCALL_HANDLERS_H

/**
 * Core handlers (0-15)
 * Manager: Scheduler, Process Manager
 */
void syscall_register_core_handlers(void);

/**
 * Process handlers (16-31)
 * Manager: Process Manager
 */
void syscall_register_process_handlers(void);

/**
 * Memory handlers (32-47)
 * Manager: PMM, Paging Manager
 */
void syscall_register_memory_handlers(void);

/**
 * SHM handlers (48-63)
 * Manager: SHM Manager
 */
void syscall_register_shm_handlers(void);

/**
 * Cell handlers (64-79)
 * Manager: Cell Manager
 */
void syscall_register_cell_handlers(void);

/**
 * Device handlers (80-95)
 * Manager: Device Manager
 */
void syscall_register_device_handlers(void);

/**
 * Module handlers (96-111)
 * Manager: GRUB Module Manager
 */
void syscall_register_module_handlers(void);

/**
 * Time handlers (112-127)
 * Manager: Time Manager
 */
void syscall_register_time_handlers(void);

/**
 * Debug handlers (240-255)
 * Manager: Klog Manager
 */
void syscall_register_debug_handlers(void);

#endif /* SYSCALL_HANDLERS_H */
