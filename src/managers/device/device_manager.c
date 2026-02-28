/**
 * MaahiOS Device Manager
 * Central registry for all device drivers.
 * 
 * Acts as a HAL-like abstraction layer:
 * - Auto-discovers and initializes all registered drivers
 * - Provides unified device API (open/close/read/write/ioctl)
 * - Manages device lifecycle
 * 
 * NOTE: Display driver is initialized separately in kernel.c STEP 5
 *       because graphics must be available before Device Manager.
 */

#include "device_manager.h"
#include "../../managers/klog/klog.h"

/* Driver headers for auto-initialization */
#include "../../drivers/drive/disk/disk_subsystem.h"
#include "../../drivers/display/display.h"
#include "../../drivers/mouse/mouse.h"
#include "../../drivers/keyboard/keyboard.h"
#include "../../drivers/rtc/rtc.h"

/* ===========================================================================
 * INTERNAL: Configuration & Data Structures
 * =========================================================================== */

#define MAX_DEVICES 32

/* ===========================================================================
 * DRIVER TABLE: All drivers that Device Manager auto-initializes
 * 
 * Format: { "name", init_function, device_id }
 * 
 * Order matters! Dependencies should be initialized first:
 *   - Disk first (filesystem access)
 *   - Input devices (mouse, keyboard)
 *   - Utility devices (RTC)
 * 
 * NOTE: Display hardware is initialized in STEP 5 (kernel.c) because
 *       graphics must be available before klog/gfx. Its device_manager
 *       registration happens here via display_register_device().
 * =========================================================================== */
typedef struct {
    const char *name;
    int (*init)(void);
    int device_id;
} driver_entry_t;

static driver_entry_t g_driver_table[] = {
    { "disk",     disk_subsystem_init,     DEV_DISK     },
    { "display",  display_register_device, DEV_DISPLAY  },
    { "mouse",    mouse_init,              DEV_MOUSE    },
    { "keyboard", keyboard_init,           DEV_KEYBOARD },
    { "rtc",      rtc_init,                DEV_RTC      },
    { NULL, NULL, 0 }  /* Terminator */
};

typedef struct {
    int id;
    char name[32];
    device_ops_t ops;
    int active;
    int open_count;
} device_entry_t;

static device_entry_t devices[MAX_DEVICES];
static int device_count = 0;
static int initialized = 0;

/* ===========================================================================
 * INTERNAL: Helper Functions
 * =========================================================================== */

static void str_copy(char* dst, const char* src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

static device_entry_t* find_device(int device_id) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (devices[i].active && devices[i].id == device_id) {
            return &devices[i];
        }
    }
    return (device_entry_t*)0;
}

static device_entry_t* find_empty_slot(void) {
    for (int i = 0; i < MAX_DEVICES; i++) {
        if (!devices[i].active) {
            return &devices[i];
        }
    }
    return (device_entry_t*)0;
}

/* ===========================================================================
 * INIT: Initialization Functions
 * =========================================================================== */

/**
 * Initialize the Device Manager and auto-discover all drivers.
 * 
 * This is the single entry point for driver initialization:
 *   1. Clear device registry
 *   2. Loop through driver table
 *   3. Call each driver's init() function
 *   4. Drivers register themselves via register_device()
 */
