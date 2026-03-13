/**
 * MaahiOS Window Library - MenuBar Control Implementation (Design System v2)
 *
 * Description:
 *   Renders a chrome menu bar with clickable dropdown menus.
 *   When a dropdown opens, the control's height expands to cover the
 *   dropdown area, ensuring it receives mouse events (back-to-front
 *   dispatch in libwindow gives last-added controls priority).
 *
 * Drawing model:
 *   - Bar: chrome bg with subtle bottom bevel edge
 *   - Menu labels: dark text, highlighted on hover/open
 *   - Dropdown: raised bevel rectangle with item list
 *   - Items: hover highlight, separator lines, disabled graying
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "menubar.h"
#include "../surface.h"
#include "../theme.h"

extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS
 *===========================================================================*/

#define MB_BAR_BG       THEME_CHROME
#define MB_BAR_FG       THEME_TEXT
#define MB_HOVER_BG     THEME_ACCENT
#define MB_HOVER_FG     THEME_TEXT_INVERSE
#define MB_DROP_BG      THEME_CHROME
#define MB_DROP_FG      THEME_TEXT
#define MB_DROP_HOVER   THEME_ACCENT
#define MB_DROP_HOVER_FG THEME_TEXT_INVERSE
#define MB_DISABLED_FG  THEME_TEXT_DISABLED
#define MB_SEP_DARK     THEME_BEVEL_DARK
#define MB_SEP_LIGHT    THEME_BEVEL_LIGHT
#define MB_BEVEL_LIGHT  THEME_BEVEL_LIGHT
#define MB_BEVEL_DARK   THEME_BEVEL_DARK

/*=============================================================================
 * INTERNAL HELPERS
 *===========================================================================*/

static void mb_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

static void mb_recalc_positions(menubar_t *mb) {
    int cx = 0;
    int i;
    for (i = 0; i < mb->menu_count; i++) {
        int tw = surface_measure_text(mb->menus[i].label, FONT_SMALL);
        mb->menus[i].x = cx;
        mb->menus[i].width = tw + MENUBAR_ITEM_PAD_X * 2;
        cx += mb->menus[i].width;
    }
}

static int mb_dropdown_height(menubar_t *mb, int menu_idx) {
    if (menu_idx < 0 || menu_idx >= mb->menu_count) return 0;
    menubar_menu_t *m = &mb->menus[menu_idx];
    int h = MENUBAR_DROP_PAD * 2;
    int i;
    for (i = 0; i < m->item_count; i++) {
        if (m->items[i].separator_before) h += 1; /* separator line */
        h += MENUBAR_DROP_ITEM_H;
    }
    return h;
}

/*=============================================================================
 * VTABLE: DRAW
 *===========================================================================*/

