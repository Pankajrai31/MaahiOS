/**
 * MaahiOS Window Library - Toolbar Control Implementation (Design System v2)
 *
 * Description:
 *   Renders a chrome toolbar strip with flat text buttons that show
 *   raised bevel on hover and sunken bevel on press.
 *
 *   Background: chrome gradient (raised bevel top/bottom edges)
 *   Buttons: flat (transparent) normally, raised on hover, sunken on press
 *   Separator: 1px dark line with 1px light line beside it
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "toolbar.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS
 *===========================================================================*/

#define TB_BG           THEME_CHROME
#define TB_FG           THEME_TEXT
#define TB_FG_DISABLED  THEME_TEXT_DISABLED
#define TB_HOVER_BG     THEME_CHROME_LIGHT
#define TB_BEVEL_LIGHT  THEME_BEVEL_LIGHT
#define TB_BEVEL_DARK   THEME_BEVEL_DARK
#define TB_SEP_DARK     THEME_BEVEL_DARK
#define TB_SEP_LIGHT    THEME_BEVEL_LIGHT

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static int tb_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void tb_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/** Recompute item positions from left to right. */
static void toolbar_layout(toolbar_t *tb) {
    int cx = 4;  /* Left margin */
    int i;
    for (i = 0; i < tb->item_count; i++) {
        toolbar_item_t *item = &tb->items[i];
        item->x = cx;
        if (item->type == TOOLBAR_ITEM_SEPARATOR) {
            item->width = TOOLBAR_SEP_WIDTH;
        } else {
            int tw = surface_measure_text(item->label, FONT_SMALL);
            item->width = tw + TOOLBAR_BTN_PAD_X * 2;
        }
        cx += item->width + TOOLBAR_BTN_GAP;
    }
}

/*=============================================================================
 * VTABLE FUNCTIONS
 *===========================================================================*/

