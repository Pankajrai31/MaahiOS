/**
 * MaahiOS WordWrite - Minimalist Text Editor
 *
 * Description:
 *   GUI windowed text editor with:
 *     - MenuBar: File → New, Open, Save
 *     - TextArea: multi-line text editing (8×16 bitmap font)
 *     - StatusBar: cursor position + modified indicator
 *     - File Open dialog: TreeView (drives/folders) + Table (files)
 *
 *   Can read files from CD-ROM (ISO9660) and MFS volumes.
 *   Supports Save to MFS volumes (ISO9660 is read-only).
 *
 * Layout:
 *   ┌─ WordWrite - filename ──────────────────────────┐
 *   │ File                                    menubar  │
 *   ├──────────────────────────────────────────────────┤
 *   │                                                  │
 *   │  Text editing area                               │
 *   │                                                  │
 *   ├──────────────────────────────────────────────────┤
 *   │ Ln 1, Col 1          │ Ready          statusbar  │
 *   └──────────────────────────────────────────────────┘
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libfs/libfs.h"
#include "../../system/libraries/liblog/liblog.h"

/*=============================================================================
 * APPLICATION CONSTANTS
 *===========================================================================*/

#define WIN_W           640
#define WIN_H           480
#define WIN_X           180
#define WIN_Y           60

#define MAX_FILENAME    44
#define MAX_DIRPATH     48

/*=============================================================================
 * APPLICATION STATE
 *===========================================================================*/

static window_t    *g_win;
static menubar_t   *g_menubar;
static textarea_t  *g_textarea;
static statusbar_t *g_statusbar;

/* Current file info */
static char  g_dir_path[MAX_DIRPATH];    /* "C:/" or "D:/docs" */
static char  g_filename[MAX_FILENAME];   /* "readme.txt" */
static int   g_file_loaded;              /* 1 = file is from disk */

/* File read buffer (shared between open dialog and main app) */
static uint8_t g_file_buf[TEXTAREA_MAX_TEXT];

/*=============================================================================
 * HELPER: STRING UTILITIES
 *===========================================================================*/

static int ww_strlen(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void ww_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static void ww_strcat(char *dst, const char *src, int max) {
    int dlen = ww_strlen(dst);
    int i;
    for (i = 0; i < max - dlen - 1 && src && src[i]; i++)
        dst[dlen + i] = src[i];
    dst[dlen + i] = '\0';
}

static int ww_strcmp(const char *a, const char *b) {
    while (*a && *b && *a == *b) { a++; b++; }
    return (unsigned char)*a - (unsigned char)*b;
}

static void ww_int_to_str(int val, char *buf, int bufsz) {
    if (bufsz < 2) { buf[0] = '\0'; return; }
    if (val < 0) { buf[0] = '-'; ww_int_to_str(-val, buf + 1, bufsz - 1); return; }
    char tmp[12];
    int i = 0;
    do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val > 0 && i < 11);
    int j;
    for (j = 0; j < i && j < bufsz - 1; j++) buf[j] = tmp[i - 1 - j];
    buf[j] = '\0';
}

/*=============================================================================
 * UI UPDATES
 *===========================================================================*/

static void update_title(void) {
    char title[WINDOW_MAX_TITLE];
    ww_strcpy(title, "WordWrite - ", WINDOW_MAX_TITLE);
    if (g_file_loaded) {
        ww_strcat(title, g_filename, WINDOW_MAX_TITLE);
    } else {
        ww_strcat(title, "Untitled", WINDOW_MAX_TITLE);
    }
    ww_strcpy(g_win->title, title, WINDOW_MAX_TITLE);
    g_win->needs_redraw = 1;
}

static void update_statusbar(void) {
    char pos_buf[32];
    char ln_str[12], col_str[12];
    ww_int_to_str(textarea_get_cursor_line(g_textarea) + 1, ln_str, 12);
    ww_int_to_str(textarea_get_cursor_col(g_textarea) + 1, col_str, 12);

    ww_strcpy(pos_buf, "Ln ", 32);
    ww_strcat(pos_buf, ln_str, 32);
    ww_strcat(pos_buf, ", Col ", 32);
    ww_strcat(pos_buf, col_str, 32);
    statusbar_set_text(g_statusbar, 0, pos_buf);

    if (textarea_is_modified(g_textarea)) {
        statusbar_set_text(g_statusbar, 1, "Modified");
    } else {
        statusbar_set_text(g_statusbar, 1, "Ready");
    }
}

