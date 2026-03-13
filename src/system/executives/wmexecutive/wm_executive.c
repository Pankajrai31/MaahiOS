/**
 * MaahiOS Window Manager Executive
 *
 * Description:
 *   Central compositor and window manager for MaahiOS.
 *   Maintains a window registry with z-order, allocates SHM
 *   surfaces for each window, and composites all visible windows
 *   onto the back-buffer when damage is reported.
 *
 *   Communication:
 *     - SHM request/response queues (standard executive pattern)
 *     - Window registry published via cell for Orbit taskbar
 *
 *   Compositing model:
 *     For each damaged rectangle:
 *       1. Paint desktop gradient (wallpaper)
 *       2. For each visible window bottom-to-top in z-order:
 *            blit the intersecting portion of window surface
 *       3. gui_flip_rect() to push to HW framebuffer
 *
 *   This eliminates save-under entirely and supports unlimited
 *   overlapping windows without ghost artifacts.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "wm_executive.h"
#include "../common/executive_queue.h"
#include "../../libraries/core/syscall_helpers.h"
#include "../../libraries/liblog/liblog.h"
#include "../../libraries/libcell/libcell.h"
#include "../../libraries/shared/wm_types.h"
#include "../../libraries/shared/taskbar_types.h"
#include "../ioexecutive/io_executive.h"
#include "../../libraries/libgui/fonts/font8x16.h"

/*=============================================================================
 * DEVICE & IOCTL CONSTANTS
 *===========================================================================*/

#define DEV_DISPLAY             3
#define DEV_MOUSE               1
#define DISPLAY_IOCTL_FLIP      4
#define DISPLAY_IOCTL_FLIP_RECT 5
#define MOUSE_LEFT              0x01

typedef struct {
    int      x;
    int      y;
    uint8_t  buttons;
} mouse_state_t;

/*=============================================================================
 * DESKTOP GRADIENT COLORS  (must match Orbit)
 *===========================================================================*/

#define DESKTOP_TOP     0x003A7BB8
#define DESKTOP_MID     0x005A9BD8
#define DESKTOP_BOT     0x003A8BC8
#define TASKBAR_H       32

/*=============================================================================
 * INTERNAL: WINDOW SLOT
 *===========================================================================*/

typedef struct {
    int      active;              /* 1 = slot in use                        */
    int32_t  pid;                 /* Owner PID                              */
    int32_t  handle;              /* Unique handle (1..)                    */
    int      x, y, w, h;         /* Screen position / size                 */
    uint8_t  state;               /* WM_STATE_*                             */
    uint8_t  z_order;             /* 0 = bottom, higher = closer to top     */
    char     title[WM_TITLE_MAX];
    int      surface_shm_id;     /* SHM containing pixel buffer            */
    uint32_t *surface_ptr;       /* Pointer after SHM attach               */
    /* Pre-maximize saved geometry */
    int      restore_x, restore_y, restore_w, restore_h;
    /* "Not Responding" detection */
    uint32_t last_heartbeat_tick; /* Last tick when we received any command  */
    uint8_t  not_responding;      /* 1 = window is not responding            */
} wm_slot_t;

/*=============================================================================
 * STATE
 *===========================================================================*/

static wm_slot_t     g_windows[WM_MAX_WINDOWS];
static int            g_next_handle = 1;

/* SHM queues */
static exec_request_queue_t  *g_req_queue  = (void *)0;
static exec_response_queue_t *g_resp_queue = (void *)0;
static int g_req_shm_id  = -1;
static int g_resp_shm_id = -1;

/* Display info */
static uint32_t *g_backbuffer  = (uint32_t *)0;
static int        g_scr_w      = 0;
static int        g_scr_h      = 0;

/* Accumulated damage rect (union of all pending damage) */
static int g_dirty = 0;
static int g_dirty_x, g_dirty_y, g_dirty_w, g_dirty_h;

/* Mouse state for input routing */
static mouse_state_t g_mouse;
static uint8_t       g_prev_buttons = 0;
static int           g_focus_handle = -1;

/* Mouse SHM fast path (reads IO Executive's shared mouse state) */
static io_mouse_state_t *g_mouse_shm = (void *)0;

/* Staleness check counter */
static uint32_t g_nr_check_counter = 0;

/*=============================================================================
 * NOT-RESPONDING DIALOG STATE
 *===========================================================================*/

/*-- Dialog layout constants (Design System V2) --*/
#define WM_DLG_W             320   /* Dialog width  */
#define WM_DLG_H             140   /* Dialog height */
#define WM_DLG_TITLE_H        24   /* Titlebar height (matches theme) */
#define WM_DLG_BTN_W           90   /* Button width */
#define WM_DLG_BTN_H           25   /* Button height (matches theme) */
#define WM_DLG_BTN_PAD         12   /* Horizontal pad between buttons */
#define WM_DLG_MARGIN          12   /* Content margin */
#define WM_DLG_BEVEL_W          2   /* Outer bevel border width */
#define WM_DLG_MAX_BUTTONS      3   /* Max buttons per dialog */
#define WM_DLG_LABEL_MAX       24   /* Max chars per button label */

/* Design System V2 themed colors (must match theme.h) */
#define WM_DLG_CHROME       0x00D8DBE8   /* Chrome background        */
#define WM_DLG_CHROME_LT    0x00E8EAF2   /* Chrome lighter            */
#define WM_DLG_CHROME_DK    0x00C0C4D4   /* Chrome darker             */
#define WM_DLG_BEVEL_LIGHT  0x00F4F5FA   /* 3D bevel highlight        */
#define WM_DLG_BEVEL_DARK   0x009498AC   /* 3D bevel shadow           */
#define WM_DLG_SURFACE      0x00FFFFFF   /* Content area bg           */
#define WM_DLG_TB_START     0x001B3F8B   /* Titlebar gradient left    */
#define WM_DLG_TB_END       0x002B5BB5   /* Titlebar gradient right   */
#define WM_DLG_TB_FG        0x00FFFFFF   /* Titlebar text             */
#define WM_DLG_TEXT         0x001A1A2E   /* Body text                 */
#define WM_DLG_TEXT_SEC     0x005A5D76   /* Secondary text            */
#define WM_DLG_SHADOW       0x00505060   /* Drop shadow               */

/**
 * Generic WM-drawn dialog.
 * Fully modular: title, message, buttons and actions are configured by caller.
 * The WM compositor renders it directly onto the backbuffer (no SHM surface).
 */
typedef struct {
    int  active;                       /* 1 = dialog visible */
    int  target_slot_idx;              /* index into g_windows[] the dialog is about */

    /* Content (set by caller) */
    char title[48];                    /* Titlebar text */
    char message[64];                  /* Primary message line */
    char message2[64];                 /* Secondary message line (or empty) */

    /* Buttons (set by caller, 0..WM_DLG_MAX_BUTTONS) */
    int  button_count;
    char btn_labels[WM_DLG_MAX_BUTTONS][WM_DLG_LABEL_MAX];
    int  btn_results[WM_DLG_MAX_BUTTONS]; /* opaque result codes */

    /* Computed geometry (screen coords, set by wm_show_dialog) */
    int  dx, dy;
    int  btn_x[WM_DLG_MAX_BUTTONS];
    int  btn_y;

    /* Interaction */
    int  hover_btn;                    /* -1 = none, 0..N-1 = button index */
    int  pressed_btn;                  /* -1 = none */
    int  result;                       /* set on dismiss, -1 = still open */
} wm_dialog_t;

static wm_dialog_t g_wm_dialog;

/*=============================================================================
 * HELPERS
 *===========================================================================*/

static void yield(void) { syscall0(SYS_YIELD); }
static void sleep_ticks(int t) { syscall1(SYS_SLEEP, t); }
static uint32_t get_ticks(void) { return (uint32_t)syscall0(SYS_TIME_GET_TICKS); }

