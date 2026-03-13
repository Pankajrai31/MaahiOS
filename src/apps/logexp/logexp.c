/**
 * logexp.mex - MaahiOS Log Explorer (Design System v2)
 *
 * Description:
 *   Windowed GUI app showing the kernel log ring buffer with
 *   real-time auto-refresh and log-level filtering.
 *
 *   Layout:
 *     ┌─ Log Explorer ─────────────────────────────────────┐
 *     │ [Refresh]             toolbar (chrome, raised)      │
 *     │ (●) All  ( ) Debug  ( ) Info  ( ) Warn  ( ) Error  │
 *     ├─────────────────────────────────────────────────────┤
 *     │  #  │ Level │ Tag      │ Message                    │
 *     │  0  │ INFO  │ KERNEL   │ MaahiOS booting...         │
 *     │  1  │ DEBUG │ PMM      │ Physical pages: 32768      │
 *     │ ... │       │          │                            │
 *     ├─────────────────────────────────────────────────────┤
 *     │ Entries: 142 (showing 142)  │ Filter: All   bar     │
 *     └─────────────────────────────────────────────────────┘
 *
 * Uses: libwindow (window + toolbar + radiogroup + table + statusbar)
 *       SYS_KLOG_READ syscall (reads kernel log buffer directly)
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "../../system/libraries/libwindow/libwindow.h"
#include "../../system/libraries/libgui/libgui.h"
#include "../../system/libraries/core/syscall_helpers.h"
#include "../../managers/syscall/syscall_numbers.h"
#include "../../managers/klog/klog.h"

/*=============================================================================
 * CONSTANTS
 *===========================================================================*/

#define WIN_W           700
#define WIN_H           520
#define MAX_LOG_ENTRIES KLOG_BUFFER_SIZE  /* 256 */
#define REFRESH_TICKS   100    /* Auto-refresh every ~2 seconds */

/* Filter indices (must match radiogroup option order) */
#define FILTER_ALL      0
#define FILTER_DEBUG    1
#define FILTER_INFO     2
#define FILTER_WARN     3
#define FILTER_ERROR    4

/*=============================================================================
 * GLOBALS
 *===========================================================================*/

static window_t      *g_win       = (window_t *)0;
static toolbar_t     *g_toolbar   = (toolbar_t *)0;
static radiogroup_t  *g_radio     = (radiogroup_t *)0;
static table_t       *g_table     = (table_t *)0;
static statusbar_t   *g_statusbar = (statusbar_t *)0;

static int g_filter        = FILTER_ALL;  /* Current filter mode */
static int g_tick_counter  = 0;
static int g_total_entries = 0;           /* Total entries from kernel */
static int g_shown_entries = 0;           /* Entries shown after filter */

/* Raw log buffer (copied from kernel) */
static klog_entry_t g_log_buf[MAX_LOG_ENTRIES];

/*=============================================================================
 * STRING HELPERS (no libc)
 *===========================================================================*/

static int le_strlen(const char *s) {
    int n = 0;
    while (s && s[n]) n++;
    return n;
}

static void le_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static void le_strcat(char *dst, const char *src, int max) {
    int dlen = le_strlen(dst);
    int i;
    for (i = 0; i < max - dlen - 1 && src && src[i]; i++)
        dst[dlen + i] = src[i];
    dst[dlen + i] = '\0';
}

static void le_int_to_str(int val, char *buf, int bufsz) {
    if (bufsz < 2) { buf[0] = '\0'; return; }
    if (val < 0) { buf[0] = '-'; le_int_to_str(-val, buf + 1, bufsz - 1); return; }
    char tmp[12];
    int i = 0;
    do { tmp[i++] = '0' + (val % 10); val /= 10; } while (val > 0 && i < 11);
    int j;
    for (j = 0; j < i && j < bufsz - 1; j++) buf[j] = tmp[i - 1 - j];
    buf[j] = '\0';
}

/*=============================================================================
 * LEVEL HELPERS
 *===========================================================================*/

static const char *level_to_str(int level) {
    switch (level) {
        case 0: return "FATAL";
        case 1: return "ERROR";
        case 2: return "WARN";
        case 3: return "INFO";
        case 4: return "DEBUG";
        case 5: return "TRACE";
        default: return "?";
    }
}

/** Check if an entry passes the current filter */
static int passes_filter(int level) {
    switch (g_filter) {
        case FILTER_ALL:   return 1;
        case FILTER_DEBUG: return (level == 4 || level == 5);
        case FILTER_INFO:  return (level == 3);
        case FILTER_WARN:  return (level == 2);
        case FILTER_ERROR: return (level <= 1);  /* FATAL + ERROR */
        default:           return 1;
    }
}

/*=============================================================================
 * REFRESH LOG TABLE
 *===========================================================================*/