/*=============================================================================
 * FILE OPEN DIALOG
 *
 * Opens a modal dialog with:
 *   Left:   TreeView (drives + expandable folders)
 *   Right:  Table (files in selected folder)
 *   Bottom: Cancel / Open buttons
 *
 * Returns 1 if file selected (fills g_dir_path, g_filename), 0 if canceled.
 *===========================================================================*/

/* Dialog dimensions */
#define FD_WIN_W    520
#define FD_WIN_H    380
#define FD_TREE_X     6
#define FD_TREE_Y     6
#define FD_TREE_W   180
#define FD_TREE_H   290
#define FD_TABLE_X  192
#define FD_TABLE_Y    6
#define FD_TABLE_W  310
#define FD_TABLE_H  290

/* Dialog state */
static window_t   *g_fd_win;
static treeview_t  *g_fd_tree;
static table_t     *g_fd_table;
static int          g_fd_result;        /* 0=cancel, 1=file selected */

/* Per-node path storage (indexed by tree node index) */
static char g_fd_paths[TREEVIEW_MAX_NODES][MAX_DIRPATH];

/* Selected dir path and filename in dialog */
static char g_fd_sel_dir[MAX_DIRPATH];
static char g_fd_sel_file[MAX_FILENAME];

/* File entries for the currently displayed directory */
static fs_file_entry_t g_fd_entries[32];
static int g_fd_entry_count;

/** Populate the right table with files from a directory path */
static void fd_populate_table(const char *path) {
    table_clear(g_fd_table);
    ww_strcpy(g_fd_sel_dir, path, MAX_DIRPATH);
    g_fd_sel_file[0] = '\0';

    g_fd_entry_count = libfs_list_dir(path, g_fd_entries, 32);
    if (g_fd_entry_count < 0) g_fd_entry_count = 0;

    /* Filter: show only files (not directories) */
    int file_count = 0;
    int i;
    for (i = 0; i < g_fd_entry_count && file_count < TABLE_MAX_ROWS; i++) {
        if (g_fd_entries[i].is_directory) continue;

        /* Skip . and .. */
        if (g_fd_entries[i].name[0] == '.') continue;

        table_set_cell(g_fd_table, file_count, 0, g_fd_entries[i].name);

        char size_str[16];
        if (g_fd_entries[i].size >= 1024) {
            ww_int_to_str((int)(g_fd_entries[i].size / 1024), size_str, 12);
            ww_strcat(size_str, " KB", 16);
        } else {
            ww_int_to_str((int)g_fd_entries[i].size, size_str, 12);
            ww_strcat(size_str, " B", 16);
        }
        table_set_cell(g_fd_table, file_count, 1, size_str);

        file_count++;
    }
    table_set_row_count(g_fd_table, file_count);
    g_fd_table->base.dirty = 1;
}

/** Tree node selected → show files from that directory */
static void fd_on_tree_select(int node_index, void *userdata) {
    (void)userdata;
    if (node_index < 0 || node_index >= TREEVIEW_MAX_NODES) return;
    fd_populate_table(g_fd_paths[node_index]);
}

/** Tree node expanded → lazy-load directory children */
static void fd_on_tree_expand(int node_index, void *userdata) {
    (void)userdata;
    if (node_index < 0 || node_index >= TREEVIEW_MAX_NODES) return;

    treeview_node_t *node = &g_fd_tree->nodes[node_index];

    /* Only load children once */
    if (node->child_count > 0) return;

    const char *path = g_fd_paths[node_index];
    fs_file_entry_t entries[32];
    int count = libfs_list_dir(path, entries, 32);
    if (count <= 0) return;

    int i;
    for (i = 0; i < count; i++) {
        if (!entries[i].is_directory) continue;
        /* Skip . and .. */
        if (entries[i].name[0] == '.') continue;

        int child = treeview_add_node(g_fd_tree, entries[i].name,
                                      node_index, 0 /* not leaf */);
        if (child >= 0) {
            /* Build child path: parent_path + "/" + dirname */
            ww_strcpy(g_fd_paths[child], path, MAX_DIRPATH);
            int plen = ww_strlen(g_fd_paths[child]);
            /* Ensure trailing slash if not present */
            if (plen > 0 && g_fd_paths[child][plen - 1] != '/') {
                ww_strcat(g_fd_paths[child], "/", MAX_DIRPATH);
            }
            ww_strcat(g_fd_paths[child], entries[i].name, MAX_DIRPATH);
        }
    }
}

/** Table row clicked → select a file */
static void fd_on_table_click(int row, void *userdata) {
    (void)userdata;
    /* Find the actual filename from the table cell */
    if (row < 0 || row >= TABLE_MAX_ROWS) return;
    /* The cell content is the filename */
    ww_strcpy(g_fd_sel_file, g_fd_table->cells[row][0], MAX_FILENAME);
}