static void toolbar_draw(control_t *ctrl, surface_t *surf) {
    toolbar_t *tb = (toolbar_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = TOOLBAR_HEIGHT;

    /* Chrome background */
    surface_fill_rect(surf, x, y, w, h, TB_BG);

    /* Top highlight line */
    surface_draw_hline(surf, x, y, w, TB_BEVEL_LIGHT);
    /* Bottom shadow line */
    surface_draw_hline(surf, x, y + h - 1, w, TB_BEVEL_DARK);

    /* Draw items */
    int i;
    int btn_y = y + 2;
    int btn_h = h - 4;
    int th = surface_text_height(FONT_SMALL);

    for (i = 0; i < tb->item_count; i++) {
        toolbar_item_t *item = &tb->items[i];
        int ix = x + item->x;

        if (item->type == TOOLBAR_ITEM_SEPARATOR) {
            /* Vertical separator: dark line + light line */
            int sx = ix + TOOLBAR_SEP_WIDTH / 2 - 1;
            surface_draw_vline(surf, sx, btn_y + 2, btn_h - 4, TB_SEP_DARK);
            surface_draw_vline(surf, sx + 1, btn_y + 2, btn_h - 4, TB_SEP_LIGHT);
            continue;
        }

        /* Button item */
        int bw = item->width;
        int text_shift = 0;

        if (i == tb->pressed_index && item->enabled) {
            /* Sunken bevel */
            surface_fill_rect(surf, ix, btn_y, bw, btn_h, TB_BG);
            surface_draw_hline(surf, ix, btn_y, bw, TB_BEVEL_DARK);
            surface_draw_vline(surf, ix, btn_y, btn_h, TB_BEVEL_DARK);
            surface_draw_hline(surf, ix, btn_y + btn_h - 1, bw, TB_BEVEL_LIGHT);
            surface_draw_vline(surf, ix + bw - 1, btn_y, btn_h, TB_BEVEL_LIGHT);
            text_shift = 1;
        } else if (i == tb->hover_index && item->enabled) {
            /* Raised bevel on hover */
            surface_fill_rect(surf, ix, btn_y, bw, btn_h, TB_HOVER_BG);
            surface_draw_hline(surf, ix, btn_y, bw, TB_BEVEL_LIGHT);
            surface_draw_vline(surf, ix, btn_y, btn_h, TB_BEVEL_LIGHT);
            surface_draw_hline(surf, ix, btn_y + btn_h - 1, bw, TB_BEVEL_DARK);
            surface_draw_vline(surf, ix + bw - 1, btn_y, btn_h, TB_BEVEL_DARK);
        }

        /* Text */
        uint32_t fg = item->enabled ? TB_FG : TB_FG_DISABLED;
        int tw = surface_measure_text(item->label, FONT_SMALL);
        int text_x = ix + (bw - tw) / 2 + text_shift;
        int text_y = btn_y + (btn_h - th) / 2 + text_shift;
        surface_draw_text(surf, text_x, text_y, item->label, FONT_SMALL, fg);
    }
}

/** Hit-test: find which item index the mouse is over. */
static int toolbar_hit(toolbar_t *tb, int mx, int my) {
    int btn_y = tb->base.y + 2;
    int btn_h = TOOLBAR_HEIGHT - 4;
    if (my < btn_y || my >= btn_y + btn_h) return -1;

    int i;
    for (i = 0; i < tb->item_count; i++) {
        toolbar_item_t *item = &tb->items[i];
        if (item->type == TOOLBAR_ITEM_SEPARATOR) continue;
        int ix = tb->base.x + item->x;
        if (mx >= ix && mx < ix + item->width) return i;
    }
    return -1;
}

static int toolbar_event(control_t *ctrl, gui_event_t *evt) {
    toolbar_t *tb = (toolbar_t *)ctrl;
    if (!ctrl->enabled) return 0;

    if (evt->type == GUI_EVENT_MOUSE_MOVE) {
        int old_hover = tb->hover_index;
        tb->hover_index = toolbar_hit(tb, evt->mouse_x, evt->mouse_y);
        if (tb->hover_index != old_hover) ctrl->dirty = 1;
        return (tb->hover_index >= 0) ? 1 : 0;
    }

    if (evt->type == GUI_EVENT_MOUSE_DOWN && evt->mouse_button == 0) {
        int idx = toolbar_hit(tb, evt->mouse_x, evt->mouse_y);
        if (idx >= 0 && tb->items[idx].enabled) {
            tb->pressed_index = idx;
            ctrl->dirty = 1;
            return 1;
        }
    }

    if (evt->type == GUI_EVENT_MOUSE_UP && evt->mouse_button == 0) {
        if (tb->pressed_index >= 0) {
            int idx = toolbar_hit(tb, evt->mouse_x, evt->mouse_y);
            if (idx == tb->pressed_index && tb->items[idx].enabled) {
                if (tb->items[idx].on_click) {
                    tb->items[idx].on_click(tb->items[idx].click_data);
                }
            }
            tb->pressed_index = -1;
            ctrl->dirty = 1;
            return 1;
        }
    }

    if (evt->type == GUI_EVENT_MOUSE_LEAVE) {
        if (tb->hover_index >= 0 || tb->pressed_index >= 0) {
            tb->hover_index = -1;
            tb->pressed_index = -1;
            ctrl->dirty = 1;
        }
        return 0;
    }

    return 0;
}

static void toolbar_destroy_impl(control_t *ctrl) {
    (void)ctrl;
}

static const control_ops_t toolbar_ops = {
    .draw    = toolbar_draw,
    .event   = toolbar_event,
    .destroy = toolbar_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

toolbar_t *toolbar_create(int x, int y, int w) {
    toolbar_t *tb = (toolbar_t *)malloc(sizeof(toolbar_t));
    if (!tb) return (toolbar_t *)0;

    /* Zero-initialize */
    {
        unsigned char *p = (unsigned char *)tb;
        unsigned int i;
        for (i = 0; i < sizeof(toolbar_t); i++) p[i] = 0;
    }

    CONTROL_INIT(&tb->base, CONTROL_PANEL, x, y, w, TOOLBAR_HEIGHT, &toolbar_ops);
    tb->item_count = 0;
    tb->hover_index = -1;
    tb->pressed_index = -1;

    return tb;
}

int toolbar_add_button(toolbar_t *tb, const char *label,
                       void (*callback)(void *userdata), void *userdata) {
    if (!tb || tb->item_count >= TOOLBAR_MAX_ITEMS) return -1;

    int idx = tb->item_count;
    toolbar_item_t *item = &tb->items[idx];
    item->type = TOOLBAR_ITEM_BUTTON;
    tb_strcpy(item->label, label, TOOLBAR_MAX_LABEL);
    item->enabled = 1;
    item->on_click = callback;
    item->click_data = userdata;
    tb->item_count++;

    toolbar_layout(tb);
    tb->base.dirty = 1;
    return idx;
}

int toolbar_add_separator(toolbar_t *tb) {
    if (!tb || tb->item_count >= TOOLBAR_MAX_ITEMS) return -1;

    int idx = tb->item_count;
    toolbar_item_t *item = &tb->items[idx];
    item->type = TOOLBAR_ITEM_SEPARATOR;
    item->label[0] = '\0';
    item->enabled = 0;
    item->on_click = (void (*)(void *))0;
    item->click_data = (void *)0;
    tb->item_count++;

    toolbar_layout(tb);
    tb->base.dirty = 1;
    return idx;
}

void toolbar_set_enabled(toolbar_t *tb, int index, int enabled) {
    if (!tb || index < 0 || index >= tb->item_count) return;
    tb->items[index].enabled = enabled;
    tb->base.dirty = 1;
}

void toolbar_destroy(toolbar_t *tb) {
    if (!tb) return;
    if (tb->base.ops->destroy) {
        tb->base.ops->destroy(&tb->base);
    }
    free(tb);
}
