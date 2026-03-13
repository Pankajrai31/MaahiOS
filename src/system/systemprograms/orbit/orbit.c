/**
 * Orbit - MaahiOS Desktop Shell
 *
 * Description:
 *   Desktop launcher (PID 7). Draws the desktop environment matching
 *   the MaahiOS Design System v2: blue gradient background, embossed
 *   chrome taskbar with Start button, dynamic task buttons for running
 *   windows, clickable desktop app shortcuts with 32x32 icons and
 *   proportional anti-aliased text, and system tray with a real-time
 *   clock.
 *
 *   Reads "system.desktop.apps" cell for desktop shortcuts (published
 *   by sysman).  Polls "system.taskbar.windows" cell for running
 *   windowed apps and renders dynamic taskbar buttons.  Clicking a
 *   minimized taskbar button writes "system.taskbar.restore" to signal
 *   the window to un-minimize.
 *
 *   Launches GRUB-module apps via libprocess and .mex apps via
 *   libfs + libmex.
 *
 * Author: MaahiOS Team
 * Date: February 2026
 */

#include <stdint.h>
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"
#include "../../libraries/libprocess/libprocess.h"
#include "../../libraries/libgui/libgui.h"
#include "../../libraries/libgui/printgui/printgui.h"
#include "../../libraries/libgui/fonts/libfont.h"
#include "../../libraries/libfs/libfs.h"
#include "../../libraries/libmex/libmex.h"
#include "../../libraries/libbmp/libbmp.h"
#include "../../libraries/libio/libio.h"
#include "../../libraries/shared/taskbar_types.h"

/*=============================================================================
 * MaahiOS Design System v2 — Color Constants
 *===========================================================================*/

/* Chrome (system UI base) */
#define COLOR_CHROME        0x00D8DBE8
#define COLOR_CHROME_LIGHT  0x00E8EAF2
#define COLOR_CHROME_DARK   0x00C0C4D4
#define COLOR_CHROME_DARKER 0x00B0B4C6

/* 3D bevel edges */
#define COLOR_BEVEL_LIGHT   0x00F4F5FA
#define COLOR_BEVEL_DARK    0x009498AC

/* Accent */
#define COLOR_ACCENT        0x002B5BB5
#define COLOR_ACCENT_DARK   0x001B4A9A
#define COLOR_TEAL          0x001E8A65

/* Text */
#define COLOR_TEXT           0x001A1A2E
#define COLOR_TEXT_SEC       0x005A5D76
#define COLOR_TEXT_INV       0x00FFFFFF

/* Desktop gradient (135-deg approximated as vertical) */
#define DESKTOP_TOP         0x003A7BB8
#define DESKTOP_MID         0x005A9BD8
#define DESKTOP_BOT         0x003A8BC8

/* Desktop shortcut */
#define COLOR_SHORTCUT_BG   COLOR_CHROME
#define COLOR_SHORTCUT_FG   COLOR_TEXT
#define COLOR_SHORTCUT_HL   0x00A8C8F0  /* Blue selection highlight */

/* Icon color key — black pixels are transparent */
#define ICON_COLORKEY       0x00000000

/*=============================================================================
 * SCREEN LAYOUT — queried at runtime via gui_get_screen_width/height()
 *===========================================================================*/

static int SCREEN_W;
static int SCREEN_H;

#define TASKBAR_H   32

/* Computed at startup */
static int TASKBAR_Y;

/* Start button within taskbar */
#define START_X     4
static int START_Y;
#define START_W     64
#define START_H     24

/* Task button area */
#define TASK_AREA_X     76
static int TASK_BTN_Y;
#define TASK_BTN_W      140
#define TASK_BTN_H      24
#define TASK_BTN_GAP    4

/* System tray (right-aligned) */
#define TRAY_W      100
static int TRAY_X;
static int TRAY_Y;
#define TRAY_H      24

/* Desktop shortcut layout — icon-based grid */
#define SHORTCUT_X_START    32          /* Left margin */
#define SHORTCUT_Y_START    40          /* Top margin */
#define SHORTCUT_W          80          /* Total shortcut cell width */
#define SHORTCUT_H          72          /* Total shortcut cell height */
#define SHORTCUT_GAP        12          /* Vertical gap between shortcuts */
#define ICON_SIZE           32          /* Icon dimension (32x32) */

/* Device IDs (mirroring kernel defines) */
#define DEV_MOUSE       1

/* Mouse button masks */
#define MOUSE_LEFT      0x01

/*=============================================================================
 * TYPES
 *===========================================================================*/

/* Must match kernel mouse_state_t (drivers/mouse/mouse.h) */
typedef struct {
    int x;
    int y;
    uint8_t buttons;
} mouse_state_t;

/* Must match kernel sys_datetime_t (managers/time/time_manager.h) */
typedef struct {
    uint16_t year;
    uint8_t  month;
    uint8_t  day;
    uint8_t  hour;
    uint8_t  minute;
    uint8_t  second;
    uint8_t  weekday;
} datetime_t;

/*=============================================================================
 * ICON STORAGE — pre-allocated pixel buffers for desktop app icons
 *===========================================================================*/

#define MAX_ICONS       DESKTOP_MAX_APPS
static uint32_t g_icon_pixels[MAX_ICONS][ICON_SIZE * ICON_SIZE];
static int      g_icon_loaded[MAX_ICONS];  /* 1 = loaded successfully */

/* Default icon pixels (for apps without a custom icon) */
static uint32_t g_default_icon[ICON_SIZE * ICON_SIZE];
static int      g_default_loaded = 0;

/*=============================================================================
 * GLOBAL STATE
 *===========================================================================*/

/* Cached desktop app shortcuts (read once from cell) */
static desktop_app_list_t g_desktop_apps;

/* Cached taskbar window list (polled frequently) */
static taskbar_window_list_t g_taskbar;
static int g_taskbar_count_prev = -1;  /* -1 = force initial draw */

/*=============================================================================
 * START MENU + SHUTDOWN DIALOG STATE
 *===========================================================================*/

/* Menu layout constants */
#define MENU_W          180
#define MENU_ITEM_H     28
#define MENU_PAD        3
#define MENU_ICON_PAD   28     /* Space reserved for icon/bullet */
#define MENU_ARROW_PAD  20     /* Space for submenu arrow */
#define MENU_ITEM_COUNT 2      /* 0=Programs, 1=Shut Down */
#define MENU_H          (MENU_PAD * 2 + MENU_ITEM_COUNT * MENU_ITEM_H + 1)

/* Submenu */
#define SUBMENU_W       180
#define SUBMENU_ITEM_COUNT 4   /* 0=Process Explorer, 1=Disk Explorer, 2=WordWrite, 3=Log Explorer */
#define SUBMENU_H       (MENU_PAD * 2 + SUBMENU_ITEM_COUNT * MENU_ITEM_H + 1)

/* Shutdown confirmation dialog */
#define SDLG_W          280
#define SDLG_H          130
#define SDLG_TITLE_H    24
#define SDLG_BTN_W      90
#define SDLG_BTN_H      25
#define SDLG_BTN_GAP    12

/* Menu item colors */
#define MENU_BG         COLOR_CHROME
#define MENU_HOVER_BG   COLOR_ACCENT
#define MENU_TEXT        COLOR_TEXT
#define MENU_HOVER_TEXT  COLOR_TEXT_INV
#define MENU_SEPARATOR   COLOR_BEVEL_DARK

