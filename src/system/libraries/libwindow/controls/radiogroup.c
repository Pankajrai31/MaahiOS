/**
 * MaahiOS Window Library - Radio Group Control Implementation (Design System v2)
 *
 * Description:
 *   Horizontal row of radio buttons.  Click to select; fires on_change.
 *   Renders 12px-diameter circles with a 6px-diameter accent dot when
 *   selected.  Labels in FONT_SMALL to the right of each circle.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "radiogroup.h"
#include "../surface.h"
#include "../theme.h"

/* Forward — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS
 *===========================================================================*/

#define RG_RING_NORMAL     THEME_BEVEL_DARK        /* Unselected ring       */
#define RG_RING_SELECTED   THEME_ACCENT            /* Selected ring         */
#define RG_DOT_COLOR       THEME_ACCENT            /* Selected dot          */
#define RG_INNER_BG        THEME_SURFACE           /* Circle interior       */
#define RG_LABEL_FG        THEME_TEXT               /* Label text color      */

/*=============================================================================
 * INTERNAL: STRING HELPERS
 *===========================================================================*/

static void rg_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*=============================================================================
 * INTERNAL: RECALCULATE ITEM POSITIONS
 *===========================================================================*/

static void rg_recalc_positions(radiogroup_t *rg) {
    int cx = 0;
    int i;
    for (i = 0; i < rg->option_count; i++) {
        rg->item_x[i] = cx;
        /* Width = circle diameter + text pad + text width + item pad */
        int text_w = surface_measure_text(rg->labels[i], FONT_SMALL);
        int w = RADIOGROUP_CIRCLE_R * 2 + RADIOGROUP_TEXT_PAD + text_w;
        rg->item_w[i] = w;
        cx += w + RADIOGROUP_ITEM_PAD;
    }
}

/*=============================================================================
 * INTERNAL: HIT TEST — which option is under (lx, ly)?
 *===========================================================================*/

static int rg_hit_option(radiogroup_t *rg, int lx, int ly) {
    (void)ly;  /* Horizontal layout — all items span full height */
    int i;
    for (i = 0; i < rg->option_count; i++) {
        if (lx >= rg->item_x[i] && lx < rg->item_x[i] + rg->item_w[i])
            return i;
    }
    return -1;
}

/*=============================================================================
 * VTABLE: DRAW
 *===========================================================================*/

static void radiogroup_draw(control_t *ctrl, surface_t *surf) {
    radiogroup_t *rg = (radiogroup_t *)ctrl;
    int base_x = ctrl->x;
    int base_y = ctrl->y;
    int h = ctrl->height;
    int circle_cy = base_y + h / 2;  /* Vertical center */
    int i;

    for (i = 0; i < rg->option_count; i++) {
        int ix = base_x + rg->item_x[i];
        int circle_cx = ix + RADIOGROUP_CIRCLE_R;

        int is_sel = (i == rg->selected);

        /* Outer ring */
        uint32_t ring_color = is_sel ? RG_RING_SELECTED : RG_RING_NORMAL;
        surface_fill_circle(surf, circle_cx, circle_cy,
                            RADIOGROUP_CIRCLE_R, ring_color);

        /* Inner background (white) — 1px smaller to create ring */
        surface_fill_circle(surf, circle_cx, circle_cy,
                            RADIOGROUP_CIRCLE_R - 2, RG_INNER_BG);

        /* Selected dot */
        if (is_sel) {
            surface_fill_circle(surf, circle_cx, circle_cy,
                                RADIOGROUP_DOT_R, RG_DOT_COLOR);
        }

        /* Label text */
        int text_x = ix + RADIOGROUP_CIRCLE_R * 2 + RADIOGROUP_TEXT_PAD;
        int text_h = surface_text_height(FONT_SMALL);
        int text_y = base_y + (h - text_h) / 2;
        surface_draw_text(surf, text_x, text_y, rg->labels[i],
                          FONT_SMALL, RG_LABEL_FG);
    }
}

