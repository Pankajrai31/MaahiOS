/**
 * Device I/O Syscall Handlers
 * Domain: 80-95 (dev_open, close, read, write, ioctl, poll, list)
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../device/device_manager.h"
#include <stdint.h>

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_dev_open - Open a device
 */
static int sys_dev_open(uint32_t device_id, uint32_t flags, uint32_t arg3,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    KLOG_DEBUG_HEX2("SYSCALL", "dev_open: id/flags: ", device_id, flags);
    
    /* Call kernel API */
    extern int kernel_device_open(int device_id, int flags);
    return kernel_device_open((int)device_id, (int)flags);
}

/**
 * sys_dev_close - Close a device
 */
static int sys_dev_close(uint32_t device_id, uint32_t handle, uint32_t arg3,
                         uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    KLOG_DEBUG_HEX2("SYSCALL", "dev_close: id/handle: ", device_id, handle);
    
    /* Call kernel API */
    extern int kernel_device_close(int device_id, int handle);
    return kernel_device_close((int)device_id, (int)handle);
}

/**
 * sys_dev_read - Read from a device
 */
static int sys_dev_read(uint32_t device_id, uint32_t buffer, uint32_t size,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!buffer) {
        return DEV_ERR_INVALID;
    }
    
    /* Call kernel API */
    extern int kernel_device_read(int device_id, void *buffer, size_t size);
    return kernel_device_read((int)device_id, (void *)buffer, (size_t)size);
}

/**
 * sys_dev_write - Write to a device
 */
static int sys_dev_write(uint32_t device_id, uint32_t buffer, uint32_t size,
                         uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    if (!buffer) {
        return DEV_ERR_INVALID;
    }
    
    /* Call kernel API */
    extern int kernel_device_write(int device_id, const void *buffer, size_t size);
    return kernel_device_write((int)device_id, (const void *)buffer, (size_t)size);
}

/**
 * sys_dev_ioctl - Device I/O control
 */
static int sys_dev_ioctl(uint32_t device_id, uint32_t cmd, uint32_t arg,
                         uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;
    
    /* Call kernel API */
    extern int kernel_device_ioctl(int device_id, int cmd, void *arg);
    return kernel_device_ioctl((int)device_id, (int)cmd, (void *)arg);
}

/**
 * sys_dev_poll - Poll device readiness
 */
static int sys_dev_poll(uint32_t device_id, uint32_t arg2, uint32_t arg3,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    
    /* Call kernel API */
    extern int kernel_device_poll(int device_id);
    return kernel_device_poll((int)device_id);
}

/**
 * sys_dev_list - List all devices
 */
static int sys_dev_list(uint32_t list_ptr, uint32_t max_count, uint32_t arg3,
                        uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;
    
    if (!list_ptr || max_count == 0) {
        return 0;
    }
    
    /* Call kernel API */
    extern int kernel_device_list_all(device_info_t *list, int max_count);
    return kernel_device_list_all((device_info_t *)list_ptr, (int)max_count);
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_device_handlers(void) {
    syscall_register(SYS_DEV_OPEN, sys_dev_open);
    syscall_register(SYS_DEV_CLOSE, sys_dev_close);
    syscall_register(SYS_DEV_READ, sys_dev_read);
    syscall_register(SYS_DEV_WRITE, sys_dev_write);
    syscall_register(SYS_DEV_IOCTL, sys_dev_ioctl);
    syscall_register(SYS_DEV_POLL, sys_dev_poll);
    syscall_register(SYS_DEV_LIST, sys_dev_list);
    
    KLOG_DEBUG("SYSCALL", "Device handlers registered (80-95)");
}
