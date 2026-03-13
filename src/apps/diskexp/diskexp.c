/**
 * diskexp.mex - MaahiOS Disk Explorer (Design System v2)
 *
 * Description:
 *   Windowed GUI app showing disk and volume information.
 *   Top section: volume table (Drive, Label, FS, Size, Status)
 *   Bottom section: disk table (Disk, Type, Status, Size, Sectors)
 *   Toolbar with Refresh button.
 *   Dark statusbar with disk/volume summary.
 *
 *   Layout:
 *     ┌─ Titlebar ──────────────────────────────────────────┐
 *     │ [Refresh]              toolbar (chrome)             │
 *     ├─────────────────────────────────────────────────────┤
 *     │ Volumes:                                            │
 *     │ Drive │ Label       │ FS     │ Size  │ Status      │
 *     │  C:   │ MaahiOS     │ ISO9660│ 8 MB  │ Mounted     │
 *     ├─────────────────────────────────────────────────────┤
 *     │ Disks:                                              │
 *     │ Disk  │ Type    │ Status │ Size   │ Sector Size    │
 *     │  0    │ CDROM   │ Online │ 8 MB   │ 2048           │
 *     ├─────────────────────────────────────────────────────┤
 *     │ Disks: 1  │ Volumes: 1               dark bar      │
 *     └─────────────────────────────────────────────────────┘
 *
 * Uses: libwindow (window + toolbar + table + label + statusbar),
 *       libdisk, libfs
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libdisk/libdisk.h"
#include "../../system/libraries/libfs/libfs.h"
#include "../../system/libraries/libgui/libgui.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define MAX_DISKS       8
#define MAX_VOLUMES     8
#define REFRESH_TICKS   100    /* Refresh every ~2 seconds */

/* Window dimensions */
#define WIN_W           540
#define WIN_H           460

/*=============================================================================
 * GLOBALS
 *===========================================================================*/

static window_t    *g_win         = (window_t *)0;
static toolbar_t   *g_toolbar     = (toolbar_t *)0;

/* Volume section */
static label_t     *g_vol_label   = (label_t *)0;
static table_t     *g_vol_table   = (table_t *)0;

/* Disk section */
static label_t     *g_disk_label  = (label_t *)0;
static table_t     *g_disk_table  = (table_t *)0;

/* Statusbar */
static statusbar_t *g_statusbar   = (statusbar_t *)0;
static int          g_panel_disks = -1;
static int          g_panel_vols  = -1;

static int          g_tick_counter = 0;

/*=============================================================================
 * STRING HELPERS (no libc)
 *===========================================================================*/

static void int_to_str(int val, char *buf, int buflen) {
    if (buflen < 2) { buf[0] = '\0'; return; }
    if (val < 0) {
        buf[0] = '-';
        int_to_str(-val, buf + 1, buflen - 1);
        return;
    }
    char tmp[12];
    int i = 0;
    if (val == 0) {
        tmp[i++] = '0';
    } else {
        while (val > 0 && i < 11) {
            tmp[i++] = '0' + (val % 10);
            val /= 10;
        }
    }
    int j;
    for (j = 0; j < i && j < buflen - 1; j++)
        buf[j] = tmp[i - 1 - j];
    buf[j] = '\0';
}

