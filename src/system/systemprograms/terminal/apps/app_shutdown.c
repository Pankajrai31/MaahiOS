/**
 * MaahiOS Shutdown Console App
 * 
 * Commands:
 *   help      - Show available commands
 *   shutdown  - Power off the system (requires confirmation)
 *   yes       - Confirm pending shutdown
 *   no        - Cancel pending shutdown
 *   exit      - Return to terminal
 * 
 * Uses: libprocess_shutdown() → Process Executive → SYS_SHUTDOWN
 */

#include "../console_app.h"
#include "../../../libraries/libprocess/libprocess.h"

static int g_shutdown_pending = 0;

static void shutdown_init(gui_console_t *con) {
    g_shutdown_pending = 0;
    gui_console_print(con, "\n");
    gui_console_print_color(con, "=== System Shutdown ===\n", APP_COLOR_HEADING);
    gui_console_print(con, "Type 'shutdown' to power off the system.\n");
    gui_console_print(con, "Type 'help' for commands, 'exit' to return.\n\n");
}

static void shutdown_handle(gui_console_t *con, const char *cmd) {
    if (app_str_equal(cmd, "help")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "Shutdown Commands:\n", APP_COLOR_HEADING);
        gui_console_print(con, "  shutdown  - Initiate system power off\n");
        gui_console_print(con, "  yes       - Confirm pending shutdown\n");
        gui_console_print(con, "  no        - Cancel pending shutdown\n");
        gui_console_print(con, "  exit      - Return to terminal\n");
    } else if (app_str_equal(cmd, "shutdown")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "WARNING: ", APP_COLOR_WARN);
        gui_console_print(con, "This will power off the system.\n");
        gui_console_print(con, "All unsaved work will be lost.\n");
        gui_console_print_color(con, "Are you sure? (yes/no): ", APP_COLOR_WARN);
        g_shutdown_pending = 1;
    } else if (app_str_equal(cmd, "yes") && g_shutdown_pending) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "Shutting down...\n", APP_COLOR_SUCCESS);
        libprocess_system_shutdown();
        /* Never returns */
    } else if (app_str_equal(cmd, "no") && g_shutdown_pending) {
        g_shutdown_pending = 0;
        gui_console_print(con, "\nShutdown cancelled.\n");
    } else {
        if (g_shutdown_pending) {
            gui_console_print(con, "\nPlease type 'yes' to confirm or 'no' to cancel.\n");
        } else {
            gui_console_print(con, "\nUnknown command. Type 'help' for available commands.\n");
        }
    }
}

static void shutdown_cleanup(gui_console_t *con) {
    (void)con;
    g_shutdown_pending = 0;
}

console_app_t app_shutdown = {
    .name        = "shutdown",
    .description = "Power off the system",
    .init        = shutdown_init,
    .handle_command = shutdown_handle,
    .cleanup     = shutdown_cleanup,
};
