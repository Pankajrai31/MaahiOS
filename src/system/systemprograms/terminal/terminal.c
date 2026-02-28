/**
 * MaahiOS Terminal
 * 
 * Description:
 *   Command-line terminal running in Ring 3.
 *   Pure consumer of library APIs — all display, font, keyboard,
 *   and console operations use libgui functions exclusively.
 * 
 *   Features:
 *   - Console widget via libgui (gui_console_*)
 *   - 8×16 VGA font via libgui (fonts/font8x16)
 *   - PS/2 keyboard input via libgui (kbd_read_event)
 *   - Simple prompt with echo + backspace + enter
 *   - Built-in commands: help, clear, dir, run, sysinfo, version
 *   - Console App framework: shutdown, restart, procman, diskman, fileman
 * 
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include <stdint.h>
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libfs/libfs.h"
#include "../../libraries/libmex/libmex.h"
#include "../../libraries/libprocess/libprocess.h"
#include "../../libraries/libgui/libgui.h"
#include "console_app.h"

/*=============================================================================
 * TERMINAL CONFIGURATION
 *===========================================================================*/

/* Terminal window area (centered black box on 1024x768) */
#define TERM_X                  112
#define TERM_Y                  84
#define TERM_W                  800
#define TERM_H                  600
#define TERM_PAD                8

/* Colors */
#define COLOR_BG                0x00000000   /* Black */
#define COLOR_FG                0x00CCCCCC   /* Light gray */
#define COLOR_PROMPT            0x0000CC00   /* Green */
#define COLOR_BORDER            0x00444444   /* Dark gray border */
#define COLOR_HEADING           0x0000AAFF   /* Blue headings */
#define COLOR_ERROR             0x00FF4444   /* Red errors */
#define COLOR_SUCCESS           0x0000CC00   /* Green success */
#define COLOR_DIR               0x00FFCC00   /* Yellow directories */

/*=============================================================================
 * GLOBAL STATE
 *===========================================================================*/

/* Console widget - all text I/O goes through this */
static gui_console_t g_con;

/* Input line buffer */
#define INPUT_MAX       256
static char input_buf[INPUT_MAX];
static int input_len = 0;

/* Prompt */
static const char *PROMPT = "MaahiOS> ";

/*=============================================================================
 * CONSOLE APP REGISTRY
 *===========================================================================*/

/* Extern app definitions (compiled separately, linked in) */
extern console_app_t app_shutdown;
extern console_app_t app_restart;
extern console_app_t app_procman;
extern console_app_t app_diskman;
extern console_app_t app_fileman;

/* App registry — null-terminated */
static console_app_t *all_apps[] = {
    &app_shutdown,
    &app_restart,
    &app_procman,
    &app_diskman,
    &app_fileman,
    0
};

/* Currently active console app (NULL = normal terminal mode) */
static console_app_t *g_active_app = 0;

/*=============================================================================
 * STRING HELPERS (minimal, no drawing)
 *===========================================================================*/

static void yield(void) {
    syscall0(SYS_YIELD);
}

static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

static int str_starts_with(const char *str, const char *prefix) {
    while (*prefix) {
        if (*str != *prefix) return 0;
        str++; prefix++;
    }
    return 1;
}

/*=============================================================================
 * BUILT-IN COMMANDS
 *===========================================================================*/

static console_app_t *find_app(const char *name) {
    for (int i = 0; all_apps[i]; i++) {
        if (str_equal(name, all_apps[i]->name)) {
            return all_apps[i];
        }
    }
    return 0;
}

static void cmd_help(void) {
    gui_console_print(&g_con, "\n");
    gui_console_print_color(&g_con, "MaahiOS Terminal - Built-in Commands:\n",
                            COLOR_HEADING);
    gui_console_print(&g_con, "  help     - Show this help message\n");
    gui_console_print(&g_con, "  clear    - Clear the terminal\n");
    gui_console_print(&g_con, "  dir      - List files in root directory\n");
    gui_console_print(&g_con, "  run <f>  - Run a .mex application\n");
    gui_console_print(&g_con, "  sysinfo  - Show system information\n");
    gui_console_print(&g_con, "  version  - Show OS version\n");
    gui_console_print(&g_con, "\n");
    gui_console_print_color(&g_con, "Console Apps (type name to launch):\n",
                            COLOR_HEADING);
    for (int i = 0; all_apps[i]; i++) {
        gui_console_print(&g_con, "  ");
        gui_console_print_color(&g_con, all_apps[i]->name, COLOR_SUCCESS);
        /* Pad to 10 chars */
        int len = 0;
        const char *n = all_apps[i]->name;
        while (n[len]) len++;
        for (int p = len; p < 10; p++) gui_console_print(&g_con, " ");
        gui_console_print(&g_con, "- ");
        gui_console_print(&g_con, all_apps[i]->description);
        gui_console_print(&g_con, "\n");
    }
}

