/**
 * Filesystem Syscall Handlers
 * Domain: 128-143 (fs_list_dir, read_file, file_count, find_dir, get_root_info)
 * 
 * Wraps the kernel ISO9660 driver for Ring 3 access.
 * Future: Will also route to MFS driver when available.
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include <stdint.h>

/* ===========================================================================
 * ISO9660 DRIVER TYPES (duplicated from iso9660.h for handler use)
 * =========================================================================== */

typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t lba;
    uint8_t  is_directory;
} iso_file_entry_t;

/* ===========================================================================
 * EXTERN KERNEL APIs (from ISO9660 driver)
 * =========================================================================== */

extern int      iso9660_list_root(iso_file_entry_t *entries, int max_entries);
extern int      iso9660_list_directory(uint32_t dir_lba, uint32_t dir_size,
                                       iso_file_entry_t *entries, int max_entries);
extern int      iso9660_find_directory(const char *name, uint32_t *out_lba,
                                        uint32_t *out_size);
extern int      iso9660_read_file(uint32_t file_lba, uint32_t file_size,
                                   void *buffer, uint32_t max_size);
extern int      iso9660_find_and_read_file(uint32_t dir_lba, uint32_t dir_size,
                                            const char *filename, void *buffer,
                                            uint32_t max_size);
extern int      iso9660_get_file_count(void);
extern uint32_t iso9660_get_root_lba(void);
extern uint32_t iso9660_get_root_size(void);

/* ===========================================================================
 * INTERNAL HELPERS
 * =========================================================================== */

/**
 * Simple string compare (case-insensitive, for path matching)
 */
static int fs_streq(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    return (*a == *b);
}

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_fs_list_dir - List files in a directory
 * arg1 = path string pointer (e.g., "/" or "/BOOT")
 * arg2 = pointer to iso_file_entry_t array (output buffer)
 * arg3 = max entries
 * Returns: count on success, negative on error
 */
static int sys_fs_list_dir(uint32_t path_ptr, uint32_t entries_ptr,
                           uint32_t max_entries, uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;

    const char *path = (const char *)path_ptr;
    iso_file_entry_t *entries = (iso_file_entry_t *)entries_ptr;

    if (!path || !entries || max_entries == 0) {
        return SYSCALL_ERR_INVALID;
    }

    KLOG_DEBUG("SYSCALL", "fs_list_dir: path=%s", path);

    /* Root directory */
    if (path[0] == '/' && path[1] == '\0') {
        return iso9660_list_root(entries, (int)max_entries);
    }

    /* Subdirectory: skip leading '/' */
    const char *dirname = path;
    if (dirname[0] == '/') dirname++;

    uint32_t dir_lba, dir_size;
    if (iso9660_find_directory(dirname, &dir_lba, &dir_size) != 0) {
        KLOG_WARN("SYSCALL", "fs_list_dir: directory not found: %s", dirname);
        return SYSCALL_ERR_NOTFOUND;
    }

    return iso9660_list_directory(dir_lba, dir_size, entries, (int)max_entries);
}

/**
 * sys_fs_read_file - Read a file's contents
 * arg1 = directory path (e.g., "/" or "/BOOT")
 * arg2 = filename
 * arg3 = output buffer
 * arg4 = max_size
 * Returns: bytes read on success, negative on error
 */