int device_manager_init(void) {
    int i;
    int success_count = 0;
    int fail_count = 0;
    
    /* Clear all entries first */
    for (i = 0; i < MAX_DEVICES; i++) {
        devices[i].id = 0;
        devices[i].name[0] = '\0';
        devices[i].active = 0;
        devices[i].open_count = 0;
        devices[i].ops.open = (void*)0;
        devices[i].ops.close = (void*)0;
        devices[i].ops.read = (void*)0;
        devices[i].ops.write = (void*)0;
        devices[i].ops.ioctl = (void*)0;
        devices[i].ops.poll = (void*)0;
    }
    
    device_count = 0;
    initialized = 1;
    
    KLOG_INFO("DEVMGR", "=== Device Manager Auto-Init ===");
    
    /* Auto-initialize all drivers from table */
    for (i = 0; g_driver_table[i].name != (void*)0; i++) {
        driver_entry_t *drv = &g_driver_table[i];
        
        KLOG_INFO("DEVMGR", "Initializing driver:");
        KLOG_INFO("DEVMGR", drv->name);
        
        if (drv->init) {
            int result = drv->init();
            if (result == 0) {
                KLOG_INFO("DEVMGR", "  -> OK");
                success_count++;
            } else {
                KLOG_WARN_HEX("DEVMGR", "  -> FAILED err=", result);
                fail_count++;
            }
        } else {
            KLOG_WARN("DEVMGR", "  -> No init function!");
            fail_count++;
        }
    }
    
    KLOG_INFO_HEX("DEVMGR", "Drivers initialized: ", success_count);
    if (fail_count > 0) {
        KLOG_WARN_HEX("DEVMGR", "Drivers failed: ", fail_count);
    }
    KLOG_INFO("DEVMGR", "=== Device Manager Ready ===");
    
    return (fail_count == 0) ? 0 : -1;
}

/* ===========================================================================
 * LIFECYCLE: Register/Unregister Functions
 * =========================================================================== */

int register_device(int device_id, const char* name, device_ops_t* ops) {
    if (!initialized) {
        return DEV_ERR_INVALID;
    }
    
    if (device_id <= DEV_NONE || !name || !ops) {
        return DEV_ERR_INVALID;
    }
    
    /* Check if already registered */
    if (find_device(device_id)) {
        return DEV_ERR_BUSY;
    }
    
    /* Find empty slot */
    device_entry_t* entry = find_empty_slot();
    if (!entry) {
        return DEV_ERR_FULL;
    }
    
    /* Fill entry */
    entry->id = device_id;
    str_copy(entry->name, name, 32);
    entry->active = 1;
    entry->open_count = 0;
    
    /* Copy operations */
    entry->ops.open = ops->open;
    entry->ops.close = ops->close;
    entry->ops.read = ops->read;
    entry->ops.write = ops->write;
    entry->ops.ioctl = ops->ioctl;
    entry->ops.poll = ops->poll;
    
    device_count++;
    
    KLOG_INFO_HEX("DEVMGR", "Device registered id=", device_id);
    
    return DEV_OK;
}

int unregister_device(int device_id) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        return DEV_ERR_NOT_FOUND;
    }
    
    /* Don't unregister if device is in use */
    if (dev->open_count > 0) {
        return DEV_ERR_BUSY;
    }
    
    /* Clear entry */
    dev->active = 0;
    dev->id = 0;
    dev->name[0] = '\0';
    device_count--;
    
    KLOG_INFO_HEX("DEVMGR", "Device unregistered id=", device_id);
    
    return DEV_OK;
}

/* ===========================================================================
 * QUERY: Device Information Functions
 * =========================================================================== */

int device_exists(int device_id) {
    return find_device(device_id) ? 1 : 0;
}

int device_get_info(int device_id, device_info_t* info) {
    if (!info) {
        return DEV_ERR_INVALID;
    }
    
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        return DEV_ERR_NOT_FOUND;
    }
    
    info->id = dev->id;
    str_copy(info->name, dev->name, 32);
    info->active = dev->active;
    info->open_count = dev->open_count;
    
    return DEV_OK;
}

int device_list_all(device_info_t* list, int max_count) {
    if (!list || max_count <= 0) {
        return 0;
    }
    
    int count = 0;
    for (int i = 0; i < MAX_DEVICES && count < max_count; i++) {
        if (devices[i].active) {
            list[count].id = devices[i].id;
            str_copy(list[count].name, devices[i].name, 32);
            list[count].active = devices[i].active;
            list[count].open_count = devices[i].open_count;
            count++;
        }
    }
    
    return count;
}

/* ===========================================================================
 * KERNEL API: Called by syscall handlers
 * =========================================================================== */

