/**
 * MaahiOS Device Manager
 * 
 * Provides a unified interface for all device drivers.
 * Drivers register themselves at init time, then apps
 * access them through generic syscalls (open/read/write/ioctl/close).
 */

#ifndef DEVICE_MANAGER_H
#define DEVICE_MANAGER_H

#include <stdint.h>
#include <stddef.h>

/* ============================================
 * Device IDs (System devices: 1-99)
 * Third-party devices: 100+
 * ============================================ */
#define DEV_NONE        0
#define DEV_MOUSE       1
#define DEV_KEYBOARD    2
#define DEV_DISPLAY     3
#define DEV_DISK        4   /* Base ID, actual disks are DEV_DISK + index */
#define DEV_TIMER       6
#define DEV_RTC         7   /* Real-Time Clock */

/* Reserved range for disks (4-19) */
#define DEV_DISK_MAX    19

#define DEV_NETWORK     20  /* E1000 NIC */

/* Third-party devices start here */
#define DEV_THIRD_PARTY_BASE 100

/* ============================================
 * IOCTL Commands (per device type)
 * ============================================ */

/* Mouse IOCTL */
#define MOUSE_IOCTL_GET_STATE       1   /* Get x, y, buttons in one call */
#define MOUSE_IOCTL_GET_IRQ_COUNT   2   /* Debug: get IRQ count */
#define MOUSE_IOCTL_RESET           3   /* Reset mouse */
#define MOUSE_IOCTL_CURSOR_HIDE     4   /* Hide SW cursor (restore bg) */
#define MOUSE_IOCTL_CURSOR_SHOW     5   /* Show SW cursor (redraw) */

/* Keyboard IOCTL */
#define KB_IOCTL_GET_SCANCODE       1   /* Get raw scancode */
#define KB_IOCTL_SET_LEDS           2   /* Set Caps/Num/Scroll LEDs */

/* Disk IOCTL */
#define DISK_IOCTL_GET_INFO         1   /* Get disk info struct */
#define DISK_IOCTL_GET_SECTOR_SIZE  2   /* Get sector size */
#define DISK_IOCTL_GET_SECTOR_COUNT 3   /* Get total sectors */
#define DISK_IOCTL_FLUSH            4   /* Flush cache */

/* Display IOCTL */
#define DISPLAY_IOCTL_GET_INFO      1   /* Get resolution, bpp */
#define DISPLAY_IOCTL_SET_MODE      2   /* Set video mode */
#define DISPLAY_IOCTL_GET_FB        3   /* Get framebuffer address */
#define DISPLAY_IOCTL_FLIP          4   /* Copy back buffer → HW framebuffer */
#define DISPLAY_IOCTL_FLIP_RECT     5   /* Copy rectangle back buf → HW fb   */

/* Network IOCTL */
#define NET_IOCTL_GET_MAC           1   /* Get MAC address (6 bytes) */
#define NET_IOCTL_LINK_STATUS       2   /* Get link status (1=up, 0=down) */
#define NET_IOCTL_GET_STATS         3   /* Get network stats */

/* ============================================
 * Error Codes
 * ============================================ */
#define DEV_OK              0
#define DEV_ERR_NOT_FOUND  -1
#define DEV_ERR_BUSY       -2
#define DEV_ERR_IO         -3
#define DEV_ERR_INVALID    -4
#define DEV_ERR_NO_MEMORY  -5
#define DEV_ERR_NOT_SUPPORTED -6
#define DEV_ERR_FULL       -7

/* ============================================
 * Device Operations Structure
 * 
 * Each driver provides these function pointers.
 * NULL = operation not supported for this device.
 * ============================================ */