static void menubar_draw(control_t *ctrl, surface_t *surf) {
    menubar_t *mb = (menubar_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;

    /* ---- Bar background ---- */
    surface_fill_rect(surf, x, y, w, MENUBAR_HEIGHT, MB_BAR_BG);
    /* Bottom edge */
    surface_draw_hline(surf, x, y + MENUBAR_HEIGHT - 1, w, MB_BEVEL_DARK);

    /* ---- Menu labels ---- */
    int i;
    for (i = 0; i < mb->menu_count; i++) {
        int mx = x + mb->menus[i].x;
        int mw = mb->menus[i].width;
        uint32_t bg = MB_BAR_BG;
        uint32_t fg = MB_BAR_FG;

        if (i == mb->open_menu || i == mb->hover_menu) {
            bg = MB_HOVER_BG;
            fg = MB_HOVER_FG;
        }

        surface_fill_rect(surf, mx, y, mw, MENUBAR_HEIGHT - 1, bg);

        int tw = surface_measure_text(mb->menus[i].label, FONT_SMALL);
        int th = surface_text_height(FONT_SMALL);
        int tx = mx + (mw - tw) / 2;
        int ty = y + (MENUBAR_HEIGHT - 1 - th) / 2;
        surface_draw_text(surf, tx, ty, mb->menus[i].label, FONT_SMALL, fg);
    }

    /* ---- Dropdown (if open) ---- */
    if (mb->open_menu >= 0 && mb->open_menu < mb->menu_count) {
        menubar_menu_t *m = &mb->menus[mb->open_menu];
        int dx = x + m->x;
        int dy = y + MENUBAR_HEIGHT;
        int dw = MENUBAR_DROP_W;
        int dh = mb_dropdown_height(mb, mb->open_menu);

        /* Dropdown background + raised bevel */
        surface_fill_rect(surf, dx, dy, dw, dh, MB_DROP_BG);
        /* Raised bevel */
        surface_draw_hline(surf, dx, dy, dw, MB_BEVEL_LIGHT);
        surface_draw_vline(surf, dx, dy, dh, MB_BEVEL_LIGHT);
        surface_draw_hline(surf, dx, dy + dh - 1, dw, MB_BEVEL_DARK);
        surface_draw_vline(surf, dx + dw - 1, dy, dh, MB_BEVEL_DARK);

        /* Items */
        int iy = dy + MENUBAR_DROP_PAD;
        for (i = 0; i < m->item_count; i++) {
            menubar_item_t *item = &m->items[i];

            /* Separator line */
            if (item->separator_before) {
                surface_draw_hline(surf, dx + 4, iy, dw - 8, MB_SEP_DARK);
                iy += 1;
            }

            /* Item background */
            uint32_t ibg = MB_DROP_BG;
            uint32_t ifg = item->enabled ? MB_DROP_FG : MB_DISABLED_FG;

            if (i == mb->hover_item && item->enabled) {
                ibg = MB_DROP_HOVER;
                ifg = MB_DROP_HOVER_FG;
            }

            surface_fill_rect(surf, dx + MENUBAR_DROP_PAD, iy,
                              dw - MENUBAR_DROP_PAD * 2,
                              MENUBAR_DROP_ITEM_H, ibg);

            /* Item text */
            int th = surface_text_height(FONT_SMALL);
            int ty2 = iy + (MENUBAR_DROP_ITEM_H - th) / 2;
            surface_draw_text(surf, dx + MENUBAR_DROP_PAD + 8, ty2,
                              item->label, FONT_SMALL, ifg);

            iy += MENUBAR_DROP_ITEM_H;
        }
    }
}

/*=============================================================================
 * VTABLE: EVENT
 *===========================================================================*/

static int menubar_event(control_t *ctrl, gui_event_t *evt) {
    menubar_t *mb = (menubar_t *)ctrl;
    if (!ctrl->enabled) return 0;

    int lx = evt->mouse_x - ctrl->x;
    int ly = evt->mouse_y - ctrl->y;

    /* ---- Mouse move → hover tracking ---- */
    if (evt->type == GUI_EVENT_MOUSE_MOVE ||
        evt->type == GUI_EVENT_MOUSE_ENTER) {

        int old_hover_menu = mb->hover_menu;
        int old_hover_item = mb->hover_item;
        mb->hover_menu = -1;
        mb->hover_item = -1;

        /* Check if hovering over bar labels */
        if (ly >= 0 && ly < MENUBAR_HEIGHT) {
            int i;
            for (i = 0; i < mb->menu_count; i++) {
                if (lx >= mb->menus[i].x &&
                    lx < mb->menus[i].x + mb->menus[i].width) {
                    mb->hover_menu = i;
                    /* If a dropdown is open and user hovers another label,
                     * switch the open dropdown */
                    if (mb->open_menu >= 0 && mb->open_menu != i) {
                        mb->open_menu = i;
                        mb->hover_item = -1;
                        /* Recalc height for new dropdown */
                        int dh = mb_dropdown_height(mb, mb->open_menu);
                        ctrl->height = MENUBAR_HEIGHT + dh;
                        ctrl->dirty = 1;
                    }
                    break;
                }
            }
        }

        /* Check if hovering over dropdown items */
        if (mb->open_menu >= 0 && ly >= MENUBAR_HEIGHT) {
            menubar_menu_t *m = &mb->menus[mb->open_menu];
            int dx = m->x;
            int dw = MENUBAR_DROP_W;
            int dy_start = MENUBAR_HEIGHT + MENUBAR_DROP_PAD;

            if (lx >= dx && lx < dx + dw) {
                int iy = dy_start;
                int i;
                for (i = 0; i < m->item_count; i++) {
                    if (m->items[i].separator_before) iy += 1;
                    if (ly >= iy && ly < iy + MENUBAR_DROP_ITEM_H) {
                        mb->hover_item = i;
                        break;
                    }
                    iy += MENUBAR_DROP_ITEM_H;
                }
            }
        }

        if (mb->hover_menu != old_hover_menu ||
            mb->hover_item != old_hover_item) {
            ctrl->dirty = 1;
        }
        return 1;
    }

    /* ---- Mouse leave ---- */
    if (evt->type == GUI_EVENT_MOUSE_LEAVE) {
        mb->hover_menu = -1;
        mb->hover_item = -1;
        ctrl->dirty = 1;
        return 0;
    }

    /* ---- Mouse down → open/close menu or click item ---- */
    if (evt->type == GUI_EVENT_MOUSE_DOWN && evt->mouse_button == 0) {
        /* Defensive: compute hover_item on click (in case MOUSE_MOVE
         * did not fire — ensures dropdown items are clickable) */
        if (mb->open_menu >= 0 && ly >= MENUBAR_HEIGHT) {
            menubar_menu_t *hm = &mb->menus[mb->open_menu];
            int hdx = hm->x;
            int hdw = MENUBAR_DROP_W;
            int hdy_start = MENUBAR_HEIGHT + MENUBAR_DROP_PAD;
            mb->hover_item = -1;
            if (lx >= hdx && lx < hdx + hdw) {
                int hiy = hdy_start;
                int hi;
                for (hi = 0; hi < hm->item_count; hi++) {
                    if (hm->items[hi].separator_before) hiy += 1;
                    if (ly >= hiy && ly < hiy + MENUBAR_DROP_ITEM_H) {
                        mb->hover_item = hi;
                        break;
                    }
                    hiy += MENUBAR_DROP_ITEM_H;
                }
            }
        }

        /* Click on bar label */
        if (ly >= 0 && ly < MENUBAR_HEIGHT) {
            int clicked_menu = -1;
            int i;
            for (i = 0; i < mb->menu_count; i++) {
                if (lx >= mb->menus[i].x &&
                    lx < mb->menus[i].x + mb->menus[i].width) {
                    clicked_menu = i;
                    break;
                }
            }
            if (clicked_menu >= 0) {
                if (mb->open_menu == clicked_menu) {
                    /* Toggle close */
                    mb->open_menu = -1;
                    mb->hover_item = -1;
                    ctrl->height = mb->saved_height;
                } else {
                    /* Open this menu */
                    mb->open_menu = clicked_menu;
                    mb->hover_item = -1;
                    int dh = mb_dropdown_height(mb, mb->open_menu);
                    ctrl->height = MENUBAR_HEIGHT + dh;
                }
                ctrl->dirty = 1;
                return 1;
            }
        }

        /* Click on dropdown item */
        if (mb->open_menu >= 0 && ly >= MENUBAR_HEIGHT) {
            if (mb->hover_item >= 0) {
                menubar_menu_t *m = &mb->menus[mb->open_menu];
                menubar_item_t *item = &m->items[mb->hover_item];
                if (item->enabled && item->on_click) {
                    /* Close menu first */
                    int menu_idx = mb->open_menu;
                    int item_idx = mb->hover_item;
                    mb->open_menu = -1;
                    mb->hover_item = -1;
                    ctrl->height = mb->saved_height;
                    ctrl->dirty = 1;
                    /* Call handler */
                    (void)menu_idx;
                    (void)item_idx;
                    item->on_click(item->click_data);
                    return 1;
                }
            }
            /* Click on dropdown but not on an item → close */
            mb->open_menu = -1;
            mb->hover_item = -1;
            ctrl->height = mb->saved_height;
            ctrl->dirty = 1;
            return 1;
        }

        /* Click outside everything → close dropdown */
        if (mb->open_menu >= 0) {
            mb->open_menu = -1;
            mb->hover_item = -1;
            ctrl->height = mb->saved_height;
            ctrl->dirty = 1;
            return 1;
        }
    }

    /* ---- Mouse up → consumed if dropdown open ---- */
    if (evt->type == GUI_EVENT_MOUSE_UP && mb->open_menu >= 0) {
        return 1;
    }

    return 0;
}

static void menubar_destroy_impl(control_t *ctrl) {
    (void)ctrl;
}

static const control_ops_t menubar_ops = {
    .draw    = menubar_draw,
    .event   = menubar_event,
    .destroy = menubar_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

menubar_t *menubar_create(int x, int y, int w) {
    menubar_t *mb = (menubar_t *)malloc(sizeof(menubar_t));
    if (!mb) return (menubar_t *)0;

    /* Zero-init */
    {
        unsigned char *p = (unsigned char *)mb;
        unsigned int i;
        for (i = 0; i < sizeof(menubar_t); i++) p[i] = 0;
    }

    CONTROL_INIT(&mb->base, CONTROL_MENUBAR, x, y, w, MENUBAR_HEIGHT,
                 &menubar_ops);

    mb->menu_count   = 0;
    mb->open_menu    = -1;
    mb->hover_menu   = -1;
    mb->hover_item   = -1;
    mb->bar_width    = w;
    mb->saved_height = MENUBAR_HEIGHT;

    return mb;
}

int menubar_add_menu(menubar_t *mb, const char *label) {
    if (!mb || mb->menu_count >= MENUBAR_MAX_MENUS) return -1;

    int idx = mb->menu_count;
    mb_strcpy(mb->menus[idx].label, label, MENUBAR_MAX_LABEL);
    mb->menus[idx].item_count = 0;
    mb->menu_count++;
    mb_recalc_positions(mb);
    mb->base.dirty = 1;
    return idx;
}

int menubar_add_item(menubar_t *mb, int menu_index, const char *label,
                     void (*callback)(void *userdata), void *userdata) {
    if (!mb) return -1;
    if (menu_index < 0 || menu_index >= mb->menu_count) return -1;

    menubar_menu_t *m = &mb->menus[menu_index];
    if (m->item_count >= MENUBAR_MAX_ITEMS) return -1;

    int idx = m->item_count;
    mb_strcpy(m->items[idx].label, label, MENUBAR_MAX_LABEL);
    m->items[idx].enabled = 1;
    m->items[idx].separator_before = 0;
    m->items[idx].on_click = callback;
    m->items[idx].click_data = userdata;
    m->item_count++;
    return idx;
}

void menubar_add_separator(menubar_t *mb, int menu_index) {
    if (!mb) return;
    if (menu_index < 0 || menu_index >= mb->menu_count) return;

    menubar_menu_t *m = &mb->menus[menu_index];
    /* Mark the NEXT item to have a separator_before.
     * Set a temporary flag on the last added item index. */
    if (m->item_count > 0 && m->item_count < MENUBAR_MAX_ITEMS) {
        /* We'll store the flag: the next item added will get separator_before=1.
         * Use a simple approach: set separator on the last item's index + 1
         * by pre-setting the next slot. */
        m->items[m->item_count].separator_before = 1;
    }
}

void menubar_set_item_enabled(menubar_t *mb, int menu_index,
                              int item_index, int enabled) {
    if (!mb) return;
    if (menu_index < 0 || menu_index >= mb->menu_count) return;
    menubar_menu_t *m = &mb->menus[menu_index];
    if (item_index < 0 || item_index >= m->item_count) return;
    m->items[item_index].enabled = enabled;
    mb->base.dirty = 1;
}

void menubar_close(menubar_t *mb) {
    if (!mb) return;
    if (mb->open_menu >= 0) {
        mb->open_menu = -1;
        mb->hover_item = -1;
        mb->base.height = mb->saved_height;
        mb->base.dirty = 1;
    }
}

void menubar_destroy(menubar_t *mb) {
    if (!mb) return;
    if (mb->base.ops->destroy) {
        mb->base.ops->destroy(&mb->base);
    }
    free(mb);
}
