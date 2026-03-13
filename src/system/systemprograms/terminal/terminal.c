/**
 * MaahiOS Terminal v4 — Windowed Terminal
 * 
 * Description:
 *   Minimal command-line shell running in Ring 3 inside a libwindow window.
 *   Gets titlebar, drag, minimize, maximize, close for free from libwindow.
 *
 *   Built-in commands: help, dir, cd, type, clear, drive switch.
 *   Everything else runs as a .mex app via auto-run by name.
 * 
 *   Console non-interactive apps write output to an SHM stdout
 *   buffer. After the app exits, terminal reads and displays it.
 * 
 *   Three .mex types:
 *   - Console non-interactive: runs, prints to stdout SHM, exits
 *   - Console interactive: (future) reads keyboard input
 *   - GUI: owns its own window (hellogui, fileman, etc.)
 * 
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include <stdint.h>
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libfs/libfs.h"
#include "../../libraries/libmex/libmex.h"
#include "../../libraries/libprocess/libprocess.h"
#include "../../libraries/libgui/libgui.h"
#include "../../libraries/libcell/libcell.h"
#include "../../libraries/libconsole/libconsole.h"
#include "../../libraries/libwindow/libwindow.h"

/*=============================================================================
 * TERMINAL CONFIGURATION
 *===========================================================================*/

/* Window size: 800x600 content + titlebar (24px) + bevel (4px) */
#define TERM_WIN_W              808    /* 800 + 4*2 bevel */
#define TERM_WIN_H              632    /* 600 + 24 titlebar + 4*2 bevel */
#define TERM_PAD                8      /* Padding inside content area */

/* Colors */
#define COLOR_BG                0x00101020   /* Dark blue-black */
#define COLOR_FG                0x00CCCCCC   /* Light gray */
#define COLOR_PROMPT            0x0000CC00   /* Green */
#define COLOR_HEADING           0x0000AAFF   /* Blue headings */
#define COLOR_ERROR             0x00FF4444   /* Red errors */
#define COLOR_DIR               0x00FFCC00   /* Yellow directories */
#define COLOR_MEX               0x0088CCFF   /* Light blue .mex files */

/* Cursor blink interval (in ticks) */
#define CURSOR_BLINK_INTERVAL   30

/*=============================================================================
 * SCREEN BUFFER — each cell holds a character + color
 *===========================================================================*/

typedef struct {
    char     ch;
    uint32_t fg;
} term_cell_t;

/* Screen dimensions computed at init from content area */
static int g_cols = 0;
static int g_rows = 0;

#define TERM_MAX_COLS   120
#define TERM_MAX_ROWS   40

static term_cell_t g_screen[TERM_MAX_ROWS][TERM_MAX_COLS];
static int g_cursor_row = 0;
static int g_cursor_col = 0;

/*=============================================================================
 * GLOBAL STATE
 *===========================================================================*/

static window_t *g_win = (window_t *)0;

#define INPUT_MAX       256
static char input_buf[INPUT_MAX];
static int input_len = 0;

/* Current Working Directory */
static char g_drive;               /* 'C', 'D', etc. */
static char g_cwd[256];           /* Internal: "/", "/BOOT/GRUB" */
static char g_prompt[280];        /* Display: "C:\BOOT\GRUB> " */

/* MEX file cache */
static char g_last_mex_name[32] = {0};
static int  g_last_mex_size = 0;

/* Cursor blink state */
static int g_cursor_visible = 1;
static int g_blink_counter = 0;

/* Child process tracking */
static int g_waiting_for_child = 0;
static int g_child_pid = -1;
static int g_child_is_gui = 0;
static int g_child_stdout_shm_id = -1;
static console_stdout_buf_t *g_child_stdout_buf = (console_stdout_buf_t *)0;

/*=============================================================================
 * STRING HELPERS
 *===========================================================================*/

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void str_copy(char *dst, const char *src, int max) {
    int i = 0;
    while (i < max - 1 && src[i]) { dst[i] = src[i]; i++; }
    dst[i] = '\0';
}

