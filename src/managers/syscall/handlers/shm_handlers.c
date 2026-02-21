/**
 * SHM (Shared Memory) Syscall Handlers
 * Domain: 48-63 (shm_create, attach, detach, destroy, info)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../shm/shm_manager.h"
#include "../../scheduler/scheduler.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_shm_create - Create shared memory region
 */
static int sys_shm_create(uint32_t size, uint32_t arg2, uint32_t arg3,
                          uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    int pid = scheduler_get_current_pid();
    
    if (pid < 0) {
        KLOG_ERROR("SYSCALL", "shm_create: Cannot get current PID");
        return SHM_INVALID_ID;
    }
    
    if (size == 0) {
        KLOG_WARN("SYSCALL", "shm_create: Size is 0");
        return SHM_INVALID_ID;
    }
    
    KLOG_DEBUG_HEX2("SYSCALL", "SHM create: PID/size: ", pid, size);
    
    /* Call kernel API */
    extern int kernel_shm_create(size_t size, int owner_pid);
    int shm_id = kernel_shm_create((size_t)size, pid);
    
    if (shm_id < 0) {
        KLOG_ERROR_HEX("SYSCALL", "shm_create failed: ", shm_id);
    }
    
    return shm_id;
}

/**
 * sys_shm_attach - Attach to shared memory region
 */
static int sys_shm_attach(uint32_t shm_id, uint32_t virt_addr, uint32_t arg3,
                          uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    int pid = scheduler_get_current_pid();
    
    if (pid < 0) {
        KLOG_ERROR("SYSCALL", "shm_attach: Cannot get current PID");
        return 0;
    }
    
    KLOG_DEBUG_HEX2("SYSCALL", "SHM attach: PID/SHM: ", pid, shm_id);
    
    /* Call kernel API */
    extern unsigned int kernel_shm_attach(int shm_id, int pid, unsigned int virt_addr);
    unsigned int addr = kernel_shm_attach((int)shm_id, pid, virt_addr);
    
    if (!addr) {
        KLOG_ERROR_HEX2("SYSCALL", "shm_attach failed: PID/SHM: ", pid, shm_id);
    }
    
    return (int)addr;
}

/**
 * sys_shm_detach - Detach from shared memory region
 */
static int sys_shm_detach(uint32_t shm_id, uint32_t arg2, uint32_t arg3,
                          uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    int pid = scheduler_get_current_pid();
    
    if (pid < 0) {
        KLOG_ERROR("SYSCALL", "shm_detach: Cannot get current PID");
        return SHM_INVALID_ID;
    }
    
    KLOG_DEBUG_HEX2("SYSCALL", "SHM detach: PID/SHM: ", pid, shm_id);
    
    /* Call kernel API */
    extern int kernel_shm_detach(int shm_id, int pid);
    return kernel_shm_detach((int)shm_id, pid);
}

/**
 * sys_shm_destroy - Destroy shared memory region
 */
static int sys_shm_destroy(uint32_t shm_id, uint32_t arg2, uint32_t arg3,
                           uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    KLOG_DEBUG_HEX("SYSCALL", "SHM destroy: ", shm_id);
    
    /* Call kernel API */
    extern int kernel_shm_destroy(int shm_id);
    return kernel_shm_destroy((int)shm_id);
}

/**
 * sys_shm_info - Get shared memory region info
 */
static int sys_shm_info(uint32_t shm_id, uint32_t info_ptr, uint32_t arg3,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (!info_ptr) {
        return SHM_INVALID_ID;
    }
    
    /* Call kernel API */
    extern int kernel_shm_get_info(int shm_id, shm_info_t *info);
    return kernel_shm_get_info((int)shm_id, (shm_info_t *)info_ptr);
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_shm_handlers(void) {
    syscall_register(SYS_SHM_CREATE, sys_shm_create);
    syscall_register(SYS_SHM_ATTACH, sys_shm_attach);
    syscall_register(SYS_SHM_DETACH, sys_shm_detach);
    syscall_register(SYS_SHM_DESTROY, sys_shm_destroy);
    syscall_register(SYS_SHM_INFO, sys_shm_info);
    
    KLOG_DEBUG("SYSCALL", "SHM handlers registered (48-63)");
}
