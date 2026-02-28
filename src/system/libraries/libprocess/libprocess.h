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
 *   int pid = libprocess_create(4, 0);  // just call it
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
 * 
 * Process Executive copies the module and creates the process.
 * Fallback: caller must have already copied module; direct syscall.
 * 
 * Returns: PID on success, negative on error
 */
int libprocess_create(uint32_t module_index, uint32_t load_address);

/**
 * libprocess_exec - Load and execute a binary in a new address space
 * @base_address:  Virtual address to load at (e.g. 0x10000000)
 * @binary_data:   Pointer to binary code+data
 * @binary_size:   Size of binary in bytes
 * @entry_offset:  Entry point offset from base
 * 
 * Routes through Process Executive for validation and security.
 * The executive clones a page directory and maps the binary privately.
 * 
 * Returns: PID on success, negative on error
 */
int libprocess_exec(uint32_t base_address, const void *binary_data,
                    uint32_t binary_size, uint32_t entry_offset);

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
 * @info: Output structure (pid + state)
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
 * Request system power off (via Process Executive → SYS_SHUTDOWN).
 * Does not return on success.
 */
void libprocess_system_shutdown(void);

/**
 * Request system restart (via Process Executive → SYS_RESTART).
 * Does not return on success.
 */
void libprocess_system_restart(void);

#endif /* LIBPROCESS_H */
