/**
 * MaahiOS Window Library - Dialog Control Implementation
 *
 * Description:
 *   Modal dialog overlay drawn on top of the parent window's surface.
 *   Supports three types: INFO (close only), CONFIRM (OK/Cancel),
 *   and CUSTOM (user-defined buttons).
 *
 *   The dialog renders as a centered overlay. The draw function
 *   paints a semi-transparent scrim, then the dialog box on top.
 *   The event function handles mouse clicks on buttons and close icon.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "dialog.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * INTERNAL: STRING HELPERS
 *===========================================================================*/

static void _dlg_str_copy(char *dst, const char *src, int max) {
    int i = 0;
    if (src) {
        while (i < max - 1 && src[i]) {
            dst[i] = src[i];
            i++;
        }
    }
    dst[i] = '\0';
}

static int _dlg_str_len(const char *s) {
    int len = 0;
    while (s && s[len]) len++;
    return len;
}

static void _dlg_memset(void *dst, int val, unsigned int size) {
    unsigned char *d = (unsigned char *)dst;
    for (unsigned int i = 0; i < size; i++) d[i] = (unsigned char)val;
}

/*=============================================================================
 * INTERNAL: GEOMETRY HELPERS
 *===========================================================================*/

/**
 * Calculate button X position within the dialog, right-aligned.
 * For "index" = button index counting from left (0-based).
 */
static int _dlg_button_x(const dialog_t *dlg, int index) {
    int total_w = dlg->button_count * DIALOG_BTN_W +
                  (dlg->button_count - 1) * DIALOG_BTN_PAD;
    int start_x = dlg->base.x + (dlg->base.width - total_w) / 2;
    return start_x + index * (DIALOG_BTN_W + DIALOG_BTN_PAD);
}

static int _dlg_button_y(const dialog_t *dlg) {
    return dlg->base.y + dlg->base.height - DIALOG_MARGIN - DIALOG_BTN_H;
}

/**
 * Check if (mx,my) is inside button at index.
 */
static int _dlg_btn_hit(const dialog_t *dlg, int index, int mx, int my) {
    int bx = _dlg_button_x(dlg, index);
    int by = _dlg_button_y(dlg);
    return (mx >= bx && mx < bx + DIALOG_BTN_W &&
            my >= by && my < by + DIALOG_BTN_H);
}

/**
 * Check if (mx,my) is on the close icon (top-right of title bar).
 */
static int _dlg_close_hit(const dialog_t *dlg, int mx, int my) {
    int cx = dlg->base.x + dlg->base.width - DIALOG_TITLE_H;
    int cy = dlg->base.y;
    return (mx >= cx && mx < cx + DIALOG_TITLE_H &&
            my >= cy && my < cy + DIALOG_TITLE_H);
}

/*=============================================================================
 * VTABLE: DRAW
 *===========================================================================*/

