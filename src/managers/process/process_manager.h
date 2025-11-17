#ifndef PROCESS_MANAGER_H
#define PROCESS_MANAGER_H

#include <stdint.h>

/* Process states */
#define PROCESS_STATE_READY    1
#define PROCESS_STATE_RUNNING  2

/* Process Control Block (PCB) */
typedef struct {
    int pid;
    uint32_t entry_point;
    uint32_t state;
} process_t;

/**
 * Initialize process manager
 */
void process_manager_init(void);

/**
 * Create sysman process (PID 1)
 * Returns: Process ID or -1 on failure
 */
int process_create_sysman(uint32_t sysman_address);

/**
 * Get process by PID
 */
process_t* process_get_by_pid(int pid);

/**
 * Get total process count
 */
int process_manager_get_count(void);

/**
 * Start a process (jump to Ring 3)
 */
void process_start(process_t *pcb);

#endif // PROCESS_MANAGER_H
