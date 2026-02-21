/**
 * GRUB Module Manager for MaahiOS
 * 
 * Manages modules loaded by GRUB bootloader.
 * Provides kernel and syscall APIs to:
 *   - Query module count and info
 *   - Copy modules to target addresses
 *   - Get module entry points
 */

#ifndef GRUB_MODULE_MANAGER_H
#define GRUB_MODULE_MANAGER_H

#include <stdint.h>
#include <stddef.h>

/* Error codes */
#define MODULE_OK            0
#define MODULE_ERR_INVALID  -1
#define MODULE_ERR_NOT_FOUND -2
#define MODULE_ERR_NO_MEMORY -3

/* Maximum modules supported */
#define MAX_GRUB_MODULES    16

/* Module info structure */
typedef struct {
    uint32_t index;         /* Module index (0-based) */
    uint32_t start_addr;    /* Physical start address */
    uint32_t end_addr;      /* Physical end address */
    uint32_t size;          /* Size in bytes */
    char name[64];          /* Module name (from GRUB cmdline) */
} grub_module_info_t;

/**
 * Initialize the GRUB module manager
 * Called once during kernel boot with multiboot info
 * @param mods_count Number of modules from multiboot
 * @param mods_addr Address of multiboot module array
 * @return 0 on success, negative error on failure
 */
int grub_module_manager_init(uint32_t mods_count, uint32_t mods_addr);

/**
 * Get total number of modules loaded by GRUB
 * @return Number of modules (0 if none)
 */
int kernel_grub_get_module_count(void);

/**
 * Get info about a specific module
 * @param index Module index (0-based)
 * @param info Output structure
 * @return MODULE_OK on success, negative error on failure
 */
int kernel_grub_get_module_info(int index, grub_module_info_t *info);

/**
 * Copy a module to a target address
 * @param index Module index
 * @param target_addr Destination address to copy to
 * @return Bytes copied on success, negative error on failure
 */
int kernel_grub_copy_module(int index, uint32_t target_addr);

/**
 * Get module start address (without copying)
 * Useful for modules that can run in-place
 * @param index Module index
 * @return Start address, or 0 on error
 */
uint32_t kernel_grub_get_module_addr(int index);

/**
 * Get module size
 * @param index Module index
 * @return Size in bytes, or 0 on error
 */
uint32_t kernel_grub_get_module_size(int index);

#endif /* GRUB_MODULE_MANAGER_H */