/** Open button clicked */
static void fd_on_open(void *userdata) {
    (void)userdata;
    if (g_fd_sel_file[0] != '\0') {
        g_fd_result = 1;
        window_close(g_fd_win);
    }
}

/** Cancel button clicked */
static void fd_on_cancel(void *userdata) {
    (void)userdata;
    g_fd_result = 0;
    window_close(g_fd_win);
}

/** Show the file open dialog (modal). Returns 1 if selected, 0 if canceled. */
static int file_dialog_open(void) {
    g_fd_result = 0;
    g_fd_sel_dir[0] = '\0';
    g_fd_sel_file[0] = '\0';

    /* Center dialog on screen */
    int dx = (1280 - FD_WIN_W) / 2;
    int dy = (800 - FD_WIN_H) / 2;

    g_fd_win = window_create("Open File", dx, dy, FD_WIN_W, FD_WIN_H);
    if (!g_fd_win) return 0;
    g_fd_win->flags |= WIN_FLAG_NO_MAXIMIZE | WIN_FLAG_NO_MINIMIZE;

    /* Compute content dimensions */
    int cw = g_fd_win->content_w;
    int ch = g_fd_win->content_h;

    /* Create tree view (left panel) */
    g_fd_tree = treeview_create(FD_TREE_X, FD_TREE_Y, FD_TREE_W, FD_TREE_H);
    if (!g_fd_tree) { window_destroy(g_fd_win); return 0; }
    treeview_set_on_select(g_fd_tree, fd_on_tree_select, (void *)0);
    treeview_set_on_expand(g_fd_tree, fd_on_tree_expand, (void *)0);

    /* Create table (right panel) */
    g_fd_table = table_create(FD_TABLE_X, FD_TABLE_Y, cw - FD_TABLE_X - 6,
                              FD_TABLE_H);
    if (!g_fd_table) { treeview_destroy(g_fd_tree); window_destroy(g_fd_win); return 0; }
    table_add_column(g_fd_table, "Name", 200, TABLE_ALIGN_LEFT);
    table_add_column(g_fd_table, "Size", 90, TABLE_ALIGN_RIGHT);
    table_set_on_row_click(g_fd_table, fd_on_table_click, (void *)0);

    /* Buttons at bottom */
    int btn_y = ch - 32;
    button_t *btn_cancel = button_create(cw - 170, btn_y, 75, 0,
                                         "Cancel", BTN_STANDARD);
    button_t *btn_open = button_create(cw - 88, btn_y, 75, 0,
                                       "Open", BTN_ACCENT);
    if (btn_cancel) button_set_on_click(btn_cancel, fd_on_cancel, (void *)0);
    if (btn_open)   button_set_on_click(btn_open, fd_on_open, (void *)0);

    /* Add controls (order: tree, table, buttons | menubar not needed here) */
    window_add_control(g_fd_win, &g_fd_tree->base);
    window_add_control(g_fd_win, &g_fd_table->base);
    if (btn_cancel) window_add_control(g_fd_win, &btn_cancel->base);
    if (btn_open)   window_add_control(g_fd_win, &btn_open->base);

    /* Populate tree with mounted volumes */
    int vol_count = libfs_vol_count();
    int v;
    for (v = 0; v < vol_count; v++) {
        libfs_vol_info_t vi;
        if (libfs_vol_info(v, &vi) != 0) continue;
        if (!vi.mounted) continue;

        /* Build label: "C: (CD-ROM)" */
        char label[TREEVIEW_MAX_LABEL];
        label[0] = vi.drive_letter;
        label[1] = ':';
        label[2] = ' ';
        label[3] = '(';
        ww_strcpy(&label[4], vi.label[0] ? vi.label : vi.fs_str,
                  TREEVIEW_MAX_LABEL - 5);
        ww_strcat(label, ")", TREEVIEW_MAX_LABEL);

        int node = treeview_add_node(g_fd_tree, label, -1, 0);
        if (node >= 0) {
            /* Path for volume root: "C:/" */
            g_fd_paths[node][0] = vi.drive_letter;
            g_fd_paths[node][1] = ':';
            g_fd_paths[node][2] = '/';
            g_fd_paths[node][3] = '\0';
        }
    }

    /* If there are volumes, select and expand the first one */
    if (g_fd_tree->node_count > 0) {
        treeview_set_selected(g_fd_tree, 0);
        treeview_set_expanded(g_fd_tree, 0, 1);
        fd_on_tree_expand(0, (void *)0);
        fd_populate_table(g_fd_paths[0]);
    }

    /* Run dialog event loop (blocks until close) */
    window_run(g_fd_win);
    window_destroy(g_fd_win);
    g_fd_win = (window_t *)0;

    /* Copy result to global state */
    if (g_fd_result) {
        ww_strcpy(g_dir_path, g_fd_sel_dir, MAX_DIRPATH);
        ww_strcpy(g_filename, g_fd_sel_file, MAX_FILENAME);
    }

    return g_fd_result;
}