/*=============================================================================
 * VTABLE: EVENT
 *===========================================================================*/

static int radiogroup_event(control_t *ctrl, gui_event_t *evt) {
    radiogroup_t *rg = (radiogroup_t *)ctrl;
    if (!ctrl->enabled) return 0;

    if (evt->type == GUI_EVENT_MOUSE_MOVE) {
        int lx = evt->mouse_x - ctrl->x;
        int ly = evt->mouse_y - ctrl->y;
        int new_hover = rg_hit_option(rg, lx, ly);
        if (new_hover != rg->hover) {
            rg->hover = new_hover;
            ctrl->dirty = 1;
        }
        return 1;
    }

    if (evt->type == GUI_EVENT_MOUSE_LEAVE) {
        if (rg->hover != -1) {
            rg->hover = -1;
            ctrl->dirty = 1;
        }
        return 0;
    }

    if (evt->type == GUI_EVENT_MOUSE_DOWN && evt->mouse_button == 0) {
        int lx = evt->mouse_x - ctrl->x;
        int ly = evt->mouse_y - ctrl->y;
        int clicked = rg_hit_option(rg, lx, ly);
        if (clicked >= 0 && clicked != rg->selected) {
            rg->selected = clicked;
            ctrl->dirty = 1;
            if (rg->on_change) {
                rg->on_change(clicked, rg->change_data);
            }
            return 1;
        }
        return (clicked >= 0) ? 1 : 0;
    }

    return 0;
}

/*=============================================================================
 * VTABLE: DESTROY
 *===========================================================================*/

static void radiogroup_destroy_impl(control_t *ctrl) {
    (void)ctrl;
}

static const control_ops_t radiogroup_ops = {
    .draw    = radiogroup_draw,
    .event   = radiogroup_event,
    .destroy = radiogroup_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

radiogroup_t *radiogroup_create(int x, int y, int w, int h) {
    radiogroup_t *rg = (radiogroup_t *)malloc(sizeof(radiogroup_t));
    if (!rg) return (radiogroup_t *)0;

    /* Zero-init */
    {
        unsigned char *p = (unsigned char *)rg;
        unsigned int i;
        for (i = 0; i < sizeof(radiogroup_t); i++) p[i] = 0;
    }

    CONTROL_INIT(&rg->base, CONTROL_RADIO, x, y, w, h, &radiogroup_ops);

    rg->option_count = 0;
    rg->selected     = -1;
    rg->hover        = -1;
    rg->on_change    = (void (*)(int, void *))0;
    rg->change_data  = (void *)0;

    return rg;
}

int radiogroup_add_option(radiogroup_t *rg, const char *label) {
    if (!rg || rg->option_count >= RADIOGROUP_MAX_OPTIONS) return -1;

    int idx = rg->option_count;
    rg_strcpy(rg->labels[idx], label, RADIOGROUP_MAX_LABEL);
    rg->option_count++;

    /* Recalculate all item positions */
    rg_recalc_positions(rg);

    rg->base.dirty = 1;
    return idx;
}

void radiogroup_set_selected(radiogroup_t *rg, int index) {
    if (!rg) return;
    if (index < 0 || index >= rg->option_count) return;
    if (rg->selected != index) {
        rg->selected = index;
        rg->base.dirty = 1;
    }
}

int radiogroup_get_selected(radiogroup_t *rg) {
    if (!rg) return -1;
    return rg->selected;
}

void radiogroup_set_on_change(radiogroup_t *rg,
                              void (*callback)(int selected, void *userdata),
                              void *userdata) {
    if (!rg) return;
    rg->on_change   = callback;
    rg->change_data = userdata;
}

void radiogroup_destroy(radiogroup_t *rg) {
    if (!rg) return;
    if (rg->base.ops->destroy) {
        rg->base.ops->destroy(&rg->base);
    }
    free(rg);
}