static void str_append(char *dst, const char *src, int max) {
    int dlen = str_len(dst);
    int i = 0;
    while (dlen + i < max - 1 && src[i]) { dst[dlen + i] = src[i]; i++; }
    dst[dlen + i] = '\0';
}

static char to_upper(char c) {
    return (c >= 'a' && c <= 'z') ? c - 32 : c;
}

static int str_equal(const char *a, const char *b) {
    while (*a && *b) { if (*a != *b) return 0; a++; b++; }
    return (*a == *b);
}

static int str_equal_nocase(const char *a, const char *b) {
    while (*a && *b) { if (to_upper(*a) != to_upper(*b)) return 0; a++; b++; }
    return (to_upper(*a) == to_upper(*b));
}

static void str_toupper(char *s) {
    while (*s) { *s = to_upper(*s); s++; }
}

/*=============================================================================
 * SCREEN BUFFER OPERATIONS
 *===========================================================================*/

/** Clear whole screen buffer */
static void screen_clear(void) {
    for (int r = 0; r < g_rows; r++) {
        for (int c = 0; c < g_cols; c++) {
            g_screen[r][c].ch = ' ';
            g_screen[r][c].fg = COLOR_FG;
        }
    }
    g_cursor_row = 0;
    g_cursor_col = 0;
}

/** Scroll screen buffer up by one line */
static void screen_scroll_up(void) {
    for (int r = 0; r < g_rows - 1; r++) {
        for (int c = 0; c < g_cols; c++) {
            g_screen[r][c] = g_screen[r + 1][c];
        }
    }
    for (int c = 0; c < g_cols; c++) {
        g_screen[g_rows - 1][c].ch = ' ';
        g_screen[g_rows - 1][c].fg = COLOR_FG;
    }
}

/** Put a character at cursor position and advance */
static void screen_putchar(char ch, uint32_t color) {
    if (ch == '\n') {
        g_cursor_col = 0;
        g_cursor_row++;
        if (g_cursor_row >= g_rows) {
            screen_scroll_up();
            g_cursor_row = g_rows - 1;
        }
        return;
    }

    if (g_cursor_col >= g_cols) {
        g_cursor_col = 0;
        g_cursor_row++;
        if (g_cursor_row >= g_rows) {
            screen_scroll_up();
            g_cursor_row = g_rows - 1;
        }
    }

    g_screen[g_cursor_row][g_cursor_col].ch = ch;
    g_screen[g_cursor_row][g_cursor_col].fg = color;
    g_cursor_col++;
}

/** Print a string with a given color */
static void screen_print(const char *str, uint32_t color) {
    while (*str) {
        screen_putchar(*str, color);
        str++;
    }
}

/** Print an integer */
static void screen_print_int(int val, uint32_t color) {
    if (val < 0) {
        screen_putchar('-', color);
        val = -val;
    }
    if (val == 0) {
        screen_putchar('0', color);
        return;
    }
    char digits[12];
    int i = 0;
    while (val > 0) {
        digits[i++] = '0' + (char)(val % 10);
        val /= 10;
    }
    while (i > 0) {
        screen_putchar(digits[--i], color);
    }
}

/** Backspace — erase last character */
static void screen_backspace(void) {
    if (g_cursor_col > 0) {
        g_cursor_col--;
    } else if (g_cursor_row > 0) {
        g_cursor_row--;
        g_cursor_col = g_cols - 1;
    } else {
        return;
    }
    g_screen[g_cursor_row][g_cursor_col].ch = ' ';
    g_screen[g_cursor_row][g_cursor_col].fg = COLOR_FG;
}

/*=============================================================================
 * PATH HELPERS
 *===========================================================================*/

