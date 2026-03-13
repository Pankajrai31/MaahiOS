/**
 * diskman.mex - MaahiOS Disk Manager
 *
 * Console non-interactive .mex application.
 * Reads command args from terminal, prints output to stdout SHM, exits.
 *
 * Usage:
 *   diskman help                 Show all commands with examples
 *   diskman list                 List all detected disks
 *   diskman info <index>         Show detailed disk information
 *   diskman status <index>       Show disk online/offline status
 *   diskman vol                  List mounted volumes with drive letters
 *   diskman format <idx> confirm Format disk with MBR + MFS
 *
 * Uses: libdisk, libconsole, libcell
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libdisk/libdisk.h"
#include "../../system/libraries/libcell/libcell.h"
#include "../../system/libraries/libfs/libfs.h"

/*=============================================================================
 * STRING HELPERS
 *===========================================================================*/

static int str_equal(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return (*a == *b);
}

static int str_equal_nocase(const char *a, const char *b) {
    while (*a && *b) {
        char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
        char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
        if (ca != cb) return 0;
        a++; b++;
    }
    char ca = (*a >= 'a' && *a <= 'z') ? *a - 32 : *a;
    char cb = (*b >= 'a' && *b <= 'z') ? *b - 32 : *b;
    return (ca == cb);
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) { if (*str != *prefix) return 0; str++; prefix++; }
    return 1;
}

static int str_to_int(const char *s) {
    while (*s == ' ') s++;
    int result = 0;
    while (*s >= '0' && *s <= '9') { result = result * 10 + (*s - '0'); s++; }
    return result;
}

static const char *skip_spaces(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/*=============================================================================
 * LOOKUP HELPERS
 *===========================================================================*/

static const char *disk_type_name(uint8_t type) {
    switch (type) {
        case 0:  return "Unknown";
        case 1:  return "HDD";
        case 2:  return "CD-ROM";
        case 3:  return "Floppy";
        default: return "Unknown";
    }
}

static const char *disk_status_name(uint8_t status) {
    switch (status) {
        case 0:  return "OFFLINE";
        case 1:  return "ONLINE";
        case 2:  return "ERROR";
        default: return "UNKNOWN";
    }
}

/* disk_exec_info_t from libdisk.h is used for disk operations */

/*=============================================================================
 * COMMANDS
 *===========================================================================*/

static void cmd_help(void) {
    console_print("Disk Manager - Usage:\n\n");
    console_print("  diskman list                 List all detected disks\n");
    console_print("  diskman info <index>         Show detailed disk information\n");
    console_print("  diskman status <index>       Show disk status\n");
    console_print("  diskman vol                  List mounted volumes\n");
    console_print("  diskman format <idx> confirm Format disk (MBR + MFS)\n");
    console_print("  diskman help                 Show this help\n");
    console_print("\nExamples:\n");
    console_print("  C:\\> diskman list\n");
    console_print("  C:\\> diskman info 0\n");
    console_print("  C:\\> diskman vol\n");
    console_print("  C:\\> diskman format 1 confirm\n");
    console_print("\n  WARNING: format erases ALL data on the target disk!\n");
}

static void cmd_list(void) {
    disk_exec_info_t disks[8];
    int count = libdisk_list(disks, 8);

    if (count < 0) {
        console_print("Error: Could not retrieve disk list.\n");
        return;
    }
    if (count == 0) {
        console_print("No disks detected.\n");
        return;
    }

    console_print("  #  Type     Status   Size       Name\n");
    console_print("  -  ----     ------   ----       ----\n");

    for (int i = 0; i < count; i++) {
        console_print("  ");
        console_print_int(disks[i].index);
        console_print("  ");

        const char *tn = disk_type_name(disks[i].disk_type);
        console_print(tn);
        int pad = 9;
        for (const char *p = tn; *p; p++) pad--;
        while (pad-- > 0) console_putchar(' ');

        const char *sn = disk_status_name(disks[i].status);
        console_print(sn);
        pad = 9;
        for (const char *p = sn; *p; p++) pad--;
        while (pad-- > 0) console_putchar(' ');

        console_print_int((int)disks[i].size_mb);
        console_print(" MB");
        if (disks[i].size_mb < 100) console_print("    ");
        else if (disks[i].size_mb < 1000) console_print("   ");
        else console_print("  ");

        console_print(disks[i].name);
        console_putchar('\n');
    }

    console_print("\n  Total: ");
    console_print_int(count);
    console_print(" disk(s)\n");
}

static void cmd_info(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print("Usage: diskman info <disk_index>\n");
        console_print("Example: diskman info 0\n");
        return;
    }

    int idx = str_to_int(arg);
    disk_exec_info_t info;
    int result = libdisk_get_info((uint8_t)idx, &info);

    if (result != 0) {
        console_print("Error: Disk not found (index ");
        console_print_int(idx);
        console_print(")\n");
        return;
    }

    console_print("Disk Information:\n");
    console_print("  Index:       ");
    console_print_int(info.index);
    console_print("\n  Name:        ");
    console_print(info.name);
    console_print("\n  Type:        ");
    console_print(disk_type_name(info.disk_type));
    console_print("\n  Status:      ");
    console_print(disk_status_name(info.status));
    console_print("\n  Sector Size: ");
    console_print_int((int)info.sector_size);
    console_print(" bytes");
    console_print("\n  Size:        ");
    console_print_int((int)info.size_mb);
    console_print(" MB\n");
}

