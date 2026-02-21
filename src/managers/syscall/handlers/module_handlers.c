/**
 * GRUB Module Syscall Handlers
 * Domain: 96-111 (mod_get_count, get_info, get_addr, get_size, copy)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../grub_module/grub_module_manager.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_mod_get_count - Get number of GRUB modules
 */
static int sys_mod_get_count(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                             uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return kernel_grub_get_module_count();
}

/**
 * sys_mod_get_info - Get module info structure
 */
static int sys_mod_get_info(uint32_t index, uint32_t info_ptr, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (!info_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    return kernel_grub_get_module_info((int)index, (grub_module_info_t *)info_ptr);
}

/**
 * sys_mod_get_addr - Get module start address
 */
static int sys_mod_get_addr(uint32_t index, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return (int)kernel_grub_get_module_addr((int)index);
}

/**
 * sys_mod_get_size - Get module size
 */
static int sys_mod_get_size(uint32_t index, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return (int)kernel_grub_get_module_size((int)index);
}

/**
 * sys_mod_copy - Copy module to target address
 */
static int sys_mod_copy(uint32_t index, uint32_t target_addr, uint32_t arg3,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (target_addr == 0) {
        return SYSCALL_ERR_INVALID;
    }
    
    return kernel_grub_copy_module((int)index, target_addr);
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_module_handlers(void) {
    syscall_register(SYS_MOD_GET_COUNT, sys_mod_get_count);
    syscall_register(SYS_MOD_GET_INFO, sys_mod_get_info);
    syscall_register(SYS_MOD_GET_ADDR, sys_mod_get_addr);
    syscall_register(SYS_MOD_GET_SIZE, sys_mod_get_size);
    syscall_register(SYS_MOD_COPY, sys_mod_copy);
    
    KLOG_DEBUG("SYSCALL", "Module handlers registered (96-111)");
}
