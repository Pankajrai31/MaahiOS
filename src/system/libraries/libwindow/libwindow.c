/**
 * MaahiOS Window Library - Implementation (Design System v2)
 *
 * Description:
 *   Creates V2 embossed chrome windows: gradient blue titlebar,
 *   raised 3D bevel border, chrome caption buttons (─ □ ✕) on the
 *   right, white content area.  Manages child controls, renders to
 *   an internal surface, blits to framebuffer.
 *
 *   Polls BOTH keyboard (via libgui/keyboard) and mouse (via
 *   SYS_DEV_READ on DEV_MOUSE) and dispatches events to controls.
 *
 *   Phase 0 = direct framebuffer.  Phase 1 = SHM + compositing.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "libwindow.h"
#include "../libgui/libgui.h"
#include "../libgui/keyboard/keyboard.h"
#include "../core/syscall_helpers.h"
#include "../libcell/libcell.h"
#include "../libwm/libwm.h"
#include "../libio/libio.h"
#include "../shared/taskbar_types.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * MOUSE CONSTANTS  (matches kernel drivers/mouse/mouse.h)
 *===========================================================================*/

#define DEV_MOUSE   1
#define MOUSE_LEFT  0x01

/* Ioctl commands (must match device_manager.h) */
#define MOUSE_IOCTL_CURSOR_HIDE  4
#define MOUSE_IOCTL_CURSOR_SHOW  5

typedef struct {
    int      x;
    int      y;
    uint8_t  buttons;
} mouse_state_t;

/*=============================================================================
 * INTERNAL: STRING HELPERS
 *===========================================================================*/

static int str_len(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static void str_copy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++) {
        dst[i] = src[i];
    }
    dst[i] = '\0';
}

/*=============================================================================
 * INTERNAL: COLOR HELPERS
 *===========================================================================*/

/** SYS_GETPID — needed for taskbar registration */
#ifndef SYS_GETPID
#define SYS_GETPID 2
#endif

/** Linearly interpolate between two 0x00RRGGBB colors. */
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
 * INTERNAL: TASKBAR RESTORE CHECK
 *
 * Kept for minimize/restore flow — client polls for Orbit's restore signal.
 * Registration/unregistration is now handled by the WM Executive.
 *===========================================================================*/

/** Check if orbit has requested us to restore (via restore cell). */
static int taskbar_check_restore(void) {
    int32_t my_pid = (int32_t)syscall0(SYS_GETPID);

    taskbar_restore_t restore;
    int rd = libcell_read(CELL_TASKBAR_RESTORE, &restore, sizeof(restore));
    if (rd < (int)sizeof(int32_t)) return 0;

    if (restore.pid == my_pid) {
        /* Clear the restore signal */
        restore.pid = 0;
        libcell_write(CELL_TASKBAR_RESTORE, &restore, sizeof(restore));
        return 1;
    }
    return 0;
}

/*=============================================================================
 * INTERNAL: DRAW 3D RAISED / SUNKEN BEVEL
 *
 * Raised: top/left = bevel_light, bottom/right = bevel_dark
 *===========================================================================*/

static void draw_raised_bevel(surface_t *surf, int x, int y, int w, int h) {
    /* Top edge */
    surface_draw_hline(surf, x, y, w, THEME_BEVEL_LIGHT);
    /* Left edge */
    surface_draw_vline(surf, x, y, h, THEME_BEVEL_LIGHT);
    /* Bottom edge */
    surface_draw_hline(surf, x, y + h - 1, w, THEME_BEVEL_DARK);
    /* Right edge */
    surface_draw_vline(surf, x + w - 1, y, h, THEME_BEVEL_DARK);
}

/*=============================================================================
 * INTERNAL: DRAW TITLEBAR (V2 gradient + chrome caption buttons)
 *===========================================================================*/