static int sys_fs_read_file(uint32_t dir_path_ptr, uint32_t filename_ptr,
                            uint32_t buf_ptr, uint32_t max_size, uint32_t arg5) {
    (void)arg5;

    const char *dir_path = (const char *)dir_path_ptr;
    const char *filename = (const char *)filename_ptr;
    void *buffer = (void *)buf_ptr;

    if (!dir_path || !filename || !buffer || max_size == 0) {
        return SYSCALL_ERR_INVALID;
    }

    KLOG_DEBUG("SYSCALL", "fs_read_file: dir=%s file=%s", dir_path, filename);

    uint32_t dir_lba, dir_size;

    /* Resolve directory */
    if (dir_path[0] == '/' && dir_path[1] == '\0') {
        dir_lba = iso9660_get_root_lba();
        dir_size = iso9660_get_root_size();
    } else {
        const char *dirname = dir_path;
        if (dirname[0] == '/') dirname++;
        if (iso9660_find_directory(dirname, &dir_lba, &dir_size) != 0) {
            return SYSCALL_ERR_NOTFOUND;
        }
    }

    return iso9660_find_and_read_file(dir_lba, dir_size, filename, buffer, max_size);
}

/**
 * sys_fs_file_count - Get number of files in a directory
 * arg1 = path string (e.g., "/" for root)
 * Returns: count on success, negative on error
 */
static int sys_fs_file_count(uint32_t path_ptr, uint32_t arg2, uint32_t arg3,
                             uint32_t arg4, uint32_t arg5) {
    (void)arg2; (void)arg3; (void)arg4; (void)arg5;

    const char *path = (const char *)path_ptr;
    if (!path) return SYSCALL_ERR_INVALID;

    /* Root directory */
    if (path[0] == '/' && path[1] == '\0') {
        return iso9660_get_file_count();
    }

    /* Subdirectory: list and count */
    const char *dirname = path;
    if (dirname[0] == '/') dirname++;

    uint32_t dir_lba, dir_size;
    if (iso9660_find_directory(dirname, &dir_lba, &dir_size) != 0) {
        return SYSCALL_ERR_NOTFOUND;
    }

    iso_file_entry_t tmp[32];
    return iso9660_list_directory(dir_lba, dir_size, tmp, 32);
}

/**
 * sys_fs_find_dir - Find a subdirectory by name
 * arg1 = directory name
 * arg2 = pointer to uint32_t for output LBA
 * arg3 = pointer to uint32_t for output size
 * Returns: 0 on success, negative on error
 */
static int sys_fs_find_dir(uint32_t name_ptr, uint32_t out_lba_ptr,
                           uint32_t out_size_ptr, uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;

    const char *name = (const char *)name_ptr;
    uint32_t *out_lba = (uint32_t *)out_lba_ptr;
    uint32_t *out_size = (uint32_t *)out_size_ptr;

    if (!name || !out_lba || !out_size) {
        return SYSCALL_ERR_INVALID;
    }

    return iso9660_find_directory(name, out_lba, out_size);
}

/**
 * sys_fs_get_root_info - Get root directory LBA and size
 * arg1 = pointer to uint32_t for LBA
 * arg2 = pointer to uint32_t for size
 * Returns: 0 on success
 */
static int sys_fs_get_root_info(uint32_t out_lba_ptr, uint32_t out_size_ptr,
                                uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    uint32_t *out_lba = (uint32_t *)out_lba_ptr;
    uint32_t *out_size = (uint32_t *)out_size_ptr;

    if (!out_lba || !out_size) {
        return SYSCALL_ERR_INVALID;
    }

    *out_lba = iso9660_get_root_lba();
    *out_size = iso9660_get_root_size();

    return (*out_lba != 0) ? SYSCALL_OK : SYSCALL_ERR_IO;
}

/* ===========================================================================
 * REGISTRATION
 * =========================================================================== */

void syscall_register_fs_handlers(void) {
    syscall_register(SYS_FS_LIST_DIR,      sys_fs_list_dir);
    syscall_register(SYS_FS_READ_FILE,     sys_fs_read_file);
    syscall_register(SYS_FS_FILE_COUNT,    sys_fs_file_count);
    syscall_register(SYS_FS_FIND_DIR,      sys_fs_find_dir);
    syscall_register(SYS_FS_GET_ROOT_INFO, sys_fs_get_root_info);

    KLOG_DEBUG("SYSCALL", "Filesystem handlers registered (128-143)");
}