static void str_copy_n(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void str_append(char *dst, const char *src, int max) {
    int dlen = str_len(dst);
    str_copy_n(dst + dlen, src, max - dlen);
}

/*=============================================================================
 * TYPE/STATUS STRINGS
 *===========================================================================*/

static const char *disk_type_str(uint8_t type) {
    switch (type) {
        case DISK_TYPE_HDD:    return "HDD";
        case DISK_TYPE_CDROM:  return "CDROM";
        case DISK_TYPE_FLOPPY: return "Floppy";
        default:               return "Unknown";
    }
}

static const char *disk_status_str(uint8_t status) {
    switch (status) {
        case DISK_STATUS_ONLINE:  return "Online";
        case DISK_STATUS_OFFLINE: return "Offline";
        case DISK_STATUS_ERROR:   return "Error";
        default:                  return "?";
    }
}

/*=============================================================================
 * REFRESH DATA
 *===========================================================================*/

static void refresh_data(void) {
    char buf[TABLE_MAX_CELL_TEXT];

    /* ---- Volumes ---- */
    int vol_count = libfs_vol_count();
    if (vol_count < 0) vol_count = 0;
    if (vol_count > MAX_VOLUMES) vol_count = MAX_VOLUMES;

    table_set_row_count(g_vol_table, vol_count);

    int v;
    for (v = 0; v < vol_count; v++) {
        libfs_vol_info_t vinfo;
        if (libfs_vol_info(v, &vinfo) < 0) continue;

        /* Drive letter */
        buf[0] = vinfo.drive_letter;
        buf[1] = ':';
        buf[2] = '\0';
        table_set_cell(g_vol_table, v, 0, buf);

        /* Label */
        table_set_cell(g_vol_table, v, 1, vinfo.label);

        /* Filesystem type string */
        table_set_cell(g_vol_table, v, 2, vinfo.fs_str);

        /* Size (MB) */
        int_to_str((int)vinfo.size_mb, buf, sizeof(buf));
        str_append(buf, " MB", sizeof(buf));
        table_set_cell(g_vol_table, v, 3, buf);

        /* Status */
        table_set_cell(g_vol_table, v, 4, vinfo.mounted ? "Mounted" : "---");
    }

    /* ---- Disks ---- */
    disk_exec_info_t disks[MAX_DISKS];
    int disk_count = libdisk_list(disks, MAX_DISKS);
    if (disk_count < 0) disk_count = 0;

    table_set_row_count(g_disk_table, disk_count);

    int d;
    for (d = 0; d < disk_count; d++) {
        /* Disk index */
        int_to_str(disks[d].index, buf, sizeof(buf));
        table_set_cell(g_disk_table, d, 0, buf);

        /* Type */
        table_set_cell(g_disk_table, d, 1, disk_type_str(disks[d].disk_type));

        /* Status */
        table_set_cell(g_disk_table, d, 2, disk_status_str(disks[d].status));

        /* Size (MB) */
        int_to_str((int)disks[d].size_mb, buf, sizeof(buf));
        str_append(buf, " MB", sizeof(buf));
        table_set_cell(g_disk_table, d, 3, buf);

        /* Sector size */
        int_to_str((int)disks[d].sector_size, buf, sizeof(buf));
        table_set_cell(g_disk_table, d, 4, buf);
    }

    /* ---- Statusbar ---- */
    {
        char txt[STATUSBAR_MAX_TEXT];
        char num[12];
        str_copy_n(txt, "Disks: ", sizeof(txt));
        int_to_str(disk_count, num, sizeof(num));
        str_append(txt, num, sizeof(txt));
        statusbar_set_text(g_statusbar, g_panel_disks, txt);
    }
    {
        char txt[STATUSBAR_MAX_TEXT];
        char num[12];
        str_copy_n(txt, "Volumes: ", sizeof(txt));
        int_to_str(vol_count, num, sizeof(num));
        str_append(txt, num, sizeof(txt));
        statusbar_set_text(g_statusbar, g_panel_vols, txt);
    }

    window_invalidate(g_win);
}

/*=============================================================================
 * TOOLBAR CALLBACKS
 *===========================================================================*/

static void on_refresh_click(void *userdata) {
    (void)userdata;
    refresh_data();
}

/*=============================================================================
 * TICK CALLBACK — periodic refresh
 *===========================================================================*/

static void on_tick(window_t *win, void *userdata) {
    (void)win;
    (void)userdata;

    g_tick_counter++;
    if (g_tick_counter >= REFRESH_TICKS) {
        g_tick_counter = 0;
        refresh_data();
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    /* Center window on screen */
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    int win_x = (scr_w - WIN_W) / 2;
    int win_y = (scr_h - WIN_H) / 2;

    window_t *win = window_create("Disk Explorer", win_x, win_y, WIN_W, WIN_H);
    if (!win) return;
    g_win = win;

    int content_w = WIN_W;
    int content_h = WIN_H - THEME_TITLEBAR_HEIGHT;
    int y_cursor = 0;

    /* ---- Toolbar ---- */
    g_toolbar = toolbar_create(0, y_cursor, content_w);
    if (g_toolbar) {
        toolbar_add_button(g_toolbar, "Refresh", on_refresh_click, (void *)0);
        window_add_control(win, &g_toolbar->base);
    }
    y_cursor += TOOLBAR_HEIGHT + 2;

    /* ---- Volumes section label ---- */
    g_vol_label = label_create(6, y_cursor + 2, "Volumes", THEME_TEXT);
    if (g_vol_label) {
        window_add_control(win, &g_vol_label->base);
    }
    y_cursor += 18;

    /* ---- Volume table ---- */
    int vol_table_h = 6 * TABLE_ROW_H + TABLE_HEADER_H + TABLE_BORDER_W * 2 + 4;
    g_vol_table = table_create(0, y_cursor, content_w, vol_table_h);
    if (g_vol_table) {
        table_add_column(g_vol_table, "Drive",   55, TABLE_ALIGN_CENTER);
        table_add_column(g_vol_table, "Label",  130, TABLE_ALIGN_LEFT);
        table_add_column(g_vol_table, "FS",      80, TABLE_ALIGN_LEFT);
        table_add_column(g_vol_table, "Size",    80, TABLE_ALIGN_RIGHT);
        table_add_column(g_vol_table, "Status",  80, TABLE_ALIGN_LEFT);
        window_add_control(win, &g_vol_table->base);
    }
    y_cursor += vol_table_h + 4;

    /* ---- Disks section label ---- */
    g_disk_label = label_create(6, y_cursor + 2, "Disks", THEME_TEXT);
    if (g_disk_label) {
        window_add_control(win, &g_disk_label->base);
    }
    y_cursor += 18;

    /* ---- Disk table (fills remaining space above statusbar) ---- */
    int disk_table_h = content_h - y_cursor - STATUSBAR_HEIGHT - 2;
    if (disk_table_h < 60) disk_table_h = 60;
    g_disk_table = table_create(0, y_cursor, content_w, disk_table_h);
    if (g_disk_table) {
        table_add_column(g_disk_table, "Disk",     50, TABLE_ALIGN_CENTER);
        table_add_column(g_disk_table, "Type",     80, TABLE_ALIGN_LEFT);
        table_add_column(g_disk_table, "Status",   80, TABLE_ALIGN_LEFT);
        table_add_column(g_disk_table, "Size",     80, TABLE_ALIGN_RIGHT);
        table_add_column(g_disk_table, "Sectors",  80, TABLE_ALIGN_RIGHT);
        window_add_control(win, &g_disk_table->base);
    }

    /* ---- StatusBar ---- */
    int sb_y = content_h - STATUSBAR_HEIGHT;
    g_statusbar = statusbar_create(0, sb_y, content_w);
    if (g_statusbar) {
        g_panel_disks = statusbar_add_panel(g_statusbar, "Disks: --", 140);
        g_panel_vols  = statusbar_add_panel(g_statusbar, "Volumes: --", 0);
        window_add_control(win, &g_statusbar->base);
    }

    /* Set tick callback for periodic refresh */
    window_set_on_tick(win, on_tick, (void *)0);

    /* Initial data load */
    refresh_data();

    /* Run the event loop (blocks until close) */
    window_run(win);

    /* Cleanup */
    window_destroy(win);
}
