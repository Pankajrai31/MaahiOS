#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <stdint.h>

/* Process states */
#define PROCESS_STATE_READY    1
#define PROCESS_STATE_RUNNING  2

/* Process types */
#define PROC_TYPE_SYSTEM       0   /* System process (executives, sysman, orbit) */
#define PROC_TYPE_USER         1   /* User application (.mex apps) */

/* Process name max length (including null terminator) */
#define PROC_NAME_MAX          32

/* Process Control Block (PCB) */
typedef struct {
    int pid;
    uint32_t entry_point;
    uint32_t state;
    uint32_t user_stack_top;    /* User stack pointer */
    uint32_t kernel_stack_top;  /* Kernel interrupt stack pointer */
    uint32_t esp;               /* Saved stack pointer (context on kernel stack) */
    uint32_t *page_directory;   /* Per-process page directory (NULL = kernel default) */
    char     name[PROC_NAME_MAX]; /* Process name (e.g. "LogExec", "diskman") */
    uint8_t  type;              /* PROC_TYPE_SYSTEM or PROC_TYPE_USER */
    uint32_t memory_alloc;      /* Total memory allocated (bytes) */
} process_t;

/**
 * Initialize process manager
 * Returns: 0 on success
 */
int process_manager_init(void);

/**
 * Create a process with a specific page directory.
 * @param entry_point  EIP for the new process
 * @param page_dir     Page directory (NULL = kernel default, shared address space)
 * Returns: Process ID or -1 on failure
 */
int process_create(uint32_t entry_point, uint32_t *page_dir);

/**
 * Get process by PID
 */
process_t* process_get_by_pid(int pid);

/**
 * Get total process count
 */
int process_manager_get_count(void);

/**
 * Create a process from a binary blob in a new address space.
 * Generic kernel function — knows nothing about .mex format.
 *
 * @param base_address  Virtual address to map binary at (e.g. 0x10000000)
 * @param binary_data   Pointer to code+data bytes (in caller's address space)
 * @param binary_size   Size of binary_data in bytes
 * @param entry_offset  Offset from base_address to entry point (usually 0)
 *
 * Clones page directory, allocates physical pages, maps at base_address,
 * copies binary, creates process with entry at base_address + entry_offset.
 *
 * Returns: PID on success, -1 on failure
 */
int process_create_from_memory(uint32_t base_address, const void *binary_data,
                               uint32_t binary_size, uint32_t entry_offset,
                               uint32_t bss_size);

/**
 * Terminate a process and free its resources
 */
int process_terminate(int pid);

/**
 * List all active processes.
 * Each entry is 48 bytes: [pid:4][state:4][name:32][type:1][pad:3][memory:4]
 * @param buffer     Output buffer (must hold max_entries * 48 bytes)
 * @param max_entries Maximum entries to return
 * @return Number of entries written, or -1 on error
 */
int process_manager_list(void *buffer, int max_entries);

/**
 * Set process name and type.
 * @param pid   Process ID
 * @param name  Name string (max 31 chars + null)
 * @param type  PROC_TYPE_SYSTEM or PROC_TYPE_USER
 * @return 0 on success, -1 on error
 */
int process_set_name(int pid, const char *name, uint8_t type);

#endif // PROCESS_MANAGER_H