static void build_prompt(void) {
    int p = 0;
    g_prompt[p++] = g_drive;
    g_prompt[p++] = ':';
    if (g_cwd[0] == '/' && g_cwd[1] == '\0') {
        g_prompt[p++] = '\\';
    } else {
        for (int i = 0; g_cwd[i] && p < 270; i++)
            g_prompt[p++] = (g_cwd[i] == '/') ? '\\' : g_cwd[i];
    }
    g_prompt[p++] = '>';
    g_prompt[p++] = ' ';
    g_prompt[p] = '\0';
}

static void print_display_path(void) {
    screen_putchar(g_drive, COLOR_HEADING);
    screen_putchar(':', COLOR_HEADING);
    if (g_cwd[0] == '/' && g_cwd[1] == '\0') {
        screen_putchar('\\', COLOR_HEADING);
    } else {
        for (int i = 0; g_cwd[i]; i++)
            screen_putchar(g_cwd[i] == '/' ? '\\' : g_cwd[i], COLOR_HEADING);
    }
}

static int path_resolve(const char *input, char *out, int out_max) {
    char cleaned[256];
    int ci = 0;
    for (int i = 0; input[i] && ci < 254; i++)
        cleaned[ci++] = (input[i] == '\\') ? '/' : input[i];
    cleaned[ci] = '\0';

    char work[256];
    if (cleaned[0] == '/') {
        str_copy(work, cleaned, 256);
    } else {
        str_copy(work, g_cwd, 256);
        if (!(work[0] == '/' && work[1] == '\0'))
            str_append(work, "/", 256);
        str_append(work, cleaned, 256);
    }

    /* Split and resolve . and .. */
    char comp[16][48];
    int cc = 0;
    int wi = (work[0] == '/') ? 1 : 0;

    while (work[wi] && cc < 16) {
        int c2 = 0;
        while (work[wi] && work[wi] != '/' && c2 < 47)
            comp[cc][c2++] = work[wi++];
        comp[cc][c2] = '\0';
        if (work[wi] == '/') wi++;

        if (str_equal(comp[cc], ".")) continue;
        else if (str_equal(comp[cc], "..")) { if (cc > 0) cc--; continue; }
        else if (c2 > 0) { str_toupper(comp[cc]); cc++; }
    }

    /* Rebuild */
    out[0] = '/'; out[1] = '\0';
    for (int i = 0; i < cc; i++) {
        if (i == 0) { out[0] = '/'; out[1] = '\0'; str_append(out, comp[0], out_max); }
        else { str_append(out, "/", out_max); str_append(out, comp[i], out_max); }
    }
    if (cc == 0) { out[0] = '/'; out[1] = '\0'; }
    return 0;
}

static void build_drive_path(char *out, int max, const char *path) {
    out[0] = g_drive;
    out[1] = ':';
    str_copy(&out[2], path, max - 2);
}

static int path_is_valid_dir(const char *path) {
    fs_file_entry_t test[1];
    char qpath[280];
    build_drive_path(qpath, 280, path);
    return (libfs_list_dir(qpath, test, 1) >= 0) ? 1 : 0;
}

/*=============================================================================
 * BUILT-IN: help
 *===========================================================================*/

static void cmd_help(void) {
    screen_print("\n", COLOR_FG);
    screen_print(" MaahiOS Terminal - Commands:\n\n", COLOR_HEADING);

    screen_print("  Navigation:\n", COLOR_HEADING);
    screen_print("    dir             List files in current directory\n", COLOR_FG);
    screen_print("    cd <path>       Change directory (cd .., cd \\, cd BOOT)\n", COLOR_FG);
    screen_print("    cd              Show current directory\n", COLOR_FG);
    screen_print("    type <file>     Display file contents\n", COLOR_FG);
    screen_print("    <drive>:        Switch drive (e.g., D:)\n", COLOR_FG);

    screen_print("\n  Applications:\n", COLOR_HEADING);
    screen_print("    <app> <args>    Run app (e.g., diskman list)\n", COLOR_FG);
    screen_print("    <app> help      Show app usage and commands\n", COLOR_FG);

    screen_print("\n  Other:\n", COLOR_HEADING);
    screen_print("    clear           Clear the terminal\n", COLOR_FG);
    screen_print("    help            Show this help\n", COLOR_FG);
}

