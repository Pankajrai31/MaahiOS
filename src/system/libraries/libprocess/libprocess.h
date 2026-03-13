/**
 * MaahiOS Process Library - libprocess.h
 * 
 * Description:
 *   User library for process management.
 *   Auto-initializes on first call (discovers SHM, attaches).
 *   Falls back to direct SYS_PROCESS_* kernel syscalls if
 *   Process Executive is not yet running.
 * 
 * Usage:
 *   #include "libprocess.h"
 *   
 *   int pid = libprocess_create(4, 0, 16384);  // just call it
 *   int count = libprocess_get_count();
 *   No init() needed — handled automatically.
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#ifndef LIBPROCESS_H
#define LIBPROCESS_H

#include <stdint.h>
#include "../../executives/processexecutive/process_executive.h"

/*=============================================================================
 * LIBRARY INITIALIZATION
 *===========================================================================*/

/**
 * libprocess_init - Explicitly initialize process library (optional)
 * 
 * Auto-called on first use of any libprocess function.
 * Falls back to direct kernel syscalls if executive not ready.
 * 
 * Returns: 0 on success, negative if executive not available
 */
int libprocess_init(void);

/**
 * libprocess_shutdown - Cleanup process library
 * 
 * Detaches from SHM queues.
 */
void libprocess_shutdown(void);

/*=============================================================================
 * PROCESS OPERATIONS
 *===========================================================================*/

/**
 * libprocess_create - Create a new process from a GRUB module
 * @module_index: GRUB module index containing the binary
 * @load_address: Target address to load the module into
 * @bss_size:     BSS section size (0 = kernel uses MIN_BSS_RESERVE)
 * 
 * Process Executive copies the module and creates the process.
 * Fallback: caller must have already copied module; direct syscall.
 * 
 * Returns: PID on success, negative on error
 */
int libprocess_create(uint32_t module_index, uint32_t load_address, uint32_t bss_size);

/**
 * libprocess_exec - Load and execute a binary in a new address space
 * @base_address:  Virtual address to load at (e.g. 0x10000000)
 * @binary_data:   Pointer to binary code+data
 * @binary_size:   Size of binary in bytes
 * @entry_offset:  Entry point offset from base
 * @bss_size:      BSS section size (extra zeroed memory after binary)
 * 
 * Routes through Process Executive for validation and security.
 * The executive clones a page directory and maps the binary privately.
 * 
 * Returns: PID on success, negative on error
 */
int libprocess_exec(uint32_t base_address, const void *binary_data,
                    uint32_t binary_size, uint32_t entry_offset,
                    uint32_t bss_size);

/**
 * libprocess_kill - Terminate a process
 * @pid: Process ID to terminate
 * 
 * Returns: 0 on success, negative on error
 */
int libprocess_kill(int32_t pid);

/**
 * libprocess_get_info - Get process information
 * @pid: Process ID to query
 * @info: Output structure (pid, state, name, type, memory_alloc)
 * 
 * Returns: 0 on success, negative on error
 */
int libprocess_get_info(int32_t pid, process_info_t *info);

/**
 * libprocess_get_count - Get total number of active processes
 * 
 * Returns: count on success, negative on error
 */
int libprocess_get_count(void);

/**
 * List all active processes.
 * @param infos   Output array of process_info_t
 * @param max     Maximum entries to fill
 * @return        Number of entries written, or negative on error
 */
int libprocess_list(process_info_t *infos, int max);

/**
 * Set process name and type.
 * @param pid   Process ID
 * @param name  Name string (max 31 chars)
 * @param type  PROC_TYPE_SYSTEM(0) or PROC_TYPE_USER(1)
 * @return 0 on success, negative on error
 */
int libprocess_set_name(int32_t pid, const char *name, uint8_t type);

/**
 * Request system power off (via Process Executive → SYS_SHUTDOWN).
 * Does not return on success.
 */
void libprocess_system_shutdown(void);

/**
 * Request system restart (via Process Executive → SYS_RESTART).
 * Does not return on success.
 */
void libprocess_system_restart(void);

/*=============================================================================
 * LIGHTWEIGHT INLINE WRAPPERS
 *
 * These are thin wrappers for Core-domain syscalls (YIELD, GETPID, SLEEP).
 * They do NOT route through the Process Executive because:
 *   - They are per-process scheduler operations, not management operations
 *   - SHM queue round-trip would defeat the purpose of yielding/sleeping
 *   - SYS_SLEEP through executive would sleep the executive, not the caller
 * Apps should call these instead of raw syscall0/1() so they never need
 * to include syscall_helpers.h or syscall_numbers.h directly.
 *===========================================================================*/

#include "../core/syscall_helpers.h"
#include "../../syscalls/syscall_numbers.h"

/**
 * libprocess_yield - Voluntarily yield the CPU to other processes
 */
static inline void libprocess_yield(void) {
    syscall0(SYS_YIELD);
}

/**
 * libprocess_get_pid - Get the calling process's PID
 * Returns: PID of the calling process
 */
static inline uint32_t libprocess_get_pid(void) {
    return (uint32_t)syscall0(SYS_GETPID);
}

/**
 * libprocess_sleep - Put the calling process to sleep
 * @ticks: Number of timer ticks to sleep
 */
static inline void libprocess_sleep(uint32_t ticks) {
    syscall1(SYS_SLEEP, (int)ticks);
}

#endif /* LIBPROCESS_H */
