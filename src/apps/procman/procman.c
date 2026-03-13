/**
 * procman.mex - MaahiOS Process Manager
 *
 * Console non-interactive .mex application.
 * Reads command args from terminal, prints output to stdout SHM, exits.
 *
 * Usage:
 *   procman help        Show all commands with examples
 *   procman list        List all running processes
 *   procman count       Show total process count
 *   procman info <pid>  Show details for a specific process
 *   procman kill <pid>  Terminate a process by PID
 *
 * Uses: libprocess, libconsole
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libprocess/libprocess.h"

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
 * STATE / TYPE NAMES
 *===========================================================================*/

static const char *state_name(uint32_t state) {
    switch (state) {
        case 1:  return "Ready";
        case 2:  return "Running";
        default: return "Unknown";
    }
}

static const char *type_name(uint8_t type) {
    switch (type) {
        case 0:  return "System";
        case 1:  return "User";
        default: return "Unknown";
    }
}

/*=============================================================================
 * MEMORY FORMAT HELPER
 *===========================================================================*/

static void print_memory(uint32_t bytes) {
    if (bytes == 0) {
        console_print("     0 B");
        return;
    }
    uint32_t kb = bytes / 1024;
    if (kb < 1024) {
        /* Print as KB */
        if (kb < 10) console_print("     ");
        else if (kb < 100) console_print("    ");
        else if (kb < 1000) console_print("   ");
        else console_print("  ");
        console_print_int((int)kb);
        console_print(" KB");
    } else {
        /* Print as MB */
        uint32_t mb = kb / 1024;
        console_print("    ");
        console_print_int((int)mb);
        console_print(" MB");
    }
}

/*=============================================================================
 * PAD HELPER (print string padded to width)
 *===========================================================================*/

static int str_len_local(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void print_padded(const char *str, int width) {
    int len = str_len_local(str);
    console_print(str);
    for (int i = len; i < width; i++)
        console_putchar(' ');
}

/*=============================================================================
 * COMMANDS
 *===========================================================================*/

static void cmd_help(void) {
    console_print("Process Manager - Usage:\n\n");
    console_print("  procman list        List all running processes\n");
    console_print("  procman count       Show total process count\n");
    console_print("  procman info <pid>  Show details for a process\n");
    console_print("  procman kill <pid>  Terminate a process\n");
    console_print("  procman help        Show this help\n");
    console_print("\nExamples:\n");
    console_print("  C:\\> procman list\n");
    console_print("  C:\\> procman info 5\n");
    console_print("  C:\\> procman kill 12\n");
}

static void cmd_list(void) {
    process_info_t infos[32];
    int count = libprocess_list(infos, 32);

    if (count < 0) {
        console_print("Error: Could not retrieve process list.\n");
        return;
    }

    /* Header */
    console_print("  PID  Name                State    Type     Memory\n");
    console_print("  ---  ----                -----    ----     ------\n");

    for (int i = 0; i < count; i++) {
        console_print("  ");
        
        /* PID (right-aligned, 3 chars) */
        int pid = infos[i].pid;
        if (pid < 10) console_print("  ");
        else if (pid < 100) console_putchar(' ');
        console_print_int(pid);
        console_print("  ");
        
        /* Name (20 chars padded) */
        print_padded(infos[i].name, 20);
        
        /* State (9 chars padded) */
        print_padded(state_name(infos[i].state), 9);
        
        /* Type (9 chars padded) */
        print_padded(type_name(infos[i].type), 9);
        
        /* Memory */
        print_memory(infos[i].memory_alloc);
        
        console_putchar('\n');
    }

    console_print("\n  Total: ");
    console_print_int(count);
    console_print(" process(es)\n");
}

static void cmd_count(void) {
    int count = libprocess_get_count();
    console_print("Active processes: ");
    console_print_int(count);
    console_putchar('\n');
}

static void cmd_info(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print("Usage: procman info <pid>\n");
        console_print("Example: procman info 5\n");
        return;
    }

    int pid = str_to_int(arg);
    if (pid <= 0) {
        console_print("Error: Invalid PID.\n");
        return;
    }

    process_info_t info;
    int result = libprocess_get_info(pid, &info);

    if (result != 0) {
        console_print("Error: Process not found (PID ");
        console_print_int(pid);
        console_print(")\n");
        return;
    }

    console_print("Process Information:\n");
    console_print("  PID:     ");
    console_print_int(info.pid);
    console_print("\n  Name:    ");
    console_print(info.name);
    console_print("\n  State:   ");
    console_print(state_name(info.state));
    console_print("\n  Type:    ");
    console_print(type_name(info.type));
    console_print("\n  Memory:  ");
    print_memory(info.memory_alloc);
    console_putchar('\n');
}

static void cmd_kill(const char *arg) {
    arg = skip_spaces(arg);
    if (*arg == '\0') {
        console_print("Usage: procman kill <pid>\n");
        console_print("Example: procman kill 12\n");
        return;
    }

    int pid = str_to_int(arg);
    if (pid <= 0) {
        console_print("Error: Invalid PID.\n");
        return;
    }

    /* Get process info to check if it's a system process */
    process_info_t info;
    int info_result = libprocess_get_info(pid, &info);
    if (info_result != 0) {
        console_print("Error: Process not found (PID ");
        console_print_int(pid);
        console_print(")\n");
        return;
    }

    if (info.type == 0 /* PROC_TYPE_SYSTEM */) {
        console_print("Error: Cannot kill system process '");
        console_print(info.name);
        console_print("' (PID ");
        console_print_int(pid);
        console_print(")\n");
        return;
    }

    int result = libprocess_kill(pid);

    if (result == 0) {
        console_print("Process ");
        console_print_int(pid);
        console_print(" terminated.\n");
    } else {
        console_print("Error: Failed to kill PID ");
        console_print_int(pid);
        console_putchar('\n');
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();
    liblog(LOG_INFO, "PROCMAN", "Process Manager starting (non-interactive)");

    if (console_init() != 0) {
        liblog(LOG_ERROR, "PROCMAN", "Failed to init console output");
        return;
    }

    char args[256];
    args[0] = '\0';
    console_get_args(args, 256);

    if (args[0] == '\0') {
        console_print("Usage: procman <command>\n");
        console_print("Type 'procman help' for available commands.\n");
        return;
    }

    /* Parse command */
    char cmd[32];
    const char *rest = "";
    int ci = 0, i = 0;
    while (args[i] && args[i] != ' ' && ci < 31) cmd[ci++] = args[i++];
    cmd[ci] = '\0';
    while (args[i] == ' ') i++;
    if (args[i]) rest = &args[i];

    if (str_equal_nocase(cmd, "help")) {
        cmd_help();
    } else if (str_equal_nocase(cmd, "list")) {
        cmd_list();
    } else if (str_equal_nocase(cmd, "count")) {
        cmd_count();
    } else if (str_equal_nocase(cmd, "info")) {
        cmd_info(rest);
    } else if (str_equal_nocase(cmd, "kill")) {
        cmd_kill(rest);
    } else {
        console_print("Unknown command: ");
        console_print(cmd);
        console_print("\nType 'procman help' for available commands.\n");
    }

    liblog(LOG_INFO, "PROCMAN", "Process Manager exiting");
}
