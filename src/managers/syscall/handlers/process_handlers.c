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
    
    /* Call kernel API — NULL page_dir means use kernel page directory */
    extern int process_create(uint32_t entry_point, uint32_t *page_dir);
    return process_create(entry_point, (uint32_t *)0);
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
    
    /* Call kernel API — returns -2 if process is system-protected */
    extern int process_terminate(int pid);
    int result = process_terminate((int)pid);
    if (result == -2) {
        return SYSCALL_ERR_PERM;
    }
    return result;
}

/**
 * sys_process_info - Get process information
 * Returns 48 bytes: [pid:4][state:4][name:32][type:1][pad:3][memory:4]
 */
static int sys_process_info(uint32_t pid, uint32_t info_ptr, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (!info_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    /* Get process by PID */
    extern process_t* process_get_by_pid(int pid);
    process_t *pcb = process_get_by_pid((int)pid);
    
    if (!pcb) {
        return SYSCALL_ERR_NOTFOUND;
    }
    
    /* Fill 48-byte info structure */
    uint8_t *info = (uint8_t *)info_ptr;
    *(int32_t *)(info + 0) = pcb->pid;
    *(uint32_t *)(info + 4) = pcb->state;
    for (int i = 0; i < 32; i++) info[8 + i] = (uint8_t)pcb->name[i];
    info[40] = pcb->type;
    info[41] = 0; info[42] = 0; info[43] = 0;
    *(uint32_t *)(info + 44) = pcb->memory_alloc;
    
    return 0;
}

/**
 * sys_process_get_count - Get total number of processes
 */
static int sys_process_get_count(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                  uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    extern int process_manager_get_count(void);
    return process_manager_get_count();
}

/**
 * sys_process_exec - Create process from binary blob in new address space
 *
 * Args: base_address, binary_ptr, code_size, entry_offset
 * Kernel allocates physical memory, clones page dir, maps at base, copies binary.
 */
static int sys_process_exec(uint32_t base_address, uint32_t binary_ptr, uint32_t code_size,
                            uint32_t entry_offset, uint32_t bss_size) {
    
    if (base_address == 0 || binary_ptr == 0 || code_size == 0) {
        KLOG_ERROR("SYSCALL", "process_exec: invalid args");
        return SYSCALL_ERR_INVALID;
    }
    
    KLOG_INFO("SYSCALL", "process_exec: base=0x%x, data=0x%x, size=%d, entry_off=%d, bss=%d",
              base_address, binary_ptr, code_size, entry_offset, bss_size);
    
    extern int process_create_from_memory(uint32_t base_address, const void *binary_data,
                                          uint32_t binary_size, uint32_t entry_offset,
                                          uint32_t bss_size);
    return process_create_from_memory(base_address, (const void *)binary_ptr,
                                      code_size, entry_offset, bss_size);
}

/**
 * sys_process_list - List all active processes
 *
 * Args: buffer_ptr (output, each entry = 48 bytes), max_count
 * Returns: number of entries written
 */
static int sys_process_list(uint32_t buffer_ptr, uint32_t max_count, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (!buffer_ptr || max_count == 0) {
        return SYSCALL_ERR_INVALID;
    }
    
    extern int process_manager_list(void *buffer, int max_entries);
    return process_manager_list((void *)buffer_ptr, (int)max_count);
}

/**
 * sys_process_set_name - Set process name and type
 *
 * Args: pid, name_ptr (C string), type (PROC_TYPE_SYSTEM=0, PROC_TYPE_USER=1)
 */
static int sys_process_set_name(uint32_t pid, uint32_t name_ptr, uint32_t type,
                                uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!name_ptr || pid == 0) {
        return SYSCALL_ERR_INVALID;
    }
    
    extern int process_set_name(int pid, const char *name, uint8_t type);
    return process_set_name((int)pid, (const char *)name_ptr, (uint8_t)type);
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_process_handlers(void) {
    syscall_register(SYS_PROCESS_CREATE, sys_process_create);
    syscall_register(SYS_PROCESS_KILL, sys_process_kill);
    syscall_register(SYS_PROCESS_INFO, sys_process_info);
    syscall_register(SYS_PROCESS_GET_COUNT, sys_process_get_count);
    syscall_register(SYS_PROCESS_EXEC, sys_process_exec);
    syscall_register(SYS_PROCESS_LIST, sys_process_list);
    syscall_register(SYS_PROCESS_SET_NAME, sys_process_set_name);
    
    KLOG_DEBUG("SYSCALL", "Process handlers registered (16-31)");
}
