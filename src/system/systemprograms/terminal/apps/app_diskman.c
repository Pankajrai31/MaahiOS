/**
 * MaahiOS Disk Manager Console App
 * 
 * Commands:
 *   help            - Show available commands
 *   list            - List all detected disks
 *   info <index>    - Show detailed disk information
 *   status <index>  - Show disk online/offline status
 *   exit            - Return to terminal
 * 
 * Uses: libdisk_list(), libdisk_get_info(), libdisk_get_status(),
 *       libdisk_get_count(), libdisk_get_sector_size()
 *       All go through Disk Executive → kernel Device Manager
 */

#include "../console_app.h"
#include "../../../libraries/libdisk/libdisk.h"

/* Disk type names */
static const char *disk_type_name(uint8_t type) {
    switch (type) {
        case 0:  return "Unknown";
        case 1:  return "HDD";
        case 2:  return "CD-ROM";
        case 3:  return "Floppy";
        default: return "Unknown";
    }
}

/* Disk status names */
static const char *disk_status_name(uint8_t status) {
    switch (status) {
        case 0:  return "OFFLINE";
        case 1:  return "ONLINE";
        case 2:  return "ERROR";
        default: return "UNKNOWN";
    }
}

static void diskman_init(gui_console_t *con) {
    gui_console_print(con, "\n");
    gui_console_print_color(con, "=== Disk Manager ===\n", APP_COLOR_HEADING);
    gui_console_print(con, "Manage storage devices on MaahiOS.\n");
    gui_console_print(con, "Type 'help' for commands, 'exit' to return.\n\n");
}

static void diskman_cmd_list(gui_console_t *con) {
    disk_exec_info_t disks[8];
    int count = libdisk_list(disks, 8);
    
    if (count < 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Could not retrieve disk list.\n");
        return;
    }
    
    if (count == 0) {
        gui_console_print(con, "\nNo disks detected.\n");
        return;
    }
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "  #  Type     Status   Sector   Size     Name\n", APP_COLOR_HEADING);
    gui_console_print_color(con, "  -  ----     ------   ------   ----     ----\n", APP_COLOR_HEADING);
    
    for (int i = 0; i < count; i++) {
        gui_console_print(con, "  ");
        gui_console_print_int(con, disks[i].index);
        gui_console_print(con, "  ");
        
        /* Type (7 chars padded) */
        const char *tn = disk_type_name(disks[i].disk_type);
        gui_console_print(con, tn);
        int pad = 9 - 0;
        for (const char *p = tn; *p; p++) pad--;
        while (pad-- > 0) gui_console_print(con, " ");
        
        /* Status (8 chars padded) */
        const char *sn = disk_status_name(disks[i].status);
        if (disks[i].status == 1) {
            gui_console_print_color(con, sn, APP_COLOR_SUCCESS);
        } else if (disks[i].status == 2) {
            gui_console_print_color(con, sn, APP_COLOR_ERROR);
        } else {
            gui_console_print(con, sn);
        }
        pad = 9;
        for (const char *p = sn; *p; p++) pad--;
        while (pad-- > 0) gui_console_print(con, " ");
        
        /* Sector size */
        gui_console_print_int(con, (int)disks[i].sector_size);
        gui_console_print(con, "    ");
        
        /* Size in MB */
        gui_console_print_int(con, (int)disks[i].size_mb);
        gui_console_print(con, "MB");
        if (disks[i].size_mb < 100) gui_console_print(con, "  ");
        else if (disks[i].size_mb < 1000) gui_console_print(con, " ");
        gui_console_print(con, "   ");
        
        /* Name */
        gui_console_print(con, disks[i].name);
        gui_console_print(con, "\n");
    }
    
    gui_console_print(con, "\n  Total: ");
    gui_console_print_int(con, count);
    gui_console_print(con, " disk(s)\n");
}

static void diskman_cmd_info(gui_console_t *con, const char *arg) {
    arg = app_skip_spaces(arg);
    if (*arg == '\0') {
        gui_console_print(con, "\nUsage: info <disk_index>\n");
        return;
    }
    
    int idx = app_str_to_int(arg);
    if (idx < 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Invalid disk index.\n");
        return;
    }
    
    disk_exec_info_t info;
    int result = libdisk_get_info((uint8_t)idx, &info);
    
    if (result != 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Disk not found (index ");
        gui_console_print_int(con, idx);
        gui_console_print(con, ")\n");
        return;
    }
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "Disk Information:\n", APP_COLOR_HEADING);
    gui_console_print(con, "  Index:       ");
    gui_console_print_int(con, info.index);
    gui_console_print(con, "\n  Name:        ");
    gui_console_print(con, info.name);
    gui_console_print(con, "\n  Type:        ");
    gui_console_print(con, disk_type_name(info.disk_type));
    gui_console_print(con, "\n  Status:      ");
    gui_console_print(con, disk_status_name(info.status));
    gui_console_print(con, "\n  Sector Size: ");
    gui_console_print_int(con, (int)info.sector_size);
    gui_console_print(con, " bytes");
    gui_console_print(con, "\n  Size:        ");
    gui_console_print_int(con, (int)info.size_mb);
    gui_console_print(con, " MB\n");
}

static void diskman_cmd_status(gui_console_t *con, const char *arg) {
    arg = app_skip_spaces(arg);
    if (*arg == '\0') {
        gui_console_print(con, "\nUsage: status <disk_index>\n");
        return;
    }
    
    int idx = app_str_to_int(arg);
    int status = libdisk_get_status((uint8_t)idx);
    
    if (status < 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Could not get status for disk ");
        gui_console_print_int(con, idx);
        gui_console_print(con, "\n");
        return;
    }
    
    gui_console_print(con, "\nDisk ");
    gui_console_print_int(con, idx);
    gui_console_print(con, " status: ");
    
    if (status == 1) {
        gui_console_print_color(con, "ONLINE\n", APP_COLOR_SUCCESS);
    } else if (status == 2) {
        gui_console_print_color(con, "ERROR\n", APP_COLOR_ERROR);
    } else {
        gui_console_print_color(con, "OFFLINE\n", APP_COLOR_WARN);
    }
}

static void diskman_handle(gui_console_t *con, const char *cmd) {
    if (app_str_equal(cmd, "help")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "Disk Manager Commands:\n", APP_COLOR_HEADING);
        gui_console_print(con, "  list            - List all detected disks\n");
        gui_console_print(con, "  info <index>    - Show detailed disk info\n");
        gui_console_print(con, "  status <index>  - Show disk online/offline status\n");
        gui_console_print(con, "  exit            - Return to terminal\n");
    } else if (app_str_equal(cmd, "list")) {
        diskman_cmd_list(con);
    } else if (app_str_starts_with(cmd, "info ")) {
        diskman_cmd_info(con, cmd + 5);
    } else if (app_str_starts_with(cmd, "status ")) {
        diskman_cmd_status(con, cmd + 7);
    } else {
        gui_console_print(con, "\nUnknown command. Type 'help' for available commands.\n");
    }
}

static void diskman_cleanup(gui_console_t *con) {
    (void)con;
}

console_app_t app_diskman = {
    .name        = "diskman",
    .description = "Disk Manager - list and inspect storage devices",
    .init        = diskman_init,
    .handle_command = diskman_handle,
    .cleanup     = diskman_cleanup,
};
