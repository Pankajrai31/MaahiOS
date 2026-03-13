/**
 * MaahiOS Window Library - TextArea Control Implementation (Design System v2)
 *
 * Description:
 *   Multi-line text editing area using the 8x16 bitmap font.
 *   Text stored as a flat char array with '\n' line delimiters.
 *   Lines that exceed the visible width are clipped (no word wrap).
 *   Vertical scrolling tracks the cursor position.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "textarea.h"
#include "../surface.h"
#include "../theme.h"

extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS
 *===========================================================================*/

#define TA_BG           THEME_SURFACE
#define TA_FG           THEME_TEXT
#define TA_CURSOR_FG    THEME_TEXT
#define TA_SELECT_BG    THEME_ACCENT
#define TA_SELECT_FG    THEME_TEXT_INVERSE
#define TA_BEVEL_LIGHT  THEME_BEVEL_LIGHT
#define TA_BEVEL_DARK   THEME_BEVEL_DARK
#define TA_LINE_NUM_BG  THEME_CHROME_LIGHTER
#define TA_LINE_NUM_FG  THEME_TEXT_SECONDARY

/* Scancodes */
#define SC_BACKSPACE    0x0E
#define SC_ENTER        0x1C
#define SC_TAB_KEY      0x0F
#define SC_UP           0x48
#define SC_DOWN         0x50
#define SC_LEFT         0x4B
#define SC_RIGHT        0x4D
#define SC_HOME         0x47
#define SC_END          0x4F
#define SC_DELETE       0x53
#define SC_PGUP         0x49
#define SC_PGDN         0x51

#define BLINK_INTERVAL  25   /* Ticks between blink toggles */

/*=============================================================================
 * INTERNAL: CURSOR LINE/COLUMN COMPUTATION
 *===========================================================================*/

static void ta_compute_linecol(textarea_t *ta) {
    int line = 0, col = 0;
    int i;
    for (i = 0; i < ta->cursor_pos && i < ta->text_len; i++) {
        if (ta->text[i] == '\n') {
            line++;
            col = 0;
        } else {
            col++;
        }
    }
    ta->cursor_line = line;
    ta->cursor_col  = col;
}

/** Find the byte offset of the start of a given line number */
static int ta_line_start(textarea_t *ta, int line) {
    if (line <= 0) return 0;
    int cur_line = 0;
    int i;
    for (i = 0; i < ta->text_len; i++) {
        if (ta->text[i] == '\n') {
            cur_line++;
            if (cur_line == line) return i + 1;
        }
    }
    return ta->text_len;  /* Line beyond end */
}

/** Find the byte offset of the end of a given line (position of '\n' or text_len) */
static int ta_line_end(textarea_t *ta, int line) {
    int start = ta_line_start(ta, line);
    int i;
    for (i = start; i < ta->text_len; i++) {
        if (ta->text[i] == '\n') return i;
    }
    return ta->text_len;
}

/** Count total lines in text */
static int ta_line_count(textarea_t *ta) {
    int count = 1;
    int i;
    for (i = 0; i < ta->text_len; i++) {
        if (ta->text[i] == '\n') count++;
    }
    return count;
}

/** Ensure cursor line is visible (adjust scroll_y) */
static void ta_ensure_visible(textarea_t *ta) {
    if (ta->cursor_line < ta->scroll_y) {
        ta->scroll_y = ta->cursor_line;
    }
    if (ta->cursor_line >= ta->scroll_y + ta->visible_lines) {
        ta->scroll_y = ta->cursor_line - ta->visible_lines + 1;
    }
    if (ta->scroll_y < 0) ta->scroll_y = 0;
}

/*=============================================================================
 * INTERNAL: TEXT MANIPULATION
 *===========================================================================*/

/** memmove - move n bytes from src to dst (handles overlapping) */
static void ta_memmove(char *dst, const char *src, int n) {
    if (dst < src) {
        int i;
        for (i = 0; i < n; i++) dst[i] = src[i];
    } else if (dst > src) {
        int i;
        for (i = n - 1; i >= 0; i--) dst[i] = src[i];
    }
}

static int ta_insert_char(textarea_t *ta, char c) {
    if (ta->text_len >= TEXTAREA_MAX_TEXT - 1) return 0;
    /* Shift text right by 1 */
    ta_memmove(&ta->text[ta->cursor_pos + 1],
               &ta->text[ta->cursor_pos],
               ta->text_len - ta->cursor_pos);
    ta->text[ta->cursor_pos] = c;
    ta->text_len++;
    ta->text[ta->text_len] = '\0';
    ta->cursor_pos++;
    ta->modified = 1;
    return 1;
}

