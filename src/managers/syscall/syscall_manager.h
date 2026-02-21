/**
 * MaahiOS Syscall Manager
 * Central dispatcher for all system calls (INT 0x80)
 * 
 * Architecture:
 *   Ring 3 → INT 0x80 → syscall_dispatch() → sys_*() handler → kernel_*() API
 * 
 * Security layers:
 *   1. Gate validation (syscall number in range)
 *   2. Handler lookup (O(1) table dispatch)
 *   3. Argument validation (in handlers)
 *   4. Audit logging (via klog)
 */

#ifndef SYSCALL_MANAGER_H
#define SYSCALL_MANAGER_H

#include <stdint.h>

/**
 * Syscall handler function signature
 * All handlers take 5 arguments (even if unused) for uniform dispatch
 * Returns: result value (passed back to Ring 3 in EAX)
 */
typedef int (*syscall_handler_t)(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                  uint32_t arg4, uint32_t arg5);

/**
 * Initialize Syscall Manager
 * Must be called during kernel boot
 * Returns: 0 on success, -1 on failure
 */
int syscall_manager_init(void);

/**
 * Main syscall dispatcher
 * Called from assembly stub (interrupt_stubs.s)
 * 
 * @param syscall_num  Syscall number (from EAX)
 * @param arg1-arg4    Arguments (from EBX, ECX, EDX, ESI)
 * @param user_esp     User stack pointer (for additional args)
 * @return             Result value (returned in EAX to user)
 */
uint32_t syscall_dispatch(uint32_t syscall_num,
                          uint32_t arg1, uint32_t arg2,
                          uint32_t arg3, uint32_t arg4,
                          uint32_t user_esp);

/**
 * Register a syscall handler
 * Called during init to populate dispatch table
 * 
 * @param syscall_num  Syscall number to register
 * @param handler      Handler function pointer
 * @return             0 on success, -1 on failure
 */
int syscall_register(int syscall_num, syscall_handler_t handler);

/**
 * Get syscall statistics (for debugging)
 */
uint32_t syscall_get_count(int syscall_num);

#endif /* SYSCALL_MANAGER_H */