/* Not-responding timeout: ~4 seconds at 50 Hz PIT = 200 ticks */
#define WM_NR_TIMEOUT_TICKS     200

/* How often to check for staleness (every 25 = 0.5 sec at 50 Hz) */
#define WM_NR_CHECK_INTERVAL    25

static void str_copy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++) dst[i] = src[i];
    dst[i] = '\0';
}

static uint32_t lerp_color(uint32_t c0, uint32_t c1, int pos, int total) {
    if (total <= 1) return c0;
    int r0 = (c0 >> 16) & 0xFF, g0 = (c0 >> 8) & 0xFF, b0 = c0 & 0xFF;
    int r1 = (c1 >> 16) & 0xFF, g1 = (c1 >> 8) & 0xFF, b1 = c1 & 0xFF;
    int r = r0 + (r1 - r0) * pos / (total - 1);
    int g = g0 + (g1 - g0) * pos / (total - 1);
    int b = b0 + (b1 - b0) * pos / (total - 1);
    return (uint32_t)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
}

/* Clip rect to screen bounds, returns 0 if fully clipped */
static int clip_rect(int *x, int *y, int *w, int *h) {
    int x1 = *x, y1 = *y;
    int x2 = x1 + *w, y2 = y1 + *h;
    if (x1 < 0) x1 = 0;
    if (y1 < 0) y1 = 0;
    if (x2 > g_scr_w) x2 = g_scr_w;
    if (y2 > g_scr_h) y2 = g_scr_h;
    *x = x1; *y = y1; *w = x2 - x1; *h = y2 - y1;
    return (*w > 0 && *h > 0);
}

/* Union a dirty rect into the accumulated damage */
static void add_damage(int x, int y, int w, int h) {
    if (!g_dirty) {
        g_dirty_x = x; g_dirty_y = y;
        g_dirty_w = w; g_dirty_h = h;
        g_dirty = 1;
    } else {
        int x2 = g_dirty_x + g_dirty_w;
        int y2 = g_dirty_y + g_dirty_h;
        int nx2 = x + w;
        int ny2 = y + h;
        if (x < g_dirty_x) g_dirty_x = x;
        if (y < g_dirty_y) g_dirty_y = y;
        if (nx2 > x2) x2 = nx2;
        if (ny2 > y2) y2 = ny2;
        g_dirty_w = x2 - g_dirty_x;
        g_dirty_h = y2 - g_dirty_y;
    }
}

/* Full screen damage */
static void damage_full(void) {
    add_damage(0, 0, g_scr_w, g_scr_h);
}

/*=============================================================================
 * WINDOW SLOT MANAGEMENT
 *===========================================================================*/

static wm_slot_t *find_slot_by_handle(int handle) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].handle == handle)
            return &g_windows[i];
    }
    return (wm_slot_t *)0;
}

static wm_slot_t *alloc_slot(void) {
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].active) return &g_windows[i];
    }
    return (wm_slot_t *)0;
}

/* Recalculate z_order so values are contiguous 0..N-1 */
static void recalc_z_order(void) {
    /* Count active windows */
    int count = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++)
        if (g_windows[i].active && g_windows[i].state != WM_STATE_MINIMIZED)
            count++;
    if (count == 0) return;

    /* Simple bubble sort by current z_order, then reassign 0..N-1 */
    int indices[WM_MAX_WINDOWS];
    int zvals[WM_MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].state != WM_STATE_MINIMIZED) {
            indices[n] = i;
            zvals[n] = g_windows[i].z_order;
            n++;
        }
    }
    /* Bubble sort by zval */
    for (int a = 0; a < n - 1; a++) {
        for (int b = a + 1; b < n; b++) {
            if (zvals[a] > zvals[b]) {
                int tmp;
                tmp = indices[a]; indices[a] = indices[b]; indices[b] = tmp;
                tmp = zvals[a]; zvals[a] = zvals[b]; zvals[b] = tmp;
            }
        }
    }
    for (int i = 0; i < n; i++)
        g_windows[indices[i]].z_order = (uint8_t)i;
}

/* Bring a window to the top of z-order */
static void raise_to_top(wm_slot_t *win) {
    uint8_t max_z = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].z_order > max_z)
            max_z = g_windows[i].z_order;
    }
    win->z_order = max_z + 1;
    recalc_z_order();
}

/*=============================================================================
 * TEXT RENDERING (using font8x16 bitmap font)
 *===========================================================================*/

/**
 * Draw a single character at (sx, sy) onto the backbuffer.
 * fg = foreground color, bg = 0xFFFFFFFF means transparent background.
 */
static void wm_draw_char(int sx, int sy, char ch, uint32_t fg, uint32_t bg) {
    if (!g_backbuffer) return;
    int ci = (int)(unsigned char)ch;
    if (ci < 32 || ci > 122) ci = '?';

    const uint8_t *glyph = font_8x16[ci];
    for (int row = 0; row < FONT_CHAR_HEIGHT; row++) {
        int py = sy + row;
        if (py < 0 || py >= g_scr_h) continue;
        uint8_t bits = glyph[row];
        for (int col = 0; col < FONT_CHAR_WIDTH; col++) {
            int px = sx + col;
            if (px < 0 || px >= g_scr_w) continue;
            if (bits & (0x80 >> col)) {
                g_backbuffer[py * g_scr_w + px] = fg;
            } else if (bg != 0xFFFFFFFF) {
                g_backbuffer[py * g_scr_w + px] = bg;
            }
        }
    }
}

/**
 * Draw a null-terminated string at (sx, sy) onto the backbuffer.
 * Returns the x position after the last character.
 */
static int wm_draw_text(int sx, int sy, const char *str, uint32_t fg, uint32_t bg) {
    int x = sx;
    for (int i = 0; str && str[i]; i++) {
        wm_draw_char(x, sy, str[i], fg, bg);
        x += FONT_CHAR_WIDTH;
    }
    return x;
}

/**
 * Measure the pixel width of a string (8 pixels per char).
 */
static int wm_text_width(const char *str) {
    int len = 0;
    while (str && str[len]) len++;
    return len * FONT_CHAR_WIDTH;
}

/**
 * Fill a rectangle in the backbuffer with a solid color.
 */
static void wm_fill_rect(int x, int y, int w, int h, uint32_t color) {
    if (!g_backbuffer) return;
    for (int row = y; row < y + h; row++) {
        if (row < 0 || row >= g_scr_h) continue;
        for (int col = x; col < x + w; col++) {
            if (col < 0 || col >= g_scr_w) continue;
            g_backbuffer[row * g_scr_w + col] = color;
        }
    }
}

/**
 * Draw a 1-pixel border rectangle (outline only).
 */
static void wm_draw_border(int x, int y, int w, int h, uint32_t color) {
    /* Top and bottom edges */
    for (int col = x; col < x + w; col++) {
        if (col >= 0 && col < g_scr_w) {
            if (y >= 0 && y < g_scr_h)
                g_backbuffer[y * g_scr_w + col] = color;
            int by = y + h - 1;
            if (by >= 0 && by < g_scr_h)
                g_backbuffer[by * g_scr_w + col] = color;
        }
    }
    /* Left and right edges */
    for (int row = y; row < y + h; row++) {
        if (row >= 0 && row < g_scr_h) {
            if (x >= 0 && x < g_scr_w)
                g_backbuffer[row * g_scr_w + x] = color;
            int rx = x + w - 1;
            if (rx >= 0 && rx < g_scr_w)
                g_backbuffer[row * g_scr_w + rx] = color;
        }
    }
}

/*=============================================================================
 * NR DIALOG: SHOW / HIDE / RENDER
 *===========================================================================*/