static void cmd_sysinfo(void) {
    gui_console_print(&g_con, "\n");
    gui_console_print_color(&g_con, "System Information:\n", COLOR_HEADING);
    gui_console_print(&g_con, "  OS:       MaahiOS\n");
    gui_console_print(&g_con, "  Arch:     x86 (i686)\n");
    gui_console_print(&g_con, "  Display:  1024x768 32bpp\n");

    int count = libprocess_get_count();
    gui_console_print(&g_con, "  Running:  ");
    gui_console_print_int(&g_con, count);
    gui_console_print(&g_con, " processes\n");

    int pid = syscall0(SYS_GETPID);
    gui_console_print(&g_con, "  Terminal: PID ");
    gui_console_print_int(&g_con, pid);
    gui_console_print(&g_con, "\n");
}

static void cmd_version(void) {
    gui_console_print(&g_con, "\n");
    gui_console_print_color(&g_con, "MaahiOS v0.1.0\n", COLOR_HEADING);
    gui_console_print(&g_con, "  Built with love, from scratch.\n");
}

static void cmd_dir(void) {
    gui_console_print(&g_con, "\n");
    gui_console_print_color(&g_con, "Directory listing: /\n", COLOR_HEADING);
    gui_console_print(&g_con, "\n");

    fs_file_entry_t entries[32];
    int count = libfs_list_dir("/", entries, 32);

    if (count < 0) {
        gui_console_print(&g_con, "  Error: could not read directory\n");
        return;
    }

    if (count == 0) {
        gui_console_print(&g_con, "  (empty)\n");
        return;
    }

    /* Print header */
    gui_console_print(&g_con, "  Type     Size       Name\n");
    gui_console_print(&g_con, "  ----     ----       ----\n");

    for (int i = 0; i < count; i++) {
        gui_console_print(&g_con, "  ");

        /* Type indicator */
        if (entries[i].is_directory) {
            gui_console_print_color(&g_con, "<DIR>  ", COLOR_DIR);
        } else {
            gui_console_print(&g_con, "<FILE> ");
        }

        /* Size: right-align in 10 chars */
        if (entries[i].is_directory) {
            gui_console_print(&g_con, "           ");
        } else {
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
            gui_console_print(&g_con, size_str);
        }

        /* Filename */
        gui_console_print(&g_con, entries[i].name);
        gui_console_print(&g_con, "\n");
    }

    /* Summary */
    gui_console_print(&g_con, "\n  ");
    gui_console_print_int(&g_con, count);
    gui_console_print(&g_con, " item(s)\n");
}

static void cmd_run(const char *filename) {
    /* Skip leading spaces */
    while (*filename == ' ') filename++;

    if (*filename == '\0') {
        gui_console_print(&g_con, "\nUsage: run <filename.mex>\n");
        return;
    }

    gui_console_print(&g_con, "\nLoading ");
    gui_console_print(&g_con, filename);
    gui_console_print(&g_con, "...\n");

    /* Read file from root directory via FS Executive */
    static uint8_t file_buf[65536];  /* 64KB buffer for .mex file */
    int bytes = libfs_read_file("/", filename, file_buf, sizeof(file_buf));
    if (bytes < 0) {
        gui_console_print_color(&g_con, "Error: ", COLOR_ERROR);
        gui_console_print(&g_con, "Could not read file '");
        gui_console_print(&g_con, filename);
        gui_console_print(&g_con, "'\n");
        return;
    }

    gui_console_print(&g_con, "Read ");
    gui_console_print_int(&g_con, bytes);
    gui_console_print(&g_con, " bytes. Parsing MEX header...\n");

    /* Parse MEX header */
    mex_info_t info;
    int result = libmex_parse(file_buf, (uint32_t)bytes, &info);
    if (result != 0) {
        gui_console_print_color(&g_con, "Error: ", COLOR_ERROR);
        gui_console_print(&g_con, libmex_error_string(result));
        gui_console_print(&g_con, "\n");
        return;
    }

    /* Execute via Process Executive */
    int pid = libmex_exec(&info);
    if (pid < 0) {
        gui_console_print_color(&g_con, "Error: ", COLOR_ERROR);
        gui_console_print(&g_con, libmex_error_string(pid));
        gui_console_print(&g_con, "\n");
        return;
    }

    /* Success */
    gui_console_print_color(&g_con, "Started '", COLOR_SUCCESS);
    gui_console_print_color(&g_con, info.name, COLOR_SUCCESS);
    gui_console_print_color(&g_con, "' as PID ", COLOR_SUCCESS);
    gui_console_print_int(&g_con, pid);
    gui_console_print(&g_con, "\n");
}

/*=============================================================================
 * COMMAND DISPATCHER
 *===========================================================================*/

