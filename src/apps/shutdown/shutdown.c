/**
 * shutdown.mex - MaahiOS Shutdown/Restart
 *
 * Console non-interactive .mex application.
 * Powers off or restarts the system.
 *
 * Usage:
 *   shutdown now        Power off the system
 *   shutdown restart    Restart the system
 *   shutdown help       Show help
 *
 * Uses: libconsole, syscall_helpers
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libprocess/libprocess.h"
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
        liblog(LOG_ERROR, "SHUTDOWN", "Failed to init console output");
        return;
    }

    char args[64];
    args[0] = '\0';
    console_get_args(args, 64);

    if (args[0] == '\0') {
        console_print("Usage: shutdown <command>\n");
        console_print("Type 'shutdown help' for available commands.\n");
        return;
    }

    if (str_equal_nocase(args, "help")) {
        console_print("Shutdown - Usage:\n\n");
        console_print("  shutdown now        Power off the system\n");
        console_print("  shutdown restart    Restart the system\n");
        console_print("  shutdown reboot     Same as restart\n");
        console_print("  shutdown help       Show this help\n");
        console_print("\nExamples:\n");
        console_print("  C:\\> shutdown now\n");
        console_print("  C:\\> shutdown restart\n");
        return;
    }

    if (str_equal_nocase(args, "now")) {
        console_print("Shutting down...\n");
        /* Small delay so the output can be read */
        syscall1(SYS_SLEEP, 50);
        libprocess_system_shutdown();
        return;
    }

    if (str_equal_nocase(args, "restart") || str_equal_nocase(args, "reboot")) {
        console_print("Restarting...\n");
        syscall1(SYS_SLEEP, 50);
        libprocess_system_restart();
        return;
    }

    console_print("Unknown command: ");
    console_print(args);
    console_print("\nType 'shutdown help' for available commands.\n");
}