/**
 * Show a generic WM dialog centered on a window slot.
 * All content (title, message, buttons) is provided by the caller.
 * The compositor renders it; no SHM surface needed.
 *
 * @param slot_idx    Window slot the dialog relates to (-1 for screen-center)
 * @param title       Titlebar text
 * @param msg1        Primary message line
 * @param msg2        Secondary message line (NULL or "" to skip)
 * @param btn_count   Number of buttons (0..WM_DLG_MAX_BUTTONS)
 * @param labels      Array of button label strings
 * @param results     Array of opaque result codes (returned on click)
 */
static void wm_show_dialog(int slot_idx, const char *title,
                           const char *msg1, const char *msg2,
                           int btn_count, const char **labels,
                           const int *results) {
    wm_dialog_t *d = &g_wm_dialog;

    d->active = 1;
    d->target_slot_idx = slot_idx;
    d->hover_btn = -1;
    d->pressed_btn = -1;
    d->result = -1;

    /* Copy content */
    str_copy(d->title, title, sizeof(d->title));
    str_copy(d->message, msg1, sizeof(d->message));
    if (msg2) str_copy(d->message2, msg2, sizeof(d->message2));
    else      d->message2[0] = '\0';

    /* Copy buttons */
    d->button_count = (btn_count > WM_DLG_MAX_BUTTONS)
                    ? WM_DLG_MAX_BUTTONS : btn_count;
    for (int i = 0; i < d->button_count; i++) {
        str_copy(d->btn_labels[i], labels[i], WM_DLG_LABEL_MAX);
        d->btn_results[i] = results[i];
    }

    /* Center dialog on the target window (or screen) */
    if (slot_idx >= 0 && slot_idx < WM_MAX_WINDOWS &&
        g_windows[slot_idx].active) {
        wm_slot_t *win = &g_windows[slot_idx];
        d->dx = win->x + (win->w - WM_DLG_W) / 2;
        d->dy = win->y + (win->h - WM_DLG_H) / 2;
    } else {
        d->dx = (g_scr_w - WM_DLG_W) / 2;
        d->dy = (g_scr_h - WM_DLG_H) / 2;
    }

    /* Clamp to screen */
    if (d->dx < 0) d->dx = 0;
    if (d->dy < 0) d->dy = 0;
    if (d->dx + WM_DLG_W > g_scr_w) d->dx = g_scr_w - WM_DLG_W;
    if (d->dy + WM_DLG_H > g_scr_h) d->dy = g_scr_h - WM_DLG_H;

    /* Calculate button positions (centered row at bottom) */
    int total_w = d->button_count * WM_DLG_BTN_W +
                  (d->button_count > 1 ? (d->button_count - 1) * WM_DLG_BTN_PAD : 0);
    int start_x = d->dx + (WM_DLG_W - total_w) / 2;
    d->btn_y = d->dy + WM_DLG_H - WM_DLG_MARGIN - WM_DLG_BTN_H;
    for (int i = 0; i < d->button_count; i++) {
        d->btn_x[i] = start_x + i * (WM_DLG_BTN_W + WM_DLG_BTN_PAD);
    }

    /* Damage the dialog area (plus shadow) */
    add_damage(d->dx, d->dy, WM_DLG_W + 4, WM_DLG_H + 4);
}

/**
 * Convenience: show the NR dialog for a frozen window.
 * Populates the generic dialog with NR-specific content.
 */
static void wm_show_nr_dialog(int slot_idx) {
    wm_slot_t *win = &g_windows[slot_idx];
    if (!win->active) return;

    /* Build message: "<title> is not responding." */
    char msg1[64];
    str_copy(msg1, win->title, sizeof(msg1));

    const char *labels[] = { "Wait", "End Task" };
    const int   codes[]  = { 0, 1 };  /* 0=Wait, 1=EndTask */

    wm_show_dialog(slot_idx, "Not Responding",
                   msg1, "is not responding.",
                   2, labels, codes);
}

/**
 * Dismiss the WM dialog.
 */
static void wm_hide_dialog(void) {
    if (!g_wm_dialog.active) return;
    add_damage(g_wm_dialog.dx, g_wm_dialog.dy, WM_DLG_W + 4, WM_DLG_H + 4);
    g_wm_dialog.active = 0;
}

/*=============================================================================
 * NOT-RESPONDING DIALOG: RENDERING PRIMITIVES & DRAW
 *===========================================================================*/

/**
 * Draw a horizontal line in the backbuffer.
 */
static void wm_draw_hline(int x, int y, int w, uint32_t color) {
    if (!g_backbuffer || y < 0 || y >= g_scr_h) return;
    for (int col = x; col < x + w; col++) {
        if (col >= 0 && col < g_scr_w)
            g_backbuffer[y * g_scr_w + col] = color;
    }
}

/**
 * Draw a vertical line in the backbuffer.
 */
static void wm_draw_vline(int x, int y, int h, uint32_t color) {
    if (!g_backbuffer || x < 0 || x >= g_scr_w) return;
    for (int row = y; row < y + h; row++) {
        if (row >= 0 && row < g_scr_h)
            g_backbuffer[row * g_scr_w + x] = color;
    }
}

/**
 * Draw a 3D raised bevel (Design System V2 embossed style).
 * top/left = bevel_light, bottom/right = bevel_dark
 */
static void wm_draw_raised_bevel(int x, int y, int w, int h) {
    wm_draw_hline(x, y, w, WM_DLG_BEVEL_LIGHT);
    wm_draw_hline(x + 1, y + 1, w - 2, WM_DLG_BEVEL_LIGHT);
    wm_draw_vline(x, y, h, WM_DLG_BEVEL_LIGHT);
    wm_draw_vline(x + 1, y + 1, h - 2, WM_DLG_BEVEL_LIGHT);
    wm_draw_hline(x, y + h - 1, w, WM_DLG_BEVEL_DARK);
    wm_draw_hline(x + 1, y + h - 2, w - 2, WM_DLG_BEVEL_DARK);
    wm_draw_vline(x + w - 1, y, h, WM_DLG_BEVEL_DARK);
    wm_draw_vline(x + w - 2, y + 1, h - 2, WM_DLG_BEVEL_DARK);
}

/**
 * Draw a 3D sunken bevel (pressed button effect).
 * top/left = bevel_dark, bottom/right = bevel_light
 */
static void wm_draw_sunken_bevel(int x, int y, int w, int h) {
    wm_draw_hline(x, y, w, WM_DLG_BEVEL_DARK);
    wm_draw_vline(x, y, h, WM_DLG_BEVEL_DARK);
    wm_draw_hline(x, y + h - 1, w, WM_DLG_BEVEL_LIGHT);
    wm_draw_vline(x + w - 1, y, h, WM_DLG_BEVEL_LIGHT);
}

/**
 * Render the WM dialog onto the backbuffer — Design System V2 style.
 * Gradient blue titlebar, 3D raised bevel, chrome background.
 * All content from g_wm_dialog struct (fully modular).
 * Called from composite() after all windows are blitted.
 */
