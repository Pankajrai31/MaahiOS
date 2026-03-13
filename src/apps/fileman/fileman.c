/**
 * fileman.mex - MaahiOS File Manager
 *
 * Console non-interactive .mex application.
 * Reads command args from terminal, prints output to stdout SHM, exits.
 *
 * Usage:
 *   fileman help                  Show all commands with examples
 *   fileman ls [path]             List files in directory (default: /)
 *   fileman cat <path> <file>     Display file contents as text
 *   fileman count [path]          Show file count in directory
 *   fileman mkdir <parent> <name> Create a directory (MFS)
 *   fileman mkfile <path> <name> [text...]  Create a text file (MFS)
 *   fileman del <path> <name>     Delete a file (MFS)
 *
 * Uses: libfs, libconsole
 */

#include "../../system/libraries/liblog/liblog.h"
#include "../../system/libraries/libconsole/libconsole.h"
#include "../../system/libraries/libfs/libfs.h"
#include "../../system/libraries/libcell/libcell.h"

/*=============================================================================
 * DRIVE STATE
 *===========================================================================*/

static char g_drive = 'C';  /* Current drive letter from terminal */

/* Prepend drive letter to path if not already qualified */
static void qualify_path(char *out, int max, const char *path) {
    /* Already has drive prefix like "D:/"? Pass through */
    if (path[0] && path[1] == ':' &&
        (path[2] == '/' || path[2] == '\\' || path[2] == '\0')) {
        int i = 0;
        while (i < max - 1 && path[i]) { out[i] = path[i]; i++; }
        out[i] = '\0';
        return;
    }
    out[0] = g_drive;
    out[1] = ':';
    int i = 0;
    while (i < max - 3 && path[i]) { out[i + 2] = path[i]; i++; }
    out[i + 2] = '\0';
}

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

static const char *skip_spaces(const char *s) {
    while (*s == ' ') s++;
    return s;
}

/* Extract next word from string; returns pointer past the word */
static const char *next_word(const char *s, char *out, int max) {
    s = skip_spaces(s);
    int i = 0;
    while (*s && *s != ' ' && i < max - 1) {
        out[i++] = *s++;
    }
    out[i] = '\0';
    return s;
}

