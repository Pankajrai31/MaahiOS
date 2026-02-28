/**
 * MaahiOS Process Manager Console App
 * 
 * Commands:
 *   help        - Show available commands
 *   list        - List all running processes
 *   count       - Show total process count
 *   info <pid>  - Show details for a specific process
 *   kill <pid>  - Terminate a process by PID
 *   exit        - Return to terminal
 * 
 * Uses: libprocess_list(), libprocess_get_info(), libprocess_kill(),
 *       libprocess_get_count()
 *       All go through Process Executive → kernel
 */

#include "../console_app.h"
#include "../../../libraries/libprocess/libprocess.h"

/* Process state names */
static const char *state_name(uint32_t state) {
    switch (state) {
        case 1:  return "READY";
        case 2:  return "RUNNING";
        default: return "UNKNOWN";
    }
}

static void procman_init(gui_console_t *con) {
    gui_console_print(con, "\n");
    gui_console_print_color(con, "=== Process Manager ===\n", APP_COLOR_HEADING);
    gui_console_print(con, "Manage running processes on MaahiOS.\n");
    gui_console_print(con, "Type 'help' for commands, 'exit' to return.\n\n");
}

static void procman_cmd_list(gui_console_t *con) {
    process_info_t infos[32];
    int count = libprocess_list(infos, 32);
    
    if (count < 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Could not retrieve process list.\n");
        return;
    }
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "  PID    State\n", APP_COLOR_HEADING);
    gui_console_print_color(con, "  ---    -----\n", APP_COLOR_HEADING);
    
    for (int i = 0; i < count; i++) {
        gui_console_print(con, "  ");
        
        /* PID (right-aligned in 4 chars) */
        int pid = infos[i].pid;
        if (pid < 10) gui_console_print(con, "  ");
        else if (pid < 100) gui_console_print(con, " ");
        gui_console_print_int(con, pid);
        
        gui_console_print(con, "    ");
        gui_console_print(con, state_name(infos[i].state));
        gui_console_print(con, "\n");
    }
    
    gui_console_print(con, "\n  Total: ");
    gui_console_print_int(con, count);
    gui_console_print(con, " process(es)\n");
}

static void procman_cmd_count(gui_console_t *con) {
    int count = libprocess_get_count();
    gui_console_print(con, "\nActive processes: ");
    gui_console_print_int(con, count);
    gui_console_print(con, "\n");
}

static void procman_cmd_info(gui_console_t *con, const char *arg) {
    arg = app_skip_spaces(arg);
    if (*arg == '\0') {
        gui_console_print(con, "\nUsage: info <pid>\n");
        return;
    }
    
    int pid = app_str_to_int(arg);
    if (pid <= 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Invalid PID.\n");
        return;
    }
    
    process_info_t info;
    int result = libprocess_get_info(pid, &info);
    
    if (result != 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Process not found (PID ");
        gui_console_print_int(con, pid);
        gui_console_print(con, ")\n");
        return;
    }
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "Process Information:\n", APP_COLOR_HEADING);
    gui_console_print(con, "  PID:    ");
    gui_console_print_int(con, info.pid);
    gui_console_print(con, "\n  State:  ");
    gui_console_print(con, state_name(info.state));
    gui_console_print(con, "\n");
}

static void procman_cmd_kill(gui_console_t *con, const char *arg) {
    arg = app_skip_spaces(arg);
    if (*arg == '\0') {
        gui_console_print(con, "\nUsage: kill <pid>\n");
        return;
    }
    
    int pid = app_str_to_int(arg);
    if (pid <= 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Invalid PID.\n");
        return;
    }
    
    /* Prevent killing critical system processes */
    if (pid <= 2) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Cannot kill system process (PID ");
        gui_console_print_int(con, pid);
        gui_console_print(con, ")\n");
        return;
    }
    
    int result = libprocess_kill(pid);
    
    if (result == 0) {
        gui_console_print_color(con, "\nProcess ", APP_COLOR_SUCCESS);
        gui_console_print_int(con, pid);
        gui_console_print_color(con, " terminated.\n", APP_COLOR_SUCCESS);
    } else {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Failed to kill PID ");
        gui_console_print_int(con, pid);
        gui_console_print(con, "\n");
    }
}

static void procman_handle(gui_console_t *con, const char *cmd) {
    if (app_str_equal(cmd, "help")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "Process Manager Commands:\n", APP_COLOR_HEADING);
        gui_console_print(con, "  list        - List all running processes\n");
        gui_console_print(con, "  count       - Show total process count\n");
        gui_console_print(con, "  info <pid>  - Show details for a process\n");
        gui_console_print(con, "  kill <pid>  - Terminate a process\n");
        gui_console_print(con, "  exit        - Return to terminal\n");
    } else if (app_str_equal(cmd, "list")) {
        procman_cmd_list(con);
    } else if (app_str_equal(cmd, "count")) {
        procman_cmd_count(con);
    } else if (app_str_starts_with(cmd, "info ")) {
        procman_cmd_info(con, cmd + 5);
    } else if (app_str_starts_with(cmd, "kill ")) {
        procman_cmd_kill(con, cmd + 5);
    } else {
        gui_console_print(con, "\nUnknown command. Type 'help' for available commands.\n");
    }
}

static void procman_cleanup(gui_console_t *con) {
    (void)con;
}

console_app_t app_procman = {
    .name        = "procman",
    .description = "Process Manager - list, inspect, kill processes",
    .init        = procman_init,
    .handle_command = procman_handle,
    .cleanup     = procman_cleanup,
};
