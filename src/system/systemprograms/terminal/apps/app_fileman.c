/**
 * MaahiOS File Manager Console App
 * 
 * Commands:
 *   help             - Show available commands
 *   ls [path]        - List files in directory (default: /)
 *   cat <file>       - Display file contents
 *   count [path]     - Show file count in directory
 *   exit             - Return to terminal
 * 
 * Uses: libfs_list_dir(), libfs_read_file(), libfs_file_count()
 *       All go through FS Executive → kernel → ISO9660 driver
 */

#include "../console_app.h"
#include "../../../libraries/libfs/libfs.h"

/* Track current directory for convenience */
static char g_cwd[128] = "/";

static void fileman_init(gui_console_t *con) {
    /* Reset to root */
    g_cwd[0] = '/';
    g_cwd[1] = '\0';
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "=== File Manager ===\n", APP_COLOR_HEADING);
    gui_console_print(con, "Browse files on MaahiOS.\n");
    gui_console_print(con, "Type 'help' for commands, 'exit' to return.\n\n");
}

static void fileman_cmd_ls(gui_console_t *con, const char *path) {
    path = app_skip_spaces(path);
    if (*path == '\0') path = g_cwd;
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "Directory: ", APP_COLOR_HEADING);
    gui_console_print(con, path);
    gui_console_print(con, "\n\n");
    
    fs_file_entry_t entries[32];
    int count = libfs_list_dir(path, entries, 32);
    
    if (count < 0) {
        gui_console_print_color(con, "Error: ", APP_COLOR_ERROR);
        gui_console_print(con, "Could not read directory.\n");
        return;
    }
    
    if (count == 0) {
        gui_console_print(con, "  (empty)\n");
        return;
    }
    
    /* Header */
    gui_console_print_color(con, "  Type     Size       Name\n", APP_COLOR_HEADING);
    gui_console_print_color(con, "  ----     ----       ----\n", APP_COLOR_HEADING);
    
    for (int i = 0; i < count; i++) {
        gui_console_print(con, "  ");
        
        if (entries[i].is_directory) {
            gui_console_print_color(con, "<DIR>  ", APP_COLOR_WARN);
            gui_console_print(con, "           ");
        } else {
            gui_console_print(con, "<FILE> ");
            
            /* Right-align size in 10 chars */
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
            while (pos > 0) {
                size_str[--pos] = ' ';
            }
            gui_console_print(con, size_str);
        }
        
        gui_console_print(con, entries[i].name);
        gui_console_print(con, "\n");
    }
    
    gui_console_print(con, "\n  ");
    gui_console_print_int(con, count);
    gui_console_print(con, " item(s)\n");
}

static void fileman_cmd_cat(gui_console_t *con, const char *filename) {
    filename = app_skip_spaces(filename);
    if (*filename == '\0') {
        gui_console_print(con, "\nUsage: cat <filename>\n");
        return;
    }
    
    gui_console_print(con, "\n");
    gui_console_print_color(con, "File: ", APP_COLOR_HEADING);
    gui_console_print(con, filename);
    gui_console_print(con, "\n\n");
    
    /* Read file from current directory */
    static uint8_t file_buf[4096];
    int bytes = libfs_read_file(g_cwd, filename, file_buf, sizeof(file_buf) - 1);
    
    if (bytes < 0) {
        gui_console_print_color(con, "Error: ", APP_COLOR_ERROR);
        gui_console_print(con, "Could not read file '");
        gui_console_print(con, filename);
        gui_console_print(con, "'\n");
        return;
    }
    
    /* Null-terminate and print as text */
    file_buf[bytes] = '\0';
    
    /* Print contents (treating as text, skip non-printable) */
    for (int i = 0; i < bytes; i++) {
        char c = (char)file_buf[i];
        if (c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c < 127)) {
            gui_console_putchar(con, c);
        } else {
            gui_console_putchar(con, '.');
        }
    }
    
    gui_console_print(con, "\n\n  (");
    gui_console_print_int(con, bytes);
    gui_console_print(con, " bytes)\n");
}

static void fileman_cmd_count(gui_console_t *con, const char *path) {
    path = app_skip_spaces(path);
    if (*path == '\0') path = g_cwd;
    
    int count = libfs_file_count(path);
    
    if (count < 0) {
        gui_console_print_color(con, "\nError: ", APP_COLOR_ERROR);
        gui_console_print(con, "Could not count files in '");
        gui_console_print(con, path);
        gui_console_print(con, "'\n");
        return;
    }
    
    gui_console_print(con, "\nFiles in ");
    gui_console_print(con, path);
    gui_console_print(con, ": ");
    gui_console_print_int(con, count);
    gui_console_print(con, "\n");
}

static void fileman_handle(gui_console_t *con, const char *cmd) {
    if (app_str_equal(cmd, "help")) {
        gui_console_print(con, "\n");
        gui_console_print_color(con, "File Manager Commands:\n", APP_COLOR_HEADING);
        gui_console_print(con, "  ls [path]      - List files (default: current dir)\n");
        gui_console_print(con, "  cat <file>     - Display file contents as text\n");
        gui_console_print(con, "  count [path]   - Show file count in directory\n");
        gui_console_print(con, "  exit           - Return to terminal\n");
    } else if (app_str_equal(cmd, "ls")) {
        fileman_cmd_ls(con, "");
    } else if (app_str_starts_with(cmd, "ls ")) {
        fileman_cmd_ls(con, cmd + 3);
    } else if (app_str_starts_with(cmd, "cat ")) {
        fileman_cmd_cat(con, cmd + 4);
    } else if (app_str_equal(cmd, "count")) {
        fileman_cmd_count(con, "");
    } else if (app_str_starts_with(cmd, "count ")) {
        fileman_cmd_count(con, cmd + 6);
    } else {
        gui_console_print(con, "\nUnknown command. Type 'help' for available commands.\n");
    }
}

static void fileman_cleanup(gui_console_t *con) {
    (void)con;
    g_cwd[0] = '/';
    g_cwd[1] = '\0';
}

console_app_t app_fileman = {
    .name        = "fileman",
    .description = "File Manager - browse and view files",
    .init        = fileman_init,
    .handle_command = fileman_handle,
    .cleanup     = fileman_cleanup,
};
