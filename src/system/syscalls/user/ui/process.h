#ifndef USER_PROCESS_SYSCALLS_H
#define USER_PROCESS_SYSCALLS_H

#include "../../syscall_numbers.h"

/**
 * Process Management Syscalls - Ring 3 User Mode
 */

/**
 * syscall_create_process - Create new process
 */
int syscall_create_process(unsigned int entry_point);

/**
 * syscall_kill_process - Terminate a process
 */
int syscall_kill_process(int pid);

/**
 * syscall_get_orbit_address - Get orbit module address from kernel
 */
int syscall_get_orbit_address(void);

/**
 * syscall_get_uimanager_address - Get UIManager module address from kernel
 */
int syscall_get_uimanager_address(void);

/**
 * syscall_get_current_pid - Get current process ID
 */
int syscall_get_current_pid(void);

/**
 * syscall_yield - Yield CPU to scheduler
 */
void syscall_yield(void);

#endif // USER_PROCESS_SYSCALLS_H
