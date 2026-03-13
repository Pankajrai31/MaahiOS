/**
 * procexp.mex - MaahiOS Process Explorer (Design System v2 — Redesigned)
 *
 * Description:
 *   Windowed GUI app showing a live-updated table of all running
 *   processes with a toolbar, rich table, and dark statusbar.
 *
 *   Layout:
 *     ┌─ Titlebar ──────────────────────────────────────────┐
 *     │ [Refresh] | [End Process]  toolbar (chrome)         │
 *     ├─────────────────────────────────────────────────────┤
 *     │ Name        │ PID │ Type   │ State   │ Memory      │
 *     │ sysman      │   1 │ System │ Running │    12 KB    │
 *     │ logexec     │   2 │ System │ Ready   │     8 KB    │
 *     │   ...       │     │        │         │             │
 *     ├─────────────────────────────────────────────────────┤
 *     │ Processes: 14  │ Uptime: 0h 12m       dark bar     │
 *     └─────────────────────────────────────────────────────┘
 *
 * Uses: libwindow (window + toolbar + table + statusbar), libprocess
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libprocess/libprocess.h"
#include "../../system/libraries/libgui/libgui.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define MAX_PROCS       32
#define REFRESH_TICKS   50     /* Refresh every ~1 second */

/* Window dimensions */
#define WIN_W           520
#define WIN_H           480

/*=============================================================================
 * GLOBALS
 *===========================================================================*/

static window_t    *g_win          = (window_t *)0;
static toolbar_t   *g_toolbar      = (toolbar_t *)0;
static table_t     *g_table        = (table_t *)0;
static statusbar_t *g_statusbar    = (statusbar_t *)0;
static int          g_tick_counter = 0;
static int          g_total_ticks  = 0;   /* Total ticks since app start */

/* Statusbar panel indices */
static int g_panel_procs   = -1;
static int g_panel_uptime  = -1;

/*=============================================================================
 * STRING HELPERS (no libc in MaahiOS user-space)
 *===========================================================================*/