/* Copy remaining string (after skipping leading spaces) */
static void rest_of_string(const char *s, char *out, int max) {
    s = skip_spaces(s);
    int i = 0;
    while (*s && i < max - 1) {
        out[i++] = *s++;
    }
    out[i] = '\0';
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

/*=============================================================================
 * COMMANDS
 *===========================================================================*/

static void cmd_help(void) {
    console_print("File Manager - Usage:\n\n");
    console_print("  fileman ls [path]                  List directory contents\n");
    console_print("  fileman cat <path> <file>          Display file contents\n");
    console_print("  fileman count [path]               File count in directory\n");
    console_print("  fileman mkdir <parent> <name>      Create directory (MFS)\n");
    console_print("  fileman mkfile <path> <name> [txt] Create text file (MFS)\n");
    console_print("  fileman del <path> <name>          Delete file (MFS)\n");
    console_print("  fileman help                       Show this help\n");
    console_print("\nExamples:\n");
    console_print("  C:\\> fileman ls /\n");
    console_print("  C:\\> fileman cat / readme.txt\n");
    console_print("  C:\\> fileman mkdir / docs\n");
    console_print("  C:\\> fileman mkfile / hello.txt Hello World!\n");
    console_print("  C:\\> fileman del / hello.txt\n");
}

static void cmd_ls(const char *arg) {
    arg = skip_spaces(arg);
    const char *path = (*arg) ? arg : "/";

    /* Extract just the first word as path */
    char dir[128];
    next_word(path, dir, 128);
    if (dir[0] == '\0') dir[0] = '/', dir[1] = '\0';

    fs_file_entry_t entries[32];
    char qdir[280];
    qualify_path(qdir, 280, dir);
    int count = libfs_list_dir(qdir, entries, 32);

    if (count < 0) {
        console_print("Error: Could not read directory '");
        console_print(dir);
        console_print("'\n");
        return;
    }

    console_print("Directory: ");
    console_print(dir);
    console_print("\n\n");

    if (count == 0) {
        console_print("  (empty)\n");
        return;
    }

    console_print("  Type     Size       Name\n");
    console_print("  ----     ----       ----\n");

    for (int i = 0; i < count; i++) {
        console_print("  ");

        if (entries[i].is_directory) {
            console_print("<DIR>  ");
            console_print("           ");
        } else {
            console_print("<FILE> ");

            /* Right-aligned size in 10-char field */
            uint32_t sz = entries[i].size;
            char size_str[12];
            int pos = 10;
            size_str[11] = '\0';
            size_str[10] = ' ';
            if (sz == 0) {
                size_str[--pos] = '0';
            } else {
                while (sz > 0 && pos > 0) {
                    size_str[--pos] = '0' + (char)(sz % 10);
                    sz /= 10;
                }
            }
            while (pos > 0) size_str[--pos] = ' ';
            console_print(size_str);
        }

        console_print(entries[i].name);
        console_putchar('\n');
    }

    console_print("\n  ");
    console_print_int(count);
    console_print(" item(s)\n");
}

static void cmd_cat(const char *arg) {
    char dir[128];
    char file[128];

    const char *rest = next_word(arg, dir, 128);
    next_word(rest, file, 128);

    if (dir[0] == '\0' || file[0] == '\0') {
        console_print("Usage: fileman cat <dir_path> <filename>\n");
        console_print("Example: fileman cat / readme.txt\n");
        return;
    }

    static uint8_t file_buf[4096];
    char qdir[280];
    qualify_path(qdir, 280, dir);
    int bytes = libfs_read_file(qdir, file, file_buf, sizeof(file_buf) - 1);

    if (bytes < 0) {
        console_print("Error: Could not read '");
        console_print(file);
        console_print("' in ");
        console_print(dir);
        console_putchar('\n');
        return;
    }

    file_buf[bytes] = '\0';

    /* Print file contents, replacing non-printables with dots */
    for (int i = 0; i < bytes; i++) {
        char c = (char)file_buf[i];
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127)) {
            console_putchar(c);
        } else {
            console_putchar('.');
        }
    }

    console_print("\n\n  (");
    console_print_int(bytes);
    console_print(" bytes)\n");
}

static void cmd_count(const char *arg) {
    arg = skip_spaces(arg);
    const char *path = (*arg) ? arg : "/";

    char dir[128];
    next_word(path, dir, 128);
    if (dir[0] == '\0') dir[0] = '/', dir[1] = '\0';

    char qdir_c[280];
    qualify_path(qdir_c, 280, dir);
    int count = libfs_file_count(qdir_c);

    if (count < 0) {
        console_print("Error: Could not count files in '");
        console_print(dir);
        console_print("'\n");
        return;
    }

    console_print("Files in ");
    console_print(dir);
    console_print(": ");
    console_print_int(count);
    console_putchar('\n');
}

static void cmd_mkdir(const char *arg) {
    char parent[128];
    char dirname[128];

    const char *rest = next_word(arg, parent, 128);
    next_word(rest, dirname, 128);

    if (parent[0] == '\0' || dirname[0] == '\0') {
        console_print("Usage: fileman mkdir <parent_path> <dir_name>\n");
        console_print("Example: fileman mkdir / docs\n");
        return;
    }

    char qparent[280];
    qualify_path(qparent, 280, parent);
    int result = libfs_create_dir(qparent, dirname);
    if (result == 0) {
        console_print("Directory '");
        console_print(dirname);
        console_print("' created in ");
        console_print(parent);
        console_putchar('\n');
    } else {
        console_print("Error: Could not create directory (");
        console_print_int(result);
        console_print(")\n");
    }
}

