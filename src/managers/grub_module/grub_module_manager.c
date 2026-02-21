/**
 * GRUB Module Manager for MaahiOS
 * 
 * Manages modules loaded by GRUB bootloader.
 */

#include "grub_module_manager.h"
#include "../klog/klog.h"

/* ===========================================================================
 * INTERNAL: Multiboot structures
 * =========================================================================== */

typedef struct {
    uint32_t mod_start;
    uint32_t mod_end;
    char *string;
    uint32_t reserved;
} multiboot_module_t;

/* ===========================================================================
 * INTERNAL: Module table
 * =========================================================================== */

static grub_module_info_t module_table[MAX_GRUB_MODULES];
static int module_count = 0;
static int initialized = 0;

/* Helper: copy string */
static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    if (src) {
        while (i < max - 1 && src[i]) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

/* ===========================================================================
 * INIT: Initialization
 * =========================================================================== */

int grub_module_manager_init(uint32_t mods_count, uint32_t mods_addr) {
    KLOG_INFO("GRUBMOD", "Initializing GRUB module manager...");
    
    if (mods_count == 0) {
        KLOG_WARN("GRUBMOD", "No modules loaded by GRUB");
        module_count = 0;
        initialized = 1;
        return 0;
    }
    
    if (mods_count > MAX_GRUB_MODULES) {
        KLOG_WARN_HEX("GRUBMOD", "Too many modules, capping at: ", MAX_GRUB_MODULES);
        mods_count = MAX_GRUB_MODULES;
    }
    
    multiboot_module_t *mods = (multiboot_module_t *)mods_addr;
    
    for (uint32_t i = 0; i < mods_count; i++) {
        module_table[i].index = i;
        module_table[i].start_addr = mods[i].mod_start;
        module_table[i].end_addr = mods[i].mod_end;
        module_table[i].size = mods[i].mod_end - mods[i].mod_start;
        str_copy(module_table[i].name, mods[i].string, 64);
        
        KLOG_DEBUG_HEX2("GRUBMOD", "Module idx/addr: ", i, mods[i].mod_start);
    }
    
    module_count = (int)mods_count;
    initialized = 1;
    
    KLOG_INFO_HEX("GRUBMOD", "Manager initialized, modules: ", module_count);
    return 0;
}

/* ===========================================================================
 * KERNEL API: Query Functions
 * =========================================================================== */

int kernel_grub_get_module_count(void) {
    return module_count;
}

int kernel_grub_get_module_info(int index, grub_module_info_t *info) {
    if (!initialized) {
        return MODULE_ERR_INVALID;
    }
    
    if (index < 0 || index >= module_count) {
        return MODULE_ERR_NOT_FOUND;
    }
    
    if (!info) {
        return MODULE_ERR_INVALID;
    }
    
    info->index = module_table[index].index;
    info->start_addr = module_table[index].start_addr;
    info->end_addr = module_table[index].end_addr;
    info->size = module_table[index].size;
    str_copy(info->name, module_table[index].name, 64);
    
    return MODULE_OK;
}

uint32_t kernel_grub_get_module_addr(int index) {
    if (!initialized || index < 0 || index >= module_count) {
        return 0;
    }
    
    /* Apply GRUB alignment fix (page-align start address) */
    return module_table[index].start_addr & 0xFFFFF000;
}

uint32_t kernel_grub_get_module_size(int index) {
    if (!initialized || index < 0 || index >= module_count) {
        return 0;
    }
    return module_table[index].size;
}

/* ===========================================================================
 * KERNEL API: Copy Module
 * =========================================================================== */

int kernel_grub_copy_module(int index, uint32_t target_addr) {
    if (!initialized) {
        KLOG_ERROR("GRUBMOD", "Manager not initialized");
        return MODULE_ERR_INVALID;
    }
    
    if (index < 0 || index >= module_count) {
        KLOG_ERROR_HEX("GRUBMOD", "Invalid module index: ", index);
        return MODULE_ERR_NOT_FOUND;
    }
    
    if (target_addr == 0) {
        KLOG_ERROR("GRUBMOD", "Invalid target address (0)");
        return MODULE_ERR_INVALID;
    }
    
    uint32_t src_addr = module_table[index].start_addr;
    uint32_t size = module_table[index].size;
    
    KLOG_INFO_HEX2("GRUBMOD", "Copying module to: ", target_addr, size);
    
    /* Perform the copy */
    uint8_t *src = (uint8_t *)src_addr;
    uint8_t *dst = (uint8_t *)target_addr;
    
    for (uint32_t i = 0; i < size; i++) {
        dst[i] = src[i];
    }
    
    KLOG_DEBUG_HEX("GRUBMOD", "Copy complete, bytes: ", size);
    return (int)size;
}