static void draw_titlebar(window_t *win) {
    surface_t *surf = &win->surface;
    int bw = 2;                     /* outer bevel border width        */
    int tb_x = bw;                  /* titlebar left, inside border    */
    int tb_y = bw;                  /* titlebar top, inside border     */
    int tb_w = win->width - bw * 2; /* titlebar width                  */
    int tb_h = THEME_TITLEBAR_HEIGHT;

    /* ---- Horizontal gradient background (column by column) ---- */
    for (int col = 0; col < tb_w; col++) {
        uint32_t c = lerp_color(THEME_TITLEBAR_START, THEME_TITLEBAR_END,
                                col, tb_w);
        surface_draw_vline(surf, tb_x + col, tb_y, tb_h, c);
    }

    /* ---- Caption buttons on the RIGHT side: ─  □  ✕ ---- */
    int btn_w = THEME_TITLEBAR_BTN_W;
    int btn_h = THEME_TITLEBAR_BTN_H;
    int gap   = THEME_TITLEBAR_BTN_GAP;
    int btn_y = tb_y + (tb_h - btn_h) / 2;

    /* Count visible buttons */
    int n_btns = 0;
    if (!(win->flags & WIN_FLAG_NO_CLOSE))    n_btns++;
    if (!(win->flags & WIN_FLAG_NO_MAXIMIZE))  n_btns++;
    if (!(win->flags & WIN_FLAG_NO_MINIMIZE))  n_btns++;

    int btns_total_w = n_btns * btn_w + (n_btns > 1 ? (n_btns - 1) * gap : 0);
    int bx = tb_x + tb_w - btns_total_w - 4;   /* 4px right margin */

    /* Helper: draw one caption button with hover/pressed visual states.
     * btn_id: 1=close, 2=minimize, 3=maximize (matches hit_test_titlebar). */
    int htb = win->hover_titlebar_btn;
    int ptb = win->pressed_titlebar_btn;

    #define DRAW_CAP_BTN(btn_id, glyph_fn) do {                              \
        uint32_t bg_col = THEME_CHROME;                                       \
        uint32_t hi_col = THEME_BEVEL_LIGHT;                                  \
        uint32_t lo_col = THEME_BEVEL_DARK;                                   \
        int offset = 0;                                                       \
        if (ptb == (btn_id)) {                                                \
            /* Pressed: sunken bevel + darker bg + shift glyph 1px */         \
            bg_col = THEME_CHROME_DARK;                                       \
            hi_col = THEME_BEVEL_DARK;                                        \
            lo_col = THEME_BEVEL_LIGHT;                                       \
            offset = 1;                                                       \
        } else if (htb == (btn_id)) {                                         \
            /* Hovered: brighter bg */                                        \
            bg_col = THEME_CHROME_LIGHTER;                                    \
        }                                                                     \
        surface_fill_rect(surf, bx, btn_y, btn_w, btn_h, bg_col);            \
        surface_draw_hline(surf, bx, btn_y, btn_w, hi_col);                   \
        surface_draw_vline(surf, bx, btn_y, btn_h, hi_col);                   \
        surface_draw_hline(surf, bx, btn_y + btn_h - 1, btn_w, lo_col);       \
        surface_draw_vline(surf, bx + btn_w - 1, btn_y, btn_h, lo_col);       \
        { int _ox = offset; (void)_ox; glyph_fn; }                            \
        bx += btn_w + gap;                                                    \
    } while(0)

    /* Minimize: horizontal line in center */
    if (!(win->flags & WIN_FLAG_NO_MINIMIZE)) {
        DRAW_CAP_BTN(2,
            surface_draw_hline(surf, bx + 5 + _ox, btn_y + btn_h / 2 + _ox,
                               btn_w - 10, THEME_TEXT)
        );
    }
    /* Maximize: small rectangle outline in center */
    if (!(win->flags & WIN_FLAG_NO_MAXIMIZE)) {
        DRAW_CAP_BTN(3,
            surface_draw_rect(surf, bx + 4 + _ox, btn_y + 3 + _ox,
                              btn_w - 8, btn_h - 6, THEME_TEXT, 1)
        );
    }
    /* Close: ✕ drawn as 'X' character */
    if (!(win->flags & WIN_FLAG_NO_CLOSE)) {
        DRAW_CAP_BTN(1,
            surface_draw_char_transparent(
                surf, bx + (btn_w - THEME_FONT_WIDTH) / 2 + _ox,
                btn_y + (btn_h - THEME_FONT_HEIGHT) / 2 + 1 + _ox,
                'X', THEME_TEXT)
        );
    }

    #undef DRAW_CAP_BTN

    /* ---- Title text — left-aligned with 8px margin, proportional font ---- */
    int text_h = surface_text_height(THEME_FONT_BODY);
    int text_y = tb_y + (tb_h - text_h) / 2;
    surface_draw_text(surf, tb_x + 8, text_y,
                      win->title, THEME_FONT_BODY, THEME_TITLEBAR_FG);
}

/*=============================================================================
 * INTERNAL: DRAW CONTENT AREA + CONTROLS
 *===========================================================================*/

static void draw_content(window_t *win) {
    surface_t *surf = &win->surface;

    /* If app provided a custom painter, use it instead of controls */
    if (win->on_paint) {
        /* Fill content background first, then let app paint over it */
        surface_fill_rect(surf, win->content_x, win->content_y,
                          win->content_w, win->content_h, THEME_WINDOW_BG);
        win->on_paint(win, surf, win->on_paint_data);
        return;
    }

    /* Fill content background */
    surface_fill_rect(surf, win->content_x, win->content_y,
                      win->content_w, win->content_h, THEME_WINDOW_BG);

    /* Draw each visible control (positions are relative to content area) */
    for (int i = 0; i < win->control_count; i++) {
        control_t *ctrl = win->controls[i];
        if (!ctrl || !ctrl->visible) continue;

        /* Offset control drawing into content area.
         * We temporarily adjust x/y so the control's draw function
         * places pixels relative to (0,0) of whole surface but offset
         * by the content area origin. */
        int orig_x = ctrl->x;
        int orig_y = ctrl->y;
        ctrl->x += win->content_x;
        ctrl->y += win->content_y;

        if (ctrl->ops && ctrl->ops->draw) {
            ctrl->ops->draw(ctrl, surf);
        }

        /* Restore original relative positions */
        ctrl->x = orig_x;
        ctrl->y = orig_y;
        ctrl->dirty = 0;
    }
}

