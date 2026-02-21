/**
 * MaahiOS Process Library - libprocess.h
 * 
 * Description:
 *   User library for process management.
 *   Applications include this header to create/manage processes.
 *   Internally communicates with Process Executive via SHM queues.
 * 
 * Usage:
 *   #include <libprocess.h>
 *   
 *   libprocess_init();
 *   int pid = libprocess_create("app.bin", 4, PRIORITY_MEDIUM, 0);
 *   libprocess_wait(pid, &exit_code, 0);
 *   libprocess_shutdown();
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
 * libprocess_init - Initialize process library
 * 
 * Connects to Process Executive's SHM queues.
 * Must be called before any other libprocess functions.
 * 
 * Returns: 0 on success, negative on error
 */
int libprocess_init(void);

/**
 * libprocess_shutdown - Cleanup process library
 */
void libprocess_shutdown(void);

/*=============================================================================
 * PROCESS OPERATIONS
 *===========================================================================*/

/**
 * libprocess_create - Create a new process
 * @name: Process name (for display)
 * @module_index: GRUB module index containing the binary
 * @priority: Process priority (PRIORITY_*)
 * @flags: Creation flags
 * 
 * Returns: PID on success, negative on error
 */
int libprocess_create(const char *name, uint32_t module_index, 
                      uint32_t priority, uint32_t flags);

/**
 * libprocess_terminate - Terminate a process
 * @pid: Process ID to terminate
 * @exit_code: Exit code
 * 
 * Returns: 0 on success, negative on error
 */
int libprocess_terminate(uint32_t pid, int32_t exit_code);

/**
 * libprocess_get_pid - Get current process ID
 * 
 * Returns: PID on success, negative on error
 */
int libprocess_get_pid(void);

/**
 * libprocess_get_parent_pid - Get parent process ID
 * 
 * Returns: Parent PID on success, negative on error
 */
int libprocess_get_parent_pid(void);

/**
 * libprocess_get_info - Get process information
 * @pid: Process ID
 * @info: Output structure
 * 
 * Returns: 0 on success, negative on error
 */
int libprocess_get_info(uint32_t pid, process_info_t *info);

/**
 * libprocess_list - List all processes
 * @offset: Starting index
 * @processes: Output array
 * @max_count: Maximum processes to return
 * @total_count: Output total count (can be NULL)
 * 
 * Returns: Number returned on success, negative on error
 */
int libprocess_list(uint32_t offset, process_info_t *processes, 
                    uint32_t max_count, uint32_t *total_count);

/**
 * libprocess_set_priority - Set process priority
 * @pid: Process ID
 * @priority: New priority
 * 
 * Returns: 0 on success, negative on error
 */
int libprocess_set_priority(uint32_t pid, uint32_t priority);

/**
 * libprocess_wait - Wait for process to exit
 * @pid: Process ID to wait for
 * @exit_code: Output exit code (can be NULL)
 * @timeout_ms: Timeout in milliseconds (0 = no wait, -1 = forever)
 * 
 * Returns: 0 on success, negative on error/timeout
 */
int libprocess_wait(uint32_t pid, int32_t *exit_code, uint32_t timeout_ms);

#endif /* LIBPROCESS_H */
