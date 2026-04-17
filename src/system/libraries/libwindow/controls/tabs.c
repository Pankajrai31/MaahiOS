/**
 * MaahiOS Window Library - Tab Control Implementation (Design System v2)
 *
 * Description:
 *   Renders a tab strip (raised chrome headers) with a sunken content area.
 *   Only the active tab's children are drawn and receive events.
 *   Click on a tab header to switch tabs.
 *
 *   Visual:
 *     Active tab = white bg, no bottom border (merges with content)
 *     Inactive = chrome bg, full bevel border
 *     Content area = sunken border, white background
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "tabs.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * INTERNAL: Compute tab header widths
 *===========================================================================*/

static int tabs_label_width(const char *label) {
    return surface_measure_text(label, FONT_SMALL) + TABS_PAD * 2;
}

/*=============================================================================
 * VTABLE FUNCTIONS
 *===========================================================================*/

static void tabs_draw(control_t *ctrl, surface_t *surf) {
    tabs_t *t = (tabs_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = ctrl->height;

    /* ---- Background behind tab headers (chrome strip) ---- */
    surface_fill_rect(surf, x, y, w, TABS_HEADER_H, THEME_CHROME);

    /* ---- Draw tab headers ---- */
    int tab_x = x + 2;
    for (int i = 0; i < t->tab_count; i++) {
        int tw = tabs_label_width(t->tabs[i].label);
        int th = TABS_HEADER_H - 2;
        int ty = y + 2;

        if (i == t->active_tab) {
            /* Active tab: white bg, raised bevel on top/left/right, no bottom */
            surface_fill_rect(surf, tab_x, ty, tw, th + 2, THEME_SURFACE);

            /* Top highlight */
            surface_draw_hline(surf, tab_x, ty, tw, THEME_BEVEL_LIGHT);
            /* Left highlight */
            surface_draw_vline(surf, tab_x, ty, th + 2, THEME_BEVEL_LIGHT);
            /* Right shadow */
            surface_draw_vline(surf, tab_x + tw - 1, ty, th + 2, THEME_BEVEL_DARK);

            /* Tab label text */
            int fh = surface_text_height(FONT_SMALL);
            int text_y = ty + (th - fh) / 2;
            if (text_y < ty) text_y = ty;
            surface_draw_text(surf, tab_x + TABS_PAD, text_y,
                              t->tabs[i].label, FONT_SMALL, THEME_TEXT);
        } else {
            /* Inactive tab: chrome bg, full bevel, slightly shorter */
            uint32_t bg = (i == t->hover_tab) ? THEME_CHROME_LIGHT : THEME_CHROME;
            surface_fill_rect(surf, tab_x, ty + 2, tw, th - 2, bg);

            /* Raised bevel */
            surface_draw_hline(surf, tab_x, ty + 2, tw, THEME_BEVEL_LIGHT);
            surface_draw_vline(surf, tab_x, ty + 2, th - 2, THEME_BEVEL_LIGHT);
            surface_draw_vline(surf, tab_x + tw - 1, ty + 2, th - 2, THEME_BEVEL_DARK);
            surface_draw_hline(surf, tab_x, ty + th - 1, tw, THEME_BEVEL_DARK);

            /* Tab label text */
            int fh = surface_text_height(FONT_SMALL);
            int text_y = ty + 2 + (th - 2 - fh) / 2;
            if (text_y < ty + 2) text_y = ty + 2;
            surface_draw_text(surf, tab_x + TABS_PAD, text_y,
                              t->tabs[i].label, FONT_SMALL, THEME_TEXT_SECONDARY);
        }
        tab_x += tw + 2;
    }

    /* ---- Dividing line under tab strip ---- */
    surface_draw_hline(surf, x, y + TABS_HEADER_H - 1, w, THEME_BEVEL_DARK);

    /* ---- Content area: sunken border + white fill ---- */
    /* Compute content rect from the draw-adjusted ctrl position,
     * NOT from the stored t->content_x/y (which are un-adjusted). */
    int cx = x;
    int cy = y + TABS_HEADER_H;
    int cw = w;
    int ch = h - TABS_HEADER_H;

    /* Sunken border: top/left = dark, bottom/right = light */
    surface_draw_hline(surf, cx, cy, cw, THEME_BEVEL_DARK);
    surface_draw_vline(surf, cx, cy, ch, THEME_BEVEL_DARK);
    surface_draw_hline(surf, cx, cy + ch - 1, cw, THEME_BEVEL_LIGHT);
    surface_draw_vline(surf, cx + cw - 1, cy, ch, THEME_BEVEL_LIGHT);

    /* Inner content fill */
    surface_fill_rect(surf, cx + TABS_BORDER, cy + TABS_BORDER,
                      cw - TABS_BORDER * 2, ch - TABS_BORDER * 2,
                      THEME_SURFACE);

    /* ---- Draw active tab's children ---- */
    /* Children's positions were permanently offset by tabs_add_child()
     * using t->content_x/y.  draw_content() added (win->content_x/y)
     * to this control's x/y, but children didn't get that adjustment.
     * Compute the delta and temporarily apply it to each child. */
    int offs_x = x - t->content_x;
    int offs_y = y - (t->content_y - TABS_HEADER_H);

    if (t->active_tab >= 0 && t->active_tab < t->tab_count) {
        tab_def_t *active = &t->tabs[t->active_tab];
        for (int i = 0; i < active->child_count; i++) {
            control_t *child = active->children[i];
            if (child && child->visible && child->ops && child->ops->draw) {
                child->x += offs_x;
                child->y += offs_y;
                child->ops->draw(child, surf);
                child->x -= offs_x;
                child->y -= offs_y;
            }
        }
    }
}

static int tabs_event(control_t *ctrl, gui_event_t *evt) {
    tabs_t *t = (tabs_t *)ctrl;

    if (!evt) return 0;

    int mx = evt->mouse_x;
    int my = evt->mouse_y;

    /* Check if click/move is in the tab header strip */
    if (my >= ctrl->y && my < ctrl->y + TABS_HEADER_H) {
        /* Determine which tab header was hit */
        int tab_x = ctrl->x + 2;
        int hit_tab = -1;
        for (int i = 0; i < t->tab_count; i++) {
            int tw = tabs_label_width(t->tabs[i].label);
            if (mx >= tab_x && mx < tab_x + tw) {
                hit_tab = i;
                break;
            }
            tab_x += tw + 2;
        }

        if (evt->type == GUI_EVENT_MOUSE_MOVE) {
            if (t->hover_tab != hit_tab) {
                t->hover_tab = hit_tab;
                ctrl->dirty = 1;
            }
            return 1;
        }

        if (evt->type == GUI_EVENT_MOUSE_DOWN && hit_tab >= 0) {
            if (t->active_tab != hit_tab) {
                t->active_tab = hit_tab;
                ctrl->dirty = 1;
                if (t->on_tab_change) {
                    t->on_tab_change(hit_tab, t->tab_change_data);
                }
            }
            return 1;
        }
        return 0;
    }

    /* Reset hover if mouse leaves tab header area */
    if (evt->type == GUI_EVENT_MOUSE_MOVE && t->hover_tab >= 0) {
        t->hover_tab = -1;
        ctrl->dirty = 1;
    }

    /* Dispatch event to active tab's children */
    if (t->active_tab >= 0 && t->active_tab < t->tab_count) {
        tab_def_t *active = &t->tabs[t->active_tab];
        for (int i = active->child_count - 1; i >= 0; i--) {
            control_t *child = active->children[i];
            if (child && child->visible && child->enabled &&
                child->ops && child->ops->event) {
                if (control_hit_test(child, mx, my)) {
                    if (child->ops->event(child, evt)) {
                        return 1;
                    }
                }
            }
        }
    }

    return 0;
}

static void tabs_destroy_impl(control_t *ctrl) {
    tabs_t *t = (tabs_t *)ctrl;

    /* Destroy all child controls in all tabs */
    for (int tab = 0; tab < t->tab_count; tab++) {
        for (int i = 0; i < t->tabs[tab].child_count; i++) {
            control_t *child = t->tabs[tab].children[i];
            if (child && child->ops && child->ops->destroy) {
                child->ops->destroy(child);
                free(child);
            }
        }
    }
}

static const control_ops_t tabs_ops = {
    .draw    = tabs_draw,
    .event   = tabs_event,
    .destroy = tabs_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

tabs_t *tabs_create(int x, int y, int w, int h) {
    tabs_t *t = (tabs_t *)malloc(sizeof(tabs_t));
    if (!t) return (tabs_t *)0;

    /* Zero-init all fields */
    char *p = (char *)t;
    for (unsigned int i = 0; i < sizeof(tabs_t); i++) p[i] = 0;

    /* Initialize base control */
    CONTROL_INIT(&t->base, CONTROL_TABS, x, y, w, h, &tabs_ops);

    t->tab_count   = 0;
    t->active_tab  = 0;
    t->hover_tab   = -1;

    /* Compute content area */
    t->content_x = x;
    t->content_y = y + TABS_HEADER_H;
    t->content_w = w;
    t->content_h = h - TABS_HEADER_H;

    return t;
}

int tabs_add_tab(tabs_t *tabs, const char *label) {
    if (!tabs || tabs->tab_count >= TABS_MAX_TABS) return -1;

    int idx = tabs->tab_count;
    tab_def_t *tab = &tabs->tabs[idx];

    int i;
    for (i = 0; i < TABS_MAX_LABEL - 1 && label && label[i]; i++) {
        tab->label[i] = label[i];
    }
    tab->label[i] = '\0';
    tab->child_count = 0;

    tabs->tab_count++;
    tabs->base.dirty = 1;
    return idx;
}

int tabs_add_child(tabs_t *tabs, int tab_index, control_t *child) {
    if (!tabs || tab_index < 0 || tab_index >= tabs->tab_count) return -1;
    if (!child) return -1;

    tab_def_t *tab = &tabs->tabs[tab_index];
    if (tab->child_count >= TABS_MAX_CHILDREN) return -1;

    /* Offset child position into tab content area */
    child->x += tabs->content_x + TABS_BORDER;
    child->y += tabs->content_y + TABS_BORDER;

    tab->children[tab->child_count++] = child;
    tabs->base.dirty = 1;
    return 0;
}

void tabs_set_active(tabs_t *tabs, int tab_index) {
    if (!tabs) return;
    if (tab_index < 0 || tab_index >= tabs->tab_count) return;
    if (tabs->active_tab != tab_index) {
        tabs->active_tab = tab_index;
        tabs->base.dirty = 1;
        if (tabs->on_tab_change) {
            tabs->on_tab_change(tab_index, tabs->tab_change_data);
        }
    }
}

int tabs_get_active(tabs_t *tabs) {
    if (!tabs) return 0;
    return tabs->active_tab;
}

void tabs_set_on_tab_change(tabs_t *tabs,
                            void (*callback)(int tab_index, void *userdata),
                            void *userdata) {
    if (!tabs) return;
    tabs->on_tab_change   = callback;
    tabs->tab_change_data = userdata;
}

void tabs_destroy(tabs_t *tabs) {
    if (!tabs) return;
    if (tabs->base.ops->destroy) {
        tabs->base.ops->destroy(&tabs->base);
    }
    free(tabs);
}
