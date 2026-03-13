/**
 * Filesystem Syscall Handlers
 * Domain: 128-143 (list_dir, read_file, file_count, find_dir, get_root_info,
 *                   write_file, delete_file, create_dir, vol_count, vol_info)
 * 
 * Routes all FS operations through the volume driver (voldrive),
 * which dispatches to ISO9660 or MFS based on volume type.
 */

#include "../syscall_manager.h"
#include "../syscall_numbers.h"
#include "../../klog/klog.h"
#include "../../../drivers/drive/partition/partdrive.h"
#include <stdint.h>

/* ===========================================================================
 * EXTERN KERNEL APIs (from Volume Driver)
 * =========================================================================== */

/* Volume driver types */
typedef struct {
    char     name[64];
    uint32_t size;
    uint32_t lba;
    uint8_t  is_directory;
} vol_file_entry_t;

extern int  voldrive_get_default(void);
extern int  voldrive_find_by_letter(char letter);
extern int  voldrive_get_count(void);
extern int  voldrive_list_dir(uint8_t vol_index, const char *path,
                              vol_file_entry_t *entries, int max);
extern int  voldrive_read_file(uint8_t vol_index, const char *dir_path,
                               const char *filename, void *buffer, uint32_t max_size);
extern int  voldrive_file_count(uint8_t vol_index, const char *path);
extern int  voldrive_find_dir(uint8_t vol_index, const char *name,
                              uint32_t *out_lba, uint32_t *out_size);
extern int  voldrive_get_root_info(uint8_t vol_index, uint32_t *out_lba, uint32_t *out_size);
extern int  voldrive_write_file(uint8_t vol_index, const char *dir_path,
                                const char *filename, const void *data, uint32_t size);
extern int  voldrive_delete_file(uint8_t vol_index, const char *dir_path,
                                 const char *filename);
extern int  voldrive_create_dir(uint8_t vol_index, const char *parent_path,
                                const char *dirname);

/* Volume info (for SYS_FS_VOL_INFO) */
typedef struct {
    uint8_t  mounted;
    uint8_t  fs_type;
    uint8_t  part_index;
    char     drive_letter;
    uint32_t size_mb;
    char     label[32];
    char     fs_str[16];
} vol_info_user_t;

extern void *voldrive_get_volume(uint8_t index);

/* ===========================================================================
 * VOLUME RESOLUTION HELPER
 *
 * Parses optional drive letter prefix from paths:
 *   "D:/"     → volume for 'D', path becomes "/"
 *   "D:/BOOT" → volume for 'D', path becomes "/BOOT"
 *   "/"       → default volume (first mounted)
 *
 * Returns volume index, updates *path_ptr to skip the prefix.
 * =========================================================================== */

static int resolve_volume(const char **path_ptr) {
    const char *p = *path_ptr;
    if (!p) return voldrive_get_default();

    /* Check for "X:/" or "X:\" pattern */
    if (((p[0] >= 'A' && p[0] <= 'Z') || (p[0] >= 'a' && p[0] <= 'z'))
        && p[1] == ':') {
        int vol = voldrive_find_by_letter(p[0]);
        if (vol >= 0) {
            /* Advance past "X:" — leave the "/" */
            if (p[2] == '/' || p[2] == '\\') {
                *path_ptr = &p[2];
            } else if (p[2] == '\0') {
                /* "D:" alone → treat as root "/" */
                static const char root[] = "/";
                *path_ptr = root;
            } else {
                *path_ptr = &p[2];  /* "D:path" → "path" */
            }
            return vol;
        }
    }

    return voldrive_get_default();
}

/* ===========================================================================
 * HANDLERS
 * =========================================================================== */

/**
 * sys_fs_list_dir - List files in a directory
 * arg1 = path string pointer (e.g., "/" or "/BOOT")
 * arg2 = pointer to vol_file_entry_t array (output buffer)
 * arg3 = max entries
 * Returns: count on success, negative on error
 */
static int sys_fs_list_dir(uint32_t path_ptr, uint32_t entries_ptr,
                           uint32_t max_entries, uint32_t arg4, uint32_t arg5) {
    (void)arg4; (void)arg5;

    const char *path = (const char *)path_ptr;
    vol_file_entry_t *entries = (vol_file_entry_t *)entries_ptr;

    if (!path || !entries || max_entries == 0) {
        return SYSCALL_ERR_INVALID;
    }

    int vol = resolve_volume(&path);
    if (vol < 0) return SYSCALL_ERR_IO;

    KLOG_DEBUG("SYSCALL", "fs_list_dir: path=%s vol=%d", path, vol);

    return voldrive_list_dir((uint8_t)vol, path, entries, (int)max_entries);
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

    int vol = resolve_volume(&dir_path);
    if (vol < 0) return SYSCALL_ERR_IO;

    KLOG_DEBUG("SYSCALL", "fs_read_file: dir=%s file=%s", dir_path, filename);

    return voldrive_read_file((uint8_t)vol, dir_path, filename, buffer, max_size);
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

    int vol = resolve_volume(&path);
    if (vol < 0) return SYSCALL_ERR_IO;

    return voldrive_file_count((uint8_t)vol, path);
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

    int vol = resolve_volume(&name);
    if (vol < 0) return SYSCALL_ERR_IO;

    return voldrive_find_dir((uint8_t)vol, name, out_lba, out_size);
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

    /* No path to parse drive from — use arg3 as drive letter if provided */
    int vol;
    if (arg3 && ((char)arg3 >= 'A' && (char)arg3 <= 'Z')) {
        vol = voldrive_find_by_letter((char)arg3);
        if (vol < 0) vol = voldrive_get_default();
    } else {
        vol = voldrive_get_default();
    }
    if (vol < 0) return SYSCALL_ERR_IO;

    return voldrive_get_root_info((uint8_t)vol, out_lba, out_size);
}