/*=============================================================================
 * INTERNAL: SIGNAL WM TO COMPOSITE
 *
 * Previously blitted surface pixels to the framebuffer directly.
 * Now the surface lives in SHM — tell the WM Executive to composite.
 *===========================================================================*/

static void blit_to_framebuffer(window_t *win) {
    /* Surface pixels are in SHM, WM reads them directly.
     * Signal full window damage so WM composites our region. */
    libwm_damage_full(win->wm_handle);
}

/*=============================================================================
 * INTERNAL: HIT-TEST TITLEBAR BUTTONS
 *
 * Returns: 0=nothing, 1=close, 2=minimize, 3=maximize
 * Buttons are on the right: [─] [□] [✕] from left to right
 *===========================================================================*/

static int hit_test_titlebar(window_t *win, int mx, int my) {
    int bw = 2;
    int tb_y = bw;
    int tb_h = THEME_TITLEBAR_HEIGHT;

    /* Not in titlebar? */
    if (my < tb_y || my >= tb_y + tb_h) return 0;

    int btn_w = THEME_TITLEBAR_BTN_W;
    int btn_h = THEME_TITLEBAR_BTN_H;
    int gap   = THEME_TITLEBAR_BTN_GAP;
    int btn_y = tb_y + (tb_h - btn_h) / 2;
    int tb_w  = win->width - bw * 2;

    /* Count visible buttons to locate them */
    int n_btns = 0;
    if (!(win->flags & WIN_FLAG_NO_MINIMIZE))  n_btns++;
    if (!(win->flags & WIN_FLAG_NO_MAXIMIZE))  n_btns++;
    if (!(win->flags & WIN_FLAG_NO_CLOSE))     n_btns++;

    int btns_total_w = n_btns * btn_w + (n_btns > 1 ? (n_btns - 1) * gap : 0);
    int bx = bw + tb_w - btns_total_w - 4;

    /* Check vertically */
    if (my < btn_y || my >= btn_y + btn_h) return 0;

    /* Minimize */
    if (!(win->flags & WIN_FLAG_NO_MINIMIZE)) {
        if (mx >= bx && mx < bx + btn_w) return 2;
        bx += btn_w + gap;
    }
    /* Maximize */
    if (!(win->flags & WIN_FLAG_NO_MAXIMIZE)) {
        if (mx >= bx && mx < bx + btn_w) return 3;
        bx += btn_w + gap;
    }
    /* Close */
    if (!(win->flags & WIN_FLAG_NO_CLOSE)) {
        if (mx >= bx && mx < bx + btn_w) return 1;
    }

    return 0;
}

/*=============================================================================
 * INTERNAL: FIND CONTROL UNDER POINT
 *
 * px, py relative to content area
 *===========================================================================*/

static int find_control_at(window_t *win, int px, int py) {
    /* Search back-to-front so topmost control wins */
    for (int i = win->control_count - 1; i >= 0; i--) {
        control_t *ctrl = win->controls[i];
        if (!ctrl || !ctrl->visible || !ctrl->enabled) continue;
        if (control_hit_test(ctrl, px, py)) return i;
    }
    return -1;
}

/*=============================================================================
 * PUBLIC API: LIFECYCLE
 *===========================================================================*/

window_t *window_create(const char *title, int x, int y,
                        int width, int height) {
    /* Ensure GUI library is initialized (gets framebuffer) */
    if (gui_init() != 0) return (window_t *)0;

    window_t *win = (window_t *)malloc(sizeof(window_t));
    if (!win) return (window_t *)0;

    /* Register with WM Executive — get handle + SHM surface ID */
    int surface_shm_id = -1;
    int wm_handle = libwm_create(x, y, width, height, title, &surface_shm_id);
    if (wm_handle < 0 || surface_shm_id < 0) {
        free(win);
        return (window_t *)0;
    }

    /* Attach to the SHM surface allocated by WM */
    uint32_t *shm_ptr = (uint32_t *)syscall2(SYS_SHM_ATTACH,
                                              surface_shm_id, 0);
    if (!shm_ptr) {
        free(win);
        return (window_t *)0;
    }

    /* Position and size */
    win->x      = x;
    win->y      = y;
    win->width  = width;
    win->height = height;

    /* Title */
    str_copy(win->title, title, WINDOW_MAX_TITLE);

    /* Flags */
    win->flags = WIN_FLAG_NONE;

    /* Set up surface backed by SHM (pixels allocated by WM) */
    win->surface.pixels = shm_ptr;
    win->surface.width  = width;
    win->surface.height = height;
    win->surface.pitch  = width * (int)sizeof(uint32_t);

    /* Content area (inside bevel border, below titlebar) */
    int bw = 2;  /* bevel border width */
    win->content_x = bw;
    win->content_y = bw + THEME_TITLEBAR_HEIGHT;
    win->content_w = width - bw * 2;
    win->content_h = height - bw * 2 - THEME_TITLEBAR_HEIGHT;

    /* Controls */
    win->control_count = 0;
    for (int i = 0; i < WINDOW_MAX_CONTROLS; i++) {
        win->controls[i] = (control_t *)0;
    }

    /* State */
    win->running         = 0;
    win->needs_redraw    = 1;
    win->focused_control = -1;
    win->hover_control   = -1;
    win->dragging        = 0;
    win->drag_offset_x   = 0;
    win->drag_offset_y   = 0;
    win->drag_ghost_x    = 0;
    win->drag_ghost_y    = 0;
    win->maximized       = 0;
    win->restore_x       = 0;
    win->restore_y       = 0;
    win->restore_w       = 0;
    win->restore_h       = 0;
    win->minimized       = 0;
    win->hover_titlebar_btn = 0;
    win->pressed_titlebar_btn = 0;

    /* WM Executive state */
    win->wm_handle      = wm_handle;
    win->surface_shm_id = surface_shm_id;

    /* Callbacks */
    win->on_close  = (void (*)(void *))0;
    win->close_data = (void *)0;

    /* Custom content callbacks */
    win->on_key     = (void (*)(window_t *, int, char, void *))0;
    win->on_key_data = (void *)0;
    win->on_paint   = (void (*)(window_t *, surface_t *, void *))0;
    win->on_paint_data = (void *)0;
    win->on_tick    = (void (*)(window_t *, void *))0;
    win->on_tick_data = (void *)0;

    return win;
}