static void dialog_draw(control_t *ctrl, surface_t *surf) {
    dialog_t *dlg = (dialog_t *)ctrl;
    if (!dlg->active) return;

    int dx = dlg->base.x;
    int dy = dlg->base.y;
    int dw = dlg->base.width;
    int dh = dlg->base.height;
    int bw = DIALOG_BEVEL_W;

    /* 1. Drop shadow (4px offset) */
    surface_fill_rect(surf, dx + 4, dy + 4, dw, dh, DIALOG_SHADOW);

    /* 2. Chrome background fill */
    surface_fill_rect(surf, dx, dy, dw, dh, DIALOG_BG);

    /* 3. Outer 3D raised bevel border */
    surface_draw_hline(surf, dx, dy, dw, THEME_BEVEL_LIGHT);
    surface_draw_hline(surf, dx + 1, dy + 1, dw - 2, THEME_BEVEL_LIGHT);
    surface_draw_vline(surf, dx, dy, dh, THEME_BEVEL_LIGHT);
    surface_draw_vline(surf, dx + 1, dy + 1, dh - 2, THEME_BEVEL_LIGHT);
    surface_draw_hline(surf, dx, dy + dh - 1, dw, THEME_BEVEL_DARK);
    surface_draw_hline(surf, dx + 1, dy + dh - 2, dw - 2, THEME_BEVEL_DARK);
    surface_draw_vline(surf, dx + dw - 1, dy, dh, THEME_BEVEL_DARK);
    surface_draw_vline(surf, dx + dw - 2, dy + 1, dh - 2, THEME_BEVEL_DARK);

    /* 4. Gradient blue titlebar (column by column, like windows) */
    {
        int tb_x = dx + bw;
        int tb_y = dy + bw;
        int tb_w = dw - bw * 2;
        int tb_h = DIALOG_TITLE_H;

        for (int col = 0; col < tb_w; col++) {
            /* Linear interpolation between titlebar start and end colors */
            uint32_t cs = THEME_TITLEBAR_START;
            uint32_t ce = THEME_TITLEBAR_END;
            int r0 = (cs >> 16) & 0xFF, g0 = (cs >> 8) & 0xFF, b0 = cs & 0xFF;
            int r1 = (ce >> 16) & 0xFF, g1 = (ce >> 8) & 0xFF, b1 = ce & 0xFF;
            int r = r0 + (r1 - r0) * col / (tb_w > 1 ? tb_w - 1 : 1);
            int g = g0 + (g1 - g0) * col / (tb_w > 1 ? tb_w - 1 : 1);
            int b = b0 + (b1 - b0) * col / (tb_w > 1 ? tb_w - 1 : 1);
            uint32_t c = (uint32_t)(((r & 0xFF) << 16) | ((g & 0xFF) << 8) | (b & 0xFF));
            surface_draw_vline(surf, tb_x + col, tb_y, tb_h, c);
        }

        /* Title text */
        int text_h = surface_text_height(THEME_FONT_BODY);
        int text_y = tb_y + (tb_h - text_h) / 2;
        surface_draw_text(surf, tb_x + 8, text_y,
                          dlg->title, THEME_FONT_BODY, DIALOG_TITLE_FG);

        /* Close button: chrome raised box with 'X' */
        int cb_w = THEME_TITLEBAR_BTN_W;
        int cb_h = THEME_TITLEBAR_BTN_H;
        int cb_x = tb_x + tb_w - cb_w - 4;
        int cb_y = tb_y + (tb_h - cb_h) / 2;

        uint32_t cb_bg = dlg->hover_close ? THEME_CHROME_LIGHTER : THEME_CHROME;
        surface_fill_rect(surf, cb_x, cb_y, cb_w, cb_h, cb_bg);
        /* Raised bevel on close button */
        surface_draw_hline(surf, cb_x, cb_y, cb_w, THEME_BEVEL_LIGHT);
        surface_draw_vline(surf, cb_x, cb_y, cb_h, THEME_BEVEL_LIGHT);
        surface_draw_hline(surf, cb_x, cb_y + cb_h - 1, cb_w, THEME_BEVEL_DARK);
        surface_draw_vline(surf, cb_x + cb_w - 1, cb_y, cb_h, THEME_BEVEL_DARK);
        /* 'X' glyph */
        surface_draw_char_transparent(
            surf, cb_x + (cb_w - THEME_FONT_WIDTH) / 2,
            cb_y + (cb_h - THEME_FONT_HEIGHT) / 2 + 1,
            'X', DIALOG_CLOSE_X_FG);
    }

    /* 5. White content area (sunken bevel) */
    {
        int ca_x = dx + bw + 4;
        int ca_y = dy + bw + DIALOG_TITLE_H + 4;
        int ca_w = dw - bw * 2 - 8;
        int ca_h = dh - bw * 2 - DIALOG_TITLE_H - DIALOG_BTN_H - 24;

        surface_fill_rect(surf, ca_x, ca_y, ca_w, ca_h, DIALOG_SURFACE);
        /* Sunken bevel: dark top/left, light bottom/right */
        surface_draw_hline(surf, ca_x, ca_y, ca_w, THEME_BEVEL_DARK);
        surface_draw_vline(surf, ca_x, ca_y, ca_h, THEME_BEVEL_DARK);
        surface_draw_hline(surf, ca_x, ca_y + ca_h - 1, ca_w, THEME_BEVEL_LIGHT);
        surface_draw_vline(surf, ca_x + ca_w - 1, ca_y, ca_h, THEME_BEVEL_LIGHT);

        /* Message text inside content area */
        surface_draw_text(surf, ca_x + 8, ca_y + 6, dlg->message,
                          THEME_FONT_BODY, DIALOG_TEXT_FG);
    }

    /* 6. Themed 3D buttons */
    for (int i = 0; i < dlg->button_count; i++) {
        int bx = _dlg_button_x(dlg, i);
        int by = _dlg_button_y(dlg);

        /* All buttons: uniform standard chrome 3D raised style */
        uint32_t bg, fg, hi, lo;
        int offset = 0;

        bg = THEME_BTN_STD_BG;
        fg = THEME_BTN_STD_FG;
        hi = THEME_BEVEL_LIGHT;
        lo = THEME_BEVEL_DARK;
        if (dlg->hover_btn == i) { bg = THEME_CHROME_LIGHTER; }

        surface_fill_rect(surf, bx, by, DIALOG_BTN_W, DIALOG_BTN_H, bg);
        /* 3D bevel edges */
        surface_draw_hline(surf, bx, by, DIALOG_BTN_W, hi);
        surface_draw_vline(surf, bx, by, DIALOG_BTN_H, hi);
        surface_draw_hline(surf, bx, by + DIALOG_BTN_H - 1, DIALOG_BTN_W, lo);
        surface_draw_vline(surf, bx + DIALOG_BTN_W - 1, by, DIALOG_BTN_H, lo);

        /* Center button label text */
        int tw = surface_measure_text(dlg->buttons[i].label, THEME_FONT_BODY);
        int th = surface_text_height(THEME_FONT_BODY);
        int tx = bx + (DIALOG_BTN_W - tw) / 2 + offset;
        int ty = by + (DIALOG_BTN_H - th) / 2 + offset;
        surface_draw_text(surf, tx, ty, dlg->buttons[i].label,
                          THEME_FONT_BODY, fg);
    }

    dlg->base.dirty = 0;
}