/*=============================================================================
 * ERROR DIALOG (simple modal window with message + OK button)
 *===========================================================================*/

static void err_dlg_ok(void *userdata) {
    /* userdata is the error dialog window pointer — close it */
    window_t *ew = (window_t *)userdata;
    if (ew) window_close(ew);
}

/** Show a blocking error dialog with title and message. */
static void show_error_dialog(const char *title, const char *message) {
    int dw = 340;
    int dh = 150;
    int dx = (1280 - dw) / 2;
    int dy = (800 - dh) / 2;

    window_t *ew = window_create(title, dx, dy, dw, dh);
    if (!ew) return;
    ew->flags |= WIN_FLAG_NO_MAXIMIZE | WIN_FLAG_NO_MINIMIZE;

    int cw = ew->content_w;
    int ch = ew->content_h;

    /* Message label */
    label_t *lbl = label_create(12, 12, message, THEME_TEXT);
    if (lbl) window_add_control(ew, &lbl->base);

    /* OK button centered at bottom — passes window as userdata */
    button_t *btn = button_create((cw - 75) / 2, ch - 34, 75, 0,
                                  "OK", BTN_ACCENT);
    if (btn) {
        button_set_on_click(btn, err_dlg_ok, (void *)ew);
        window_add_control(ew, &btn->base);
    }

    window_run(ew);
    window_destroy(ew);
}

/** Check if the given drive letter is on a writable filesystem (MFS). */
static int is_drive_writable(char drive_letter) {
    int vol_count = libfs_vol_count();
    int i;
    for (i = 0; i < vol_count; i++) {
        libfs_vol_info_t vi;
        if (libfs_vol_info(i, &vi) != 0) continue;
        if (!vi.mounted) continue;
        if (vi.drive_letter == drive_letter) {
            /* fs_type 1 = ISO9660 (read-only), 2 = MFS (read-write) */
            return (vi.fs_type == 2) ? 1 : 0;
        }
    }
    return 0;  /* Unknown volume = not writable */
}

/*=============================================================================
 * MENU CALLBACKS
 *===========================================================================*/

static void on_menu_new(void *userdata) {
    (void)userdata;
    textarea_set_text(g_textarea, "");
    g_file_loaded = 0;
    g_dir_path[0] = '\0';
    g_filename[0] = '\0';
    update_title();
    update_statusbar();
    g_win->needs_redraw = 1;
}

static void on_menu_open(void *userdata) {
    (void)userdata;

    if (!file_dialog_open()) {
        /* User canceled — redraw main window to restore it */
        g_win->needs_redraw = 1;
        return;
    }

    /* Read the selected file */
    int bytes = libfs_read_file(g_dir_path, g_filename,
                                g_file_buf, sizeof(g_file_buf) - 1);
    if (bytes < 0) {
        char msg[DIALOG_MAX_MESSAGE];
        ww_strcpy(msg, "Could not read file: ", DIALOG_MAX_MESSAGE);
        ww_strcat(msg, g_filename, DIALOG_MAX_MESSAGE);
        show_error_dialog("Open Error", msg);
        g_file_loaded = 0;
        g_filename[0] = '\0';
        update_title();
        update_statusbar();
        g_win->needs_redraw = 1;
        return;
    }
    g_file_buf[bytes] = '\0';

    /* Load into textarea */
    textarea_set_text(g_textarea, (const char *)g_file_buf);
    g_file_loaded = 1;
    update_title();
    update_statusbar();
    g_win->needs_redraw = 1;
}