static void cmd_mkfile(const char *arg) {
    char dir[128];
    char filename[128];
    char content[512];

    const char *rest = next_word(arg, dir, 128);
    rest = next_word(rest, filename, 128);
    rest_of_string(rest, content, 512);

    /* Support single-path syntax: "fileman mkfile /hello.txt" */
    if (dir[0] != '\0' && filename[0] == '\0') {
        /* Split dir into parent directory + filename */
        int len = str_len(dir);
        int last_slash = -1;
        for (int i = 0; i < len; i++)
            if (dir[i] == '/' || dir[i] == '\\') last_slash = i;
        if (last_slash >= 0) {
            int fi = 0;
            for (int i = last_slash + 1; i < len && fi < 127; i++)
                filename[fi++] = dir[i];
            filename[fi] = '\0';
            if (last_slash == 0) { dir[0] = '/'; dir[1] = '\0'; }
            else dir[last_slash] = '\0';
            /* Move any remaining words to content */
            rest_of_string(rest, content, 512);
        }
    }

    if (dir[0] == '\0' || filename[0] == '\0') {
        console_print("Usage: fileman mkfile <path> [content]\n");
        console_print("   or: fileman mkfile <dir> <file> [content]\n");
        console_print("Example: fileman mkfile /hello.txt Hello World!\n");
        return;
    }

    int data_len = str_len(content);
    char qdir_w[280];
    qualify_path(qdir_w, 280, dir);
    int result = libfs_write_file(qdir_w, filename,
                                  (data_len > 0) ? content : (void*)0,
                                  (uint32_t)data_len);
    if (result >= 0) {
        console_print("File '");
        console_print(filename);
        console_print("' created in ");
        console_print(dir);
        if (data_len > 0) {
            console_print(" (");
            console_print_int(data_len);
            console_print(" bytes)");
        }
        console_putchar('\n');
    } else {
        console_print("Error: Could not create file (");
        console_print_int(result);
        console_print(")\n");
    }
}

static void cmd_del(const char *arg) {
    char dir[128];
    char filename[128];

    const char *rest = next_word(arg, dir, 128);
    next_word(rest, filename, 128);

    if (dir[0] == '\0' || filename[0] == '\0') {
        console_print("Usage: fileman del <dir_path> <filename>\n");
        console_print("Example: fileman del / hello.txt\n");
        return;
    }

    char qdir_d[280];
    qualify_path(qdir_d, 280, dir);
    int result = libfs_delete_file(qdir_d, filename);
    if (result >= 0) {
        console_print("Deleted '");
        console_print(filename);
        console_print("' from ");
        console_print(dir);
        console_putchar('\n');
    } else {
        console_print("Error: Could not delete file (");
        console_print_int(result);
        console_print(")\n");
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    liblog_init();
    liblog(LOG_INFO, "FILEMAN", "File Manager starting (non-interactive)");

    /* Initialize console output (stdout SHM pipe) */
    if (console_init() != 0) {
        liblog(LOG_ERROR, "FILEMAN", "Failed to init console output");
        return;
    }

    /* Read current drive letter from terminal cell */
    char drive_buf = 'C';
    if (libcell_read(CONSOLE_DRIVE_CELL, &drive_buf, 1) > 0) {
        g_drive = drive_buf;
    }

    /* Read command arguments from terminal */
    char args[512];
    args[0] = '\0';
    console_get_args(args, 512);

    /* No arguments: show usage hint */
    if (args[0] == '\0') {
        console_print("Usage: fileman <command>\n");
        console_print("Type 'fileman help' for available commands.\n");
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
    } else if (str_equal_nocase(cmd, "ls")) {
        cmd_ls(rest);
    } else if (str_equal_nocase(cmd, "cat")) {
        cmd_cat(rest);
    } else if (str_equal_nocase(cmd, "count")) {
        cmd_count(rest);
    } else if (str_equal_nocase(cmd, "mkdir")) {
        cmd_mkdir(rest);
    } else if (str_equal_nocase(cmd, "mkfile")) {
        cmd_mkfile(rest);
    } else if (str_equal_nocase(cmd, "del")) {
        cmd_del(rest);
    } else {
        console_print("Unknown command: ");
        console_print(cmd);
        console_print("\nType 'fileman help' for available commands.\n");
    }

    liblog(LOG_INFO, "FILEMAN", "File Manager exiting");
}
