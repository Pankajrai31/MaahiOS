/**
 * MaahiOS Syscall Manager
 * Central dispatcher for all system calls (INT 0x80)
 */

#include "syscall_manager.h"
#include "syscall_numbers.h"
#include "../klog/klog.h"
#include <stdint.h>

/* ===========================================================================
 * INTERNAL: Dispatch Table
 * =========================================================================== */

static syscall_handler_t syscall_table[SYSCALL_MAX + 1];
static uint32_t syscall_counts[SYSCALL_MAX + 1];
static int initialized = 0;

/* ===========================================================================
 * INIT: Initialization
 * =========================================================================== */

/* Forward declarations of handler registration functions */
extern void syscall_register_core_handlers(void);
extern void syscall_register_process_handlers(void);
extern void syscall_register_memory_handlers(void);
extern void syscall_register_shm_handlers(void);
extern void syscall_register_cell_handlers(void);
extern void syscall_register_device_handlers(void);
extern void syscall_register_module_handlers(void);
extern void syscall_register_time_handlers(void);
extern void syscall_register_debug_handlers(void);
extern void syscall_register_fs_handlers(void);
extern void syscall_register_network_handlers(void);

int syscall_manager_init(void) {
    KLOG_INFO("SYSCALL", "Initializing syscall manager...");
    
    /* Clear dispatch table */
    for (int i = 0; i <= SYSCALL_MAX; i++) {
        syscall_table[i] = (syscall_handler_t)0;
        syscall_counts[i] = 0;
    }
    
    /* Register handlers for existing managers only */
    syscall_register_core_handlers();       /* 0-15:   Core */
    syscall_register_process_handlers();    /* 16-31:  Process */
    syscall_register_memory_handlers();     /* 32-47:  Memory */
    syscall_register_shm_handlers();        /* 48-63:  SHM */
    syscall_register_cell_handlers();       /* 64-79:  Cell */
    syscall_register_device_handlers();     /* 80-95:  Device */
    syscall_register_module_handlers();     /* 96-111: Module */
    syscall_register_time_handlers();        /* 112-127: Time */
    syscall_register_debug_handlers();      /* 240-255: Debug */
    syscall_register_fs_handlers();          /* 128-143: Filesystem */
    syscall_register_network_handlers();     /* 144-159: Network */
    
    initialized = 1;
    
    /* Count registered handlers */
    int count = 0;
    for (int i = 0; i <= SYSCALL_MAX; i++) {
        if (syscall_table[i]) count++;
    }
    
    KLOG_INFO_HEX("SYSCALL", "Manager initialized, handlers: ", count);
    return 0;
}

/* ===========================================================================
 * OPERATIONS: Handler Registration
 * =========================================================================== */

int syscall_register(int syscall_num, syscall_handler_t handler) {
    if (syscall_num < 0 || syscall_num > SYSCALL_MAX) {
        KLOG_ERROR_HEX("SYSCALL", "Invalid syscall number: ", syscall_num);
        return -1;
    }
    
    if (syscall_table[syscall_num]) {
        KLOG_WARN_HEX("SYSCALL", "Overwriting handler for: ", syscall_num);
    }
    
    syscall_table[syscall_num] = handler;
    return 0;
}

/* ===========================================================================
 * OPERATIONS: Main Dispatcher
 * =========================================================================== */

uint32_t syscall_dispatch(uint32_t syscall_num,
                          uint32_t arg1, uint32_t arg2,
                          uint32_t arg3, uint32_t arg4,
                          uint32_t user_esp) {
    /* Re-enable interrupts during syscall handling */
    __asm__ volatile("sti");
    
    /* GATE 1: Manager initialized? */
    if (!initialized) {
        KLOG_ERROR("SYSCALL", "Manager not initialized!");
        return (uint32_t)SYSCALL_ERR_NOSYS;
    }
    
    /* GATE 2: Range check */
    if (syscall_num > SYSCALL_MAX) {
        KLOG_WARN_HEX("SYSCALL", "Invalid syscall number: ", syscall_num);
        return (uint32_t)SYSCALL_ERR_NOSYS;
    }
    
    /* GATE 3: Handler exists? */
    syscall_handler_t handler = syscall_table[syscall_num];
    if (!handler) {
        KLOG_WARN_HEX("SYSCALL", "Unimplemented syscall: ", syscall_num);
        return (uint32_t)SYSCALL_ERR_NOSYS;
    }
    
    /* Get additional argument from user stack if needed */
    uint32_t arg5 = 0;
    if (user_esp > 0) {
        uint32_t *user_stack = (uint32_t *)user_esp;
        arg5 = user_stack[0];
    }
    
    /* AUDIT: Log syscall entry (trace level - only if enabled) */
    KLOG_TRACE_HEX2("SYSCALL", "Enter: num/arg1: ", syscall_num, arg1);
    
    /* Update statistics */
    syscall_counts[syscall_num]++;
    
    /* DISPATCH: Call the handler */
    int result = handler(arg1, arg2, arg3, arg4, arg5);
    
    /* AUDIT: Log syscall exit (trace level) */
    KLOG_TRACE_HEX2("SYSCALL", "Exit: num/result: ", syscall_num, (uint32_t)result);
    
    return (uint32_t)result;
}

/* ===========================================================================
 * QUERY: Statistics
 * =========================================================================== */

uint32_t syscall_get_count(int syscall_num) {
    if (syscall_num < 0 || syscall_num > SYSCALL_MAX) {
        return 0;
    }
    return syscall_counts[syscall_num];
}