/**
 * sys_fs_write_file - Write a file (MFS only)
 * arg1 = directory path
 * arg2 = filename
 * arg3 = data buffer
 * arg4 = size
 * Returns: 0 on success, negative on error
 */
static int sys_fs_write_file(uint32_t dir_path_ptr, uint32_t filename_ptr,
                             uint32_t buf_ptr, uint32_t size, uint32_t arg5) {
    (void)arg5;

    const char *dir_path = (const char *)dir_path_ptr;
    const char *filename = (const char *)filename_ptr;
    const void *data = (const void *)buf_ptr;

    if (!dir_path || !filename) {
        return SYSCALL_ERR_INVALID;
    }
    if (size > 0 && !data) {
        return SYSCALL_ERR_INVALID;
    }

    int vol = resolve_volume(&dir_path);
    if (vol < 0) return SYSCALL_ERR_IO;

    return voldrive_write_file((uint8_t)vol, dir_path, filename, data, size);
}

/**
 * sys_fs_delete_file - Delete a file (MFS only)
 * arg1 = directory path
 * arg2 = filename
 * Returns: 0 on success, negative on error
 */
static int sys_fs_delete_file(uint32_t dir_path_ptr, uint32_t filename_ptr,
                              uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    const char *dir_path = (const char *)dir_path_ptr;
    const char *filename = (const char *)filename_ptr;

    if (!dir_path || !filename) {
        return SYSCALL_ERR_INVALID;
    }

    int vol = resolve_volume(&dir_path);
    if (vol < 0) return SYSCALL_ERR_IO;

    return voldrive_delete_file((uint8_t)vol, dir_path, filename);
}

/**
 * sys_fs_create_dir - Create a directory (MFS only)
 * arg1 = parent directory path
 * arg2 = new directory name
 * Returns: 0 on success, negative on error
 */
static int sys_fs_create_dir(uint32_t parent_path_ptr, uint32_t dirname_ptr,
                             uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    const char *parent_path = (const char *)parent_path_ptr;
    const char *dirname = (const char *)dirname_ptr;

    if (!parent_path || !dirname) {
        return SYSCALL_ERR_INVALID;
    }

    int vol = resolve_volume(&parent_path);
    if (vol < 0) return SYSCALL_ERR_IO;

    return voldrive_create_dir((uint8_t)vol, parent_path, dirname);
}

/**
 * sys_fs_vol_count - Get number of mounted volumes
 * Returns: volume count
 */
static int sys_fs_vol_count(uint32_t arg1, uint32_t arg2, uint32_t arg3,
                            uint32_t arg4, uint32_t arg5) {
    (void)arg1; (void)arg2; (void)arg3; (void)arg4; (void)arg5;
    return voldrive_get_count();
}

/**
 * sys_fs_vol_info - Get volume info
 * arg1 = volume index
 * arg2 = pointer to vol_info_user_t struct
 * Returns: 0 on success, negative on error
 */
static int sys_fs_vol_info(uint32_t vol_index, uint32_t info_ptr,
                           uint32_t arg3, uint32_t arg4, uint32_t arg5) {
    (void)arg3; (void)arg4; (void)arg5;

    vol_info_user_t *info = (vol_info_user_t *)info_ptr;
    if (!info) return SYSCALL_ERR_INVALID;

    /* Get volume struct (opaque pointer, copy relevant fields) */
    /* Volume struct starts with: mounted, fs_type, part_index, drive_letter, label[32] */
    uint8_t *vol_raw = (uint8_t *)voldrive_get_volume((uint8_t)vol_index);
    if (!vol_raw) return SYSCALL_ERR_NOTFOUND;

    info->mounted      = vol_raw[0];
    info->fs_type      = vol_raw[1];
    info->part_index   = vol_raw[2];
    info->drive_letter = (char)vol_raw[3];

    /* Copy label (starts at offset 4 in volume_t) */
    for (int i = 0; i < 31; i++) {
        info->label[i] = (char)vol_raw[4 + i];
        if (!vol_raw[4 + i]) break;
    }
    info->label[31] = '\0';

    /* Get size from the partition */
    info->size_mb = 0;
    {
        partition_info_t *pinfo = partdrive_get_info(info->part_index);
        if (pinfo) {
            info->size_mb = pinfo->size_mb;
        }
    }

    /* FS type string */
    info->fs_str[0] = '\0';
    if (info->fs_type == 1) {       /* VOL_FS_ISO9660 */
        const char *s = "ISO 9660";
        for (int i = 0; s[i] && i < 15; i++) { info->fs_str[i] = s[i]; info->fs_str[i+1] = '\0'; }
    } else if (info->fs_type == 2) { /* VOL_FS_MFS */
        info->fs_str[0] = 'M'; info->fs_str[1] = 'F'; info->fs_str[2] = 'S'; info->fs_str[3] = '\0';
    }

    return SYSCALL_OK;
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
    syscall_register(SYS_FS_WRITE_FILE,    sys_fs_write_file);
    syscall_register(SYS_FS_DELETE_FILE,   sys_fs_delete_file);
    syscall_register(SYS_FS_CREATE_DIR,    sys_fs_create_dir);
    syscall_register(SYS_FS_VOL_COUNT,     sys_fs_vol_count);
    syscall_register(SYS_FS_VOL_INFO,      sys_fs_vol_info);

    KLOG_DEBUG("SYSCALL", "Filesystem handlers registered (128-143)");
}