static void wm_render_dialog(void) {
    wm_dialog_t *d = &g_wm_dialog;
    if (!d->active) return;
    if (d->target_slot_idx >= 0) {
        if (d->target_slot_idx >= WM_MAX_WINDOWS) return;
        wm_slot_t *win = &g_windows[d->target_slot_idx];
        if (!win->active) { d->active = 0; return; }
    }

    int dx = d->dx;
    int dy = d->dy;
    int bw = WM_DLG_BEVEL_W;

    /* 1. Drop shadow */
    wm_fill_rect(dx + 4, dy + 4, WM_DLG_W, WM_DLG_H, WM_DLG_SHADOW);

    /* 2. Chrome background fill */
    wm_fill_rect(dx, dy, WM_DLG_W, WM_DLG_H, WM_DLG_CHROME);

    /* 3. Outer 3D raised bevel border */
    wm_draw_raised_bevel(dx, dy, WM_DLG_W, WM_DLG_H);

    /* 4. Gradient blue titlebar */
    {
        int tb_x = dx + bw;
        int tb_y = dy + bw;
        int tb_w = WM_DLG_W - bw * 2;
        int tb_h = WM_DLG_TITLE_H;
        for (int col = 0; col < tb_w; col++) {
            uint32_t c = lerp_color(WM_DLG_TB_START, WM_DLG_TB_END, col, tb_w);
            wm_draw_vline(tb_x + col, tb_y, tb_h, c);
        }
        /* Title text from struct */
        int text_y = tb_y + (tb_h - FONT_CHAR_HEIGHT) / 2;
        wm_draw_text(tb_x + 8, text_y, d->title, WM_DLG_TB_FG, 0xFFFFFFFF);

        /* Close button (chrome raised box with 'X') */
        int cb_w = 20, cb_h = 18;
        int cb_x = tb_x + tb_w - cb_w - 4;
        int cb_y = tb_y + (tb_h - cb_h) / 2;
        wm_fill_rect(cb_x, cb_y, cb_w, cb_h, WM_DLG_CHROME);
        wm_draw_hline(cb_x, cb_y, cb_w, WM_DLG_BEVEL_LIGHT);
        wm_draw_vline(cb_x, cb_y, cb_h, WM_DLG_BEVEL_LIGHT);
        wm_draw_hline(cb_x, cb_y + cb_h - 1, cb_w, WM_DLG_BEVEL_DARK);
        wm_draw_vline(cb_x + cb_w - 1, cb_y, cb_h, WM_DLG_BEVEL_DARK);
        int cx = cb_x + (cb_w - FONT_CHAR_WIDTH) / 2;
        int cy = cb_y + (cb_h - FONT_CHAR_HEIGHT) / 2 + 1;
        wm_draw_char(cx, cy, 'X', WM_DLG_TEXT, 0xFFFFFFFF);
    }

    /* 5. Content area (white surface, sunken) */
    {
        int ca_x = dx + bw + 4;
        int ca_y = dy + bw + WM_DLG_TITLE_H + 4;
        int ca_w = WM_DLG_W - bw * 2 - 8;
        int ca_h = WM_DLG_H - bw * 2 - WM_DLG_TITLE_H - WM_DLG_BTN_H - 24;
        wm_fill_rect(ca_x, ca_y, ca_w, ca_h, WM_DLG_SURFACE);
        wm_draw_sunken_bevel(ca_x, ca_y, ca_w, ca_h);

        /* Message text from struct */
        int msg_x = ca_x + 8;
        int msg_y = ca_y + 6;
        wm_draw_text(msg_x, msg_y, d->message, WM_DLG_TEXT, 0xFFFFFFFF);
        if (d->message2[0])
            wm_draw_text(msg_x, msg_y + 20, d->message2, WM_DLG_TEXT_SEC, 0xFFFFFFFF);
    }

    /* 6. Buttons — all uniform chrome 3D raised style */
    for (int i = 0; i < d->button_count; i++) {
        int bx = d->btn_x[i];
        int by = d->btn_y;
        uint32_t bg = WM_DLG_CHROME;
        uint32_t hi = WM_DLG_BEVEL_LIGHT;
        uint32_t lo = WM_DLG_BEVEL_DARK;
        int offset = 0;

        if (d->pressed_btn == i) {
            bg = WM_DLG_CHROME_DK;
            hi = WM_DLG_BEVEL_DARK;  /* Invert bevel = sunken */
            lo = WM_DLG_BEVEL_LIGHT;
            offset = 1;
        } else if (d->hover_btn == i) {
            bg = WM_DLG_CHROME_LT;
        }

        wm_fill_rect(bx, by, WM_DLG_BTN_W, WM_DLG_BTN_H, bg);
        wm_draw_hline(bx, by, WM_DLG_BTN_W, hi);
        wm_draw_vline(bx, by, WM_DLG_BTN_H, hi);
        wm_draw_hline(bx, by + WM_DLG_BTN_H - 1, WM_DLG_BTN_W, lo);
        wm_draw_vline(bx + WM_DLG_BTN_W - 1, by, WM_DLG_BTN_H, lo);

        int tw = wm_text_width(d->btn_labels[i]);
        int tx = bx + (WM_DLG_BTN_W - tw) / 2 + offset;
        int ty = by + (WM_DLG_BTN_H - FONT_CHAR_HEIGHT) / 2 + offset;
        wm_draw_text(tx, ty, d->btn_labels[i], WM_DLG_TEXT, 0xFFFFFFFF);
    }
}

/*=============================================================================
 * COMPOSITOR: PAINT DESKTOP GRADIENT IN A RECT
 *===========================================================================*/

static void paint_desktop_rect(int rx, int ry, int rw, int rh) {
    if (!g_backbuffer) return;
    int taskbar_y = g_scr_h - TASKBAR_H;
    int mid = taskbar_y / 2;

    int cx0 = rx, cy0 = ry;
    int cx1 = rx + rw, cy1 = ry + rh;
    if (cx0 < 0) cx0 = 0;
    if (cy0 < 0) cy0 = 0;
    if (cx1 > g_scr_w) cx1 = g_scr_w;
    if (cy1 > g_scr_h) cy1 = g_scr_h;

    for (int row = cy0; row < cy1; row++) {
        uint32_t color;
        if (row < mid)
            color = lerp_color(DESKTOP_TOP, DESKTOP_MID, row, mid);
        else if (row < taskbar_y)
            color = lerp_color(DESKTOP_MID, DESKTOP_BOT, row - mid, taskbar_y - mid);
        else
            color = 0x00D8DBE8; /* Taskbar chrome */

        uint32_t *dst = &g_backbuffer[row * g_scr_w + cx0];
        for (int col = cx0; col < cx1; col++)
            *dst++ = color;
    }
}

/*=============================================================================
 * COMPOSITOR: BLIT ONE WINDOW'S SURFACE ONTO BACKBUFFER (clipped to rect)
 *===========================================================================*/

static void blit_window_in_rect(wm_slot_t *win, int rx, int ry, int rw, int rh) {
    if (!win->surface_ptr || !g_backbuffer) return;
    if (win->state == WM_STATE_MINIMIZED) return;

    /* Intersect damage rect with window rect */
    int wx0 = win->x, wy0 = win->y;
    int wx1 = win->x + win->w, wy1 = win->y + win->h;
    int dx0 = rx, dy0 = ry;
    int dx1 = rx + rw, dy1 = ry + rh;

    int ix0 = (wx0 > dx0) ? wx0 : dx0;
    int iy0 = (wy0 > dy0) ? wy0 : dy0;
    int ix1 = (wx1 < dx1) ? wx1 : dx1;
    int iy1 = (wy1 < dy1) ? wy1 : dy1;

    if (ix0 >= ix1 || iy0 >= iy1) return; /* No intersection */

    /* Clip to screen */
    if (ix0 < 0) ix0 = 0;
    if (iy0 < 0) iy0 = 0;
    if (ix1 > g_scr_w) ix1 = g_scr_w;
    if (iy1 > g_scr_h) iy1 = g_scr_h;

    for (int row = iy0; row < iy1; row++) {
        int src_row = row - win->y;
        int src_col_start = ix0 - win->x;
        int count = ix1 - ix0;
        if (src_row < 0 || src_row >= win->h) continue;
        if (src_col_start < 0) { count += src_col_start; src_col_start = 0; }
        if (src_col_start + count > win->w) count = win->w - src_col_start;
        if (count <= 0) continue;

        uint32_t *src = &win->surface_ptr[src_row * win->w + src_col_start];
        uint32_t *dst = &g_backbuffer[row * g_scr_w + ix0];

        if (win->not_responding) {
            /* Frosted/ghost overlay: blend 50% white on a checkerboard.
             * This produces the classic "Not Responding" washed-out look
             * similar to Windows DWM's ghost window effect. */
            for (int i = 0; i < count; i++) {
                uint32_t px = src[i];
                /* Blend with white: average each channel with 0xFF */
                uint32_t r = ((px >> 16) & 0xFF);
                uint32_t g = ((px >> 8) & 0xFF);
                uint32_t b = (px & 0xFF);
                r = (r + 0xFF) >> 1;
                g = (g + 0xFF) >> 1;
                b = (b + 0xFF) >> 1;
                dst[i] = (r << 16) | (g << 8) | b;
            }
        } else {
            for (int i = 0; i < count; i++)
                dst[i] = src[i];
        }
    }
}