/*=============================================================================
 * BUILT-IN: dir
 *===========================================================================*/

static void cmd_dir(void) {
    screen_print("\n", COLOR_FG);
    screen_print(" Directory of ", COLOR_HEADING);
    print_display_path();
    screen_print("\n\n", COLOR_FG);

    fs_file_entry_t entries[32];
    char qpath[280];
    build_drive_path(qpath, 280, g_cwd);
    int count = libfs_list_dir(qpath, entries, 32);

    if (count < 0) {
        screen_print(" Error: ", COLOR_ERROR);
        screen_print("Could not read directory\n", COLOR_FG);
        return;
    }
    if (count == 0) {
        screen_print(" (empty directory)\n", COLOR_FG);
        return;
    }

    int dir_count = 0, file_count = 0;

    for (int i = 0; i < count; i++) {
        screen_print("  ", COLOR_FG);
        if (entries[i].is_directory) {
            screen_print("<DIR>   ", COLOR_DIR);
            screen_print("          ", COLOR_FG);
            screen_print(entries[i].name, COLOR_DIR);
            dir_count++;
        } else {
            screen_print("        ", COLOR_FG);
            /* Right-align size */
            uint32_t sz = entries[i].size;
            char ss[12]; int pos = 10;
            ss[11] = '\0'; ss[10] = ' ';
            if (sz == 0) { ss[--pos] = '0'; }
            else { while (sz > 0 && pos > 0) { ss[--pos] = '0' + (char)(sz % 10); sz /= 10; } }
            while (pos > 0) ss[--pos] = ' ';
            screen_print(ss, COLOR_FG);
            /* Color .mex files */
            int nlen = str_len(entries[i].name);
            if (nlen > 4 && str_equal_nocase(entries[i].name + nlen - 4, ".MEX"))
                screen_print(entries[i].name, COLOR_MEX);
            else
                screen_print(entries[i].name, COLOR_FG);
            file_count++;
        }
        screen_print("\n", COLOR_FG);
    }

    screen_print("\n  ", COLOR_FG);
    screen_print_int(file_count, COLOR_FG);
    screen_print(" file(s), ", COLOR_FG);
    screen_print_int(dir_count, COLOR_FG);
    screen_print(" dir(s)\n", COLOR_FG);
}

/*=============================================================================
 * BUILT-IN: cd
 *===========================================================================*/

static void cmd_cd(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == '\0') {
        print_display_path();
        screen_putchar('\n', COLOR_FG);
        return;
    }

    char new_path[256];
    if (path_resolve(arg, new_path, 256) < 0) {
        screen_print("Error: ", COLOR_ERROR);
        screen_print("Path too long\n", COLOR_FG);
        return;
    }

    if (new_path[0] == '/' && new_path[1] == '\0') {
        str_copy(g_cwd, new_path, 256);
        build_prompt();
        return;
    }

    if (!path_is_valid_dir(new_path)) {
        screen_print("Error: ", COLOR_ERROR);
        screen_print("Directory not found\n", COLOR_FG);
        return;
    }

    str_copy(g_cwd, new_path, 256);
    build_prompt();
}

/*=============================================================================
 * BUILT-IN: type
 *===========================================================================*/

