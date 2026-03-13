/**
 * sysinfo.mex - MaahiOS System Information
 *
 * Console non-interactive .mex application.
 * Displays system information and exits.
 *
 * Usage:
 *   sysinfo             Show system information
 *   sysinfo help        Show help
 *
 * Uses: libconsole, libprocess, libcell
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libprocess/libprocess.h"
#include "../../system/libraries/libcell/libcell.h"
#include "../../system/libraries/core/syscall_helpers.h"

/*=============================================================================
 * STRING HELPERS
 *===========================================================================*/

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

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();

    if (console_init() != 0) {
        liblog(LOG_ERROR, "SYSINFO", "Failed to init console output");
        return;
    }

    char args[64];
    args[0] = '\0';
    console_get_args(args, 64);

    if (str_equal_nocase(args, "help")) {
        console_print("System Information - Usage:\n\n");
        console_print("  sysinfo        Show system information\n");
        console_print("  sysinfo help   Show this help\n");
        console_print("\nExample:\n");
        console_print("  C:\\> sysinfo\n");
        return;
    }

    console_print("MaahiOS System Information\n\n");
    console_print("  OS:          MaahiOS v0.2.0\n");
    console_print("  Arch:        x86 (i686) 32-bit\n");

    /* Query display dimensions from cell registry */
    uint32_t disp_w = 0, disp_h = 0;
    libcell_read("system.gui.width",  &disp_w, sizeof(uint32_t));
    libcell_read("system.gui.height", &disp_h, sizeof(uint32_t));
    console_print("  Display:     ");
    console_print_int((int)disp_w);
    console_putchar('x');
    console_print_int((int)disp_h);
    console_print(" 32bpp\n");

    int count = libprocess_get_count();
    console_print("  Processes:   ");
    console_print_int(count);
    console_print(" running\n");

    int pid = syscall0(SYS_GETPID);
    console_print("  My PID:      ");
    console_print_int(pid);
    console_putchar('\n');

    /* Read disk count */
    int disk_count = 0;
    libcell_read("device.disk.count", &disk_count, sizeof(int));
    console_print("  Volumes:     ");
    console_print_int(disk_count);
    console_print(" mounted\n");
}