/* Menu state */
static int g_menu_open = 0;
static int g_submenu_open = 0;
static int g_menu_hover = -1;
static int g_submenu_hover = -1;
static int g_shutdown_dlg = 0;
static int g_sdlg_hover_btn = -1;  /* 0=Shutdown, 1=Cancel */

/* Computed menu positions */
static int MENU_X;
static int MENU_Y;
static int SUBMENU_X;
static int SUBMENU_Y;

/*=============================================================================
 * CONVENIENCE
 *===========================================================================*/

static void yield(void) {
    syscall0(SYS_YIELD);
}

static void sleep_ticks(int ticks) {
    syscall1(SYS_SLEEP, ticks);
}

static int str_len(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void str_copy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static int str_equal(const char *a, const char *b) {
    while (*a && *b) {
        if (*a != *b) return 0;
        a++; b++;
    }
    return (*a == *b);
}

/*=============================================================================
 * COLOR HELPERS
 *===========================================================================*/

static uint32_t lerp_color(uint32_t c0, uint32_t c1, int pos, int total) {
    if (total <= 1) return c0;
    int r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r = r0 + (r1 - r0) * pos / (total - 1);
    int g = g0 + (g1 - g0) * pos / (total - 1);
    int b = b0 + (b1 - b0) * pos / (total - 1);
    return (uint32_t)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
}

/*=============================================================================
 * DRAWING HELPERS
 *===========================================================================*/

static void hline(int x, int y, int w, uint32_t color) {
    gui_fill_rect(x, y, w, 1, color);
}

static void vline(int x, int y, int h, uint32_t color) {
    gui_fill_rect(x, y, 1, h, color);
}

static void draw_raised_rect(int x, int y, int w, int h, uint32_t fill) {
    gui_fill_rect(x, y, w, h, fill);
    hline(x, y, w, COLOR_BEVEL_LIGHT);
    vline(x, y, h, COLOR_BEVEL_LIGHT);
    hline(x, y + h - 1, w, COLOR_BEVEL_DARK);
    vline(x + w - 1, y, h, COLOR_BEVEL_DARK);
}

static void draw_sunken_rect(int x, int y, int w, int h, uint32_t fill) {
    gui_fill_rect(x, y, w, h, fill);
    hline(x, y, w, COLOR_BEVEL_DARK);
    vline(x, y, h, COLOR_BEVEL_DARK);
    hline(x, y + h - 1, w, COLOR_BEVEL_LIGHT);
    vline(x + w - 1, y, h, COLOR_BEVEL_LIGHT);
}

/** Center proportional text within a rectangle. */
static void draw_centered_text_prop(int x, int y, int w, int h,
                                    const char *text, uint32_t fg,
                                    font_size_t size) {
    int tw = gui_measure_text(text, size);
    int th = gui_text_height(size);
    int tx = x + (w - tw) / 2;
    int ty = y + (h - th) / 2;
    gui_draw_text(tx, ty, text, fg, size);
}

/** Center legacy 8x16 text (fallback). */
static void draw_centered_text(int x, int y, int w, int h,
                               const char *text, uint32_t fg, uint32_t bg) {
    int tw = str_len(text) * 8;
    int tx = x + (w - tw) / 2;
    int ty = y + (h - 16) / 2;
    gui_draw_string(tx, ty, text, fg, bg);
}

/*=============================================================================
 * ICON LOADING
 *===========================================================================*/

/** Load a single BMP icon from the ISO filesystem root. */
static int load_icon_bmp(const char *filename, uint32_t *out_pixels) {
    static uint8_t file_buf[8192];  /* BMP is 32*32*4 + 54 = ~4150 bytes */
    int bytes = libfs_read_file("C:/", filename, file_buf, sizeof(file_buf));
    if (bytes <= 0) return -1;

    int w = 0, h = 0;
    if (libbmp_decode(file_buf, bytes, out_pixels, ICON_SIZE * ICON_SIZE,
                      &w, &h) != 0) {
        return -1;
    }
    if (w != ICON_SIZE || h != ICON_SIZE) return -1;
    return 0;
}

/** Load all desktop app icons. */
static void load_desktop_icons(void) {
    /* Load default icon first */
    if (load_icon_bmp("DEFAULT.BMP", g_default_icon) == 0) {
        g_default_loaded = 1;
        liblog(LOG_INFO, "ORBIT", "Default icon loaded");
    }

    for (int i = 0; i < g_desktop_apps.count && i < MAX_ICONS; i++) {
        g_icon_loaded[i] = 0;
        if (g_desktop_apps.entries[i].icon_file[0] != '\0') {
            if (load_icon_bmp(g_desktop_apps.entries[i].icon_file,
                              g_icon_pixels[i]) == 0) {
                g_icon_loaded[i] = 1;
            }
        }
        /* Fallback to default icon if loading failed */
        if (!g_icon_loaded[i] && g_default_loaded) {
            int j;
            for (j = 0; j < ICON_SIZE * ICON_SIZE; j++)
                g_icon_pixels[i][j] = g_default_icon[j];
            g_icon_loaded[i] = 1;
        }
    }
    liblog_hex(LOG_INFO, "ORBIT", "Desktop icons loaded:",
               (uint32_t)g_desktop_apps.count);
}

/*=============================================================================
 * DESKTOP DRAWING
 *===========================================================================*/

static void draw_desktop_background(void) {
    int mid = TASKBAR_Y / 2;

    for (int row = 0; row < mid; row++) {
        uint32_t c = lerp_color(DESKTOP_TOP, DESKTOP_MID, row, mid);
        gui_fill_rect(0, row, SCREEN_W, 1, c);
    }
    for (int row = mid; row < TASKBAR_Y; row++) {
        uint32_t c = lerp_color(DESKTOP_MID, DESKTOP_BOT, row - mid,
                                TASKBAR_Y - mid);
        gui_fill_rect(0, row, SCREEN_W, 1, c);
    }
}

static void draw_taskbar(void) {
    draw_raised_rect(0, TASKBAR_Y, SCREEN_W, TASKBAR_H, COLOR_CHROME);
    hline(0, TASKBAR_Y, SCREEN_W, COLOR_BEVEL_DARK);
    hline(0, TASKBAR_Y + 1, SCREEN_W, COLOR_BEVEL_LIGHT);
}

static void draw_start_button(int pressed) {
    if (pressed) {
        draw_sunken_rect(START_X, START_Y, START_W, START_H, COLOR_CHROME_LIGHT);
        draw_centered_text_prop(START_X + 1, START_Y + 1, START_W, START_H,
                                "Start", COLOR_TEXT, FONT_BODY);
    } else {
        draw_raised_rect(START_X, START_Y, START_W, START_H, COLOR_CHROME);
        draw_centered_text_prop(START_X, START_Y, START_W, START_H,
                                "Start", COLOR_TEXT, FONT_BODY);
    }
}

static void draw_system_tray(const char *clock_text) {
    draw_sunken_rect(TRAY_X, TRAY_Y, TRAY_W, TRAY_H, COLOR_CHROME_DARK);
    draw_centered_text_prop(TRAY_X, TRAY_Y, TRAY_W, TRAY_H,
                            clock_text, COLOR_TEXT_SEC, FONT_SMALL);
}

/*=============================================================================
 * START MENU — popup overlay above taskbar
 *
 *   ┌────────────────────────┐
 *   │  ■ Programs          ▶ │  ← item 0 (opens submenu)
 *   │  ─────────────────────  │  ← separator
 *   │  ■ Shut Down           │  ← item 1 (shutdown dialog)
 *   └────────────────────────┘
 *===========================================================================*/

/* Forward declarations for functions used by erase_menu_area */
static void draw_desktop_shortcuts(void);
static void draw_taskbar_buttons(void);
static void format_clock(char *buf, int buflen);

/** Draw a small filled square icon (8x8) as a bullet/icon placeholder. */
static void draw_menu_bullet(int x, int y, uint32_t color) {
    gui_fill_rect(x, y, 8, 8, color);
    /* Raised mini-bevel */
    hline(x, y, 8, COLOR_BEVEL_LIGHT);
    vline(x, y, 8, COLOR_BEVEL_LIGHT);
    hline(x, y + 7, 8, COLOR_BEVEL_DARK);
    vline(x + 7, y, 8, COLOR_BEVEL_DARK);
}

/** Draw the right-pointing submenu arrow (▶). */
static void draw_submenu_arrow(int x, int y, uint32_t color) {
    /* 5-pixel tall right-pointing triangle */
    int i;
    for (i = 0; i < 5; i++) {
        gui_fill_rect(x + i, y + i, 1, 9 - 2 * i, color);
    }
}

/** Draw a single menu item. */
static void draw_menu_item(int mx, int my, int mw, int idx, int hovered,
                            const char *text, int has_arrow) {
    int iy = my + MENU_PAD + idx * MENU_ITEM_H;
    /* If separator before shutdown item */
    if (idx == 1) iy += 1;  /* account for separator pixel */

    uint32_t bg = hovered ? MENU_HOVER_BG : MENU_BG;
    uint32_t fg = hovered ? MENU_HOVER_TEXT : MENU_TEXT;

    gui_fill_rect(mx + MENU_PAD, iy, mw - MENU_PAD * 2, MENU_ITEM_H, bg);

    /* Bullet icon */
    uint32_t bullet_color = hovered ? 0x0080B0FF : COLOR_ACCENT;
    draw_menu_bullet(mx + MENU_PAD + 6, iy + (MENU_ITEM_H - 8) / 2,
                     bullet_color);

    /* Text */
    int text_x = mx + MENU_PAD + MENU_ICON_PAD;
    int th = gui_text_height(FONT_BODY);
    int text_y = iy + (MENU_ITEM_H - th) / 2;
    gui_draw_text(text_x, text_y, text, fg, FONT_BODY);

    /* Submenu arrow */
    if (has_arrow) {
        int ax = mx + mw - MENU_PAD - MENU_ARROW_PAD + 4;
        int ay = iy + (MENU_ITEM_H - 9) / 2;
        draw_submenu_arrow(ax, ay, fg);
    }
}

/** Draw the Start Menu popup. */
static void draw_start_menu(void) {
    int mx = MENU_X;
    int my = MENU_Y;
    int mw = MENU_W;
    int mh = MENU_H;

    /* Menu background with raised bevel */
    draw_raised_rect(mx, my, mw, mh, MENU_BG);
    /* Extra inner highlight */
    hline(mx + 1, my + 1, mw - 2, COLOR_BEVEL_LIGHT);
    vline(mx + 1, my + 1, mh - 2, COLOR_BEVEL_LIGHT);

    /* Item 0: Programs (with arrow) */
    draw_menu_item(mx, my, mw, 0, (g_menu_hover == 0), "Programs", 1);

    /* Separator line */
    int sep_y = my + MENU_PAD + MENU_ITEM_H;
    hline(mx + MENU_PAD + 4, sep_y, mw - MENU_PAD * 2 - 8, MENU_SEPARATOR);

    /* Item 1: Shut Down */
    draw_menu_item(mx, my, mw, 1, (g_menu_hover == 1), "Shut Down", 0);
}

/** Draw the Programs submenu popup. */
static void draw_submenu(void) {
    int sx = SUBMENU_X;
    int sy = SUBMENU_Y;
    int sw = SUBMENU_W;
    int sh = SUBMENU_H;

    /* Submenu background with raised bevel */
    draw_raised_rect(sx, sy, sw, sh, MENU_BG);
    hline(sx + 1, sy + 1, sw - 2, COLOR_BEVEL_LIGHT);
    vline(sx + 1, sy + 1, sh - 2, COLOR_BEVEL_LIGHT);

    /* Draw submenu items using helper */
    int iy;
    int th = gui_text_height(FONT_BODY);

    /* Item 0: Process Explorer */
    iy = sy + MENU_PAD + 0 * MENU_ITEM_H;
    {
        uint32_t bg = (g_submenu_hover == 0) ? MENU_HOVER_BG : MENU_BG;
        uint32_t fg = (g_submenu_hover == 0) ? MENU_HOVER_TEXT : MENU_TEXT;
        gui_fill_rect(sx + MENU_PAD, iy, sw - MENU_PAD * 2, MENU_ITEM_H, bg);
        uint32_t bc = (g_submenu_hover == 0) ? 0x0080B0FF : COLOR_TEAL;
        draw_menu_bullet(sx + MENU_PAD + 6, iy + (MENU_ITEM_H - 8) / 2, bc);
        int text_x = sx + MENU_PAD + MENU_ICON_PAD;
        int text_y = iy + (MENU_ITEM_H - th) / 2;
        gui_draw_text(text_x, text_y, "Process Explorer", fg, FONT_BODY);
    }

    /* Item 1: Disk Explorer */
    iy = sy + MENU_PAD + 1 * MENU_ITEM_H;
    {
        uint32_t bg = (g_submenu_hover == 1) ? MENU_HOVER_BG : MENU_BG;
        uint32_t fg = (g_submenu_hover == 1) ? MENU_HOVER_TEXT : MENU_TEXT;
        gui_fill_rect(sx + MENU_PAD, iy, sw - MENU_PAD * 2, MENU_ITEM_H, bg);
        uint32_t bc = (g_submenu_hover == 1) ? 0x0080B0FF : COLOR_ACCENT;
        draw_menu_bullet(sx + MENU_PAD + 6, iy + (MENU_ITEM_H - 8) / 2, bc);
        int text_x = sx + MENU_PAD + MENU_ICON_PAD;
        int text_y = iy + (MENU_ITEM_H - th) / 2;
        gui_draw_text(text_x, text_y, "Disk Explorer", fg, FONT_BODY);
    }

    /* Item 2: WordWrite */
    iy = sy + MENU_PAD + 2 * MENU_ITEM_H;
    {
        uint32_t bg = (g_submenu_hover == 2) ? MENU_HOVER_BG : MENU_BG;
        uint32_t fg = (g_submenu_hover == 2) ? MENU_HOVER_TEXT : MENU_TEXT;
        gui_fill_rect(sx + MENU_PAD, iy, sw - MENU_PAD * 2, MENU_ITEM_H, bg);
        uint32_t bc = (g_submenu_hover == 2) ? 0x0080B0FF : COLOR_TEAL;
        draw_menu_bullet(sx + MENU_PAD + 6, iy + (MENU_ITEM_H - 8) / 2, bc);
        int text_x = sx + MENU_PAD + MENU_ICON_PAD;
        int text_y = iy + (MENU_ITEM_H - th) / 2;
        gui_draw_text(text_x, text_y, "WordWrite", fg, FONT_BODY);
    }

    /* Item 3: Log Explorer */
    iy = sy + MENU_PAD + 3 * MENU_ITEM_H;
    {
        uint32_t bg = (g_submenu_hover == 3) ? MENU_HOVER_BG : MENU_BG;
        uint32_t fg = (g_submenu_hover == 3) ? MENU_HOVER_TEXT : MENU_TEXT;
        gui_fill_rect(sx + MENU_PAD, iy, sw - MENU_PAD * 2, MENU_ITEM_H, bg);
        uint32_t bc = (g_submenu_hover == 3) ? 0x0080B0FF : COLOR_ACCENT;
        draw_menu_bullet(sx + MENU_PAD + 6, iy + (MENU_ITEM_H - 8) / 2, bc);
        int text_x = sx + MENU_PAD + MENU_ICON_PAD;
        int text_y = iy + (MENU_ITEM_H - th) / 2;
        gui_draw_text(text_x, text_y, "Log Explorer", fg, FONT_BODY);
    }
}

/** Erase the Start Menu area by redrawing the desktop/taskbar behind it. */
static void erase_menu_area(void) {
    /* Redraw desktop gradient behind menu */
    int top_y = MENU_Y;
    int bot_y = MENU_Y + MENU_H;
    if (top_y < 0) top_y = 0;

    /* Also cover submenu if it was open */
    int right_x = MENU_X + MENU_W;
    if (g_submenu_open) {
        right_x = SUBMENU_X + SUBMENU_W;
        /* Redraw gradient rows behind submenu too */
        int sub_top = SUBMENU_Y;
        if (sub_top < top_y) top_y = sub_top;
    }

    /* Redraw desktop gradient rows */
    int mid = TASKBAR_Y / 2;
    int row;
    for (row = top_y; row < TASKBAR_Y && row < bot_y + 10; row++) {
        uint32_t c;
        if (row < mid) c = lerp_color(DESKTOP_TOP, DESKTOP_MID, row, mid);
        else c = lerp_color(DESKTOP_MID, DESKTOP_BOT, row - mid, TASKBAR_Y - mid);
        gui_fill_rect(0, row, right_x + 10, 1, c);
    }

    /* Redraw any desktop shortcuts that overlap */
    draw_desktop_shortcuts();

    /* Redraw taskbar portion */
    draw_taskbar();
    draw_start_button(0);
    draw_taskbar_buttons();
}

/*=============================================================================
 * SHUTDOWN CONFIRMATION DIALOG — centered overlay
 *
 *   ┌── Shut Down MaahiOS ──────────┐
 *   │                                │
 *   │    Are you sure you want to    │
 *   │    shut down?                  │
 *   │                                │
 *   │  [ Shut Down ]    [ Cancel ]   │
 *   └────────────────────────────────┘
 *===========================================================================*/

/** Draw the shutdown confirmation dialog. */
static void draw_shutdown_dialog(void) {
    /* Center on screen */
    int dx = (SCREEN_W - SDLG_W) / 2;
    int dy = (SCREEN_H - SDLG_H) / 2 - 20;  /* Slightly above center */

    /* Shadow */
    gui_fill_rect(dx + 3, dy + 3, SDLG_W, SDLG_H, 0x00606070);

    /* Dialog background with raised bevel */
    draw_raised_rect(dx, dy, SDLG_W, SDLG_H, COLOR_CHROME);
    /* Inner bevel */
    hline(dx + 1, dy + 1, SDLG_W - 2, COLOR_BEVEL_LIGHT);
    vline(dx + 1, dy + 1, SDLG_H - 2, COLOR_BEVEL_LIGHT);

    /* Titlebar gradient */
    {
        int tx = dx + 2;
        int ty = dy + 2;
        int tw = SDLG_W - 4;
        int th = SDLG_TITLE_H;
        int row;
        for (row = 0; row < th; row++) {
            uint32_t c = lerp_color(0x001B3F8B, 0x002B5BB5, row, th);
            gui_fill_rect(tx, ty + row, tw, 1, c);
        }
        /* Title text */
        draw_centered_text_prop(tx, ty, tw, th,
                                "Shut Down MaahiOS", COLOR_TEXT_INV, FONT_BODY);
    }

    /* Message area */
    {
        int msg_x = dx + 2;
        int msg_y = dy + 2 + SDLG_TITLE_H;
        int msg_w = SDLG_W - 4;
        int msg_h = SDLG_H - 4 - SDLG_TITLE_H;
        gui_fill_rect(msg_x, msg_y, msg_w, msg_h, 0x00FFFFFF);

        /* Message text */
        const char *line1 = "Are you sure you want to";
        const char *line2 = "shut down?";
        int tw1 = gui_measure_text(line1, FONT_BODY);
        int tw2 = gui_measure_text(line2, FONT_BODY);
        int lh = gui_text_height(FONT_BODY);
        int text_y = msg_y + 14;
        gui_draw_text(msg_x + (msg_w - tw1) / 2, text_y, line1,
                      COLOR_TEXT, FONT_BODY);
        gui_draw_text(msg_x + (msg_w - tw2) / 2, text_y + lh + 2, line2,
                      COLOR_TEXT, FONT_BODY);
    }

    /* Buttons */
    int btn_area_y = dy + SDLG_H - SDLG_BTN_H - 14;
    int total_btn_w = SDLG_BTN_W * 2 + SDLG_BTN_GAP;
    int btn_x_start = dx + (SDLG_W - total_btn_w) / 2;

    /* Shut Down button (Standard chrome style) */
    {
        int bx = btn_x_start;
        int by = btn_area_y;
        if (g_sdlg_hover_btn == 0) {
            /* Sunken (pressed look) */
            draw_sunken_rect(bx, by, SDLG_BTN_W, SDLG_BTN_H, COLOR_CHROME_LIGHT);
            draw_centered_text_prop(bx + 1, by + 1, SDLG_BTN_W, SDLG_BTN_H,
                                    "Shut Down", COLOR_TEXT, FONT_BODY);
        } else {
            draw_raised_rect(bx, by, SDLG_BTN_W, SDLG_BTN_H, COLOR_CHROME);
            draw_centered_text_prop(bx, by, SDLG_BTN_W, SDLG_BTN_H,
                                    "Shut Down", COLOR_TEXT, FONT_BODY);
        }
    }

    /* Cancel button (Standard style: chrome) */
    {
        int bx = btn_x_start + SDLG_BTN_W + SDLG_BTN_GAP;
        int by = btn_area_y;
        if (g_sdlg_hover_btn == 1) {
            draw_sunken_rect(bx, by, SDLG_BTN_W, SDLG_BTN_H, COLOR_CHROME_LIGHT);
            draw_centered_text_prop(bx + 1, by + 1, SDLG_BTN_W, SDLG_BTN_H,
                                    "Cancel", COLOR_TEXT, FONT_BODY);
        } else {
            draw_raised_rect(bx, by, SDLG_BTN_W, SDLG_BTN_H, COLOR_CHROME);
            draw_centered_text_prop(bx, by, SDLG_BTN_W, SDLG_BTN_H,
                                    "Cancel", COLOR_TEXT, FONT_BODY);
        }
    }
}

/** Erase the shutdown dialog by redrawing what's behind it. */
static void erase_shutdown_dialog(void) {
    int dx = (SCREEN_W - SDLG_W) / 2;
    int dy = (SCREEN_H - SDLG_H) / 2 - 20;

    /* Redraw desktop gradient behind dialog + shadow */
    int mid = TASKBAR_Y / 2;
    int row;
    for (row = dy; row < dy + SDLG_H + 4 && row < TASKBAR_Y; row++) {
        uint32_t c;
        if (row < mid) c = lerp_color(DESKTOP_TOP, DESKTOP_MID, row, mid);
        else c = lerp_color(DESKTOP_MID, DESKTOP_BOT, row - mid, TASKBAR_Y - mid);
        gui_fill_rect(dx, row, SDLG_W + 4, 1, c);
    }

    /* Redraw any desktop shortcuts that overlap */
    draw_desktop_shortcuts();
}

/** Hit-test a menu item. Returns item index (0..count-1) or -1. */
static int hit_test_menu(int mx, int my, int menu_x, int menu_y,
                         int menu_w, int item_count) {
    if (mx < menu_x + MENU_PAD || mx >= menu_x + menu_w - MENU_PAD)
        return -1;
    if (my < menu_y + MENU_PAD || my >= menu_y + MENU_PAD + item_count * MENU_ITEM_H)
        return -1;

    int idx = (my - menu_y - MENU_PAD) / MENU_ITEM_H;
    if (idx >= 0 && idx < item_count) return idx;
    return -1;
}

/** Close the Start Menu and submenu, erase them. */
static void close_start_menu(void) {
    if (g_menu_open || g_submenu_open) {
        erase_menu_area();
        g_menu_open = 0;
        g_submenu_open = 0;
        g_menu_hover = -1;
        g_submenu_hover = -1;
        /* Redraw system tray too */
        char clk[8];
        format_clock(clk, sizeof(clk));
        draw_system_tray(clk);
        gui_flip();
    }
}

/** Launch Process Explorer .mex file. */
static void launch_process_explorer(void) {
    static uint8_t pe_buf[131072];  /* 128 KB max */
    int bytes = libfs_read_file("C:/", "procexp.mex", pe_buf, sizeof(pe_buf));
    if (bytes < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to read procexp.mex");
        return;
    }
    mex_info_t info;
    int result = libmex_parse(pe_buf, (uint32_t)bytes, &info);
    if (result != 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to parse procexp.mex");
        return;
    }
    int pid = libmex_exec(&info);
    if (pid < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to exec procexp.mex");
        return;
    }
    libprocess_set_name(pid, "ProcExplorer", 1);  /* PROC_TYPE_USER */
    liblog_hex(LOG_INFO, "ORBIT", "Process Explorer launched, PID:", (uint32_t)pid);
}

/** Launch Disk Explorer .mex file. */
static void launch_disk_explorer(void) {
    static uint8_t de_buf[131072];  /* 128 KB max */
    int bytes = libfs_read_file("C:/", "diskexp.mex", de_buf, sizeof(de_buf));
    if (bytes < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to read diskexp.mex");
        return;
    }
    mex_info_t info;
    int result = libmex_parse(de_buf, (uint32_t)bytes, &info);
    if (result != 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to parse diskexp.mex");
        return;
    }
    int pid = libmex_exec(&info);
    if (pid < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to exec diskexp.mex");
        return;
    }
    libprocess_set_name(pid, "DiskExplorer", 1);  /* PROC_TYPE_USER */
    liblog_hex(LOG_INFO, "ORBIT", "Disk Explorer launched, PID:", (uint32_t)pid);
}

/** Launch WordWrite .mex file. */
static void launch_wordwrite(void) {
    static uint8_t ww_buf[131072];  /* 128 KB max */
    int bytes = libfs_read_file("C:/", "wordwrit.mex", ww_buf, sizeof(ww_buf));
    if (bytes < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to read wordwrit.mex");
        return;
    }
    mex_info_t info;
    int result = libmex_parse(ww_buf, (uint32_t)bytes, &info);
    if (result != 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to parse wordwrit.mex");
        return;
    }
    int pid = libmex_exec(&info);
    if (pid < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to exec wordwrit.mex");
        return;
    }
    libprocess_set_name(pid, "WordWrite", 1);  /* PROC_TYPE_USER */
    liblog_hex(LOG_INFO, "ORBIT", "WordWrite launched, PID:", (uint32_t)pid);
}

/** Launch Log Explorer .mex file. */
static void launch_log_explorer(void) {
    static uint8_t le_buf[131072];  /* 128 KB max */
    int bytes = libfs_read_file("C:/", "logexp.mex", le_buf, sizeof(le_buf));
    if (bytes < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to read logexp.mex");
        return;
    }
    mex_info_t info;
    int result = libmex_parse(le_buf, (uint32_t)bytes, &info);
    if (result != 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to parse logexp.mex");
        return;
    }
    int pid = libmex_exec(&info);
    if (pid < 0) {
        liblog(LOG_ERROR, "ORBIT", "Failed to exec logexp.mex");
        return;
    }
    libprocess_set_name(pid, "LogExplorer", 1);  /* PROC_TYPE_USER */
    liblog_hex(LOG_INFO, "ORBIT", "Log Explorer launched, PID:", (uint32_t)pid);
}

/*=============================================================================
 * DESKTOP APP SHORTCUTS — Icon + Text layout
 *
 * Each shortcut is SHORTCUT_W x SHORTCUT_H pixels:
 *   ┌──────────────────────┐
 *   │      [32x32 icon]    │  4px top padding
 *   │                      │  4px gap
 *   │    "App Name"        │  proportional text centered
 *   │                      │  bottom padding
 *   └──────────────────────┘
 *===========================================================================*/

/** Draw a single desktop shortcut with icon and text. */
static void draw_shortcut(int i, int hovered) {
    int sx = SHORTCUT_X_START;
    int sy = SHORTCUT_Y_START + i * (SHORTCUT_H + SHORTCUT_GAP);

    uint32_t bg = hovered ? COLOR_SHORTCUT_HL : COLOR_SHORTCUT_BG;

    /* Draw background with bevel */
    draw_raised_rect(sx, sy, SHORTCUT_W, SHORTCUT_H, bg);

    /* Draw icon centered horizontally, 4px from top */
    int icon_x = sx + (SHORTCUT_W - ICON_SIZE) / 2;
    int icon_y = sy + 4;

    if (g_icon_loaded[i]) {
        gui_blit_icon(icon_x, icon_y, g_icon_pixels[i],
                      ICON_SIZE, ICON_SIZE, ICON_COLORKEY);
    }

    /* Draw app name centered below icon using proportional font */
    int text_y = icon_y + ICON_SIZE + 4;
    int text_area_h = SHORTCUT_H - (ICON_SIZE + 4 + 4);  /* remaining space */
    const char *name = g_desktop_apps.entries[i].name;
    int tw = gui_measure_text(name, FONT_SMALL);
    int tx = sx + (SHORTCUT_W - tw) / 2;
    if (tx < sx + 2) tx = sx + 2;  /* Clamp to shortcut bounds */
    gui_draw_text(tx, text_y, name, COLOR_SHORTCUT_FG, FONT_SMALL);
    (void)text_area_h;
}

static void draw_desktop_shortcuts(void) {
    for (int i = 0; i < g_desktop_apps.count && i < DESKTOP_MAX_APPS; i++) {
        draw_shortcut(i, 0);
    }
}

/** Check if a point is inside a desktop shortcut. Returns index or -1. */
static int hit_test_desktop_shortcut(int mx, int my) {
    for (int i = 0; i < g_desktop_apps.count && i < DESKTOP_MAX_APPS; i++) {
        int sx = SHORTCUT_X_START;
        int sy = SHORTCUT_Y_START + i * (SHORTCUT_H + SHORTCUT_GAP);
        if (mx >= sx && mx < sx + SHORTCUT_W &&
            my >= sy && my < sy + SHORTCUT_H) {
            return i;
        }
    }
    return -1;
}

/*=============================================================================
 * DYNAMIC TASKBAR BUTTONS — Proportional text
 *===========================================================================*/

/** Draw the taskbar buttons area based on current g_taskbar state. */
static void draw_taskbar_buttons(void) {
    /* Clear the task button area first (fill with chrome) */
    int area_w = TRAY_X - TASK_AREA_X - 8;  /* Available width */
    gui_fill_rect(TASK_AREA_X, TASKBAR_Y + 2, area_w, TASKBAR_H - 4,
                  COLOR_CHROME);

    for (int i = 0; i < g_taskbar.count && i < TASKBAR_MAX_WINDOWS; i++) {
        int bx = TASK_AREA_X + i * (TASK_BTN_W + TASK_BTN_GAP);
        /* Stop if we'd overflow into tray */
        if (bx + TASK_BTN_W > TRAY_X - 8) break;

        if (g_taskbar.entries[i].minimized) {
            /* Minimized: raised (looks "up") */
            draw_raised_rect(bx, TASK_BTN_Y, TASK_BTN_W, TASK_BTN_H,
                             COLOR_CHROME);
            draw_centered_text_prop(bx, TASK_BTN_Y, TASK_BTN_W, TASK_BTN_H,
                                    g_taskbar.entries[i].title,
                                    COLOR_TEXT_SEC, FONT_SMALL);
        } else {
            /* Active: sunken (looks "pressed in") */
            draw_sunken_rect(bx, TASK_BTN_Y, TASK_BTN_W, TASK_BTN_H,
                             COLOR_CHROME_LIGHT);
            draw_centered_text_prop(bx + 1, TASK_BTN_Y + 1,
                                    TASK_BTN_W, TASK_BTN_H,
                                    g_taskbar.entries[i].title,
                                    COLOR_TEXT, FONT_SMALL);
        }
    }
}

/** Hit-test taskbar buttons. Returns index into g_taskbar or -1. */
static int hit_test_taskbar_button(int mx, int my) {
    if (my < TASK_BTN_Y || my >= TASK_BTN_Y + TASK_BTN_H) return -1;

    for (int i = 0; i < g_taskbar.count && i < TASKBAR_MAX_WINDOWS; i++) {
        int bx = TASK_AREA_X + i * (TASK_BTN_W + TASK_BTN_GAP);
        if (bx + TASK_BTN_W > TRAY_X - 8) break;
        if (mx >= bx && mx < bx + TASK_BTN_W) return i;
    }
    return -1;
}

/*=============================================================================
 * CLOCK
 *===========================================================================*/

static void format_clock(char *buf, int buflen) {
    datetime_t dt;
    int ret = syscall1(SYS_TIME_GET_DATETIME, (int)(uint32_t)&dt);
    if (ret < 0) {
        buf[0] = '-'; buf[1] = '-'; buf[2] = ':';
        buf[3] = '-'; buf[4] = '-'; buf[5] = '\0';
        return;
    }

    int h = dt.hour;
    int m = dt.minute;
    buf[0] = '0' + (h / 10);
    buf[1] = '0' + (h % 10);
    buf[2] = ':';
    buf[3] = '0' + (m / 10);
    buf[4] = '0' + (m % 10);
    buf[5] = '\0';
    (void)buflen;
}

/*=============================================================================
 * MOUSE INPUT
 *===========================================================================*/

static int point_in_rect(int px, int py, int rx, int ry, int rw, int rh) {
    return (px >= rx && px < rx + rw && py >= ry && py < ry + rh);
}

/*=============================================================================
 * APP LAUNCHING
 *===========================================================================*/

/** Launch a desktop app entry. Returns PID on success, <0 on error. */
static int launch_app(const desktop_app_entry_t *app) {
    if (app->app_type == DESKTOP_APP_TYPE_MODULE) {
        /* GRUB module — use libprocess_create with BSS size from desktop entry */
        int pid = libprocess_create((uint32_t)app->module_idx, 0, app->bss_size);
        if (pid < 0) {
            liblog(LOG_ERROR, "ORBIT", "Failed to launch module app");
            return -1;
        }
        libprocess_set_name(pid, app->name, 1);  /* PROC_TYPE_USER — killable */
        liblog_hex(LOG_INFO, "ORBIT", "Module app launched, PID:", (uint32_t)pid);
        return pid;
    }

    if (app->app_type == DESKTOP_APP_TYPE_MEX) {
        /* MEX file on filesystem — read, parse, exec */
        static uint8_t file_buf[131072];  /* 128 KB — must fit largest .mex */
        int bytes = libfs_read_file("C:/", app->command, file_buf,
                                    sizeof(file_buf));
        if (bytes < 0) {
            liblog(LOG_ERROR, "ORBIT", "Failed to read MEX file");
            return -1;
        }

        mex_info_t info;
        int result = libmex_parse(file_buf, (uint32_t)bytes, &info);
        if (result != 0) {
            liblog(LOG_ERROR, "ORBIT", "Failed to parse MEX file");
            return -1;
        }

        int pid = libmex_exec(&info);
        if (pid < 0) {
            liblog(LOG_ERROR, "ORBIT", "Failed to exec MEX app");
            return -1;
        }

        libprocess_set_name(pid, app->name, 1);
        liblog_hex(LOG_INFO, "ORBIT", "MEX app launched, PID:", (uint32_t)pid);
        return pid;
    }

    return -1;
}

/*=============================================================================
 * TASKBAR MANAGEMENT
 *===========================================================================*/

/** Poll the taskbar windows cell and redraw buttons if changed. */
static int poll_taskbar(void) {
    taskbar_window_list_t new_list;
    int rd = libcell_read(CELL_TASKBAR_WINDOWS, &new_list, sizeof(new_list));
    if (rd < (int)sizeof(int32_t)) return 0;

    /* Simple change detection: compare count and minimized states */
    int changed = 0;
    if (new_list.count != g_taskbar.count) {
        changed = 1;
    } else {
        for (int i = 0; i < new_list.count; i++) {
            if (new_list.entries[i].pid != g_taskbar.entries[i].pid ||
                new_list.entries[i].minimized != g_taskbar.entries[i].minimized) {
                changed = 1;
                break;
            }
        }
    }

    if (changed) {
        g_taskbar = new_list;
        return 1;
    }
    return 0;
}

/** Signal a minimized window to restore via the restore cell. */
static void signal_restore(int32_t pid) {
    taskbar_restore_t restore;
    restore.pid = pid;
    libcell_write(CELL_TASKBAR_RESTORE, &restore, sizeof(restore));
}

/*=============================================================================
 * MAIN ENTRY POINT
 *===========================================================================*/

void orbit_main_c(void) {
    liblog(LOG_INFO, "ORBIT", "========================================");
    liblog(LOG_INFO, "ORBIT", "  Orbit Desktop Shell Starting");
    liblog(LOG_INFO, "ORBIT", "========================================");

    /* Initialize display via GUI library */
    if (gui_init() != 0) {
        liblog(LOG_ERROR, "ORBIT", "GUI init failed, halting");
        while (1) yield();
    }

    /* Query actual screen dimensions */
    SCREEN_W  = (int)gui_get_screen_width();
    SCREEN_H  = (int)gui_get_screen_height();
    TASKBAR_Y = SCREEN_H - TASKBAR_H;
    START_Y   = TASKBAR_Y + 4;
    TASK_BTN_Y = TASKBAR_Y + 4;
    TRAY_X    = SCREEN_W - TRAY_W - 6;
    TRAY_Y    = TASKBAR_Y + 4;
    liblog(LOG_INFO, "ORBIT", "Screen dims queried from GUI executive");

    /* ---- Draw full desktop environment ---- */
    draw_desktop_background();
    liblog(LOG_INFO, "ORBIT", "Desktop gradient drawn");

    draw_taskbar();
    draw_start_button(0);
    liblog(LOG_INFO, "ORBIT", "Taskbar drawn");

    /* Read desktop app shortcuts from cell */
    {
        int i;
        for (i = 0; i < (int)sizeof(g_desktop_apps); i++)
            ((uint8_t *)&g_desktop_apps)[i] = 0;
        int rd = libcell_read(CELL_DESKTOP_APPS, &g_desktop_apps,
                              sizeof(g_desktop_apps));
        if (rd < (int)sizeof(int32_t) || g_desktop_apps.count < 0) {
            g_desktop_apps.count = 0;
        }
        liblog_hex(LOG_INFO, "ORBIT", "Desktop shortcuts loaded:",
                   (uint32_t)g_desktop_apps.count);
    }

    /* Initialize icon storage */
    {
        int i;
        for (i = 0; i < MAX_ICONS; i++) g_icon_loaded[i] = 0;
        for (i = 0; i < ICON_SIZE * ICON_SIZE; i++) g_default_icon[i] = 0;
    }

    /* Load desktop app icons from filesystem */
    load_desktop_icons();

    /* Draw desktop shortcuts (with icons + proportional text) */
    draw_desktop_shortcuts();

    /* Initialize empty taskbar state */
    {
        int i;
        for (i = 0; i < (int)sizeof(g_taskbar); i++)
            ((uint8_t *)&g_taskbar)[i] = 0;
    }

    /* Initial clock */
    char clock_buf[8];
    format_clock(clock_buf, sizeof(clock_buf));
    draw_system_tray(clock_buf);
    liblog(LOG_INFO, "ORBIT", "System tray drawn");

    /* Flip initial desktop to screen */
    gui_flip();

    /* Give executives a moment to finish init */
    sleep_ticks(5);

    liblog(LOG_INFO, "ORBIT", "Orbit running. Cursor handled by kernel IRQ.");

    /* Compute Start Menu positions */
    MENU_X = START_X;
    MENU_Y = TASKBAR_Y - MENU_H;
    SUBMENU_X = MENU_X + MENU_W - 2;
    SUBMENU_Y = MENU_Y;

    /* ---- Main event loop ---- */
    mouse_state_t ms;
    uint8_t prev_buttons = 0;
    int start_pressed = 0;
    int clock_counter = 0;
    int taskbar_poll_counter = 0;
    int hover_shortcut = -1;  /* Currently hovered desktop shortcut */

    while (1) {
        /* Poll mouse state (via IO Executive) */
        int rd = libio_dev_read(DEV_MOUSE, &ms, sizeof(ms));
        if (rd < 0) {
            continue;
        }

        /* Detect rising edge of left button (press) */
        int left_down = (ms.buttons & MOUSE_LEFT) &&
                        !(prev_buttons & MOUSE_LEFT);
        /* Detect falling edge (release) */
        int left_up = !(ms.buttons & MOUSE_LEFT) &&
                      (prev_buttons & MOUSE_LEFT);

        /*=============================================================
         * SHUTDOWN DIALOG — modal, blocks all other input
         *=============================================================*/
        if (g_shutdown_dlg) {
            /* Hit-test dialog buttons on mouse move */
            int dx = (SCREEN_W - SDLG_W) / 2;
            int dy = (SCREEN_H - SDLG_H) / 2 - 20;
            int btn_area_y = dy + SDLG_H - SDLG_BTN_H - 14;
            int total_btn_w = SDLG_BTN_W * 2 + SDLG_BTN_GAP;
            int btn_x_start = dx + (SDLG_W - total_btn_w) / 2;

            int old_hover = g_sdlg_hover_btn;
            g_sdlg_hover_btn = -1;

            /* Button 0: Shut Down */
            if (point_in_rect(ms.x, ms.y, btn_x_start, btn_area_y,
                              SDLG_BTN_W, SDLG_BTN_H)) {
                g_sdlg_hover_btn = 0;
            }
            /* Button 1: Cancel */
            if (point_in_rect(ms.x, ms.y,
                              btn_x_start + SDLG_BTN_W + SDLG_BTN_GAP,
                              btn_area_y, SDLG_BTN_W, SDLG_BTN_H)) {
                g_sdlg_hover_btn = 1;
            }

            if (g_sdlg_hover_btn != old_hover) {
                draw_shutdown_dialog();
                gui_flip_rect(dx, dy, SDLG_W + 4, SDLG_H + 4);
            }

            /* Click handling */
            if (left_down) {
                if (g_sdlg_hover_btn == 0) {
                    /* Shut Down clicked! */
                    liblog(LOG_INFO, "ORBIT", "User confirmed shutdown");
                    libprocess_system_shutdown();
                    /* Does not return on success */
                } else if (g_sdlg_hover_btn == 1) {
                    /* Cancel clicked */
                    g_shutdown_dlg = 0;
                    erase_shutdown_dialog();
                    gui_flip();
                } else {
                    /* Click outside buttons → dismiss */
                    if (!point_in_rect(ms.x, ms.y, dx, dy, SDLG_W, SDLG_H)) {
                        g_shutdown_dlg = 0;
                        erase_shutdown_dialog();
                        gui_flip();
                    }
                }
            }

            prev_buttons = ms.buttons;
            yield();
            continue;  /* Skip all other input while dialog is open */
        }

        /*=============================================================
         * START MENU & SUBMENU — overlay input handling
         *=============================================================*/
        if (g_menu_open) {
            /* Track hover over menu items */
            int old_menu_hover = g_menu_hover;
            int old_sub_hover = g_submenu_hover;
            int old_sub_open = g_submenu_open;

            g_menu_hover = hit_test_menu(ms.x, ms.y,
                                         MENU_X, MENU_Y, MENU_W,
                                         MENU_ITEM_COUNT);
            if (g_submenu_open) {
                g_submenu_hover = hit_test_menu(ms.x, ms.y,
                                                SUBMENU_X, SUBMENU_Y,
                                                SUBMENU_W,
                                                SUBMENU_ITEM_COUNT);
            }

            /* Open/close submenu when hovering "Programs" */
            if (g_menu_hover == 0 && !g_submenu_open) {
                g_submenu_open = 1;
                draw_submenu();
                gui_flip_rect(SUBMENU_X, SUBMENU_Y, SUBMENU_W, SUBMENU_H);
            }
            if (g_menu_hover == 1 && g_submenu_open) {
                /* Hovering Shut Down — close submenu only if mouse
                   isn't in the submenu area */
                if (g_submenu_hover < 0) {
                    g_submenu_open = 0;
                    /* Erase submenu by redrawing desktop behind it */
                    int mid2 = TASKBAR_Y / 2;
                    int row2;
                    for (row2 = SUBMENU_Y; row2 < SUBMENU_Y + SUBMENU_H && row2 < TASKBAR_Y; row2++) {
                        uint32_t c;
                        if (row2 < mid2) c = lerp_color(DESKTOP_TOP, DESKTOP_MID, row2, mid2);
                        else c = lerp_color(DESKTOP_MID, DESKTOP_BOT, row2 - mid2, TASKBAR_Y - mid2);
                        gui_fill_rect(SUBMENU_X, row2, SUBMENU_W, 1, c);
                    }
                    gui_flip_rect(SUBMENU_X, SUBMENU_Y, SUBMENU_W, SUBMENU_H);
                }
            }

            /* Redraw menu items if hover changed */
            if (g_menu_hover != old_menu_hover) {
                draw_start_menu();
                gui_flip_rect(MENU_X, MENU_Y, MENU_W, MENU_H);
            }
            if (g_submenu_open && g_submenu_hover != old_sub_hover) {
                draw_submenu();
                gui_flip_rect(SUBMENU_X, SUBMENU_Y, SUBMENU_W, SUBMENU_H);
            }

            /* Click handling */
            if (left_down) {
                int handled = 0;

                /* Click on menu item */
                if (g_menu_hover == 0) {
                    /* Programs — submenu is already showing, do nothing */
                    handled = 1;
                }
                if (g_menu_hover == 1) {
                    /* Shut Down — open confirmation dialog */
                    close_start_menu();
                    g_shutdown_dlg = 1;
                    g_sdlg_hover_btn = -1;
                    draw_shutdown_dialog();
                    gui_flip();
                    handled = 1;
                }

                /* Click on submenu item */
                if (g_submenu_open && g_submenu_hover == 0) {
                    /* Process Explorer */
                    close_start_menu();
                    launch_process_explorer();
                    handled = 1;
                }
                if (g_submenu_open && g_submenu_hover == 1) {
                    /* Disk Explorer */
                    close_start_menu();
                    launch_disk_explorer();
                    handled = 1;
                }
                if (g_submenu_open && g_submenu_hover == 2) {
                    /* WordWrite */
                    close_start_menu();
                    launch_wordwrite();
                    handled = 1;
                }
                if (g_submenu_open && g_submenu_hover == 3) {
                    /* Log Explorer */
                    close_start_menu();
                    launch_log_explorer();
                    handled = 1;
                }

                /* Click on Start button again → close menu */
                if (!handled && point_in_rect(ms.x, ms.y,
                                              START_X, START_Y,
                                              START_W, START_H)) {
                    close_start_menu();
                    handled = 1;
                }

                /* Click outside menu → close */
                if (!handled) {
                    close_start_menu();
                }
            }

            prev_buttons = ms.buttons;

            /* Still poll taskbar and clock */
            goto poll_section;
        }

        /*=============================================================
         * NORMAL INPUT (no menu/dialog open)
         *=============================================================*/

        /* Start button interaction */
        if (left_down && point_in_rect(ms.x, ms.y,
                                       START_X, START_Y, START_W, START_H)) {
            start_pressed = 1;
            draw_start_button(1);
            gui_flip_rect(START_X, START_Y, START_W, START_H);
        }
        if (left_up && start_pressed) {
            start_pressed = 0;
            draw_start_button(0);
            gui_flip_rect(START_X, START_Y, START_W, START_H);
            /* Open Start Menu */
            g_menu_open = 1;
            g_submenu_open = 0;
            g_menu_hover = -1;
            g_submenu_hover = -1;
            draw_start_menu();
            gui_flip_rect(MENU_X, MENU_Y, MENU_W, MENU_H);
        }

        /* Desktop shortcut click */
        if (left_down && ms.y < TASKBAR_Y) {
            int idx = hit_test_desktop_shortcut(ms.x, ms.y);
            if (idx >= 0) {
                liblog_hex(LOG_INFO, "ORBIT", "Desktop shortcut clicked:",
                           (uint32_t)idx);
                launch_app(&g_desktop_apps.entries[idx]);
            }
        }

        /* Taskbar button click */
        if (left_down && ms.y >= TASKBAR_Y) {
            int idx = hit_test_taskbar_button(ms.x, ms.y);
            if (idx >= 0 && idx < g_taskbar.count) {
                if (g_taskbar.entries[idx].minimized) {
                    /* Signal the window to restore */
                    signal_restore(g_taskbar.entries[idx].pid);
                }
                /* If already visible, do nothing (future: bring to front) */
            }
        }

        prev_buttons = ms.buttons;

        /* Desktop shortcut hover effect */
        {
            int new_hover = hit_test_desktop_shortcut(ms.x, ms.y);
            if (new_hover != hover_shortcut) {
                /* Un-hover previous */
                if (hover_shortcut >= 0 && hover_shortcut < g_desktop_apps.count) {
                    /* Redraw background patch under the old shortcut */
                    int osy = SHORTCUT_Y_START + hover_shortcut * (SHORTCUT_H + SHORTCUT_GAP);
                    /* Redraw gradient slice behind it */
                    for (int row = osy; row < osy + SHORTCUT_H && row < TASKBAR_Y; row++) {
                        int mid = TASKBAR_Y / 2;
                        uint32_t c;
                        if (row < mid) c = lerp_color(DESKTOP_TOP, DESKTOP_MID, row, mid);
                        else c = lerp_color(DESKTOP_MID, DESKTOP_BOT, row - mid, TASKBAR_Y - mid);
                        gui_fill_rect(SHORTCUT_X_START, row, SHORTCUT_W, 1, c);
                    }
                    draw_shortcut(hover_shortcut, 0);
                }
                /* Hover new */
                if (new_hover >= 0 && new_hover < g_desktop_apps.count)
                    draw_shortcut(new_hover, 1);
                hover_shortcut = new_hover;
                /* Flip the shortcut column area */
                gui_flip_rect(SHORTCUT_X_START, SHORTCUT_Y_START,
                              SHORTCUT_W,
                              DESKTOP_MAX_APPS * (SHORTCUT_H + SHORTCUT_GAP));
            }
        }

        /*=============================================================
         * PERIODIC TASKS (taskbar poll, clock update)
         *=============================================================*/
poll_section:

        /* Poll taskbar windows cell every 3 iterations for responsiveness */
        taskbar_poll_counter++;
        if (taskbar_poll_counter >= 3) {
            taskbar_poll_counter = 0;
            if (poll_taskbar()) {
                draw_taskbar_buttons();
                gui_flip_rect(TASK_AREA_X, TASKBAR_Y,
                              TRAY_X - TASK_AREA_X, TASKBAR_H);
            }
        }

        /* Update clock roughly every 200 iterations */
        clock_counter++;
        if (clock_counter >= 200) {
            clock_counter = 0;
            format_clock(clock_buf, sizeof(clock_buf));
            draw_system_tray(clock_buf);
            gui_flip_rect(TRAY_X, TRAY_Y, TRAY_W, TRAY_H);
        }

        /* Sleep 1 tick (~20ms at 50Hz PIT) to avoid burning CPU */
        yield();
    }
}
