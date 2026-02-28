/**
 * MaahiOS Restart Console App
 * 
 * Commands:
 *   help      - Show available commands
 *   restart   - Restart the system (requires confirmation)
 *   yes       - Confirm pending restart
 *   no        - Cancel pending restart
 *   exit      - Return to terminal
 * 
 * Uses: libprocess_restart() → Process Executive → SYS_RESTART
 */

#include "../console_app.h"
#include "../../../libraries/libprocess/libprocess.h"

static int g_restart_pending = 0;

static void restart_init(gui_console_t *con) {
    g_restart_pending = 0;
    gui_console_print(con, "\n");
    gui_console_print_color(con, "=== System Restart ===\n", APP_COLOR_HEADING);
    gui_console_print(con, "Type 'restart' to reboot the system.\n");
    gui_console_print(con, "Type 'help' for commands, 'exit' to return.\n\n");
}

static void restart_handle(gui_console_t *con, const char *cmd) {
    if (app_str_equal(cmd, "help")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "Restart Commands:\n", APP_COLOR_HEADING);
        gui_console_print(con, "  restart   - Initiate system reboot\n");
        gui_console_print(con, "  yes       - Confirm pending restart\n");
        gui_console_print(con, "  no        - Cancel pending restart\n");
        gui_console_print(con, "  exit      - Return to terminal\n");
    } else if (app_str_equal(cmd, "restart")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "WARNING: ", APP_COLOR_WARN);
        gui_console_print(con, "This will restart the system.\n");
        gui_console_print(con, "All unsaved work will be lost.\n");
        gui_console_print_color(con, "Are you sure? (yes/no): ", APP_COLOR_WARN);
        g_restart_pending = 1;
    } else if (app_str_equal(cmd, "yes") && g_restart_pending) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "Restarting...\n", APP_COLOR_SUCCESS);
        libprocess_system_restart();
        /* Never returns */
    } else if (app_str_equal(cmd, "no") && g_restart_pending) {
        g_restart_pending = 0;
        gui_console_print(con, "\nRestart cancelled.\n");
    } else {
        if (g_restart_pending) {
            gui_console_print(con, "\nPlease type 'yes' to confirm or 'no' to cancel.\n");
        } else {
            gui_console_print(con, "\nUnknown command. Type 'help' for available commands.\n");
        }
    }
}

static void restart_cleanup(gui_console_t *con) {
    (void)con;
    g_restart_pending = 0;
}

console_app_t app_restart = {
    .name        = "restart",
    .description = "Reboot the system",
    .init        = restart_init,
    .handle_command = restart_handle,
    .cleanup     = restart_cleanup,
};