int kernel_device_open(int device_id, int flags) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        KLOG_WARN_HEX("DEVMGR", "open: device not found id=", device_id);
        return DEV_ERR_NOT_FOUND;
    }
    
    if (!dev->ops.open) {
        /* No open function = always accessible */
        dev->open_count++;
        KLOG_DEBUG_HEX("DEVMGR", "open: ok (default) id=", device_id);
        return 0;  /* Default handle */
    }
    
    int handle = dev->ops.open(flags);
    if (handle >= 0) {
        dev->open_count++;
        KLOG_DEBUG_HEX2("DEVMGR", "open: ok id=", device_id, handle);
    } else {
        KLOG_WARN_HEX2("DEVMGR", "open: failed id=", device_id, handle);
    }
    
    return handle;
}

int kernel_device_close(int device_id, int handle) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        KLOG_WARN_HEX("DEVMGR", "close: device not found id=", device_id);
        return DEV_ERR_NOT_FOUND;
    }
    
    if (dev->open_count <= 0) {
        KLOG_WARN_HEX("DEVMGR", "close: not open id=", device_id);
        return DEV_ERR_INVALID;
    }
    
    int result = DEV_OK;
    if (dev->ops.close) {
        result = dev->ops.close(handle);
    }
    
    if (result >= 0) {
        dev->open_count--;
        KLOG_DEBUG_HEX("DEVMGR", "close: ok id=", device_id);
    } else {
        KLOG_WARN_HEX2("DEVMGR", "close: failed id=", device_id, result);
    }
    
    return result;
}

int kernel_device_read(int device_id, void* buffer, size_t size) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        KLOG_WARN_HEX("DEVMGR", "read: device not found id=", device_id);
        return DEV_ERR_NOT_FOUND;
    }
    
    if (!dev->ops.read) {
        KLOG_WARN_HEX("DEVMGR", "read: not supported id=", device_id);
        return DEV_ERR_NOT_SUPPORTED;
    }
    
    /* Note: For simplicity, using handle=0 (single handle per device)
     * Future: Track handles per-process */
    int result = dev->ops.read(0, buffer, size);
    return result;
}

int kernel_device_write(int device_id, const void* buffer, size_t size) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        KLOG_WARN_HEX("DEVMGR", "write: device not found id=", device_id);
        return DEV_ERR_NOT_FOUND;
    }
    
    if (!dev->ops.write) {
        KLOG_WARN_HEX("DEVMGR", "write: not supported id=", device_id);
        return DEV_ERR_NOT_SUPPORTED;
    }
    
    int result = dev->ops.write(0, buffer, size);
    KLOG_DEBUG_HEX2("DEVMGR", "write: id=", device_id, result);
    return result;
}

int kernel_device_ioctl(int device_id, int cmd, void* arg) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        KLOG_WARN_HEX("DEVMGR", "ioctl: device not found id=", device_id);
        return DEV_ERR_NOT_FOUND;
    }
    
    if (!dev->ops.ioctl) {
        KLOG_WARN_HEX("DEVMGR", "ioctl: not supported id=", device_id);
        return DEV_ERR_NOT_SUPPORTED;
    }
    
    int result = dev->ops.ioctl(0, cmd, arg);
    KLOG_DEBUG_HEX2("DEVMGR", "ioctl: id=", device_id, result);
    return result;
}

int kernel_device_poll(int device_id) {
    device_entry_t* dev = find_device(device_id);
    if (!dev) {
        KLOG_WARN_HEX("DEVMGR", "poll: device not found id=", device_id);
        return DEV_ERR_NOT_FOUND;
    }
    
    if (!dev->ops.poll) {
        /* No poll = always ready */
        return 1;
    }
    
    int result = dev->ops.poll(0);
    KLOG_TRACE_HEX2("DEVMGR", "poll: id=", device_id, result);
    return result;
}

int kernel_device_list_all(device_info_t* list, int max_count) {
    return device_list_all(list, max_count);
}
