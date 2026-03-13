/**
 * MaahiOS Volume Driver (Layer 5)
 * 
 * Top of the storage stack. Manages mounted volumes:
 *   - Auto-mounts partitions found by partdrive
 *   - For ISO9660 partitions: initializes iso9660 driver, routes FS ops
 *   - For MFS partitions: mounts MFS, routes FS ops
 *   - Provides unified filesystem API for syscall handlers
 */

#include "voldrive.h"
#include "../partition/partdrive.h"
#include "../disk/disk.h"
#include "../ata/ata.h"
#include "../iso9660/iso9660.h"
#include "../../../managers/klog/klog.h"

/* ============================================
 * Volume registry
 * ============================================ */
static volume_t g_volumes[MAX_VOLUMES] = {0};
static int g_volume_count = 0;

/* ============================================
 * Helpers
 * ============================================ */

static void vol_strcpy(char *dst, const char *src, int max) {
    int i = 0;
    while (src[i] && i < max - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = '\0';
}

/* ============================================
 * Internal: mount a partition as a volume
 * ============================================ */

static int mount_iso9660(uint8_t part_index, char drive_letter) {
    if (g_volume_count >= MAX_VOLUMES) return -1;

    /* Initialize ISO9660 driver (finds ATAPI, reads PVD) */
    if (iso9660_init() != 0) {
        KLOG_WARN("VOL", "ISO9660 init failed on partition %d", part_index);
        return -2;
    }

    volume_t *vol = &g_volumes[g_volume_count];
    vol->mounted      = 1;
    vol->fs_type      = VOL_FS_ISO9660;
    vol->part_index   = part_index;
    vol->drive_letter = drive_letter;
    vol_strcpy(vol->label, "MaahiOS_ISO", sizeof(vol->label));

    KLOG_INFO("VOL", "Mounted ISO9660 volume as %c: (partition %d)",
              drive_letter, part_index);

    g_volume_count++;
    return 0;
}

static int mount_mfs(uint8_t part_index, char drive_letter) {
    if (g_volume_count >= MAX_VOLUMES) return -1;

    volume_t *vol = &g_volumes[g_volume_count];
    vol->part_index   = part_index;
    vol->drive_letter = drive_letter;

    /* Try to mount MFS */
    if (mfs_mount(&vol->mfs_ctx, part_index) != 0) {
        KLOG_WARN("VOL", "MFS mount failed on partition %d", part_index);
        return -2;
    }

    vol->mounted = 1;
    vol->fs_type = VOL_FS_MFS;
    vol_strcpy(vol->label, vol->mfs_ctx.sb.volume_label, sizeof(vol->label));

    KLOG_INFO("VOL", "Mounted MFS volume as %c: (partition %d, '%s')",
              drive_letter, part_index, vol->label);

    g_volume_count++;
    return 0;
}

/* ============================================
 * Public API
 * ============================================ */

int voldrive_init(void) {
    KLOG_INFO("VOL", "Initializing volume driver");

    g_volume_count = 0;
    char drive_letter = 'C';  /* Start from C: */

    int part_count = partdrive_get_count();

    for (uint8_t i = 0; i < (uint8_t)part_count; i++) {
        partition_info_t *pinfo = partdrive_get_info(i);
        if (!pinfo || !pinfo->active) continue;

        int result = -1;

        if (pinfo->type == PART_TYPE_ISO9660) {
            result = mount_iso9660(i, drive_letter);
        } else if (pinfo->type == PART_TYPE_MFS) {
            result = mount_mfs(i, drive_letter);
        } else {
            KLOG_INFO("VOL", "Skipping partition %d (type 0x%02X)", i, pinfo->type);
            continue;
        }

        if (result == 0) {
            drive_letter++;
        }
    }

    KLOG_INFO("VOL", "Volume driver initialized (%d volumes mounted)", g_volume_count);
    return 0;
}

int voldrive_get_count(void) {
    return g_volume_count;
}

volume_t *voldrive_get_volume(uint8_t index) {
    if (index >= g_volume_count) return 0;
    if (!g_volumes[index].mounted) return 0;
    return &g_volumes[index];
}

int voldrive_find_by_letter(char letter) {
    /* Case-insensitive */
    char upper = (letter >= 'a' && letter <= 'z') ? letter - 32 : letter;
    for (int i = 0; i < g_volume_count; i++) {
        if (g_volumes[i].mounted && g_volumes[i].drive_letter == upper) {
            return i;
        }
    }
    return -1;
}

int voldrive_get_default(void) {
    if (g_volume_count > 0 && g_volumes[0].mounted) return 0;
    return -1;
}

/* ============================================
 * Unified FS operations
 * ============================================ */

int voldrive_list_dir(uint8_t vol_index, const char *path,
                      vol_file_entry_t *entries, int max) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted || !path || !entries) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        /* Root directory */
        if (path[0] == '/' && path[1] == '\0') {
            return iso9660_list_root((void *)entries, max);
        }

        /* Subdirectory */
        const char *dirname = path;
        if (dirname[0] == '/') dirname++;

        uint32_t dir_lba, dir_size;
        if (iso9660_find_directory(dirname, &dir_lba, &dir_size) != 0) {
            return -2;
        }
        return iso9660_list_directory(dir_lba, dir_size, (void *)entries, max);
    }
    else if (vol->fs_type == VOL_FS_MFS) {
        /* For MFS, resolve path to MFT index */
        uint16_t dir_entry = MFS_ROOT_ENTRY;

        if (!(path[0] == '/' && path[1] == '\0')) {
            /* Non-root: find the directory */
            const char *dirname = path;
            if (dirname[0] == '/') dirname++;
            int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, dirname);
            if (found < 0) return -2;
            dir_entry = (uint16_t)found;
        }

        /* MFS returns mfs_file_entry_t which has same layout as vol_file_entry_t */
        return mfs_list_dir(&vol->mfs_ctx, dir_entry, (mfs_file_entry_t *)entries, max);
    }

    return -1;
}