static void cmd_type(const char *arg) {
    while (*arg == ' ') arg++;
    if (*arg == '\0') {
        screen_print("\nUsage: type <filename>\n", COLOR_FG);
        return;
    }

    char filename[64];
    str_copy(filename, arg, 64);
    str_toupper(filename);

    static uint8_t read_buf[8192];
    char qpath[280];
    build_drive_path(qpath, 280, g_cwd);
    int bytes = libfs_read_file(qpath, filename, read_buf, sizeof(read_buf));
    if (bytes < 0) bytes = libfs_read_file(qpath, arg, read_buf, sizeof(read_buf));

    if (bytes < 0) {
        screen_print("\nError: ", COLOR_ERROR);
        screen_print("Cannot read '", COLOR_FG);
        screen_print(arg, COLOR_FG);
        screen_print("'\n", COLOR_FG);
        return;
    }

    screen_print("\n", COLOR_FG);
    for (int i = 0; i < bytes; i++) {
        char c = (char)read_buf[i];
        if (c == '\n') screen_putchar('\n', COLOR_FG);
        else if (c == '\r') { /* skip */ }
        else if (c == '\t') screen_print("    ", COLOR_FG);
        else if (c >= 32 && c < 127) screen_putchar(c, COLOR_FG);
        else screen_putchar('.', COLOR_FG);
    }
    screen_putchar('\n', COLOR_FG);
    if (bytes >= (int)sizeof(read_buf))
        screen_print("(file truncated at 8KB)\n", COLOR_DIR);
}

/*=============================================================================
 * BUILT-IN: drive switch
 *===========================================================================*/

static void cmd_drive_switch(char letter) {
    letter = to_upper(letter);

    char test_path[4];
    test_path[0] = letter;
    test_path[1] = ':';
    test_path[2] = '/';
    test_path[3] = '\0';

    fs_file_entry_t test[1];
    if (libfs_list_dir(test_path, test, 1) < 0) {
        screen_print("Error: ", COLOR_ERROR);
        screen_print("Drive ", COLOR_FG);
        screen_putchar(letter, COLOR_FG);
        screen_print(": not available\n", COLOR_FG);
        return;
    }

    g_drive = letter;
    str_copy(g_cwd, "/", 256);
    build_prompt();

    libcell_write(CONSOLE_DRIVE_CELL, &g_drive, 1);
}

/*=============================================================================
 * MEX LAUNCHING with stdout SHM pipe
 *
 * Now async: sets g_waiting_for_child = 1 and returns immediately.
 * The on_tick handler polls for child exit and prints output.
 *===========================================================================*/