/*=============================================================================
 * COMPOSITOR: COMPOSITE AND FLIP
 *===========================================================================*/

static void composite(int rx, int ry, int rw, int rh) {
    /* Clip to screen */
    if (!clip_rect(&rx, &ry, &rw, &rh)) return;

    /* 1. Paint desktop gradient in the rect */
    paint_desktop_rect(rx, ry, rw, rh);

    /* 2. Gather visible windows sorted by z_order (bottom-up) */
    int order[WM_MAX_WINDOWS];
    int n = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (g_windows[i].active && g_windows[i].state != WM_STATE_MINIMIZED)
            order[n++] = i;
    }
    /* Sort by z_order ascending (bottom first) */
    for (int a = 0; a < n - 1; a++) {
        for (int b = a + 1; b < n; b++) {
            if (g_windows[order[a]].z_order > g_windows[order[b]].z_order) {
                int tmp = order[a]; order[a] = order[b]; order[b] = tmp;
            }
        }
    }

    /* 3. Blit each visible window bottom-to-top */
    for (int i = 0; i < n; i++)
        blit_window_in_rect(&g_windows[order[i]], rx, ry, rw, rh);

    /* 4. Render NR dialog on top of everything */
    wm_render_dialog();

    /* 5. Flip rect to HW framebuffer */
    int rect[4] = { rx, ry, rw, rh };
    syscall3(SYS_DEV_IOCTL, DEV_DISPLAY, DISPLAY_IOCTL_FLIP_RECT,
             (int)(uint32_t)rect);
}

static void flush_damage(void) {
    if (!g_dirty) return;
    composite(g_dirty_x, g_dirty_y, g_dirty_w, g_dirty_h);
    g_dirty = 0;
}

/*=============================================================================
 * PUBLISH WINDOW REGISTRY TO CELL (for Orbit taskbar)
 *===========================================================================*/

static void publish_registry(void) {
    wm_window_registry_t reg;
    exe_memset(&reg, 0, sizeof(reg));
    reg.count = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].active) continue;
        wm_window_entry_t *e = &reg.windows[reg.count];
        e->pid     = g_windows[i].pid;
        e->handle  = g_windows[i].handle;
        e->x       = (int16_t)g_windows[i].x;
        e->y       = (int16_t)g_windows[i].y;
        e->w       = (int16_t)g_windows[i].w;
        e->h       = (int16_t)g_windows[i].h;
        e->state   = g_windows[i].state;
        e->z_order = g_windows[i].z_order;
        e->minimized = (g_windows[i].state == WM_STATE_MINIMIZED) ? 1 : 0;
        e->not_responding = g_windows[i].not_responding;
        str_copy(e->title, g_windows[i].title, WM_TITLE_MAX);
        reg.count++;
    }

    uint32_t sz = (uint32_t)(sizeof(int32_t) +
                  reg.count * sizeof(wm_window_entry_t));
    libcell_write(CELL_WM_REGISTRY, &reg, sz);

    /* Also update the legacy taskbar cell so Orbit can read it unchanged */
    taskbar_window_list_t tbar;
    exe_memset(&tbar, 0, sizeof(tbar));
    tbar.count = 0;
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        if (!g_windows[i].active) continue;
        if (tbar.count >= TASKBAR_MAX_WINDOWS) break;
        taskbar_entry_t *te = &tbar.entries[tbar.count];
        te->pid = g_windows[i].pid;
        str_copy(te->title, g_windows[i].title, TASKBAR_TITLE_MAX);
        te->minimized = (g_windows[i].state == WM_STATE_MINIMIZED) ? 1 : 0;
        tbar.count++;
    }
    uint32_t tsz = (uint32_t)(sizeof(int32_t) +
                   tbar.count * sizeof(taskbar_entry_t));
    libcell_write(CELL_TASKBAR_WINDOWS, &tbar, tsz);
}

/*=============================================================================
 * COMMAND HANDLERS
 *===========================================================================*/

static void handle_create(const exec_request_t *req, exec_response_t *resp) {
    const wm_create_payload_t *p = (const wm_create_payload_t *)req->payload;

    wm_slot_t *slot = alloc_slot();
    if (!slot) {
        resp->status = EXEC_ERR_NO_MEMORY;
        return;
    }

    int handle = g_next_handle++;
    int surface_bytes = p->w * p->h * 4;

    /* Create SHM for the window surface */
    int shm_id = syscall1(SYS_SHM_CREATE, surface_bytes);
    if (shm_id < 0) {
        resp->status = EXEC_ERR_NO_MEMORY;
        return;
    }

    /* Attach SHM so WM can read the surface */
    uint32_t *ptr = (uint32_t *)syscall2(SYS_SHM_ATTACH, shm_id, 0);
    if (!ptr) {
        syscall1(SYS_SHM_DESTROY, shm_id);
        resp->status = EXEC_ERR_NO_MEMORY;
        return;
    }

    /* Clear surface to white */
    int pixel_count = p->w * p->h;
    for (int i = 0; i < pixel_count; i++) ptr[i] = 0x00FFFFFF;

    /* Fill slot */
    exe_memset(slot, 0, sizeof(wm_slot_t));
    slot->active         = 1;
    slot->pid            = (int32_t)req->sender_pid;
    slot->handle         = handle;
    slot->x              = p->x;
    slot->y              = p->y;
    slot->w              = p->w;
    slot->h              = p->h;
    slot->state          = WM_STATE_NORMAL;
    slot->surface_shm_id = shm_id;
    slot->surface_ptr    = ptr;
    str_copy(slot->title, p->title, WM_TITLE_MAX);
    slot->last_heartbeat_tick = get_ticks();
    slot->not_responding = 0;

    /* New window goes on top */
    raise_to_top(slot);

    /* Focus the new window */
    g_focus_handle = handle;

    /* Damage the window's rect so it gets composited */
    add_damage(slot->x, slot->y, slot->w, slot->h);

    /* Build response */
    wm_create_response_t *rp = (wm_create_response_t *)resp->payload;
    rp->handle         = handle;
    rp->surface_shm_id = shm_id;
    resp->status        = EXEC_OK;
    resp->result        = (uint32_t)handle;
    resp->payload_size  = sizeof(wm_create_response_t);

    publish_registry();
    liblog_hex(LOG_INFO, "WM", "Window created, handle:", (uint32_t)handle);
}

static void handle_destroy(const exec_request_t *req, exec_response_t *resp) {
    const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    /* Damage the area first so it gets redrawn after removal */
    add_damage(slot->x, slot->y, slot->w, slot->h);

    /* Detach and destroy SHM surface */
    if (slot->surface_ptr) {
        syscall1(SYS_SHM_DETACH, slot->surface_shm_id);
        syscall1(SYS_SHM_DESTROY, slot->surface_shm_id);
    }

    /* Clear focus if this was focused */
    if (g_focus_handle == slot->handle) {
        g_focus_handle = -1;
        /* Give focus to the next topmost window */
        int max_z = -1;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (g_windows[i].active && g_windows[i].handle != slot->handle &&
                g_windows[i].state != WM_STATE_MINIMIZED &&
                (int)g_windows[i].z_order > max_z) {
                max_z = g_windows[i].z_order;
                g_focus_handle = g_windows[i].handle;
            }
        }
    }

    slot->active = 0;
    recalc_z_order();
    publish_registry();

    resp->status = EXEC_OK;
    liblog_hex(LOG_INFO, "WM", "Window destroyed, handle:", (uint32_t)p->handle);
}