static void process_command(void) {
    if (input_len == 0) return;

    /* ── App Mode: forward commands to active app ── */
    if (g_active_app) {
        if (str_equal(input_buf, "exit")) {
            /* Close the app */
            gui_console_print(&g_con, "\n");
            g_active_app->cleanup(&g_con);
            gui_console_print(&g_con, "Exiting ");
            gui_console_print(&g_con, g_active_app->name);
            gui_console_print(&g_con, ".\n");
            g_active_app = 0;
        } else {
            g_active_app->handle_command(&g_con, input_buf);
        }
        return;
    }

    /* ── Normal Terminal Mode ── */
    if (str_equal(input_buf, "help")) {
        cmd_help();
    } else if (str_equal(input_buf, "clear")) {
        gui_console_clear(&g_con);
        return;   /* Don't print newline before prompt */
    } else if (str_equal(input_buf, "dir")) {
        cmd_dir();
    } else if (str_starts_with(input_buf, "run ")) {
        cmd_run(input_buf + 4);
    } else if (str_equal(input_buf, "sysinfo")) {
        cmd_sysinfo();
    } else if (str_equal(input_buf, "version")) {
        cmd_version();
    } else {
        /* Check if it's a console app name */
        console_app_t *app = find_app(input_buf);
        if (app) {
            g_active_app = app;
            app->init(&g_con);
            return;
        }
        
        gui_console_print(&g_con, "\n");
        gui_console_print(&g_con, "Unknown command: ");
        gui_console_print(&g_con, input_buf);
        gui_console_print(&g_con, "\n");
        gui_console_print(&g_con, "Type 'help' for available commands.\n");
    }
}

/*=============================================================================
 * MAIN ENTRY POINT
 *===========================================================================*/

void terminal_main(void) {
    liblog(LOG_INFO, "TERM", "========================================");
    liblog(LOG_INFO, "TERM", "  MaahiOS Terminal Starting");
    liblog(LOG_INFO, "TERM", "========================================");

    /* Initialize GUI library (reads framebuffer from GUI Executive cells) */
    if (gui_init() != 0) {
        liblog(LOG_ERROR, "TERM", "Failed to init libgui, halting");
        while (1) yield();
    }
    liblog_hex(LOG_INFO, "TERM", "Framebuffer at:",
               (uint32_t)gui_get_framebuffer());

    /* Create and draw the console widget */
    gui_console_init(&g_con, TERM_X, TERM_Y, TERM_W, TERM_H, TERM_PAD,
                     COLOR_BG, COLOR_FG, COLOR_BORDER);
    gui_console_draw_window(&g_con);

    /* Welcome message */
    gui_console_print_color(&g_con, "MaahiOS Terminal v0.1\n", COLOR_HEADING);
    gui_console_print(&g_con, "Type 'help' for available commands.\n\n");

    /* Initial prompt */
    gui_console_print_color(&g_con, PROMPT, COLOR_PROMPT);
    gui_console_draw_cursor(&g_con);

    liblog(LOG_INFO, "TERM", "Terminal ready, entering input loop");

    /* Main input loop */
    key_event_t evt;
    int tick_count = 0;

    while (1) {
        int result = kbd_read_event(&evt);

        if (result > 0 && evt.type == KEY_PRESSED) {
            /* Clear cursor at old position */
            if (g_con.cursor_visible) {
                gui_console_erase_cursor(&g_con);
            }

            if (evt.scancode == SC_ENTER) {
                gui_console_erase_cursor(&g_con);
                input_buf[input_len] = '\0';
                gui_console_putchar(&g_con, '\n');

                process_command();

                /* Reset input buffer and show new prompt */
                input_len = 0;
                input_buf[0] = '\0';
                
                /* Show appropriate prompt */
                if (g_active_app) {
                    gui_console_print_color(&g_con, g_active_app->name, COLOR_PROMPT);
                    gui_console_print_color(&g_con, "> ", COLOR_PROMPT);
                } else {
                    gui_console_print_color(&g_con, PROMPT, COLOR_PROMPT);
                }

            } else if (evt.scancode == SC_BACKSPACE) {
                if (input_len > 0) {
                    input_len--;
                    input_buf[input_len] = '\0';
                    gui_console_backspace(&g_con);
                }

            } else if (evt.ascii >= 32 && evt.ascii < 127) {
                if (input_len < INPUT_MAX - 1) {
                    input_buf[input_len] = (char)evt.ascii;
                    input_len++;
                    input_buf[input_len] = '\0';
                    gui_console_putchar(&g_con, (char)evt.ascii);
                }
            }

            /* Redraw cursor at new position */
            gui_console_draw_cursor(&g_con);
            tick_count = 0;
        }

        /* Simple cursor blink (toggle every ~30 iterations) */
        tick_count++;
        if (tick_count >= 30) {
            tick_count = 0;
            if (g_con.cursor_visible) {
                gui_console_erase_cursor(&g_con);
            } else {
                gui_console_draw_cursor(&g_con);
            }
        }

        yield();
    }
}