static void on_menu_save(void *userdata) {
    (void)userdata;

    if (!g_file_loaded || g_filename[0] == '\0') {
        show_error_dialog("Save Error",
            "No file is currently open. Use\nFile > Open to load a file first.");
        g_win->needs_redraw = 1;
        return;
    }

    /* Check if the target drive supports writing */
    char drive = g_dir_path[0];
    if (!is_drive_writable(drive)) {
        char msg[DIALOG_MAX_MESSAGE];
        ww_strcpy(msg, "Cannot save to drive ", DIALOG_MAX_MESSAGE);
        {
            char dl[3];
            dl[0] = drive;
            dl[1] = ':';
            dl[2] = '\0';
            ww_strcat(msg, dl, DIALOG_MAX_MESSAGE);
        }
        ww_strcat(msg, "\nThis drive is read-only (ISO9660).", DIALOG_MAX_MESSAGE);
        show_error_dialog("Read-Only Drive", msg);
        g_win->needs_redraw = 1;
        return;
    }

    const char *text = textarea_get_text(g_textarea);
    int len = textarea_get_text_len(g_textarea);

    int result = libfs_write_file(g_dir_path, g_filename,
                                  text, (uint32_t)len);
    if (result == 0) {
        textarea_clear_modified(g_textarea);
        statusbar_set_text(g_statusbar, 1, "Saved");
    } else {
        char msg[DIALOG_MAX_MESSAGE];
        ww_strcpy(msg, "Failed to save file: ", DIALOG_MAX_MESSAGE);
        ww_strcat(msg, g_filename, DIALOG_MAX_MESSAGE);
        ww_strcat(msg, "\nError code: ", DIALOG_MAX_MESSAGE);
        {
            char ec[12];
            ww_int_to_str(result, ec, 12);
            ww_strcat(msg, ec, DIALOG_MAX_MESSAGE);
        }
        show_error_dialog("Save Error", msg);
    }
    g_win->needs_redraw = 1;
}

/*=============================================================================
 * WINDOW CALLBACKS
 *===========================================================================*/

static void on_key(window_t *win, int scancode, char ascii, void *userdata) {
    (void)win;
    (void)userdata;

    /* ESC closes the app */
    if (scancode == 0x01) {
        window_close(g_win);
        return;
    }

    /* Forward all other keys to the textarea */
    textarea_handle_key(g_textarea, scancode, ascii);
    update_statusbar();
}

static void on_tick(window_t *win, void *userdata) {
    (void)win;
    (void)userdata;

    /* Advance cursor blink */
    textarea_tick(g_textarea);
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    /* Initialize state */
    g_file_loaded = 0;
    g_dir_path[0] = '\0';
    g_filename[0] = '\0';

    /* Create main window */
    g_win = window_create("WordWrite - Untitled", WIN_X, WIN_Y, WIN_W, WIN_H);
    if (!g_win) return;

    /* Get content area dimensions */
    int cw = g_win->content_w;
    int ch = g_win->content_h;

    /* ---- Create TextArea (main editing area) ---- */
    int ta_y = MENUBAR_HEIGHT + 2;
    int ta_h = ch - ta_y - STATUSBAR_HEIGHT;
    g_textarea = textarea_create(0, ta_y, cw, ta_h);
    if (!g_textarea) {
        window_destroy(g_win);
        return;
    }

    /* ---- Create StatusBar (bottom) ---- */
    g_statusbar = statusbar_create(0, ch - STATUSBAR_HEIGHT, cw);
    if (!g_statusbar) {
        textarea_destroy(g_textarea);
        window_destroy(g_win);
        return;
    }
    statusbar_add_panel(g_statusbar, "Ln 1, Col 1", 120);
    statusbar_add_panel(g_statusbar, "Ready", 0);  /* 0 = fill remaining */

    /* ---- Create MenuBar (top of content) ---- */
    g_menubar = menubar_create(0, 0, cw);
    if (!g_menubar) {
        statusbar_destroy(g_statusbar);
        textarea_destroy(g_textarea);
        window_destroy(g_win);
        return;
    }

    /* File menu */
    int file_menu = menubar_add_menu(g_menubar, "File");
    menubar_add_item(g_menubar, file_menu, "New", on_menu_new, (void *)0);
    menubar_add_item(g_menubar, file_menu, "Open...", on_menu_open, (void *)0);
    menubar_add_separator(g_menubar, file_menu);
    menubar_add_item(g_menubar, file_menu, "Save", on_menu_save, (void *)0);

    /* ---- Add controls to window ----
     * Order matters for z-order:
     *   - TextArea first (lowest z)
     *   - StatusBar next
     *   - MenuBar LAST (highest z, gets dropdown event priority)
     */
    window_add_control(g_win, &g_textarea->base);
    window_add_control(g_win, &g_statusbar->base);
    window_add_control(g_win, &g_menubar->base);

    /* Set callbacks */
    window_set_on_key(g_win, on_key, (void *)0);
    window_set_on_tick(g_win, on_tick, (void *)0);

    /* Enter event loop */
    window_run(g_win);

    /* Cleanup */
    window_destroy(g_win);
}