int voldrive_read_file(uint8_t vol_index, const char *dir_path,
                       const char *filename, void *buffer, uint32_t max_size) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted || !dir_path || !filename || !buffer) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        uint32_t dir_lba, dir_size;

        if (dir_path[0] == '/' && dir_path[1] == '\0') {
            dir_lba = iso9660_get_root_lba();
            dir_size = iso9660_get_root_size();
        } else {
            const char *dirname = dir_path;
            if (dirname[0] == '/') dirname++;
            if (iso9660_find_directory(dirname, &dir_lba, &dir_size) != 0) {
                return -2;
            }
        }

        return iso9660_find_and_read_file(dir_lba, dir_size, filename, buffer, max_size);
    }
    else if (vol->fs_type == VOL_FS_MFS) {
        /* Resolve dir path */
        uint16_t dir_entry = MFS_ROOT_ENTRY;
        if (!(dir_path[0] == '/' && dir_path[1] == '\0')) {
            const char *dirname = dir_path;
            if (dirname[0] == '/') dirname++;
            int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, dirname);
            if (found < 0) return -2;
            dir_entry = (uint16_t)found;
        }

        /* Find file in directory */
        int file_idx = mfs_find(&vol->mfs_ctx, dir_entry, filename);
        if (file_idx < 0) return -3;

        return mfs_read_file(&vol->mfs_ctx, (uint16_t)file_idx, buffer, max_size);
    }

    return -1;
}

int voldrive_file_count(uint8_t vol_index, const char *path) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted || !path) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        if (path[0] == '/' && path[1] == '\0') {
            return iso9660_get_file_count();
        }
        /* Subdir: list and count */
        const char *dirname = path;
        if (dirname[0] == '/') dirname++;

        uint32_t dir_lba, dir_size;
        if (iso9660_find_directory(dirname, &dir_lba, &dir_size) != 0) {
            return -2;
        }
        vol_file_entry_t tmp[32];
        return iso9660_list_directory(dir_lba, dir_size, (void *)tmp, 32);
    }
    else if (vol->fs_type == VOL_FS_MFS) {
        uint16_t dir_entry = MFS_ROOT_ENTRY;
        if (!(path[0] == '/' && path[1] == '\0')) {
            const char *dirname = path;
            if (dirname[0] == '/') dirname++;
            int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, dirname);
            if (found < 0) return -2;
            dir_entry = (uint16_t)found;
        }
        return mfs_file_count(&vol->mfs_ctx, dir_entry);
    }

    return -1;
}

