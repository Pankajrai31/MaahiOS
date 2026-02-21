/**
 * Core Syscall Handlers
 * Domain: 0-15 (exit, yield, getpid, sleep)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../scheduler/scheduler.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_exit - Terminate current process
 */
static int sys_exit(uint32_t code, uint32_t arg2, uint32_t arg3,
                    uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    int pid = scheduler_get_current_pid();
    KLOG_INFO_HEX2("SYSCALL", "Exit: PID/code: ", pid, code);
    
    /* Terminate current process */
    extern int kernel_process_terminate(int pid);
    if (pid > 0) {
        kernel_process_terminate(pid);
    }
    
    /* Halt if kernel process or last process */
    __asm__ volatile("cli");
    while (1) {
        __asm__ volatile("hlt");
    }
    
    return 0;  /* Never reached */
}

/**
 * sys_yield - Yield CPU to scheduler
 */
static int sys_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                     uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    /* Force scheduler tick */
    scheduler_tick();
    return 0;
}

/**
 * sys_getpid - Get current process ID
 */
static int sys_getpid(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                      uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    return scheduler_get_current_pid();
}

/**
 * sys_sleep - Sleep for N timer ticks
 */
static int sys_sleep(uint32_t ticks, uint32_t arg2, uint32_t arg3,
                     uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    extern unsigned int pit_get_ticks(void);
    uint32_t end_tick = pit_get_ticks() + ticks;
    
    while (pit_get_ticks() < end_tick) {
        scheduler_tick();  /* Yield during sleep */
        __asm__ volatile("pause");
    }
    
    return 0;
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_core_handlers(void) {
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_YIELD, sys_yield);
    syscall_register(SYS_GETPID, sys_getpid);
    syscall_register(SYS_SLEEP, sys_sleep);
    
    KLOG_DEBUG("SYSCALL", "Core handlers registered (0-15)");
}