static void int_to_str(int val, char *buf, int buflen) {
    if (buflen < 2) { buf[0] = '\0'; return; }

    if (val < 0) {
        buf[0] = '-';
        int_to_str(-val, buf + 1, buflen - 1);
        return;
    }

    /* Convert digits in reverse */
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

    /* Copy reversed */
    int j;
    for (j = 0; j < i && j < buflen - 1; j++) {
        buf[j] = tmp[i - 1 - j];
    }
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
 * PROCESS TYPE / STATE STRINGS
 *===========================================================================*/

static const char *proc_type_str(uint8_t type) {
    switch (type) {
        case 0: return "System";
        case 1: return "User";
        default: return "?";
    }
}

static const char *proc_state_str(uint32_t state) {
    switch (state) {
        case 0: return "Ready";
        case 1: return "Running";
        case 2: return "Blocked";
        case 3: return "Sleeping";
        case 4: return "Dead";
        default: return "Unknown";
    }
}

/*=============================================================================
 * REFRESH TABLE DATA
 *===========================================================================*/

static void refresh_process_table(void) {
    static process_info_t procs[MAX_PROCS];
    int old_count = g_table->row_count;
    int count = libprocess_list(procs, MAX_PROCS);
    if (count < 0) count = 0;

    table_set_row_count(g_table, count);

    int i;
    for (i = 0; i < count; i++) {
        char buf[TABLE_MAX_CELL_TEXT];

        /* Name */
        table_set_cell(g_table, i, 0, procs[i].name);

        /* PID */
        int_to_str(procs[i].pid, buf, sizeof(buf));
        table_set_cell(g_table, i, 1, buf);

        /* Type */
        table_set_cell(g_table, i, 2, proc_type_str(procs[i].type));

        /* State */
        table_set_cell(g_table, i, 3, proc_state_str(procs[i].state));

        /* Memory (KB) */
        int kb = (int)(procs[i].memory_alloc / 1024);
        int_to_str(kb, buf, sizeof(buf));
        str_append(buf, " KB", sizeof(buf));
        table_set_cell(g_table, i, 4, buf);
    }

    /* Update statusbar: process count */
    {
        char status[STATUSBAR_MAX_TEXT];
        char num[12];
        str_copy_n(status, "Processes: ", sizeof(status));
        int_to_str(count, num, sizeof(num));
        str_append(status, num, sizeof(status));
        statusbar_set_text(g_statusbar, g_panel_procs, status);
    }

    /* Update statusbar: uptime (based on ticks since app start) */
    {
        char uptime_str[STATUSBAR_MAX_TEXT];
        uint32_t secs = (uint32_t)(g_total_ticks / 50);  /* PIT at ~50Hz */
        uint32_t mins = secs / 60;
        uint32_t hours = mins / 60;
        char num[12];

        str_copy_n(uptime_str, "Uptime: ", sizeof(uptime_str));
        int_to_str((int)hours, num, sizeof(num));
        str_append(uptime_str, num, sizeof(uptime_str));
        str_append(uptime_str, "h ", sizeof(uptime_str));
        int_to_str((int)(mins % 60), num, sizeof(num));
        str_append(uptime_str, num, sizeof(uptime_str));
        str_append(uptime_str, "m", sizeof(uptime_str));
        statusbar_set_text(g_statusbar, g_panel_uptime, uptime_str);
    }

    /* If new processes appeared, scroll so the last one is visible */
    if (count > old_count && g_table->visible_rows > 0) {
        int max_scroll = count - g_table->visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        g_table->scroll_offset = max_scroll;
    }

    /* Mark window for redraw */
    window_invalidate(g_win);
}

/*=============================================================================
 * TOOLBAR CALLBACKS
 *===========================================================================*/

static void on_refresh_click(void *userdata) {
    (void)userdata;
    refresh_process_table();
}

static void on_end_process_click(void *userdata) {
    (void)userdata;
    /* Kill selected process if any */
    if (g_table->selected_row >= 0 && g_table->selected_row < g_table->row_count) {
        /* Read PID from column 1 */
        const char *pid_text = g_table->cells[g_table->selected_row][1];
        int pid = 0;
        int k;
        for (k = 0; pid_text[k] >= '0' && pid_text[k] <= '9'; k++) {
            pid = pid * 10 + (pid_text[k] - '0');
        }
        if (pid > 0) {
            libprocess_kill(pid);
            refresh_process_table();
        }
    }
}

/*=============================================================================
 * TICK CALLBACK — auto-refresh
 *===========================================================================*/

static void on_tick(window_t *win, void *userdata) {
    (void)win;
    (void)userdata;

    g_total_ticks++;
    g_tick_counter++;
    if (g_tick_counter >= REFRESH_TICKS) {
        g_tick_counter = 0;
        refresh_process_table();
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

    window_t *win = window_create("Process Explorer", win_x, win_y, WIN_W, WIN_H);
    if (!win) return;
    g_win = win;

    /* Content area dimensions (inside titlebar) */
    int content_w = WIN_W;
    int content_h = WIN_H - THEME_TITLEBAR_HEIGHT;

    /* ---- Toolbar ---- */
    g_toolbar = toolbar_create(0, 0, content_w);
    if (g_toolbar) {
        toolbar_add_button(g_toolbar, "Refresh", on_refresh_click, (void *)0);
        toolbar_add_separator(g_toolbar);
        toolbar_add_button(g_toolbar, "End Process", on_end_process_click, (void *)0);
        window_add_control(win, &g_toolbar->base);
    }

    /* ---- Table ---- */
    int table_y = TOOLBAR_HEIGHT + 2;
    int table_h = content_h - table_y - STATUSBAR_HEIGHT - 2;
    g_table = table_create(0, table_y, content_w, table_h);
    if (g_table) {
        table_add_column(g_table, "Name",   160, TABLE_ALIGN_LEFT);
        table_add_column(g_table, "PID",     55, TABLE_ALIGN_RIGHT);
        table_add_column(g_table, "Type",    70, TABLE_ALIGN_LEFT);
        table_add_column(g_table, "State",   80, TABLE_ALIGN_LEFT);
        table_add_column(g_table, "Memory", 100, TABLE_ALIGN_RIGHT);
        window_add_control(win, &g_table->base);
    }

    /* ---- StatusBar ---- */
    int sb_y = content_h - STATUSBAR_HEIGHT;
    g_statusbar = statusbar_create(0, sb_y, content_w);
    if (g_statusbar) {
        g_panel_procs  = statusbar_add_panel(g_statusbar, "Processes: --", 160);
        g_panel_uptime = statusbar_add_panel(g_statusbar, "Uptime: --", 0);
        window_add_control(win, &g_statusbar->base);
    }

    /* Set tick callback for auto-refresh */
    window_set_on_tick(win, on_tick, (void *)0);

    /* Initial data load */
    refresh_process_table();

    /* Run the event loop (blocks until close) */
    window_run(win);

    /* Cleanup */
    window_destroy(win);
}