static void cmd_status(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print("Usage: diskman status <disk_index>\n");
        console_print("Example: diskman status 0\n");
        return;
    }

    int idx = str_to_int(arg);
    int status = libdisk_get_status((uint8_t)idx);

    if (status < 0) {
        console_print("Error: Could not get status for disk ");
        console_print_int(idx);
        console_putchar('\n');
        return;
    }

    console_print("Disk ");
    console_print_int(idx);
    console_print(" status: ");
    console_print(disk_status_name((uint8_t)status));
    console_putchar('\n');
}

static void cmd_vol(void) {
    console_print("Mounted Volumes:\n\n");
    console_print("  Drive  Filesystem   Size         Label\n");
    console_print("  -----  ----------   ----         -----\n");

    int vol_count = libfs_vol_count();

    if (vol_count <= 0) {
        console_print("  (no volumes mounted)\n");
        return;
    }

    for (int i = 0; i < vol_count; i++) {
        libfs_vol_info_t vinfo;
        for (int j = 0; j < (int)sizeof(vinfo); j++) ((uint8_t *)&vinfo)[j] = 0;

        int result = libfs_vol_info(i, &vinfo);
        if (result < 0 || !vinfo.mounted) continue;

        console_print("  ");
        console_putchar(vinfo.drive_letter);
        console_print(":     ");

        /* Filesystem type */
        if (vinfo.fs_str[0]) {
            console_print(vinfo.fs_str);
            int flen = 0;
            for (const char *p = vinfo.fs_str; *p; p++) flen++;
            for (int j = flen; j < 13; j++) console_putchar(' ');
        } else {
            console_print("Unknown      ");
        }

        /* Size */
        if (vinfo.size_mb >= 1024) {
            console_print_int((int)(vinfo.size_mb / 1024));
            console_putchar('.');
            console_print_int((int)((vinfo.size_mb % 1024) * 10 / 1024));
            console_print(" GB");
        } else {
            console_print_int((int)vinfo.size_mb);
            console_print(" MB");
        }
        console_print("     ");

        if (vinfo.label[0])
            console_print(vinfo.label);

        console_putchar('\n');
    }

    console_print("\n  Total: ");
    console_print_int(vol_count);
    console_print(" volume(s)\n");
}