static void handle_move(const exec_request_t *req, exec_response_t *resp) {
    const wm_move_payload_t *p = (const wm_move_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    /* Damage old position */
    add_damage(slot->x, slot->y, slot->w, slot->h);
    slot->x = p->new_x;
    slot->y = p->new_y;

    /* A moved window is always brought to front + focused */
    raise_to_top(slot);
    g_focus_handle = slot->handle;

    /* Damage new position */
    add_damage(slot->x, slot->y, slot->w, slot->h);

    publish_registry();
    resp->status = EXEC_OK;
}

static void handle_damage(const exec_request_t *req, exec_response_t *resp) {
    const wm_damage_payload_t *p = (const wm_damage_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    /* Clamp damage rect to window bounds */
    int dx = p->x, dy = p->y, dw = p->w, dh = p->h;
    if (dx < 0) { dw += dx; dx = 0; }
    if (dy < 0) { dh += dy; dy = 0; }
    if (dx + dw > slot->w) dw = slot->w - dx;
    if (dy + dh > slot->h) dh = slot->h - dy;
    if (dw <= 0 || dh <= 0) { resp->status = EXEC_OK; return; }

    /* Convert window-local rect to screen coords */
    int sx = slot->x + dx;
    int sy = slot->y + dy;
    add_damage(sx, sy, dw, dh);
    resp->status = EXEC_OK;
}

static void handle_minimize(const exec_request_t *req, exec_response_t *resp) {
    const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    slot->state = WM_STATE_MINIMIZED;
    add_damage(slot->x, slot->y, slot->w, slot->h);

    /* Move focus to next topmost */
    if (g_focus_handle == slot->handle) {
        g_focus_handle = -1;
        int max_z = -1;
        for (int i = 0; i < WM_MAX_WINDOWS; i++) {
            if (g_windows[i].active && g_windows[i].handle != slot->handle &&
                g_windows[i].state != WM_STATE_MINIMIZED &&
                (int)g_windows[i].z_order > max_z) {
                max_z = g_windows[i].z_order;
                g_focus_handle = g_windows[i].handle;
            }
        }
    }

    recalc_z_order();
    publish_registry();
    resp->status = EXEC_OK;
}

static void handle_restore(const exec_request_t *req, exec_response_t *resp) {
    const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    if (slot->state == WM_STATE_MINIMIZED) {
        slot->state = WM_STATE_NORMAL;
    } else if (slot->state == WM_STATE_MAXIMIZED) {
        slot->state = WM_STATE_NORMAL;
        slot->x = slot->restore_x;
        slot->y = slot->restore_y;
        slot->w = slot->restore_w;
        slot->h = slot->restore_h;
    }

    raise_to_top(slot);
    g_focus_handle = slot->handle;
    add_damage(slot->x, slot->y, slot->w, slot->h);
    publish_registry();
    resp->status = EXEC_OK;
}

static void handle_maximize(const exec_request_t *req, exec_response_t *resp) {
    const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    if (slot->state == WM_STATE_MAXIMIZED) {
        /* Un-maximize */
        add_damage(slot->x, slot->y, slot->w, slot->h);
        slot->state = WM_STATE_NORMAL;
        slot->x = slot->restore_x;
        slot->y = slot->restore_y;
        slot->w = slot->restore_w;
        slot->h = slot->restore_h;
    } else {
        /* Maximize */
        slot->restore_x = slot->x;
        slot->restore_y = slot->y;
        slot->restore_w = slot->w;
        slot->restore_h = slot->h;
        add_damage(slot->x, slot->y, slot->w, slot->h);
        slot->x = 0;
        slot->y = 0;
        slot->w = g_scr_w;
        slot->h = g_scr_h - TASKBAR_H;
        slot->state = WM_STATE_MAXIMIZED;
    }

    /* Reallocate surface SHM for new size */
    if (slot->surface_ptr) {
        syscall1(SYS_SHM_DETACH, slot->surface_shm_id);
        syscall1(SYS_SHM_DESTROY, slot->surface_shm_id);
    }
    int new_bytes = slot->w * slot->h * 4;
    int new_shm = syscall1(SYS_SHM_CREATE, new_bytes);
    if (new_shm >= 0) {
        uint32_t *new_ptr = (uint32_t *)syscall2(SYS_SHM_ATTACH, new_shm, 0);
        if (new_ptr) {
            int cnt = slot->w * slot->h;
            for (int i = 0; i < cnt; i++) new_ptr[i] = 0x00FFFFFF;
            slot->surface_shm_id = new_shm;
            slot->surface_ptr    = new_ptr;
        }
    }

    raise_to_top(slot);
    g_focus_handle = slot->handle;
    add_damage(slot->x, slot->y, slot->w, slot->h);
    publish_registry();

    /* Return new SHM ID so client can remap */
    wm_create_response_t *rp = (wm_create_response_t *)resp->payload;
    rp->handle         = slot->handle;
    rp->surface_shm_id = slot->surface_shm_id;
    resp->status        = EXEC_OK;
    resp->result        = (uint32_t)slot->handle;
    resp->payload_size  = sizeof(wm_create_response_t);
}

static void handle_raise(const exec_request_t *req, exec_response_t *resp) {
    const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    raise_to_top(slot);
    g_focus_handle = slot->handle;
    add_damage(slot->x, slot->y, slot->w, slot->h);
    publish_registry();
    resp->status = EXEC_OK;
}

static void handle_set_title(const exec_request_t *req, exec_response_t *resp) {
    const wm_title_payload_t *p = (const wm_title_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    str_copy(slot->title, p->title, WM_TITLE_MAX);
    publish_registry();
    resp->status = EXEC_OK;
}

static void handle_full_damage(const exec_request_t *req, exec_response_t *resp) {
    (void)req;
    damage_full();
    resp->status = EXEC_OK;
}

static void handle_heartbeat(const exec_request_t *req, exec_response_t *resp) {
    const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
    wm_slot_t *slot = find_slot_by_handle(p->handle);
    if (!slot) { resp->status = EXEC_ERR_NOT_FOUND; return; }

    slot->last_heartbeat_tick = get_ticks();
    /* If the window was previously not-responding, it's back alive */
    if (slot->not_responding) {
        slot->not_responding = 0;
        /* Dismiss NR dialog if it was targeting this window */
        if (g_wm_dialog.active &&
            g_wm_dialog.target_slot_idx >= 0 &&
            g_wm_dialog.target_slot_idx < WM_MAX_WINDOWS &&
            &g_windows[g_wm_dialog.target_slot_idx] == slot) {
            wm_hide_dialog();
        }
        /* Redamage to remove the ghost overlay */
        add_damage(slot->x, slot->y, slot->w, slot->h);
        publish_registry();
        liblog(LOG_INFO, "WM", "Window responding again");
    }
    resp->status = EXEC_OK;
}

/*=============================================================================
 * COMMAND DISPATCHER
 *===========================================================================*/

static void dispatch(const exec_request_t *req, exec_response_t *resp) {
    exe_memset(resp, 0, sizeof(exec_response_t));
    resp->msg_id = req->msg_id;

    switch (req->func_id) {
        case WM_CMD_CREATE:     handle_create(req, resp);     break;
        case WM_CMD_DESTROY:    handle_destroy(req, resp);    break;
        case WM_CMD_MOVE:       handle_move(req, resp);       break;
        case WM_CMD_DAMAGE:     handle_damage(req, resp);     break;
        case WM_CMD_MINIMIZE:   handle_minimize(req, resp);   break;
        case WM_CMD_RESTORE:    handle_restore(req, resp);    break;
        case WM_CMD_MAXIMIZE:   handle_maximize(req, resp);   break;
        case WM_CMD_RAISE:      handle_raise(req, resp);      break;
        case WM_CMD_SET_TITLE:  handle_set_title(req, resp);  break;
        case WM_CMD_FULL_DAMAGE: handle_full_damage(req, resp); break;
        case WM_CMD_HEARTBEAT:  handle_heartbeat(req, resp);  break;
        default:
            resp->status = EXEC_ERR_INVALID;
            break;
    }

    /* Update heartbeat tick for any command targeting a window.
     * This ensures that any WM activity counts as "alive". */
    if (req->func_id != WM_CMD_FULL_DAMAGE &&
        req->func_id != WM_CMD_CREATE) {
        const wm_handle_payload_t *p = (const wm_handle_payload_t *)req->payload;
        wm_slot_t *slot = find_slot_by_handle(p->handle);
        if (slot) {
            slot->last_heartbeat_tick = get_ticks();
            if (slot->not_responding) {
                slot->not_responding = 0;
                /* Dismiss NR dialog if targeting this window */
                for (int di = 0; di < WM_MAX_WINDOWS; di++) {
                    if (&g_windows[di] == slot && g_wm_dialog.active &&
                        g_wm_dialog.target_slot_idx == di) {
                        wm_hide_dialog();
                        break;
                    }
                }
                add_damage(slot->x, slot->y, slot->w, slot->h);
                publish_registry();
            }
        }
    }
}

/*=============================================================================
 * NOTE: Taskbar restore is handled by the CLIENT (libwindow).
 * The client polls CELL_TASKBAR_RESTORE and calls WM_CMD_RESTORE.
 * We do NOT poll here to avoid a race condition where the WM clears
 * the restore signal before the client sees it, leaving the client
 * stuck in its minimize loop.
 *===========================================================================*/

/*=============================================================================
 * MOUSE SHM FAST PATH (reads IO Executive's shared mouse state)
 *===========================================================================*/

/**
 * Try to discover and attach to the IO Executive's mouse state SHM.
 * Called lazily — retries until successful.
 */
static void wm_init_mouse_shm(void) {
    if (g_mouse_shm) return;

    int shm_id = -1;
    libcell_read("system.io.mouse.state_shm", &shm_id, sizeof(int));
    if (shm_id < 0) return;

    io_mouse_state_t *st = (io_mouse_state_t *)syscall2(SYS_SHM_ATTACH,
                                                          shm_id, 0);
    if (!st || (uint32_t)st == 0xFFFFFFFF) return;
    g_mouse_shm = st;
    liblog(LOG_INFO, "WM", "Mouse SHM attached for NR detection");
}

/**
 * Poll mouse state from the IO Executive's SHM slot.
 * Updates g_mouse and g_prev_buttons.
 */
static void wm_poll_mouse(void) {
    if (!g_mouse_shm) {
        wm_init_mouse_shm();
        if (!g_mouse_shm) return;
    }

    /* Read with seq consistency check */
    uint32_t seq1 = g_mouse_shm->seq;
    __asm__ volatile("" ::: "memory");
    g_mouse.x       = g_mouse_shm->x;
    g_mouse.y       = g_mouse_shm->y;
    g_mouse.buttons  = g_mouse_shm->buttons;
    __asm__ volatile("" ::: "memory");
    uint32_t seq2 = g_mouse_shm->seq;
    if (seq1 != seq2) {
        g_mouse.x       = g_mouse_shm->x;
        g_mouse.y       = g_mouse_shm->y;
        g_mouse.buttons  = g_mouse_shm->buttons;
    }
}

/*=============================================================================
 * NOT-RESPONDING: STALENESS CHECK
 *===========================================================================*/

#define NR_TITLEBAR_H   24   /* Must match THEME_TITLEBAR_HEIGHT */
#define NR_CLOSE_BTN_W  24   /* Approximate close button hit area */

/**
 * Check all windows for staleness. If a window hasn't sent any command
 * for WM_NR_TIMEOUT_TICKS, mark it as "Not Responding" and draw
 * a ghost overlay.
 */
static void wm_check_staleness(void) {
    uint32_t now = get_ticks();
    int changed = 0;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_slot_t *slot = &g_windows[i];
        if (!slot->active) continue;
        if (slot->state == WM_STATE_MINIMIZED) continue;

        /* Skip if already flagged or if first_heartbeat hasn't been set */
        if (slot->last_heartbeat_tick == 0) {
            /* Window just created — seed with current time */
            slot->last_heartbeat_tick = now;
            continue;
        }

        uint32_t age = now - slot->last_heartbeat_tick;
        if (age > WM_NR_TIMEOUT_TICKS && !slot->not_responding) {
            slot->not_responding = 1;
            add_damage(slot->x, slot->y, slot->w, slot->h);
            changed = 1;
            liblog(LOG_WARN, "WM", "Window not responding!");
            liblog_hex(LOG_WARN, "WM", "  handle:", (uint32_t)slot->handle);
            liblog_hex(LOG_WARN, "WM", "  pid:", (uint32_t)slot->pid);

            /* Show the NR dialog if none is already active */
            if (!g_wm_dialog.active) {
                wm_show_nr_dialog(i);
            }
        }
    }

    if (changed) publish_registry();
}

/**
 * Kill a not-responding window's owner process and clean up ALL windows
 * belonging to that process. Called when user clicks "End Task" on the
 * NR dialog.
 */
static void wm_kill_nr_window(wm_slot_t *slot) {
    int32_t target_pid = slot->pid;
    liblog_hex(LOG_WARN, "WM", "Killing NR process, pid:", (uint32_t)target_pid);

    /* Kill the process */
    syscall1(SYS_PROCESS_KILL, (int)target_pid);

    /* Clean up ALL windows belonging to this PID */
    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_slot_t *s = &g_windows[i];
        if (!s->active || s->pid != target_pid) continue;

        /* Damage the area for repaint */
        add_damage(s->x, s->y, s->w, s->h);

        /* Detach and destroy SHM surface */
        if (s->surface_ptr) {
            syscall1(SYS_SHM_DETACH, s->surface_shm_id);
            syscall1(SYS_SHM_DESTROY, s->surface_shm_id);
            s->surface_ptr = (void *)0;
        }

        /* Clear focus if this was focused */
        if (g_focus_handle == s->handle) {
            g_focus_handle = -1;
        }

        s->active = 0;
    }

    /* Find new focus window */
    if (g_focus_handle == -1) {
        int max_z = -1;
        for (int j = 0; j < WM_MAX_WINDOWS; j++) {
            if (g_windows[j].active &&
                g_windows[j].state != WM_STATE_MINIMIZED &&
                (int)g_windows[j].z_order > max_z) {
                max_z = g_windows[j].z_order;
                g_focus_handle = g_windows[j].handle;
            }
        }
    }

    recalc_z_order();
    publish_registry();

    /* Dismiss dialog */
    wm_hide_dialog();
}

/**
 * Handle mouse click when the WM dialog is active or on NR windows.
 * Uses generic button indices — the dialog struct defines the meaning.
 * For NR dialogs: result 0 = Wait, result 1 = End Task.
 */
static void wm_handle_dialog_click(int mx, int my) {
    wm_dialog_t *d = &g_wm_dialog;

    /* If dialog is active, check dialog buttons first */
    if (d->active) {
        /* Check each button generically */
        for (int i = 0; i < d->button_count; i++) {
            if (mx >= d->btn_x[i] &&
                mx < d->btn_x[i] + WM_DLG_BTN_W &&
                my >= d->btn_y &&
                my < d->btn_y + WM_DLG_BTN_H) {

                int result = d->btn_results[i];
                int slot_idx = d->target_slot_idx;

                /* NR dialog actions: result 0 = Wait, 1 = End Task */
                if (result == 0) {
                    /* Wait — clear NR, reset heartbeat */
                    if (slot_idx >= 0 && slot_idx < WM_MAX_WINDOWS) {
                        wm_slot_t *win = &g_windows[slot_idx];
                        if (win->active) {
                            win->not_responding = 0;
                            win->last_heartbeat_tick = get_ticks();
                            add_damage(win->x, win->y, win->w, win->h);
                            publish_registry();
                            liblog(LOG_INFO, "WM", "User chose Wait");
                        }
                    }
                    wm_hide_dialog();
                } else if (result == 1) {
                    /* End Task — kill process and clean up */
                    if (slot_idx >= 0 && slot_idx < WM_MAX_WINDOWS) {
                        wm_slot_t *win = &g_windows[slot_idx];
                        if (win->active) {
                            liblog(LOG_WARN, "WM", "User chose End Task");
                            wm_kill_nr_window(win);
                        } else {
                            wm_hide_dialog();
                        }
                    } else {
                        wm_hide_dialog();
                    }
                } else {
                    /* Unknown result code — just dismiss */
                    wm_hide_dialog();
                }
                return;
            }
        }

        /* Click inside dialog but not on a button — ignore */
        if (mx >= d->dx && mx < d->dx + WM_DLG_W &&
            my >= d->dy && my < d->dy + WM_DLG_H) {
            return;
        }

        /* Click outside dialog — dismiss (treat as Wait for NR) */
        int slot_idx = d->target_slot_idx;
        if (slot_idx >= 0 && slot_idx < WM_MAX_WINDOWS) {
            wm_slot_t *win = &g_windows[slot_idx];
            if (win->active) {
                win->not_responding = 0;
                win->last_heartbeat_tick = get_ticks();
                add_damage(win->x, win->y, win->w, win->h);
                publish_registry();
            }
        }
        wm_hide_dialog();
        return;
    }

    /* No dialog active — check if click is on an NR window */
    int max_z = -1;
    int top_idx = -1;

    for (int i = 0; i < WM_MAX_WINDOWS; i++) {
        wm_slot_t *slot = &g_windows[i];
        if (!slot->active || !slot->not_responding) continue;
        if (slot->state == WM_STATE_MINIMIZED) continue;

        if (mx >= slot->x && mx < slot->x + slot->w &&
            my >= slot->y && my < slot->y + slot->h) {
            if ((int)slot->z_order > max_z) {
                max_z = slot->z_order;
                top_idx = i;
            }
        }
    }

    if (top_idx >= 0) {
        wm_show_nr_dialog(top_idx);
    }
}

/**
 * Update NR dialog hover state based on mouse position.
 * Called every frame to update button highlights.
 */
static void wm_update_dialog_hover(int mx, int my) {
    wm_dialog_t *d = &g_wm_dialog;
    if (!d->active) return;

    int old_hover = d->hover_btn;
    d->hover_btn = -1;

    /* Hit-test each button generically */
    for (int i = 0; i < d->button_count; i++) {
        if (mx >= d->btn_x[i] &&
            mx < d->btn_x[i] + WM_DLG_BTN_W &&
            my >= d->btn_y &&
            my < d->btn_y + WM_DLG_BTN_H) {
            d->hover_btn = i;
            break;
        }
    }

    /* If hover changed, damage the dialog area for repaint */
    if (old_hover != d->hover_btn) {
        add_damage(d->dx, d->dy, WM_DLG_W + 4, WM_DLG_H + 4);
    }
}

/*=============================================================================
 * INITIALIZATION
 *===========================================================================*/

static int init_shm_queues(void) {
    /* Create request queue SHM */
    g_req_shm_id = syscall1(SYS_SHM_CREATE, (int)sizeof(exec_request_queue_t));
    if (g_req_shm_id < 0) return -1;

    g_req_queue = (exec_request_queue_t *)syscall2(SYS_SHM_ATTACH, g_req_shm_id, 0);
    if (!g_req_queue) return -1;
    exe_request_queue_init(g_req_queue);

    /* Create response queue SHM */
    g_resp_shm_id = syscall1(SYS_SHM_CREATE, (int)sizeof(exec_response_queue_t));
    if (g_resp_shm_id < 0) return -1;

    g_resp_queue = (exec_response_queue_t *)syscall2(SYS_SHM_ATTACH, g_resp_shm_id, 0);
    if (!g_resp_queue) return -1;
    exe_response_queue_init(g_resp_queue);

    /* Publish SHM IDs via cells */
    libcell_write(CELL_WM_REQ_QUEUE, &g_req_shm_id, sizeof(int));
    libcell_write(CELL_WM_RESP_QUEUE, &g_resp_shm_id, sizeof(int));

    return 0;
}

static int init_display(void) {
    /* Read framebuffer (back buffer) pointer from GUI executive cells */
    uint32_t fb_addr = 0, w = 0, h = 0;
    for (int attempt = 0; attempt < 50; attempt++) {
        syscall3(SYS_CELL_READ, (uint32_t)"system.gui.framebuffer",
                 (uint32_t)&fb_addr, sizeof(uint32_t));
        syscall3(SYS_CELL_READ, (uint32_t)"system.gui.width",
                 (uint32_t)&w, sizeof(uint32_t));
        syscall3(SYS_CELL_READ, (uint32_t)"system.gui.height",
                 (uint32_t)&h, sizeof(uint32_t));
        if (fb_addr && w && h) break;
        yield(); sleep_ticks(1);
    }
    if (!fb_addr || !w || !h) return -1;

    g_backbuffer = (uint32_t *)fb_addr;
    g_scr_w = (int)w;
    g_scr_h = (int)h;
    return 0;
}

/*=============================================================================
 * MAIN ENTRY
 *===========================================================================*/

void wm_executive_main(void) {
    liblog(LOG_INFO, "WM", "========================================");
    liblog(LOG_INFO, "WM", "  Window Manager Executive Starting");
    liblog(LOG_INFO, "WM", "========================================");

    /* Initialize display info */
    if (init_display() < 0) {
        liblog(LOG_ERROR, "WM", "Display init failed!");
        while (1) yield();
    }
    liblog_hex(LOG_INFO, "WM", "Screen width:", (uint32_t)g_scr_w);
    liblog_hex(LOG_INFO, "WM", "Screen height:", (uint32_t)g_scr_h);

    /* Initialize SHM command queues */
    if (init_shm_queues() < 0) {
        liblog(LOG_ERROR, "WM", "SHM queue init failed!");
        while (1) yield();
    }
    liblog(LOG_INFO, "WM", "SHM queues ready");

    /* Clear window slots */
    exe_memset(g_windows, 0, sizeof(g_windows));

    /* Publish empty registries */
    publish_registry();

    /* Signal that WM is ready */
    int32_t ready = 1;
    libcell_write(CELL_WM_READY, &ready, sizeof(ready));
    liblog(LOG_INFO, "WM", "WM Executive running");

    /* ---- Main event loop ---- */
    exec_request_t req;
    exec_response_t resp;

    while (1) {
        /* Process all pending commands */
        while (exe_request_queue_pop(g_req_queue, &req) == EXEC_OK) {
            dispatch(&req, &resp);
            /* Only push response if caller expects one (sync requests) */
            if (!(req.flags & EXEC_FLAG_NO_RESPONSE))
                exe_response_queue_push(g_resp_queue, &resp);
        }

        /* ---- Mouse polling for Not-Responding dialog interaction ---- */
        uint8_t old_buttons = g_prev_buttons;
        wm_poll_mouse();
        int left_now  = g_mouse.buttons & MOUSE_LEFT;
        int left_down = (left_now && !(old_buttons & MOUSE_LEFT));
        g_prev_buttons = g_mouse.buttons;

        /* Update dialog hover state (for button highlights) */
        wm_update_dialog_hover(g_mouse.x, g_mouse.y);

        if (left_down) {
            wm_handle_dialog_click(g_mouse.x, g_mouse.y);
        }

        /* ---- Periodic staleness check (~every 0.5 sec) ---- */
        g_nr_check_counter++;
        if (g_nr_check_counter >= WM_NR_CHECK_INTERVAL) {
            g_nr_check_counter = 0;
            wm_check_staleness();
        }

        /* Flush all accumulated damage */
        flush_damage();

        yield();
    }
}
