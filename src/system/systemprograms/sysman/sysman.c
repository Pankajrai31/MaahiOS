/**
 * MaahiOS System Manager (sysman)
 * 
 * First ring-3 process (PID 1). Responsible for:
 *   - Loading executives one by one
 *   - Loading Orbit (desktop shell)
 *   - Then idling forever (timer handles multitasking)
 */

#include <stdint.h>
#include "../../libraries/liblog/liblog.h"

/*=============================================================================
 * Syscall Numbers
 *===========================================================================*/
#define SYS_YIELD               1
#define SYS_PROCESS_CREATE      16
#define SYS_MOD_GET_COUNT       96
#define SYS_MOD_GET_ADDR        98
#define SYS_KLOG                240
#define SYS_KLOG_HEX            241

/* Log levels */
#define LOG_INFO    3
#define LOG_WARN    2
#define LOG_ERROR   1

/*=============================================================================
 * Syscall Wrappers
 *===========================================================================*/

static inline int syscall0(int num) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num) : "memory");
    return ret;
}

static inline int syscall1(int num, uint32_t a) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a) : "memory");
    return ret;
}

static inline int syscall3(int num, uint32_t a, uint32_t b, uint32_t c) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c) : "memory");
    return ret;
}

static inline int syscall4(int num, uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    int ret;
    __asm__ volatile("int $0x80" : "=a"(ret) : "a"(num), "b"(a), "c"(b), "d"(c), "S"(d) : "memory");
    return ret;
}

/*=============================================================================
 * KLOG Wrapper
 *===========================================================================*/

static void klog(int level, const char *tag, const char *msg) {
    syscall3(SYS_KLOG, level, (uint32_t)tag, (uint32_t)msg);
}

static void klog_hex(int level, const char *tag, const char *msg, uint32_t value) {
    syscall4(SYS_KLOG_HEX, level, (uint32_t)tag, (uint32_t)msg, value);
}

/*=============================================================================
 * Helper Functions
 *===========================================================================*/

static uint32_t get_module_addr(int index) {
    return syscall1(SYS_MOD_GET_ADDR, index);
}

static int get_module_count(void) {
    return syscall0(SYS_MOD_GET_COUNT);
}

static int create_process(uint32_t entry_addr) {
    return syscall1(SYS_PROCESS_CREATE, entry_addr);
}

static void yield(void) {
    syscall0(SYS_YIELD);
}

static void delay(int count) {
    for (int i = 0; i < count; i++) {
        yield();
    }
}

/*=============================================================================
 * Main Entry Point
 *===========================================================================*/

#define MOD_SYSMAN          0
#define MOD_CELLEXEC        1
#define MOD_LOGEXEC         2
#define MOD_UIMANAGER       3
#define MOD_ORBIT           4

void sysman_main_c(void) {
    klog(LOG_INFO, "SYSMAN", "========================================");
    klog(LOG_INFO, "SYSMAN", "  MaahiOS System Manager v2.0");
    klog(LOG_INFO, "SYSMAN", "========================================");
    klog(LOG_INFO, "SYSMAN", "Starting system initialization...");
    
    /* Get module count */
    int mod_count = get_module_count();
    klog_hex(LOG_INFO, "SYSMAN", "GRUB modules available:", mod_count);
    
    /*=========================================================================
     * STEP 1: Load Cell Executive
     *=======================================================================*/
    klog(LOG_INFO, "SYSMAN", "Loading Cell Executive...");
    
    uint32_t cellexec_addr = get_module_addr(MOD_CELLEXEC);
    if (cellexec_addr == 0) {
        klog(LOG_ERROR, "SYSMAN", "Cell Executive not found!");
        while(1) __asm__ volatile("pause");
    }
    
    klog_hex(LOG_INFO, "SYSMAN", "  Address:", cellexec_addr);
    
    int cellexec_pid = create_process(cellexec_addr);
    if (cellexec_pid < 0) {
        klog(LOG_ERROR, "SYSMAN", "Failed to start Cell Executive!");
        while(1) __asm__ volatile("pause");
    }
    
    klog_hex(LOG_INFO, "SYSMAN", "  Started as PID:", cellexec_pid);
    delay(10);
    
    /*=========================================================================
     * STEP 2: Load Log Executive
     *=======================================================================*/
    klog(LOG_INFO, "SYSMAN", "Loading Log Executive...");
    
    uint32_t logexec_addr = get_module_addr(MOD_LOGEXEC);
    if (logexec_addr == 0) {
        klog(LOG_ERROR, "SYSMAN", "Log Executive not found!");
        /* Continue anyway - logging will fall back to klog */
    } else {
        klog_hex(LOG_INFO, "SYSMAN", "  Address:", logexec_addr);
        
        int logexec_pid = create_process(logexec_addr);
        if (logexec_pid < 0) {
            klog(LOG_ERROR, "SYSMAN", "Failed to start Log Executive!");
        } else {
            klog_hex(LOG_INFO, "SYSMAN", "  Started as PID:", logexec_pid);
        }
    }
    delay(20);  /* Give Log Executive time to initialize */
    
    /* Try to switch to user logging via Log Executive */
    if (liblog_init() == 0) {
        liblog(LOG_INFO, "SYSMAN", "Switched to Log Executive for logging");
    } else {
        klog(LOG_WARN, "SYSMAN", "Could not connect to Log Executive");
    }
    
    /*=========================================================================
     * STEP 3: Skip UIManager and Orbit for now
     *=======================================================================*/
    if (liblog_ready()) {
        liblog(LOG_WARN, "SYSMAN", "UIManager and Orbit disabled for testing");
    } else {
        klog(LOG_WARN, "SYSMAN", "UIManager and Orbit disabled for testing");
    }
    
    /*=========================================================================
     * IDLE LOOP
     *=======================================================================*/
    if (liblog_ready()) {
        liblog(LOG_INFO, "SYSMAN", "Initialization complete. Entering idle.");
        liblog(LOG_INFO, "SYSMAN", "========================================");
    } else {
        klog(LOG_INFO, "SYSMAN", "Initialization complete. Entering idle.");
        klog(LOG_INFO, "SYSMAN", "========================================");
    }
    
    while(1) {
        yield();
    }
}