/*=============================================================================
 * VTABLE: EVENT
 *===========================================================================*/

static int dialog_event(control_t *ctrl, gui_event_t *evt) {
    dialog_t *dlg = (dialog_t *)ctrl;
    if (!dlg->active) return 0;

    int mx = evt->mouse_x;
    int my = evt->mouse_y;

    switch (evt->type) {
        case GUI_EVENT_MOUSE_MOVE: {
            int old_hover = dlg->hover_btn;
            int old_close = dlg->hover_close;
            dlg->hover_btn = -1;
            dlg->hover_close = 0;

            for (int i = 0; i < dlg->button_count; i++) {
                if (_dlg_btn_hit(dlg, i, mx, my)) {
                    dlg->hover_btn = i;
                    break;
                }
            }
            if (_dlg_close_hit(dlg, mx, my)) {
                dlg->hover_close = 1;
            }

            if (old_hover != dlg->hover_btn || old_close != dlg->hover_close)
                dlg->base.dirty = 1;

            return 1; /* Consume all mouse moves when dialog is active */
        }

        case GUI_EVENT_MOUSE_DOWN: {
            /* Check buttons */
            for (int i = 0; i < dlg->button_count; i++) {
                if (_dlg_btn_hit(dlg, i, mx, my)) {
                    dialog_dismiss(dlg, dlg->buttons[i].result);
                    return 1;
                }
            }
            /* Check close icon */
            if (_dlg_close_hit(dlg, mx, my)) {
                dialog_dismiss(dlg, DIALOG_RESULT_CLOSED);
                return 1;
            }
            return 1; /* Consume click even if missed buttons (modal) */
        }

        case GUI_EVENT_KEY_DOWN: {
            /* ESC dismisses dialog */
            if (evt->key_code == 0x01 /* ESC scancode */) {
                dialog_dismiss(dlg, DIALOG_RESULT_CANCEL);
                return 1;
            }
            /* Enter triggers OK/primary button */
            if (evt->key_char == '\n' || evt->key_code == 0x1C) {
                if (dlg->button_count > 0) {
                    /* Find the accent/primary button, or the last button */
                    for (int i = 0; i < dlg->button_count; i++) {
                        if (dlg->buttons[i].is_accent) {
                            dialog_dismiss(dlg, dlg->buttons[i].result);
                            return 1;
                        }
                    }
                    /* No accent button — use first button */
                    dialog_dismiss(dlg, dlg->buttons[0].result);
                }
                return 1;
            }
            return 1; /* Consume all keys when modal */
        }

        default:
            break;
    }

    return 0;
}