static int ta_delete_at(textarea_t *ta, int pos) {
    if (pos < 0 || pos >= ta->text_len) return 0;
    ta_memmove(&ta->text[pos], &ta->text[pos + 1],
               ta->text_len - pos - 1);
    ta->text_len--;
    ta->text[ta->text_len] = '\0';
    ta->modified = 1;
    return 1;
}

/*=============================================================================
 * VTABLE: DRAW
 *===========================================================================*/

static void textarea_draw(control_t *ctrl, surface_t *surf) {
    textarea_t *ta = (textarea_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = ctrl->height;

    /* ---- Sunken border (2px) ---- */
    surface_draw_hline(surf, x, y, w, TA_BEVEL_DARK);
    surface_draw_hline(surf, x + 1, y + 1, w - 2, TA_BEVEL_DARK);
    surface_draw_vline(surf, x, y, h, TA_BEVEL_DARK);
    surface_draw_vline(surf, x + 1, y + 1, h - 2, TA_BEVEL_DARK);
    surface_draw_hline(surf, x, y + h - 1, w, TA_BEVEL_LIGHT);
    surface_draw_hline(surf, x + 1, y + h - 2, w - 2, TA_BEVEL_LIGHT);
    surface_draw_vline(surf, x + w - 1, y, h, TA_BEVEL_LIGHT);
    surface_draw_vline(surf, x + w - 2, y + 1, h - 2, TA_BEVEL_LIGHT);

    int bw = TEXTAREA_BORDER_W;
    int inner_x = x + bw + TEXTAREA_PAD_X;
    int inner_y = y + bw + TEXTAREA_PAD_Y;
    int inner_w = w - bw * 2;
    int inner_h = h - bw * 2;

    /* Fill background */
    surface_fill_rect(surf, x + bw, y + bw, inner_w, inner_h, TA_BG);

    /* Compute visible metrics */
    int text_area_w = inner_w - TEXTAREA_PAD_X * 2;
    int text_area_h = inner_h - TEXTAREA_PAD_Y * 2;
    ta->visible_cols  = text_area_w / TEXTAREA_CHAR_W;
    ta->visible_lines = text_area_h / TEXTAREA_CHAR_H;
    if (ta->visible_cols < 1) ta->visible_cols = 1;
    if (ta->visible_lines < 1) ta->visible_lines = 1;

    /* Draw text lines */
    int total_lines = ta_line_count(ta);
    int max_scroll = total_lines - ta->visible_lines;
    if (max_scroll < 0) max_scroll = 0;
    if (ta->scroll_y > max_scroll) ta->scroll_y = max_scroll;

    int line;
    for (line = 0; line < ta->visible_lines; line++) {
        int abs_line = line + ta->scroll_y;
        if (abs_line >= total_lines) break;

        int ls = ta_line_start(ta, abs_line);
        int le = ta_line_end(ta, abs_line);
        int py = inner_y + line * TEXTAREA_CHAR_H;

        /* Draw each character on this line */
        int col;
        for (col = 0; col < ta->visible_cols && (ls + col) < le; col++) {
            char c = ta->text[ls + col];
            if (c == '\n' || c == '\0') break;
            int px = inner_x + col * TEXTAREA_CHAR_W;
            surface_draw_char(surf, px, py, c, TA_FG, TA_BG);
        }
    }

    /* Draw cursor */
    if (ta->cursor_visible && !ta->readonly) {
        int cursor_screen_line = ta->cursor_line - ta->scroll_y;
        if (cursor_screen_line >= 0 && cursor_screen_line < ta->visible_lines) {
            int cx = inner_x + ta->cursor_col * TEXTAREA_CHAR_W;
            int cy = inner_y + cursor_screen_line * TEXTAREA_CHAR_H;
            /* Draw cursor as a vertical line (2px wide) */
            surface_fill_rect(surf, cx, cy, 2, TEXTAREA_CHAR_H, TA_CURSOR_FG);
        }
    }
}

/*=============================================================================
 * VTABLE: EVENT
 *===========================================================================*/

static int textarea_event(control_t *ctrl, gui_event_t *evt) {
    textarea_t *ta = (textarea_t *)ctrl;

    /* Mouse click = position cursor */
    if (evt->type == GUI_EVENT_MOUSE_DOWN && evt->mouse_button == 0) {
        int bw = TEXTAREA_BORDER_W;
        int lx = evt->mouse_x - ctrl->x - bw - TEXTAREA_PAD_X;
        int ly = evt->mouse_y - ctrl->y - bw - TEXTAREA_PAD_Y;
        if (lx < 0) lx = 0;
        if (ly < 0) ly = 0;

        int click_line = ly / TEXTAREA_CHAR_H + ta->scroll_y;
        int click_col  = (lx + TEXTAREA_CHAR_W / 2) / TEXTAREA_CHAR_W;

        int total = ta_line_count(ta);
        if (click_line >= total) click_line = total - 1;
        if (click_line < 0) click_line = 0;

        int ls = ta_line_start(ta, click_line);
        int le = ta_line_end(ta, click_line);
        int line_len = le - ls;

        if (click_col > line_len) click_col = line_len;
        ta->cursor_pos = ls + click_col;
        ta_compute_linecol(ta);
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        ctrl->dirty = 1;
        return 1;
    }

    return 0;
}

static void textarea_destroy_impl(control_t *ctrl) {
    (void)ctrl;
}

static const control_ops_t textarea_ops = {
    .draw    = textarea_draw,
    .event   = textarea_event,
    .destroy = textarea_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

textarea_t *textarea_create(int x, int y, int w, int h) {
    textarea_t *ta = (textarea_t *)malloc(sizeof(textarea_t));
    if (!ta) return (textarea_t *)0;

    /* Zero-init */
    {
        unsigned char *p = (unsigned char *)ta;
        unsigned int i;
        for (i = 0; i < sizeof(textarea_t); i++) p[i] = 0;
    }

    CONTROL_INIT(&ta->base, CONTROL_TEXTAREA, x, y, w, h, &textarea_ops);

    ta->text[0]          = '\0';
    ta->text_len         = 0;
    ta->cursor_pos       = 0;
    ta->cursor_line      = 0;
    ta->cursor_col       = 0;
    ta->cursor_visible   = 1;
    ta->cursor_blink_tick = 0;
    ta->scroll_y         = 0;
    ta->visible_lines    = 1;
    ta->visible_cols     = 1;
    ta->modified         = 0;
    ta->readonly         = 0;

    return ta;
}

void textarea_set_text(textarea_t *ta, const char *text) {
    if (!ta) return;
    int i;
    for (i = 0; i < TEXTAREA_MAX_TEXT - 1 && text && text[i]; i++) {
        ta->text[i] = text[i];
    }
    ta->text[i]    = '\0';
    ta->text_len   = i;
    ta->cursor_pos = 0;
    ta->scroll_y   = 0;
    ta->modified   = 0;
    ta_compute_linecol(ta);
    ta->base.dirty = 1;
}

const char *textarea_get_text(textarea_t *ta) {
    if (!ta) return "";
    return ta->text;
}

int textarea_get_text_len(textarea_t *ta) {
    if (!ta) return 0;
    return ta->text_len;
}

int textarea_is_modified(textarea_t *ta) {
    if (!ta) return 0;
    return ta->modified;
}

void textarea_clear_modified(textarea_t *ta) {
    if (!ta) return;
    ta->modified = 0;
}

void textarea_set_readonly(textarea_t *ta, int readonly) {
    if (!ta) return;
    ta->readonly = readonly;
}

int textarea_handle_key(textarea_t *ta, int scancode, char ascii) {
    if (!ta) return 0;

    int handled = 0;

    /* --- Navigation keys (always allowed even in readonly) --- */

    if (scancode == SC_LEFT) {
        if (ta->cursor_pos > 0) {
            ta->cursor_pos--;
            ta_compute_linecol(ta);
            ta_ensure_visible(ta);
            ta->base.dirty = 1;
        }
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_RIGHT) {
        if (ta->cursor_pos < ta->text_len) {
            ta->cursor_pos++;
            ta_compute_linecol(ta);
            ta_ensure_visible(ta);
            ta->base.dirty = 1;
        }
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_UP) {
        if (ta->cursor_line > 0) {
            int target_col = ta->cursor_col;
            int prev_start = ta_line_start(ta, ta->cursor_line - 1);
            int prev_end   = ta_line_end(ta, ta->cursor_line - 1);
            int prev_len = prev_end - prev_start;
            if (target_col > prev_len) target_col = prev_len;
            ta->cursor_pos = prev_start + target_col;
            ta_compute_linecol(ta);
            ta_ensure_visible(ta);
            ta->base.dirty = 1;
        }
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_DOWN) {
        int total = ta_line_count(ta);
        if (ta->cursor_line < total - 1) {
            int target_col = ta->cursor_col;
            int next_start = ta_line_start(ta, ta->cursor_line + 1);
            int next_end   = ta_line_end(ta, ta->cursor_line + 1);
            int next_len = next_end - next_start;
            if (target_col > next_len) target_col = next_len;
            ta->cursor_pos = next_start + target_col;
            ta_compute_linecol(ta);
            ta_ensure_visible(ta);
            ta->base.dirty = 1;
        }
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_HOME) {
        int ls = ta_line_start(ta, ta->cursor_line);
        ta->cursor_pos = ls;
        ta_compute_linecol(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_END) {
        int le = ta_line_end(ta, ta->cursor_line);
        ta->cursor_pos = le;
        ta_compute_linecol(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_PGUP) {
        int jump = ta->visible_lines > 1 ? ta->visible_lines - 1 : 1;
        int target_line = ta->cursor_line - jump;
        if (target_line < 0) target_line = 0;
        int ls = ta_line_start(ta, target_line);
        int le = ta_line_end(ta, target_line);
        int line_len = le - ls;
        int col = ta->cursor_col;
        if (col > line_len) col = line_len;
        ta->cursor_pos = ls + col;
        ta_compute_linecol(ta);
        ta_ensure_visible(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_PGDN) {
        int total = ta_line_count(ta);
        int jump = ta->visible_lines > 1 ? ta->visible_lines - 1 : 1;
        int target_line = ta->cursor_line + jump;
        if (target_line >= total) target_line = total - 1;
        int ls = ta_line_start(ta, target_line);
        int le = ta_line_end(ta, target_line);
        int line_len = le - ls;
        int col = ta->cursor_col;
        if (col > line_len) col = line_len;
        ta->cursor_pos = ls + col;
        ta_compute_linecol(ta);
        ta_ensure_visible(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    /* --- Editing keys (only if not readonly) --- */

    if (ta->readonly) return 0;

    if (scancode == SC_BACKSPACE) {
        if (ta->cursor_pos > 0) {
            ta->cursor_pos--;
            ta_delete_at(ta, ta->cursor_pos);
            ta_compute_linecol(ta);
            ta_ensure_visible(ta);
            ta->base.dirty = 1;
            handled = 1;
        }
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_DELETE) {
        if (ta->cursor_pos < ta->text_len) {
            ta_delete_at(ta, ta->cursor_pos);
            ta_compute_linecol(ta);
            ta->base.dirty = 1;
            handled = 1;
        }
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_ENTER) {
        ta_insert_char(ta, '\n');
        ta_compute_linecol(ta);
        ta_ensure_visible(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    if (scancode == SC_TAB_KEY) {
        /* Insert 4 spaces */
        int i;
        for (i = 0; i < 4; i++) {
            if (!ta_insert_char(ta, ' ')) break;
        }
        ta_compute_linecol(ta);
        ta_ensure_visible(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    /* Printable ASCII chars */
    if (ascii >= 0x20 && ascii <= 0x7E) {
        ta_insert_char(ta, ascii);
        ta_compute_linecol(ta);
        ta_ensure_visible(ta);
        ta->base.dirty = 1;
        ta->cursor_visible = 1;
        ta->cursor_blink_tick = 0;
        return 1;
    }

    (void)handled;
    return 0;
}

void textarea_tick(textarea_t *ta) {
    if (!ta) return;
    ta->cursor_blink_tick++;
    if (ta->cursor_blink_tick >= BLINK_INTERVAL) {
        ta->cursor_blink_tick = 0;
        ta->cursor_visible = !ta->cursor_visible;
        ta->base.dirty = 1;
    }
}

int textarea_get_cursor_line(textarea_t *ta) {
    if (!ta) return 0;
    return ta->cursor_line;
}

int textarea_get_cursor_col(textarea_t *ta) {
    if (!ta) return 0;
    return ta->cursor_col;
}

void textarea_destroy(textarea_t *ta) {
    if (!ta) return;
    if (ta->base.ops->destroy) {
        ta->base.ops->destroy(&ta->base);
    }
    free(ta);
}