typedef struct device_ops {
    /**
     * Open device. Called when app requests access.
     * @param flags Open flags (future: read-only, exclusive, etc.)
     * @return Handle (>=0) on success, negative error on failure
     */
    int (*open)(int flags);
    
    /**
     * Close device. Called when app is done.
     * @param handle Handle from open()
     * @return 0 on success, negative error on failure
     */
    int (*close)(int handle);
    
    /**
     * Read from device.
     * @param handle Handle from open()
     * @param buffer Output buffer
     * @param size Bytes to read
     * @return Bytes read (>=0), or negative error
     */
    int (*read)(int handle, void* buffer, size_t size);
    
    /**
     * Write to device.
     * @param handle Handle from open()
     * @param buffer Input buffer
     * @param size Bytes to write
     * @return Bytes written (>=0), or negative error
     */
    int (*write)(int handle, const void* buffer, size_t size);
    
    /**
     * Device-specific control commands.
     * @param handle Handle from open()
     * @param cmd IOCTL command (device-specific)
     * @param arg Command argument (device-specific)
     * @return Command result, or negative error
     */
    int (*ioctl)(int handle, int cmd, void* arg);
    
    /**
     * Poll device for data availability.
     * @param handle Handle from open()
     * @return 1 if data available, 0 if not, negative error
     */
    int (*poll)(int handle);
    
} device_ops_t;

/* ============================================
 * Device Info Structure (for listing)
 * ============================================ */
typedef struct device_info {
    int id;
    char name[32];
    int active;
    int open_count;
} device_info_t;

/* ============================================
 * Device Manager API
 * ============================================ */

/**
 * Initialize the device manager.
 * Must be called before any driver init.
 */
int device_manager_init(void);

/**
 * Register a device driver.
 * Called by drivers during their init.
 * 
 * @param device_id Unique device ID (use DEV_* constants)
 * @param name Human-readable name (max 31 chars)
 * @param ops Pointer to operations structure
 * @return 0 on success, negative error on failure
 */
int register_device(int device_id, const char* name, device_ops_t* ops);

/**
 * Unregister a device driver.
 * @param device_id Device ID to unregister
 * @return 0 on success, negative error on failure
 */
int unregister_device(int device_id);

/**
 * Check if a device is registered.
 * @param device_id Device ID to check
 * @return 1 if registered, 0 if not
 */
int device_exists(int device_id);

/**
 * Get device info.
 * @param device_id Device ID
 * @param info Output info structure
 * @return 0 on success, negative error on failure
 */
int device_get_info(int device_id, device_info_t* info);

/**
 * List all registered devices.
 * @param list Output array of device_info_t
 * @param max_count Maximum entries to return
 * @return Number of devices found
 */
int device_list_all(device_info_t* list, int max_count);

/* ============================================
 * Kernel API (called by syscall handlers)
 * ============================================ */

/**
 * Open a device (kernel API).
 * @param device_id Device to open
 * @param flags Open flags (reserved for future)
 * @return Handle (>=0) on success, negative error
 */
int kernel_device_open(int device_id, int flags);

/**
 * Close a device (kernel API).
 * @param device_id Device to close
 * @param handle Handle from open
 * @return 0 on success, negative error
 */
int kernel_device_close(int device_id, int handle);

/**
 * Read from a device (kernel API).
 * @param device_id Device to read from
 * @param buffer Output buffer
 * @param size Bytes to read
 * @return Bytes read (>=0), negative error
 */
int kernel_device_read(int device_id, void* buffer, size_t size);

/**
 * Write to a device (kernel API).
 * @param device_id Device to write to
 * @param buffer Input buffer
 * @param size Bytes to write
 * @return Bytes written (>=0), negative error
 */
int kernel_device_write(int device_id, const void* buffer, size_t size);

/**
 * Device control command (kernel API).
 * @param device_id Device to control
 * @param cmd IOCTL command
 * @param arg Command argument
 * @return Command result, negative error
 */
int kernel_device_ioctl(int device_id, int cmd, void* arg);

/**
 * Poll device for readiness (kernel API).
 * @param device_id Device to poll
 * @return 1 if ready, 0 if not, negative error
 */
int kernel_device_poll(int device_id);

/**
 * List all devices (kernel API).
 * @param list Output array
 * @param max_count Maximum entries
 * @return Number of devices found
 */
int kernel_device_list_all(device_info_t* list, int max_count);

#endif /* DEVICE_MANAGER_H */