void window_destroy(window_t *win) {
    if (!win) return;

    /* Destroy all controls */
    for (int i = 0; i < win->control_count; i++) {
        control_t *ctrl = win->controls[i];
        if (ctrl && ctrl->ops && ctrl->ops->destroy) {
            ctrl->ops->destroy(ctrl);
            free(ctrl);
        }
    }

    /* Detach SHM surface if still attached
     * (normally already detached by window_run exit) */
    if (win->surface_shm_id >= 0 && win->surface.pixels) {
        syscall1(SYS_SHM_DETACH, win->surface_shm_id);
        win->surface.pixels = (uint32_t *)0;
    }

    /* Note: do NOT call surface_destroy — pixels were SHM, not malloc */

    free(win);
}

/*=============================================================================
 * PUBLIC API: CONTROL MANAGEMENT
 *===========================================================================*/

int window_add_control(window_t *win, control_t *ctrl) {
    if (!win || !ctrl) return -1;
    if (win->control_count >= WINDOW_MAX_CONTROLS) return -1;

    win->controls[win->control_count++] = ctrl;
    win->needs_redraw = 1;
    return 0;
}

int window_remove_control(window_t *win, control_t *ctrl) {
    if (!win || !ctrl) return -1;

    for (int i = 0; i < win->control_count; i++) {
        if (win->controls[i] == ctrl) {
            /* Shift remaining controls down */
            for (int j = i; j < win->control_count - 1; j++) {
                win->controls[j] = win->controls[j + 1];
            }
            win->control_count--;
            win->controls[win->control_count] = (control_t *)0;
            win->needs_redraw = 1;
            return 0;
        }
    }
    return -1;
}

/*=============================================================================
 * PUBLIC API: DRAWING
 *===========================================================================*/

void window_draw(window_t *win) {
    if (!win) return;

    /* Fill entire surface with chrome (visible in border area) */
    surface_fill_rect(&win->surface, 0, 0, win->width, win->height,
                      THEME_CHROME);

    /* Outer 2px raised embossed bevel */
    draw_raised_bevel(&win->surface, 0, 0, win->width, win->height);
    /* Inner bevel line (second pixel of border) */
    draw_raised_bevel(&win->surface, 1, 1, win->width - 2, win->height - 2);

    /* Draw titlebar */
    if (!(win->flags & WIN_FLAG_NO_TITLEBAR)) {
        draw_titlebar(win);
    }

    /* Draw content area + all controls */
    draw_content(win);

    /* Blit to screen */
    blit_to_framebuffer(win);

    win->needs_redraw = 0;
}

/**
 * Lightweight redraw: only repaint the content area (controls) and blit.
 * Skips chrome, bevels, and titlebar — roughly 3-5x faster than window_draw.
 * Use when only content changed (key press, control dirty) but NOT the window
 * chrome (move, resize, titlebar hover).
 */
static void window_draw_content_only(window_t *win) {
    if (!win) return;

    /* Repaint just the content area */
    draw_content(win);

    /* Signal WM to composite just the content region */
    libwm_damage(win->wm_handle, win->content_x, win->content_y,
                 win->content_w, win->content_h);
}

void window_invalidate(window_t *win) {
    if (win) win->needs_redraw = 1;
}

/*=============================================================================
 * INTERNAL: XOR DRAG OUTLINE
 *
 * Draws (or erases) a 2px dashed rectangle outline on the framebuffer
 * using XOR 0x00FFFFFF.  Call once to draw, call again with the same
 * coordinates to erase — perfectly restoring original pixels.
 *
 * Used for lightweight "outline drag" so we never need to clear+redraw
 * the entire window during a move.
 *===========================================================================*/