/*=============================================================================
 * VTABLE: DESTROY
 *===========================================================================*/

static void dialog_control_destroy(control_t *ctrl) {
    /* Nothing to free besides the struct itself, handled by dialog_destroy */
    (void)ctrl;
}

/*=============================================================================
 * STATIC VTABLE
 *===========================================================================*/

static const control_ops_t dialog_ops = {
    .draw    = dialog_draw,
    .event   = dialog_event,
    .destroy = dialog_control_destroy,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

dialog_t *dialog_create(dialog_type_t type, const char *title,
                        const char *message, int w, int h) {
    dialog_t *dlg = (dialog_t *)malloc(sizeof(dialog_t));
    if (!dlg) return (void *)0;

    _dlg_memset(dlg, 0, sizeof(dialog_t));

    if (w <= 0) w = 300;
    if (h <= 0) h = 140;

    CONTROL_INIT(&dlg->base, CONTROL_ALERT, 0, 0, w, h, &dialog_ops);
    dlg->base.visible = 0; /* Hidden until dialog_show() */

    dlg->type = type;
    _dlg_str_copy(dlg->title, title, DIALOG_MAX_TITLE);
    _dlg_str_copy(dlg->message, message, DIALOG_MAX_MESSAGE);
    dlg->active = 0;
    dlg->result = DIALOG_RESULT_NONE;
    dlg->hover_btn = -1;
    dlg->hover_close = 0;
    dlg->button_count = 0;
    dlg->on_result = (void *)0;
    dlg->result_data = (void *)0;

    /* Auto-create buttons for DIALOG_CONFIRM */
    if (type == DIALOG_CONFIRM) {
        dialog_add_button(dlg, "Cancel", DIALOG_RESULT_CANCEL, 0);
        dialog_add_button(dlg, "OK", DIALOG_RESULT_OK, 1);
    }

    return dlg;
}

int dialog_add_button(dialog_t *dlg, const char *label,
                      dialog_result_t result, int is_accent) {
    if (!dlg || dlg->button_count >= DIALOG_MAX_BUTTONS) return -1;

    int idx = dlg->button_count;
    _dlg_str_copy(dlg->buttons[idx].label, label, DIALOG_MAX_BTN_LABEL);
    dlg->buttons[idx].result = result;
    dlg->buttons[idx].is_accent = (uint8_t)(is_accent ? 1 : 0);
    dlg->button_count++;
    return 0;
}

void dialog_set_on_result(dialog_t *dlg,
                          void (*callback)(dialog_result_t result,
                                           void *userdata),
                          void *userdata) {
    if (!dlg) return;
    dlg->on_result = callback;
    dlg->result_data = userdata;
}

void dialog_show(dialog_t *dlg) {
    if (!dlg) return;
    dlg->active = 1;
    dlg->base.visible = 1;
    dlg->base.dirty = 1;
    dlg->result = DIALOG_RESULT_NONE;
    dlg->hover_btn = -1;
    dlg->hover_close = 0;
}

void dialog_dismiss(dialog_t *dlg, dialog_result_t result) {
    if (!dlg) return;
    dlg->active = 0;
    dlg->base.visible = 0;
    dlg->result = result;

    if (dlg->on_result) {
        dlg->on_result(result, dlg->result_data);
    }
}

void dialog_destroy(dialog_t *dlg) {
    if (!dlg) return;
    free(dlg);
}
