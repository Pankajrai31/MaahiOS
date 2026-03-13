/**
 * Time Syscall Handlers
 * Domain: 112-127 (time_get_datetime, time_get_unix, time_get_uptime, time_get_ticks)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../time/time_manager.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_time_get_datetime - Get current date/time
 */
static int sys_time_get_datetime(uint32_t dt_ptr, uint32_t arg2, uint32_t arg3,
                                  uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (!dt_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    return kernel_time_get_datetime((sys_datetime_t *)dt_ptr);
}

/**
 * sys_time_get_unix - Get Unix timestamp
 */
static int sys_time_get_unix(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                              uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return (int)kernel_time_get_unix();
}

/**
 * sys_time_get_uptime - Get system uptime
 */
static int sys_time_get_uptime(uint32_t up_ptr, uint32_t arg2, uint32_t arg3,
                                uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (!up_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    return kernel_time_get_uptime((sys_uptime_t *)up_ptr);
}

/**
 * sys_time_get_ticks - Get raw tick count (lower 32 bits)
 */
static int sys_time_get_ticks(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                               uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    /* Return lower 32 bits of tick count */
    return (int)(kernel_time_get_ticks() & 0xFFFFFFFF);
}

/**
 * sys_time_get_tick_freq - Get tick frequency in Hz
 */
static int sys_time_get_tick_freq(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                   uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return (int)kernel_time_get_tick_frequency();
}

/**
 * sys_time_get_shm_id - Get shared time SHM ID
 */
static int sys_time_get_shm_id(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return kernel_time_get_shared_shm_id();
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_time_handlers(void) {
    syscall_register(SYS_TIME_GET_DATETIME, sys_time_get_datetime);
    syscall_register(SYS_TIME_GET_UNIX,     sys_time_get_unix);
    syscall_register(SYS_TIME_GET_UPTIME,   sys_time_get_uptime);
    syscall_register(SYS_TIME_GET_TICKS,    sys_time_get_ticks);
    syscall_register(SYS_TIME_GET_TICK_FREQ, sys_time_get_tick_freq);
    syscall_register(SYS_TIME_GET_SHM_ID,   sys_time_get_shm_id);
    
    KLOG_INFO("SYSCALL", "Time handlers registered (112-117)");
}