static void try_run_mex(const char *command, const char *args) {
    /* Build .MEX filename (uppercase for ISO9660) */
    char mex_name[64];
    str_copy(mex_name, command, 56);
    str_toupper(mex_name);
    str_append(mex_name, ".MEX", 64);

    screen_print("\n", COLOR_FG);

    /* Read MEX file (cached if same command) */
    static uint8_t file_buf[65536];
    int bytes;

    if (str_equal(mex_name, g_last_mex_name) && g_last_mex_size > 0) {
        bytes = g_last_mex_size;
    } else {
        char qpath[280];
        build_drive_path(qpath, 280, g_cwd);
        bytes = libfs_read_file(qpath, mex_name, file_buf, sizeof(file_buf));
        if (bytes < 0) {
            build_drive_path(qpath, 280, "/");
            bytes = libfs_read_file(qpath, mex_name, file_buf, sizeof(file_buf));
        }

        if (bytes < 0) {
            screen_print("'", COLOR_FG);
            screen_print(command, COLOR_FG);
            screen_print("' is not a command or application.\n", COLOR_FG);
            screen_print("Type 'help' for terminal commands.\n", COLOR_FG);
            g_last_mex_name[0] = '\0';
            g_last_mex_size = 0;
            return;
        }

        str_copy(g_last_mex_name, mex_name, 32);
        g_last_mex_size = bytes;
    }

    /* Parse MEX header */
    mex_info_t info;
    int result = libmex_parse(file_buf, (uint32_t)bytes, &info);
    if (result != 0) {
        screen_print("Error: ", COLOR_ERROR);
        screen_print(libmex_error_string(result), COLOR_FG);
        screen_print("\n", COLOR_FG);
        return;
    }

    int is_gui_app = (info.flags & MEX_FLAG_GUI) ? 1 : 0;

    /* Setup stdout SHM for console apps */
    int stdout_shm_id = -1;
    console_stdout_buf_t *stdout_buf = (console_stdout_buf_t *)0;

    if (!is_gui_app) {
        stdout_shm_id = syscall1(SYS_SHM_CREATE, (int)sizeof(console_stdout_buf_t));
        if (stdout_shm_id > 0) {
            stdout_buf = (console_stdout_buf_t *)syscall2(SYS_SHM_ATTACH, stdout_shm_id, 0);
            if (stdout_buf) {
                stdout_buf->write_pos = 0;
                stdout_buf->max_size = CONSOLE_STDOUT_DATA_SIZE;
                for (int i = 0; i < CONSOLE_STDOUT_DATA_SIZE; i++)
                    stdout_buf->data[i] = 0;
                libcell_write(CONSOLE_STDOUT_CELL, &stdout_shm_id, sizeof(int));
            }
        }
    }

    /* Write args to cell */
    if (args && args[0]) {
        libcell_write(CONSOLE_ARGS_CELL, args, (uint32_t)str_len(args) + 1);
    } else {
        char empty = '\0';
        libcell_write(CONSOLE_ARGS_CELL, &empty, 1);
    }

    /* Launch process */
    int pid = libmex_exec(&info);
    if (pid < 0) {
        screen_print("Error: ", COLOR_ERROR);
        screen_print(libmex_error_string(pid), COLOR_FG);
        screen_print("\n", COLOR_FG);
        if (stdout_buf) {
            syscall1(SYS_SHM_DETACH, stdout_shm_id);
            syscall1(SYS_SHM_DESTROY, stdout_shm_id);
        }
        return;
    }

    /* Set process name */
    libprocess_set_name(pid, command, 1);

    if (is_gui_app) {
        /* GUI apps are fire-and-forget: they own their own window.
         * Don't wait — caller will show prompt since g_waiting_for_child=0 */
        screen_print("Launched.\n", COLOR_HEADING);
    } else {
        /* Console apps: track async, poll for exit in on_tick */
        g_waiting_for_child = 1;
        g_child_pid = pid;
        g_child_is_gui = 0;
        g_child_stdout_shm_id = stdout_shm_id;
        g_child_stdout_buf = stdout_buf;
        screen_print("Running...\n", COLOR_HEADING);
    }
}

/** Called by on_tick when child (console app) has exited */
static void finish_child_process(void) {
    if (g_child_stdout_buf) {
        /* Console app: read stdout buffer */
        for (uint32_t i = 0; i < g_child_stdout_buf->write_pos &&
                             i < g_child_stdout_buf->max_size; i++) {
            char c = g_child_stdout_buf->data[i];
            if (c == '\n')
                screen_putchar('\n', COLOR_FG);
            else if (c >= 32 && c < 127)
                screen_putchar(c, COLOR_FG);
            else if (c == '\t')
                screen_print("    ", COLOR_FG);
        }
    }

    /* Cleanup stdout SHM */
    if (g_child_stdout_shm_id > 0) {
        syscall1(SYS_SHM_DETACH, g_child_stdout_shm_id);
        syscall1(SYS_SHM_DESTROY, g_child_stdout_shm_id);
    }

    g_waiting_for_child = 0;
    g_child_pid = -1;
    g_child_is_gui = 0;
    g_child_stdout_shm_id = -1;
    g_child_stdout_buf = (console_stdout_buf_t *)0;

    /* Show new prompt */
    screen_print(g_prompt, COLOR_PROMPT);
    if (g_win) window_invalidate(g_win);
}

/*=============================================================================
 * COMMAND DISPATCHER
 *===========================================================================*/

