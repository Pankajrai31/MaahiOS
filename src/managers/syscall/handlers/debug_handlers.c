/**
 * Debug Syscall Handlers
 * Domain: 240-255 (klog, system info)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include <stdint.h>

/* ===========================================================================
 * KLOG HANDLER
 * =========================================================================== */

/**
 * sys_klog - Write to log from Ring 3
 * 
 * Since this is called via syscall (from user space / ring 3),
 * we output with [U] prefix to distinguish from kernel logs [K].
 */
static int sys_klog(uint32_t level, uint32_t tag_ptr, uint32_t msg_ptr,
                    uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!tag_ptr || !msg_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    /* Clamp level to valid range */
    if (level > LOG_TRACE) {
        level = LOG_TRACE;
    }
    
    const char *tag = (const char *)tag_ptr;
    const char *msg = (const char *)msg_ptr;
    
    /* Use ulog for [U] prefix since request is from ring 3 */
    extern void ulog(int level, const char *tag, const char *msg);
    ulog(level, tag, msg);
    
    return SYSCALL_OK;
}

/**
 * sys_klog_hex - Write to log with hex value from Ring 3
 * 
 * Outputs with [U] prefix since called from user space.
 */
static int sys_klog_hex(uint32_t level, uint32_t tag_ptr, uint32_t msg_ptr,
                        uint32_t value, uint32_t arg5) {
    (void)arg5;
    
    if (!tag_ptr || !msg_ptr) {
        return SYSCALL_ERR_INVALID;
    }
    
    if (level > LOG_TRACE) {
        level = LOG_TRACE;
    }
    
    const char *tag = (const char *)tag_ptr;
    const char *msg = (const char *)msg_ptr;
    
    /* Use ulog_hex for [U] prefix since request is from ring 3 */
    extern void ulog_hex(int level, const char *tag, const char *msg, unsigned int value);
    ulog_hex(level, tag, msg, value);
    
    return SYSCALL_OK;
}

/**
 * sys_klog_get_shm_id - Get KLOG SHM ID for direct access (deprecated)
 */
static int sys_klog_get_shm_id(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                                uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return -1;  /* No longer supported */
}

/**
 * sys_klog_read - Copy klog ring buffer entries to user-space buffer
 * arg1: pointer to user buffer (array of klog_entry_t)
 * arg2: max entries to copy
 * Returns: number of entries copied, or negative on error
 */
static int sys_klog_read(uint32_t buf_ptr, uint32_t max_entries,
                         uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    if (!buf_ptr) return SYSCALL_ERR_INVALID;
    if (max_entries == 0) return 0;

    /* Limit to buffer size */
    if (max_entries > KLOG_BUFFER_SIZE)
        max_entries = KLOG_BUFFER_SIZE;

    extern int klog_read_entries(klog_entry_t *dst, int max_entries);
    klog_entry_t *dst = (klog_entry_t *)buf_ptr;

    return klog_read_entries(dst, (int)max_entries);
}

/* ===========================================================================
 * SYSTEM INFO HANDLERS (Stubs)
 * =========================================================================== */

static int sys_get_cpu_info(uint32_t info_ptr, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)info_ptr; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return SYSCALL_ERR_NOSYS;
}

static int sys_get_mem_info(uint32_t info_ptr, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    if (!info_ptr) return SYSCALL_ERR_INVALID;
    
    extern uint32_t pmm_get_total_pages(void);
    extern uint32_t pmm_get_used_pages(void);
    
    uint32_t total = pmm_get_total_pages();
    uint32_t used  = pmm_get_used_pages();
    uint32_t free  = total - used;
    
    uint32_t *info = (uint32_t *)info_ptr;
    info[0] = total * 4096;   /* total_memory (bytes) */
    info[1] = free  * 4096;   /* free_memory  (bytes) */
    info[2] = used  * 4096;   /* used_memory  (bytes) */
    
    return SYSCALL_OK;
}

static int sys_get_pic_mask(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    extern unsigned int irq_get_pic_mask(void);
    return (int)irq_get_pic_mask();
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_debug_handlers(void) {
    syscall_register(SYS_KLOG, sys_klog);
    syscall_register(SYS_KLOG_HEX, sys_klog_hex);
    syscall_register(SYS_KLOG_GET_SHM, sys_klog_get_shm_id);
    syscall_register(SYS_KLOG_READ, sys_klog_read);
    syscall_register(SYS_GET_CPU_INFO, sys_get_cpu_info);
    syscall_register(SYS_GET_MEM_INFO, sys_get_mem_info);
    syscall_register(SYS_GET_PIC_MASK, sys_get_pic_mask);
    
    KLOG_INFO("SYSCALL", "Debug handlers registered (240-255)");
}