int voldrive_find_dir(uint8_t vol_index, const char *name,
                      uint32_t *out_lba, uint32_t *out_size) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted || !name || !out_lba || !out_size) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        return iso9660_find_directory(name, out_lba, out_size);
    }
    else if (vol->fs_type == VOL_FS_MFS) {
        int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, name);
        if (found < 0) return -1;
        *out_lba = (uint32_t)found;  /* MFT index */
        *out_size = 0;               /* Not applicable for MFS */
        return 0;
    }

    return -1;
}

int voldrive_get_root_info(uint8_t vol_index, uint32_t *out_lba, uint32_t *out_size) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted || !out_lba || !out_size) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        *out_lba = iso9660_get_root_lba();
        *out_size = iso9660_get_root_size();
        return (*out_lba != 0) ? 0 : -1;
    }
    else if (vol->fs_type == VOL_FS_MFS) {
        *out_lba = MFS_ROOT_ENTRY;
        *out_size = 0;
        return 0;
    }

    return -1;
}

int voldrive_write_file(uint8_t vol_index, const char *dir_path,
                        const char *filename, const void *data, uint32_t size) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        KLOG_WARN("VOL", "ISO9660 is read-only");
        return -2;  /* ISO9660 = read-only */
    }

    if (vol->fs_type == VOL_FS_MFS) {
        uint16_t dir_entry = MFS_ROOT_ENTRY;
        if (dir_path && !(dir_path[0] == '/' && dir_path[1] == '\0')) {
            const char *dirname = dir_path;
            if (dirname[0] == '/') dirname++;
            int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, dirname);
            if (found < 0) return -3;
            dir_entry = (uint16_t)found;
        }
        return mfs_write_file(&vol->mfs_ctx, dir_entry, filename, data, size);
    }

    return -1;
}

int voldrive_delete_file(uint8_t vol_index, const char *dir_path,
                         const char *filename) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        return -2;  /* Read-only */
    }

    if (vol->fs_type == VOL_FS_MFS) {
        uint16_t dir_entry = MFS_ROOT_ENTRY;
        if (dir_path && !(dir_path[0] == '/' && dir_path[1] == '\0')) {
            const char *dirname = dir_path;
            if (dirname[0] == '/') dirname++;
            int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, dirname);
            if (found < 0) return -3;
            dir_entry = (uint16_t)found;
        }

        int file_idx = mfs_find(&vol->mfs_ctx, dir_entry, filename);
        if (file_idx < 0) return -4;

        return mfs_delete(&vol->mfs_ctx, (uint16_t)file_idx);
    }

    return -1;
}

int voldrive_create_dir(uint8_t vol_index, const char *parent_path,
                        const char *dirname) {
    if (vol_index >= g_volume_count) return -1;
    volume_t *vol = &g_volumes[vol_index];
    if (!vol->mounted) return -1;

    if (vol->fs_type == VOL_FS_ISO9660) {
        return -2;  /* Read-only */
    }

    if (vol->fs_type == VOL_FS_MFS) {
        uint16_t parent_entry = MFS_ROOT_ENTRY;
        if (parent_path && !(parent_path[0] == '/' && parent_path[1] == '\0')) {
            const char *pname = parent_path;
            if (pname[0] == '/') pname++;
            int found = mfs_find(&vol->mfs_ctx, MFS_ROOT_ENTRY, pname);
            if (found < 0) return -3;
            parent_entry = (uint16_t)found;
        }

        return mfs_create_dir(&vol->mfs_ctx, parent_entry, dirname);
    }

    return -1;
}