static void process_command(void) {
    if (input_len == 0) return;

    /* Parse command + args */
    char command[64];
    const char *args = "";
    int ci = 0, i = 0;
    while (i < input_len && input_buf[i] != ' ' && ci < 63)
        command[ci++] = input_buf[i++];
    command[ci] = '\0';
    while (i < input_len && input_buf[i] == ' ') i++;
    if (i < input_len) args = &input_buf[i];

    /* Drive switch: "C:" or "D:" */
    if (ci == 2 && command[1] == ':' &&
        ((command[0] >= 'A' && command[0] <= 'Z') ||
         (command[0] >= 'a' && command[0] <= 'z'))) {
        screen_print("\n", COLOR_FG);
        cmd_drive_switch(command[0]);
        return;
    }

    /* Built-in commands (case-insensitive) */
    if (str_equal_nocase(command, "help")) {
        cmd_help();
    } else if (str_equal_nocase(command, "clear") ||
               str_equal_nocase(command, "cls")) {
        screen_clear();
        return;
    } else if (str_equal_nocase(command, "dir") ||
               str_equal_nocase(command, "ls")) {
        cmd_dir();
    } else if (str_equal_nocase(command, "cd")) {
        screen_print("\n", COLOR_FG);
        cmd_cd(args);
    } else if (str_equal_nocase(command, "type") ||
               str_equal_nocase(command, "cat")) {
        cmd_type(args);
    } else {
        /* Not a built-in: try to run as .mex app */
        try_run_mex(command, args);
        return;  /* Don't print prompt yet — wait for child */
    }
}

/*=============================================================================
 * WINDOW CALLBACKS
 *===========================================================================*/

/**
 * on_paint — render the screen buffer into the window's surface.
 * Called by window_draw() whenever a redraw is needed.
 */
static void term_on_paint(window_t *win, surface_t *surf, void *userdata) {
    (void)userdata;

    int cx = win->content_x + TERM_PAD;
    int cy = win->content_y + TERM_PAD;

    /* Fill terminal background (overwrite the white default with dark bg) */
    surface_fill_rect(surf, win->content_x, win->content_y,
                      win->content_w, win->content_h, COLOR_BG);

    /* Draw each cell from the screen buffer */
    for (int r = 0; r < g_rows; r++) {
        for (int c = 0; c < g_cols; c++) {
            char ch = g_screen[r][c].ch;
            if (ch > ' ' && ch < 127) {
                surface_draw_char(surf,
                    cx + c * THEME_FONT_WIDTH,
                    cy + r * THEME_FONT_HEIGHT,
                    ch,
                    g_screen[r][c].fg,
                    COLOR_BG);
            }
        }
    }

    /* Draw cursor block */
    if (g_cursor_visible && !g_waiting_for_child) {
        int cur_px = cx + g_cursor_col * THEME_FONT_WIDTH;
        int cur_py = cy + g_cursor_row * THEME_FONT_HEIGHT;
        surface_fill_rect(surf, cur_px, cur_py,
                          THEME_FONT_WIDTH, THEME_FONT_HEIGHT, COLOR_FG);
    }
}

/**
 * on_key — handle keyboard input for the terminal.
 * Called by window_run() for every KEY_PRESSED event.
 */
static void term_on_key(window_t *win, int scancode, char ascii,
                        void *userdata) {
    (void)userdata;

    /* Ignore keyboard when waiting for child process */
    if (g_waiting_for_child) return;

    if (scancode == 0x1C) {  /* Enter */
        input_buf[input_len] = '\0';
        screen_putchar('\n', COLOR_FG);

        process_command();

        /* If not waiting for async child, show prompt immediately */
        if (!g_waiting_for_child) {
            input_len = 0;
            input_buf[0] = '\0';
            screen_print(g_prompt, COLOR_PROMPT);
        } else {
            input_len = 0;
            input_buf[0] = '\0';
        }

    } else if (scancode == 0x0E) {  /* Backspace */
        if (input_len > 0) {
            input_len--;
            input_buf[input_len] = '\0';
            screen_backspace();
        }
    } else if (ascii >= 32 && ascii < 127) {
        if (input_len < INPUT_MAX - 1) {
            input_buf[input_len] = ascii;
            input_len++;
            input_buf[input_len] = '\0';
            screen_putchar(ascii, COLOR_FG);
        }
    }

    /* Reset cursor blink on keystroke */
    g_cursor_visible = 1;
    g_blink_counter = 0;

    window_invalidate(win);
}