static void cmd_format(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print("Usage: diskman format <disk_index> confirm\n");
        console_print("\n  WARNING: This will erase ALL data on the disk!\n");
        console_print("  You must include 'confirm' to proceed.\n");
        console_print("\nExample:\n");
        console_print("  C:\\> diskman format 1 confirm\n");
        return;
    }

    /* Parse disk index */
    int idx = str_to_int(arg);

    /* Advance past the number to find 'confirm' */
    while (*arg >= '0' && *arg <= '9') arg++;
    arg = skip_spaces(arg);

    if (!str_equal_nocase(arg, "confirm")) {
        console_print("  WARNING: Format will erase ALL data on disk ");
        console_print_int(idx);
        console_print("!\n");
        console_print("  All partitions, files, and data will be lost.\n\n");
        console_print("  To proceed, type:\n");
        console_print("  C:\\> diskman format ");
        console_print_int(idx);
        console_print(" confirm\n");
        return;
    }

    /* Verify disk exists and is HDD */
    disk_exec_info_t info;
    int result = libdisk_get_info((uint8_t)idx, &info);
    if (result != 0) {
        console_print("Error: Disk not found (index ");
        console_print_int(idx);
        console_print(")\n");
        return;
    }
    if (info.disk_type == 2) {  /* CD-ROM */
        console_print("Error: Cannot format a CD-ROM drive.\n");
        return;
    }

    console_print("Formatting disk ");
    console_print_int(idx);
    console_print(" (");
    console_print(info.name);
    console_print(", ");
    console_print_int((int)info.size_mb);
    console_print(" MB)...\n");
    console_print("  Creating MBR partition table...\n");
    console_print("  Writing MFS filesystem...\n");

    result = libdisk_format((uint8_t)idx, "MaahiOS");

    if (result == 0) {
        console_print("  Assigning drive letter...\n");
        console_print("\nFormat complete! Disk ");
        console_print_int(idx);
        console_print(" is now ready.\n");
    } else {
        console_print("\nFormat FAILED (error ");
        console_print_int(result);
        console_print(")\n");
        if (result == -2) {
            console_print("  Disk is not a hard drive.\n");
        } else if (result == -1) {
            console_print("  Disk index out of range.\n");
        } else if (result == -3) {
            console_print("  MBR partition creation failed.\n");
        } else if (result == -4) {
            console_print("  MFS filesystem format failed.\n");
        } else if (result == -5) {
            console_print("  No drive letters available.\n");
        }
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();
    liblog(LOG_INFO, "DISKMAN", "Disk Manager starting (non-interactive)");

    /* Initialize console output (stdout SHM pipe) */
    if (console_init() != 0) {
        liblog(LOG_ERROR, "DISKMAN", "Failed to init console output");
        return;
    }

    /* Read command arguments from terminal */
    char args[256];
    args[0] = '\0';
    console_get_args(args, 256);

    /* No arguments: show usage hint */
    if (args[0] == '\0') {
        console_print("Usage: diskman <command>\n");
        console_print("Type 'diskman help' for available commands.\n");
        return;
    }

    /* Parse command (first word of args) */
    char cmd[32];
    const char *rest = "";
    int ci = 0;
    int i = 0;
    while (args[i] && args[i] != ' ' && ci < 31) cmd[ci++] = args[i++];
    cmd[ci] = '\0';
    while (args[i] == ' ') i++;
    if (args[i]) rest = &args[i];

    /* Dispatch */
    if (str_equal_nocase(cmd, "help")) {
        cmd_help();
    } else if (str_equal_nocase(cmd, "list")) {
        cmd_list();
    } else if (str_equal_nocase(cmd, "info")) {
        cmd_info(rest);
    } else if (str_equal_nocase(cmd, "status")) {
        cmd_status(rest);
    } else if (str_equal_nocase(cmd, "vol")) {
        cmd_vol();
    } else if (str_equal_nocase(cmd, "format")) {
        cmd_format(rest);
    } else {
        console_print("Unknown command: ");
        console_print(cmd);
        console_print("\nType 'diskman help' for available commands.\n");
    }

    liblog(LOG_INFO, "DISKMAN", "Disk Manager exiting");
}