static void xor_drag_outline(int gx, int gy, int w, int h) {
    uint32_t *fb = gui_get_framebuffer();
    if (!fb) return;

    int scr_w = (int)gui_get_screen_width();
    int scr_h = (int)gui_get_screen_height();

    /* NOTE: Drawing to back buffer (no HW cursor to worry about).
     * Caller must call gui_flip_rect() after this to make outline visible. */

    for (int t = 0; t < 2; t++) {
        /* Top edge */
        for (int col = 0; col < w; col++) {
            if (((col >> 2) ^ t) & 1) continue;       /* dashed pattern */
            int px = gx + col, py = gy + t;
            if (px >= 0 && px < scr_w && py >= 0 && py < scr_h)
                fb[py * scr_w + px] ^= 0x00FFFFFF;
        }
        /* Bottom edge */
        for (int col = 0; col < w; col++) {
            if (((col >> 2) ^ t) & 1) continue;
            int px = gx + col, py = gy + h - 1 - t;
            if (px >= 0 && px < scr_w && py >= 0 && py < scr_h)
                fb[py * scr_w + px] ^= 0x00FFFFFF;
        }
        /* Left edge (skip corners already drawn) */
        for (int row = 2; row < h - 2; row++) {
            if (((row >> 2) ^ t) & 1) continue;
            int px = gx + t, py = gy + row;
            if (px >= 0 && px < scr_w && py >= 0 && py < scr_h)
                fb[py * scr_w + px] ^= 0x00FFFFFF;
        }
        /* Right edge */
        for (int row = 2; row < h - 2; row++) {
            if (((row >> 2) ^ t) & 1) continue;
            int px = gx + w - 1 - t, py = gy + row;
            if (px >= 0 && px < scr_w && py >= 0 && py < scr_h)
                fb[py * scr_w + px] ^= 0x00FFFFFF;
        }
    }
}

/*=============================================================================
 * INTERNAL: TOGGLE MAXIMIZE
 *
 * Resizes window to fill screen (minus 32px taskbar) or restores to
 * the original saved size.  Recreates the pixel surface.
 *===========================================================================*/

static void window_toggle_maximize(window_t *win) {
    if (!win) return;

    /* Restore pixels behind the current position */
    /* Tell WM to toggle maximize — WM reallocates surface SHM */
    int new_shm_id = -1;
    int result = libwm_maximize(win->wm_handle, &new_shm_id);
    if (result < 0) return;

    /* Detach old SHM surface */
    if (win->surface.pixels && win->surface_shm_id >= 0) {
        syscall1(SYS_SHM_DETACH, win->surface_shm_id);
    }

    if (win->maximized) {
        /* Restore */
        win->x      = win->restore_x;
        win->y      = win->restore_y;
        win->width  = win->restore_w;
        win->height = win->restore_h;
    } else {
        /* Save current geometry */
        win->restore_x = win->x;
        win->restore_y = win->y;
        win->restore_w = win->width;
        win->restore_h = win->height;

        /* Maximize: fill screen, leave 32px for taskbar at bottom */
        win->x      = 0;
        win->y      = 0;
        win->width  = (int)gui_get_screen_width();
        win->height = (int)gui_get_screen_height() - 32;
    }

    /* Attach new SHM surface from WM */
    win->surface_shm_id = new_shm_id;
    uint32_t *new_ptr = (uint32_t *)syscall2(SYS_SHM_ATTACH, new_shm_id, 0);
    win->surface.pixels = new_ptr;
    win->surface.width  = win->width;
    win->surface.height = win->height;
    win->surface.pitch  = win->width * (int)sizeof(uint32_t);

    /* Recalculate content area */
    int bw = 2;
    win->content_x = bw;
    win->content_y = bw + THEME_TITLEBAR_HEIGHT;
    win->content_w = win->width  - bw * 2;
    win->content_h = win->height - bw * 2 - THEME_TITLEBAR_HEIGHT;

    win->maximized = !win->maximized;
    win->needs_redraw = 1;
}

/*=============================================================================
 * PUBLIC API: EVENT LOOP
 *===========================================================================*/