int voldrive_format_disk(uint8_t disk_index, const char *label) {
    KLOG_INFO("VOL", "Format disk %d requested (label='%s')", disk_index, label ? label : "");

    /* 1. Validate disk exists and is an HDD */
    disk_info_t *dinfo = disk_get_info(disk_index);
    if (!dinfo || !dinfo->active) {
        KLOG_ERROR("VOL", "Format: disk %d not found", disk_index);
        return -1;
    }
    if (dinfo->disk_type == DISK_TYPE_CDROM) {
        KLOG_ERROR("VOL", "Format: cannot format CD-ROM");
        return -2;
    }

    /* 2. Get total sectors from ATA driver */
    ata_drive_t *ata = ata_get_drive(dinfo->ata_drive_id);
    if (!ata || ata->total_sectors == 0) {
        KLOG_ERROR("VOL", "Format: cannot get disk geometry for ATA drive %d", dinfo->ata_drive_id);
        return -1;
    }

    uint32_t total_sectors = ata->total_sectors;
    KLOG_INFO("VOL", "Format: disk %d has %u sectors (%u MB)",
              disk_index, total_sectors, total_sectors / 2048);

    /* 3. Unmount any existing volume from this disk */
    for (int i = 0; i < g_volume_count; i++) {
        if (g_volumes[i].mounted) {
            partition_info_t *pinfo = partdrive_get_info(g_volumes[i].part_index);
            if (pinfo && pinfo->disk_index == disk_index) {
                if (g_volumes[i].fs_type == VOL_FS_MFS) {
                    mfs_unmount(&g_volumes[i].mfs_ctx);
                }
                g_volumes[i].mounted = 0;
                KLOG_INFO("VOL", "Format: unmounted existing volume %c:", g_volumes[i].drive_letter);
            }
        }
    }

    /* 4. Create MBR with one MFS partition spanning the whole disk */
    int part_idx = partdrive_create_mbr(disk_index, total_sectors, PART_TYPE_MFS);
    if (part_idx < 0) {
        KLOG_ERROR("VOL", "Format: MBR creation failed (err=%d)", part_idx);
        return -3;
    }

    KLOG_INFO("VOL", "Format: MBR created, partition index=%d", part_idx);

    /* 5. Format MFS on the new partition */
    const char *vol_label = (label && label[0]) ? label : "MaahiOS";
    int ret = mfs_format((uint8_t)part_idx, vol_label);
    if (ret != 0) {
        KLOG_ERROR("VOL", "Format: MFS format failed (err=%d)", ret);
        return -4;
    }

    KLOG_INFO("VOL", "Format: MFS formatted successfully on partition %d", part_idx);

    /* 6. Mount the newly formatted volume */
    /* Find next available drive letter (drive letters are volume-only) */
    char drive_letter = 0;
    for (char c = 'C'; c <= 'Z'; c++) {
        if (voldrive_find_by_letter(c) < 0) {
            drive_letter = c;
            break;
        }
    }
    if (drive_letter == 0) {
        KLOG_ERROR("VOL", "Format: no drive letters available");
        return -5;
    }

    ret = mount_mfs((uint8_t)part_idx, drive_letter);
    if (ret != 0) {
        KLOG_ERROR("VOL", "Format: volume mount failed (err=%d)", ret);
        return -5;
    }

    /* 7. Update disk info to reflect MFS filesystem */
    dinfo->fs_type = FS_TYPE_MFS;
    {
        /* Copy "MFS" to fs_str */
        dinfo->fs_str[0] = 'M'; dinfo->fs_str[1] = 'F'; dinfo->fs_str[2] = 'S';
        dinfo->fs_str[3] = '\0';
    }

    /* Re-publish disk info to cells */
    {
        char key[32] = "device.disk.0";
        key[12] = '0' + (char)disk_index;
        extern void kernel_cell_write(const char *key, const void *data, uint32_t size);
        kernel_cell_write(key, dinfo, sizeof(disk_info_t));
    }

    KLOG_INFO("VOL", "Format complete: disk %d → %c: (MFS, '%s')",
              disk_index, drive_letter, vol_label);

    return 0;
}