/**
 * on_tick — per-frame callback for cursor blink and child process polling.
 */
static void term_on_tick(window_t *win, void *userdata) {
    (void)userdata;

    /* Check if child process has exited */
    if (g_waiting_for_child && g_child_pid >= 0) {
        process_info_t pinfo;
        if (libprocess_get_info(g_child_pid, &pinfo) != 0) {
            /* Process gone = exited */
            finish_child_process();
        }
    }

    /* Cursor blink */
    g_blink_counter++;
    if (g_blink_counter >= CURSOR_BLINK_INTERVAL) {
        g_blink_counter = 0;
        g_cursor_visible = !g_cursor_visible;
        window_invalidate(win);
    }
}

/*=============================================================================
 * MAIN ENTRY POINT
 *===========================================================================*/

void terminal_main(void) {
    liblog(LOG_INFO, "TERM", "========================================");
    liblog(LOG_INFO, "TERM", "  MaahiOS Terminal v4 (Windowed)");
    liblog(LOG_INFO, "TERM", "========================================");

    if (gui_init() != 0) {
        liblog(LOG_ERROR, "TERM", "Failed to init libgui, halting");
        while (1) syscall0(SYS_YIELD);
    }

    /* Center the window on screen */
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    int win_x = (scr_w - TERM_WIN_W) / 2;
    int win_y = (scr_h - TERM_WIN_H) / 2 - 16;  /* slightly above center */
    if (win_y < 0) win_y = 0;

    /* Create the window */
    window_t *win = window_create("Terminal", win_x, win_y,
                                  TERM_WIN_W, TERM_WIN_H);
    if (!win) {
        liblog(LOG_ERROR, "TERM", "Failed to create window, halting");
        while (1) syscall0(SYS_YIELD);
    }
    g_win = win;

    /* Compute text grid from content area */
    int text_w = win->content_w - 2 * TERM_PAD;
    int text_h = win->content_h - 2 * TERM_PAD;
    g_cols = text_w / THEME_FONT_WIDTH;
    g_rows = text_h / THEME_FONT_HEIGHT;
    if (g_cols > TERM_MAX_COLS) g_cols = TERM_MAX_COLS;
    if (g_rows > TERM_MAX_ROWS) g_rows = TERM_MAX_ROWS;
    if (g_cols < 10) g_cols = 10;
    if (g_rows < 4)  g_rows = 4;

    liblog_hex(LOG_INFO, "TERM", "Text grid cols:", (uint32_t)g_cols);
    liblog_hex(LOG_INFO, "TERM", "Text grid rows:", (uint32_t)g_rows);

    /* Initialize CWD state */
    g_drive = 'C';
    str_copy(g_cwd, "/", 256);
    build_prompt();

    /* Publish initial drive letter */
    libcell_write(CONSOLE_DRIVE_CELL, &g_drive, 1);

    /* Clear screen buffer */
    screen_clear();

    /* Welcome banner */
    screen_print(" MaahiOS Terminal\n", COLOR_HEADING);
    screen_print(" Type 'help' for commands. ", COLOR_FG);
    screen_print("Run apps by name (e.g., diskman help).\n\n", COLOR_FG);
    screen_print(g_prompt, COLOR_PROMPT);

    /* Set window callbacks */
    window_set_on_paint(win, term_on_paint, (void *)0);
    window_set_on_key(win, term_on_key, (void *)0);
    window_set_on_tick(win, term_on_tick, (void *)0);

    liblog(LOG_INFO, "TERM", "Terminal ready, entering window event loop");

    /* Enter the window event loop (blocks until close) */
    window_run(win);

    /* Cleanup */
    window_destroy(win);
    g_win = (window_t *)0;

    liblog(LOG_INFO, "TERM", "Terminal exited");

    /* Halt (terminal shouldn't normally exit) */
    while (1) syscall0(SYS_YIELD);
}