void window_run(window_t *win) {
    if (!win) return;

    win->running = 1;

    /* WM handles taskbar registration (via publish_registry at create).
     * WM also set focus to this window in handle_create. */

    /* Initial draw — renders to SHM surface, WM composites */
    window_draw(win);

    key_event_t kevt;
    mouse_state_t ms;
    int prev_left = 0;       /* previous left-button state */
    int prev_mx = -1, prev_my = -1;
    int pressed_ctrl = -1;   /* control that received MOUSE_DOWN */

    while (win->running) {
        int any_dirty = 0;

        /* ---- Heartbeat: tell WM we're still alive ---- */
        libwm_heartbeat(win->wm_handle);

        /* ---- Focus tracking: ask WM ---- */
        int have_focus = libwm_is_focused(win->wm_handle);

        /* ---- Mouse polling (via IO Executive) ---- */
        int rd = libio_dev_read(DEV_MOUSE, &ms, sizeof(ms));
        if (rd > 0) {
            /* Convert screen coords to window-relative */
            int wx = ms.x - win->x;
            int wy = ms.y - win->y;

            int inside = (wx >= 0 && wx < win->width &&
                          wy >= 0 && wy < win->height);

            int left_now  = ms.buttons & MOUSE_LEFT;
            int left_down = (left_now && !prev_left);   /* press  */
            int left_up   = (!left_now && prev_left);   /* release */

            /* Content-relative coords (valid only when inside) */
            int cx = wx - win->content_x;
            int cy = wy - win->content_y;

            /* ---- Claim focus when user clicks inside this window ---- */
            if (left_down && inside && !have_focus) {
                libwm_raise(win->wm_handle);
                win->needs_redraw = 1;
            }

            /* ---- DRAG: outline drag (XOR dashed rectangle) ---- */
            if (win->dragging) {
                if (left_now) {
                    /* Compute where ghost outline should be */
                    int new_gx = ms.x - win->drag_offset_x;
                    int new_gy = ms.y - win->drag_offset_y;

                    /* Clamp to screen */
                    int scr_w = (int)gui_get_screen_width();
                    int scr_h = (int)gui_get_screen_height();
                    if (new_gx < 0) new_gx = 0;
                    if (new_gy < 0) new_gy = 0;
                    if (new_gx + win->width  > scr_w)
                        new_gx = scr_w - win->width;
                    if (new_gy + win->height > scr_h)
                        new_gy = scr_h - win->height;

                    if (new_gx != win->drag_ghost_x ||
                        new_gy != win->drag_ghost_y) {
                        /* Save old position BEFORE updating */
                        int old_gx = win->drag_ghost_x;
                        int old_gy = win->drag_ghost_y;

                        /* Erase old outline (XOR again) */
                        xor_drag_outline(old_gx, old_gy,
                                         win->width, win->height);
                        /* Draw new outline */
                        win->drag_ghost_x = new_gx;
                        win->drag_ghost_y = new_gy;
                        xor_drag_outline(new_gx, new_gy,
                                         win->width, win->height);
                        /* Flip the union of old and new outline regions */
                        {
                            int rx = (old_gx < new_gx) ? old_gx : new_gx;
                            int ry = (old_gy < new_gy) ? old_gy : new_gy;
                            int rx2 = ((old_gx + win->width) >
                                       (new_gx + win->width)) ?
                                       (old_gx + win->width) :
                                       (new_gx + win->width);
                            int ry2 = ((old_gy + win->height) >
                                       (new_gy + win->height)) ?
                                       (old_gy + win->height) :
                                       (new_gy + win->height);
                            gui_flip_rect(rx, ry, rx2 - rx, ry2 - ry);
                        }
                    }
                } else {
                    /* Released — erase XOR outline */
                    xor_drag_outline(win->drag_ghost_x,
                                     win->drag_ghost_y,
                                     win->width, win->height);
                    /* Update local position */
                    win->x = win->drag_ghost_x;
                    win->y = win->drag_ghost_y;
                    /* Tell WM to move (composites old+new rects) */
                    libwm_move(win->wm_handle, win->x, win->y);
                    win->dragging = 0;
                    win->needs_redraw = 1;
                }
            }

            /* --- Mouse UP: always dispatch to the control that got
             *     MOUSE_DOWN, regardless of current position.
             *     This ensures quick clicks register even if the mouse
             *     moves slightly between press and release. --- */
            if (left_up && pressed_ctrl >= 0 &&
                pressed_ctrl < win->control_count) {
                control_t *c = win->controls[pressed_ctrl];
                if (c && c->ops && c->ops->event) {
                    gui_event_t ue = {
                        .type = GUI_EVENT_MOUSE_UP,
                        .mouse_x = cx,
                        .mouse_y = cy,
                        .mouse_button = 0,
                    };
                    c->ops->event(c, &ue);
                    any_dirty = 1;
                }
                pressed_ctrl = -1;
            }

            if (inside) {
                /* --- Titlebar button hover tracking --- */
                int new_tb_hover = hit_test_titlebar(win, wx, wy);
                if (new_tb_hover != win->hover_titlebar_btn) {
                    win->hover_titlebar_btn = new_tb_hover;
                    any_dirty = 1;
                    win->needs_redraw = 1; /* titlebar must repaint */
                }

                /* --- Titlebar button press on left_down --- */
                if (left_down && new_tb_hover > 0) {
                    win->pressed_titlebar_btn = new_tb_hover;
                    any_dirty = 1;
                    win->needs_redraw = 1;
                }

                /* --- Titlebar button release: perform action --- */
                if (left_up && win->pressed_titlebar_btn > 0) {
                    int act = win->pressed_titlebar_btn;
                    win->pressed_titlebar_btn = 0;
                    any_dirty = 1;
                    win->needs_redraw = 1;

                    /* Only trigger if released over the same button */
                    if (act == new_tb_hover) {
                        if (act == 1) {          /* Close */
                            window_close(win);
                            break;
                        }
                        if (act == 2) {          /* Minimize */
                            /* Tell WM to minimize (composites area) */
                            libwm_minimize(win->wm_handle);
                            win->minimized = 1;

                            /* Poll for restore signal from Orbit */
                            while (win->running && win->minimized) {
                                if (taskbar_check_restore()) {
                                    /* Tell WM to restore */
                                    win->minimized = 0;
                                    libwm_restore(win->wm_handle);
                                    win->needs_redraw = 1;
                                    window_draw(win);
                                    break;
                                }
                                /* Call on_tick even while minimized */
                                if (win->on_tick) {
                                    win->on_tick(win, win->on_tick_data);
                                }
                                syscall0(SYS_YIELD);
                            }
                            prev_left = left_now;
                            prev_mx = ms.x;
                            prev_my = ms.y;
                            continue;
                        }
                        if (act == 3) {          /* Maximize toggle */
                            window_toggle_maximize(win);
                            prev_left = left_now;
                            prev_mx = ms.x;
                            prev_my = ms.y;
                            if (win->needs_redraw) window_draw(win);
                            syscall0(SYS_YIELD);
                            continue;
                        }
                    }
                    /* Released over a different button or empty area —
                     * just cancel the press, no action. */
                }

                /* Clear pressed state after release handling is done.
                 * This MUST be after the left_up check above, otherwise
                 * pressed_titlebar_btn is 0 when the release handler reads it. */
                if (!left_now) {
                    win->pressed_titlebar_btn = 0;
                }

                /* If left_down on titlebar but NOT on a button,
                 * start outline drag */
                if (left_down) {
                    int tb_hit = hit_test_titlebar(win, wx, wy);
                    if (tb_hit == 0 && wy < (2 + THEME_TITLEBAR_HEIGHT)) {
                        /* Raise to front before starting drag */
                        if (!have_focus) {
                            libwm_raise(win->wm_handle);
                            win->needs_redraw = 1;
                        }
                        win->dragging = 1;
                        win->drag_offset_x = wx;
                        win->drag_offset_y = wy;
                        win->drag_ghost_x  = win->x;
                        win->drag_ghost_y  = win->y;
                        /* Draw initial outline on back buffer + flip */
                        xor_drag_outline(win->x, win->y,
                                         win->width, win->height);
                        gui_flip_rect(win->x, win->y, win->width, win->height);
                        prev_left = left_now;
                        prev_mx = ms.x;
                        prev_my = ms.y;
                        syscall0(SYS_YIELD);
                        continue;
                    }
                }

                /* --- Control hover tracking --- */
                int ctrl_idx = -1;
                if (cy >= 0 && cy < win->content_h &&
                    cx >= 0 && cx < win->content_w) {
                    ctrl_idx = find_control_at(win, cx, cy);
                }

                /* Hover enter/leave */
                if (ctrl_idx != win->hover_control) {
                    /* Leave old */
                    if (win->hover_control >= 0 &&
                        win->hover_control < win->control_count) {
                        control_t *oldc = win->controls[win->hover_control];
                        if (oldc && oldc->ops && oldc->ops->event) {
                            gui_event_t le = { .type = GUI_EVENT_MOUSE_LEAVE };
                            oldc->ops->event(oldc, &le);
                            any_dirty = 1;
                        }
                    }
                    /* Enter new */
                    if (ctrl_idx >= 0 && ctrl_idx < win->control_count) {
                        control_t *newc = win->controls[ctrl_idx];
                        if (newc && newc->ops && newc->ops->event) {
                            gui_event_t en = { .type = GUI_EVENT_MOUSE_ENTER };
                            newc->ops->event(newc, &en);
                            any_dirty = 1;
                        }
                    }
                    win->hover_control = ctrl_idx;
                }

                /* Dispatch MOUSE_MOVE to the hovered control when mouse moves */
                if ((ms.x != prev_mx || ms.y != prev_my) &&
                    ctrl_idx >= 0 && ctrl_idx < win->control_count) {
                    control_t *mc = win->controls[ctrl_idx];
                    if (mc && mc->ops && mc->ops->event) {
                        gui_event_t me = {
                            .type = GUI_EVENT_MOUSE_MOVE,
                            .mouse_x = cx,
                            .mouse_y = cy,
                            .mouse_button = 0,
                        };
                        mc->ops->event(mc, &me);
                        any_dirty = 1;
                    }
                }

                /* Mouse down on a control — remember which got pressed */
                if (left_down && ctrl_idx >= 0 &&
                    ctrl_idx < win->control_count) {
                    control_t *c = win->controls[ctrl_idx];
                    if (c && c->ops && c->ops->event) {
                        gui_event_t de = {
                            .type = GUI_EVENT_MOUSE_DOWN,
                            .mouse_x = cx,
                            .mouse_y = cy,
                            .mouse_button = 0,
                        };
                        c->ops->event(c, &de);
                        any_dirty = 1;
                    }
                    pressed_ctrl = ctrl_idx;
                }
            } else {
                /* Mouse left the window — clear hover */
                if (win->hover_control >= 0 &&
                    win->hover_control < win->control_count) {
                    control_t *oldc = win->controls[win->hover_control];
                    if (oldc && oldc->ops && oldc->ops->event) {
                        gui_event_t le = { .type = GUI_EVENT_MOUSE_LEAVE };
                        oldc->ops->event(oldc, &le);
                        any_dirty = 1;
                    }
                    win->hover_control = -1;
                }
                /* Also clear titlebar button hover */
                if (win->hover_titlebar_btn != 0) {
                    win->hover_titlebar_btn = 0;
                    any_dirty = 1;
                    win->needs_redraw = 1;
                }
            }

            prev_left = left_now;
            prev_mx = ms.x;
            prev_my = ms.y;
        }

        /* ---- Keyboard polling ---- */
        while (kbd_read_event(&kevt) > 0) {
            if (kevt.type == KEY_PRESSED) {
                /* If app provided an on_key handler, delegate to it.
                 * ESC still closes the window unless consumed by app. */
                if (win->on_key) {
                    win->on_key(win, kevt.scancode, (char)kevt.ascii,
                                win->on_key_data);
                    any_dirty = 1;
                    continue;
                }

                /* Escape closes the window */
                if (kevt.scancode == SC_ESCAPE) {
                    window_close(win);
                    break;
                }

                /* Tab cycles focus between controls */
                if (kevt.scancode == SC_TAB) {
                    /* Move focus to next enabled control */
                    int start = win->focused_control;
                    int next = (start < 0) ? 0 : start + 1;
                    for (int attempts = 0; attempts < win->control_count; attempts++) {
                        if (next >= win->control_count) next = 0;
                        control_t *c = win->controls[next];
                        if (c && c->visible && c->enabled) {
                            /* Blur old */
                            if (win->focused_control >= 0 &&
                                win->focused_control < win->control_count) {
                                control_t *old = win->controls[win->focused_control];
                                if (old) {
                                    old->focused = 0;
                                    old->dirty = 1;
                                    gui_event_t blur_evt = { .type = GUI_EVENT_BLUR };
                                    if (old->ops && old->ops->event)
                                        old->ops->event(old, &blur_evt);
                                }
                            }
                            /* Focus new */
                            c->focused = 1;
                            c->dirty = 1;
                            win->focused_control = next;
                            gui_event_t focus_evt = { .type = GUI_EVENT_FOCUS };
                            if (c->ops && c->ops->event)
                                c->ops->event(c, &focus_evt);
                            break;
                        }
                        next++;
                    }
                    any_dirty = 1;
                    continue;
                }

                /* Enter/Space = click on focused button */
                if (kevt.ascii == ' ' || kevt.ascii == '\n' || kevt.scancode == 0x1C) {
                    if (win->focused_control >= 0 &&
                        win->focused_control < win->control_count) {
                        control_t *fc = win->controls[win->focused_control];
                        if (fc && fc->enabled && fc->type == CONTROL_BUTTON) {
                            gui_event_t down = { .type = GUI_EVENT_MOUSE_DOWN, .mouse_button = 0 };
                            gui_event_t up   = { .type = GUI_EVENT_MOUSE_UP,   .mouse_button = 0 };
                            if (fc->ops && fc->ops->event) {
                                fc->ops->event(fc, &down);
                                fc->ops->event(fc, &up);
                            }
                            any_dirty = 1;
                        }
                    }
                    continue;
                }

                /* Forward key to focused control */
                if (win->focused_control >= 0 &&
                    win->focused_control < win->control_count) {
                    control_t *fc = win->controls[win->focused_control];
                    if (fc && fc->ops && fc->ops->event) {
                        gui_event_t ke = {
                            .type = GUI_EVENT_KEY_DOWN,
                            .key_code = kevt.scancode,
                            .key_char = (char)kevt.ascii,
                        };
                        fc->ops->event(fc, &ke);
                        any_dirty = 1;
                    }
                }
            }
        }

        if (!win->running) break;

        /* In the WM model, every window draws to its own SHM surface.
         * No need to gate on focus — WM composites based on z-order. */
        if (win->needs_redraw) {
            window_draw(win);
        } else if (any_dirty) {
            /* Fast path: only content area changed */
            window_draw_content_only(win);
        }

        /* Per-frame tick callback (cursor blink, child polling, etc.) */
        if (win->on_tick) {
            win->on_tick(win, win->on_tick_data);
        }

        syscall0(SYS_YIELD);
    }

    /* Tell WM to remove and composite the area.
     * WM detaches+destroys SHM on its side. */
    libwm_destroy(win->wm_handle);

    /* Detach our SHM mapping */
    if (win->surface_shm_id >= 0 && win->surface.pixels) {
        syscall1(SYS_SHM_DETACH, win->surface_shm_id);
        win->surface.pixels = (uint32_t *)0;
        win->surface_shm_id = -1;
    }
    win->wm_handle = -1;
}

void window_close(window_t *win) {
    if (!win) return;

    /* Fire close callback */
    if (win->on_close) {
        win->on_close(win->close_data);
    }

    win->running = 0;
}

/*=============================================================================
 * PUBLIC API: CALLBACK SETTERS
 *===========================================================================*/

void window_set_on_key(window_t *win,
                       void (*callback)(window_t *win, int scancode,
                                        char ascii, void *userdata),
                       void *userdata) {
    if (!win) return;
    win->on_key  = callback;
    win->on_key_data = userdata;
}

void window_set_on_paint(window_t *win,
                         void (*callback)(window_t *win, surface_t *surf,
                                          void *userdata),
                         void *userdata) {
    if (!win) return;
    win->on_paint  = callback;
    win->on_paint_data = userdata;
}

void window_set_on_tick(window_t *win,
                        void (*callback)(window_t *win, void *userdata),
                        void *userdata) {
    if (!win) return;
    win->on_tick  = callback;
    win->on_tick_data = userdata;
}