static void refresh_log_table(void) {
    /* Read log entries from kernel via syscall */
    int count = syscall2(SYS_KLOG_READ,
                         (int)(unsigned int)g_log_buf,
                         MAX_LOG_ENTRIES);
    if (count < 0) count = 0;
    g_total_entries = count;

    /* Apply filter and populate table */
    int row = 0;
    int i;
    for (i = 0; i < count && row < TABLE_MAX_ROWS; i++) {
        if (!passes_filter(g_log_buf[i].level)) continue;

        /* Column 0: entry number */
        char num_buf[12];
        le_int_to_str(i, num_buf, sizeof(num_buf));
        table_set_cell(g_table, row, 0, num_buf);

        /* Column 1: level */
        table_set_cell(g_table, row, 1, level_to_str(g_log_buf[i].level));

        /* Column 2: tag */
        table_set_cell(g_table, row, 2, g_log_buf[i].tag);

        /* Column 3: message (truncated to cell text limit) */
        table_set_cell(g_table, row, 3, g_log_buf[i].msg);

        row++;
    }

    table_set_row_count(g_table, row);
    g_shown_entries = row;

    /* Update statusbar */
    {
        char status[64];
        char num[12];
        le_strcpy(status, "Entries: ", sizeof(status));
        le_int_to_str(g_total_entries, num, sizeof(num));
        le_strcat(status, num, sizeof(status));
        le_strcat(status, " (showing ", sizeof(status));
        le_int_to_str(g_shown_entries, num, sizeof(num));
        le_strcat(status, num, sizeof(status));
        le_strcat(status, ")", sizeof(status));
        statusbar_set_text(g_statusbar, 0, status);
    }

    /* Filter status */
    {
        static const char *filter_names[] = {
            "All", "Debug", "Info", "Warn", "Error"
        };
        char fbuf[32];
        le_strcpy(fbuf, "Filter: ", sizeof(fbuf));
        le_strcat(fbuf, filter_names[g_filter], sizeof(fbuf));
        statusbar_set_text(g_statusbar, 1, fbuf);
    }

    /* Scroll to bottom to show latest entries */
    if (g_table->visible_rows > 0) {
        int max_scroll = row - g_table->visible_rows;
        if (max_scroll < 0) max_scroll = 0;
        g_table->scroll_offset = max_scroll;
    }

    window_invalidate(g_win);
}

/*=============================================================================
 * CALLBACKS
 *===========================================================================*/

static void on_refresh_click(void *userdata) {
    (void)userdata;
    refresh_log_table();
}

static void on_filter_change(int selected, void *userdata) {
    (void)userdata;
    g_filter = selected;
    refresh_log_table();
}

static void on_tick(window_t *win, void *userdata) {
    (void)win;
    (void)userdata;
    g_tick_counter++;
    if (g_tick_counter >= REFRESH_TICKS) {
        g_tick_counter = 0;
        refresh_log_table();
    }
}

/*=============================================================================
 * ENTRY POINT
 *===========================================================================*/

void mex_main(void) {
    /* Center window */
    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();
    int win_x = (scr_w - WIN_W) / 2;
    int win_y = (scr_h - WIN_H) / 2;

    window_t *win = window_create("Log Explorer", win_x, win_y, WIN_W, WIN_H);
    if (!win) return;
    g_win = win;

    int cw = win->content_w;
    int ch = win->content_h;

    /* ---- Toolbar ---- */
    g_toolbar = toolbar_create(0, 0, cw);
    if (g_toolbar) {
        toolbar_add_button(g_toolbar, "Refresh", on_refresh_click, (void *)0);
        window_add_control(win, &g_toolbar->base);
    }

    /* ---- Radio group (filter bar) ---- */
    int radio_y = TOOLBAR_HEIGHT + 4;
    g_radio = radiogroup_create(8, radio_y, cw - 16, 20);
    if (g_radio) {
        radiogroup_add_option(g_radio, "All");
        radiogroup_add_option(g_radio, "Debug");
        radiogroup_add_option(g_radio, "Info");
        radiogroup_add_option(g_radio, "Warn");
        radiogroup_add_option(g_radio, "Error");
        radiogroup_set_selected(g_radio, FILTER_ALL);
        radiogroup_set_on_change(g_radio, on_filter_change, (void *)0);
        window_add_control(win, &g_radio->base);
    }

    /* ---- Table ---- */
    int table_y = radio_y + 24 + 4;
    int table_h = ch - table_y - STATUSBAR_HEIGHT - 2;
    g_table = table_create(0, table_y, cw, table_h);
    if (g_table) {
        table_add_column(g_table, "#",       42, TABLE_ALIGN_RIGHT);
        table_add_column(g_table, "Level",   60, TABLE_ALIGN_LEFT);
        table_add_column(g_table, "Tag",     90, TABLE_ALIGN_LEFT);
        table_add_column(g_table, "Message", cw - 42 - 60 - 90 - 12,
                         TABLE_ALIGN_LEFT);
        window_add_control(win, &g_table->base);
    }

    /* ---- StatusBar ---- */
    int sb_y = ch - STATUSBAR_HEIGHT;
    g_statusbar = statusbar_create(0, sb_y, cw);
    if (g_statusbar) {
        statusbar_add_panel(g_statusbar, "Entries: --", 200);
        statusbar_add_panel(g_statusbar, "Filter: All", 0);
        window_add_control(win, &g_statusbar->base);
    }

    /* Set tick callback for auto-refresh */
    window_set_on_tick(win, on_tick, (void *)0);

    /* Initial data load */
    refresh_log_table();

    /* Run event loop */
    window_run(win);

    /* Cleanup */
    window_destroy(win);
}
