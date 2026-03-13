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
    extern int process_terminate(int pid);
    if (pid > 0) {
        process_terminate(pid);
    }
    
    /* Re-enable interrupts so scheduler timer can fire, then idle */
    __asm__ volatile("sti");
    while (1) {
        __asm__ volatile("hlt");
    }
    
    return 0;  /* Never reached */
}

/**
 * sys_yield - Yield CPU to scheduler
 *
 * Instead of just calling scheduler_tick() and returning (which only
 * sets should_switch — the actual context switch wouldn't happen
 * until the next PIT interrupt, up to 20ms later), we trigger a
 * software INT 0x20 which enters irq0_stub.  That performs the full
 * context-switch sequence immediately: scheduler_tick → save ESP →
 * load next process → iret.  The pit_request_yield() flag tells the
 * PIT handler to skip incrementing pit_ticks so the system clock
 * stays accurate.
 */
extern void pit_request_yield(void);

static int sys_yield(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                     uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    /* Mark this as a yield (don't increment system tick counter) */
    pit_request_yield();
    /* Trigger timer interrupt via software — causes immediate
     * context switch through the same irq0_stub path as the
     * hardware PIT, but without waiting up to 20ms. */
    __asm__ volatile("int $0x20");
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

/**
 * sys_shutdown - Power off the system via ACPI (QEMU)
 */
static int sys_shutdown(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    KLOG_INFO("SYSCALL", "System shutdown requested");
    
    /* QEMU ACPI power off: write SLP_TYP | SLP_EN to PM1a_CNT_BLK */
    __asm__ volatile("outw %0, %1" :: "a"((uint16_t)0x2000), "Nd"((uint16_t)0x604));
    
    /* Bochs fallback */
    __asm__ volatile("outw %0, %1" :: "a"((uint16_t)0x2000), "Nd"((uint16_t)0xB004));
    
    /* If ACPI failed, halt CPU */
    __asm__ volatile("cli");
    while (1) { __asm__ volatile("hlt"); }
    
    return 0;  /* Never reached */
}

/**
 * sys_restart - Reset the system via keyboard controller (standard x86)
 */
static int sys_restart(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                       uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    KLOG_INFO("SYSCALL", "System restart requested");
    
    /* Standard x86 reset: pulse CPU RESET via keyboard controller */
    __asm__ volatile("outb %0, %1" :: "a"((uint8_t)0xFE), "Nd"((uint16_t)0x64));
    
    /* If keyboard controller reset failed, halt */
    __asm__ volatile("cli");
    while (1) { __asm__ volatile("hlt"); }
    
    return 0;  /* Never reached */
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_core_handlers(void) {
    syscall_register(SYS_EXIT, sys_exit);
    syscall_register(SYS_YIELD, sys_yield);
    syscall_register(SYS_GETPID, sys_getpid);
    syscall_register(SYS_SLEEP, sys_sleep);
    syscall_register(SYS_SHUTDOWN, sys_shutdown);
    syscall_register(SYS_RESTART, sys_restart);
    
    KLOG_DEBUG("SYSCALL", "Core handlers registered (0-15)");
}
