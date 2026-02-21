/**
 * Process Syscall Handlers
 * Domain: 16-31 (process_create, kill, info)
 * 
 * Note: Apps should use GRUB module manager syscalls (SYS_MOD_GET_ADDR)
 * to get module addresses and SYS_PROCESS_CREATE to launch them.
 * No hardcoded app-specific syscalls!
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../process/process_manager.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_process_create - Create a new process
 */
static int sys_process_create(uint32_t entry_point, uint32_t arg2, uint32_t arg3,
                              uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (entry_point == 0) {
        KLOG_ERROR("SYSCALL", "process_create: NULL entry point");
        return SYSCALL_ERR_INVALID;
    }
    
    KLOG_INFO_HEX("SYSCALL", "Creating process at entry: ", entry_point);
    
    /* Call kernel API */
    extern int kernel_process_create(uint32_t entry_point);
    return kernel_process_create(entry_point);
}

/**
 * sys_process_kill - Terminate a process
 */
static int sys_process_kill(uint32_t pid, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (pid == 0) {
        KLOG_ERROR("SYSCALL", "process_kill: Cannot kill PID 0");
        return SYSCALL_ERR_INVALID;
    }
    
    KLOG_INFO_HEX("SYSCALL", "Killing process PID: ", pid);
    
    /* Call kernel API */
    extern int kernel_process_terminate(int pid);
    return kernel_process_terminate((int)pid);
}

/**
 * sys_process_info - Get process information
 */
static int sys_process_info(uint32_t pid, uint32_t info_ptr, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (!info_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    /* Get process by PID */
    extern process_t* kernel_process_get_by_pid(int pid);
    process_t *pcb = kernel_process_get_by_pid((int)pid);
    
    if (!pcb) {
        return SYSCALL_ERR_NOTFOUND;
    }
    
    /* Copy basic info (simplified) */
    uint32_t *info = (uint32_t *)info_ptr;
    info[0] = pcb->pid;
    info[1] = pcb->state;
    
    return 0;
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_process_handlers(void) {
    syscall_register(SYS_PROCESS_CREATE, sys_process_create);
    syscall_register(SYS_PROCESS_KILL, sys_process_kill);
    syscall_register(SYS_PROCESS_INFO, sys_process_info);
    
    KLOG_DEBUG("SYSCALL", "Process handlers registered (16-31)");
}
