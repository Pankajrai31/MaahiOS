/**
 * MaahiOS Window Library - TreeView Control Implementation (Design System v2)
 *
 * Description:
 *   Renders a hierarchical tree with expand/collapse, selection,
 *   and scrolling. Uses sunken border, alternating tints, and accent
 *   selection highlight matching the Design System v2 theme.
 *
 *   The tree is a flat array of nodes with parent/child relationships.
 *   Visible rows are computed by walking the tree in depth-first order,
 *   skipping children of collapsed nodes.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "treeview.h"
#include "../surface.h"
#include "../theme.h"

extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS
 *===========================================================================*/

#define TV_BG           THEME_SURFACE
#define TV_BG_ALT       0x00F0F1F6
#define TV_FG           THEME_TEXT
#define TV_SELECT_BG    THEME_ACCENT
#define TV_SELECT_FG    THEME_TEXT_INVERSE
#define TV_ICON_FG      THEME_TEXT_SECONDARY
#define TV_BEVEL_LIGHT  THEME_BEVEL_LIGHT
#define TV_BEVEL_DARK   THEME_BEVEL_DARK

/*=============================================================================
 * INTERNAL: STRING HELPERS
 *===========================================================================*/

static void tv_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*=============================================================================
 * INTERNAL: BUILD VISIBLE ROWS LIST
 *
 * Walk the tree DFS from root nodes. If a node is collapsed, skip children.
 * Fill a flat array of (node_index, depth) pairs representing visible rows.
 *===========================================================================*/

typedef struct {
    int node_index;
    int depth;
} tv_visible_row_t;

#define TV_MAX_VISIBLE  128   /* Max visible rows we can track */

static int tv_build_visible(treeview_t *tv, tv_visible_row_t *out, int max) {
    int count = 0;

    /* Recursive DFS using explicit stack */
    int stack[TREEVIEW_MAX_NODES * 2];  /* pairs: (node_index, depth) */
    int sp = 0;

    /* Push root nodes in reverse order so first root appears first */
    int i;
    int root_count = 0;
    int roots[TREEVIEW_MAX_NODES];
    for (i = 0; i < tv->node_count; i++) {
        if (tv->nodes[i].parent == -1) {
            roots[root_count++] = i;
        }
    }
    for (i = root_count - 1; i >= 0; i--) {
        stack[sp++] = roots[i];
        stack[sp++] = 0;  /* depth */
    }

    while (sp > 0 && count < max) {
        int depth = stack[--sp];
        int idx   = stack[--sp];

        out[count].node_index = idx;
        out[count].depth      = depth;
        count++;

        /* If expanded, push children in reverse order */
        if (tv->nodes[idx].expanded && tv->nodes[idx].child_count > 0) {
            int c;
            for (c = tv->nodes[idx].child_count - 1; c >= 0; c--) {
                int child = tv->nodes[idx].children[c];
                stack[sp++] = child;
                stack[sp++] = depth + 1;
            }
        }
    }

    return count;
}

/*=============================================================================
 * VTABLE: DRAW
 *===========================================================================*/

static void treeview_draw(control_t *ctrl, surface_t *surf) {
    treeview_t *tv = (treeview_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = ctrl->height;

    /* ---- Sunken border (2px) ---- */
    surface_draw_hline(surf, x, y, w, TV_BEVEL_DARK);
    surface_draw_hline(surf, x + 1, y + 1, w - 2, TV_BEVEL_DARK);
    surface_draw_vline(surf, x, y, h, TV_BEVEL_DARK);
    surface_draw_vline(surf, x + 1, y + 1, h - 2, TV_BEVEL_DARK);
    surface_draw_hline(surf, x, y + h - 1, w, TV_BEVEL_LIGHT);
    surface_draw_hline(surf, x + 1, y + h - 2, w - 2, TV_BEVEL_LIGHT);
    surface_draw_vline(surf, x + w - 1, y, h, TV_BEVEL_LIGHT);
    surface_draw_vline(surf, x + w - 2, y + 1, h - 2, TV_BEVEL_LIGHT);

    int bw = TREEVIEW_BORDER_W;
    int inner_x = x + bw;
    int inner_y = y + bw;
    int inner_w = w - bw * 2;
    int inner_h = h - bw * 2;

    /* Fill background */
    surface_fill_rect(surf, inner_x, inner_y, inner_w, inner_h, TV_BG);

    /* Build visible rows */
    tv_visible_row_t vis[TV_MAX_VISIBLE];
    int total_vis = tv_build_visible(tv, vis, TV_MAX_VISIBLE);

    /* Compute visible rows in clip area */
    tv->visible_rows = inner_h / TREEVIEW_ROW_H;
    if (tv->visible_rows < 0) tv->visible_rows = 0;

    /* Clamp scroll */
    int max_scroll = total_vis - tv->visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (tv->scroll_offset > max_scroll) tv->scroll_offset = max_scroll;
    if (tv->scroll_offset < 0) tv->scroll_offset = 0;

    /* Draw visible rows */
    int r;
    for (r = 0; r < tv->visible_rows; r++) {
        int vis_idx = r + tv->scroll_offset;
        if (vis_idx >= total_vis) break;

        int node_idx = vis[vis_idx].node_index;
        int depth    = vis[vis_idx].depth;
        treeview_node_t *node = &tv->nodes[node_idx];

        int ry = inner_y + r * TREEVIEW_ROW_H;

        /* Row background */
        uint32_t row_bg, row_fg;
        if (node_idx == tv->selected) {
            row_bg = TV_SELECT_BG;
            row_fg = TV_SELECT_FG;
        } else if (r & 1) {
            row_bg = TV_BG_ALT;
            row_fg = TV_FG;
        } else {
            row_bg = TV_BG;
            row_fg = TV_FG;
        }
        surface_fill_rect(surf, inner_x, ry, inner_w, TREEVIEW_ROW_H, row_bg);

        /* Indent */
        int indent = TREEVIEW_PAD_X + depth * TREEVIEW_INDENT;
        int icon_x = inner_x + indent;
        int text_x = icon_x + TREEVIEW_ICON_W;

        /* Expand/collapse icon */
        if (!node->is_leaf) {
            /* Draw triangle: ▸ (collapsed) or ▾ (expanded) */
            int cy_mid = ry + TREEVIEW_ROW_H / 2;
            uint32_t ic = (node_idx == tv->selected) ? TV_SELECT_FG : TV_ICON_FG;

            if (node->expanded) {
                /* ▾ = down-pointing triangle */
                int tx = icon_x + 2;
                int ty = cy_mid - 2;
                int row_i;
                for (row_i = 0; row_i < 5; row_i++) {
                    int half = (5 - row_i) / 2;
                    if (half < 1) half = 1;
                    surface_draw_hline(surf, tx + row_i / 2, ty + row_i,
                                       5 - row_i, ic);
                }
            } else {
                /* ▸ = right-pointing triangle */
                int tx = icon_x + 3;
                int ty = cy_mid - 3;
                int row_i;
                for (row_i = 0; row_i < 6; row_i++) {
                    int half = (row_i < 3) ? row_i : (5 - row_i);
                    if (half < 0) half = 0;
                    surface_draw_vline(surf, tx + half, ty + row_i, 1, ic);
                    if (half > 0)
                        surface_draw_vline(surf, tx, ty + row_i, half + 1, ic);
                }
            }
        }

        /* Label text */
        int th = surface_text_height(FONT_SMALL);
        int ty = ry + (TREEVIEW_ROW_H - th) / 2;
        if (text_x + 4 < inner_x + inner_w) {
            surface_draw_text(surf, text_x, ty, node->label,
                              FONT_SMALL, row_fg);
        }
    }
}

/*=============================================================================
 * VTABLE: EVENT
 *===========================================================================*/

static int treeview_event(control_t *ctrl, gui_event_t *evt) {
    treeview_t *tv = (treeview_t *)ctrl;
    if (!ctrl->enabled) return 0;

    if (evt->type == GUI_EVENT_MOUSE_DOWN && evt->mouse_button == 0) {
        int bw = TREEVIEW_BORDER_W;
        int ly = evt->mouse_y - ctrl->y - bw;
        int lx = evt->mouse_x - ctrl->x - bw;

        if (ly < 0 || lx < 0) return 0;

        int clicked_row = ly / TREEVIEW_ROW_H;

        /* Build visible rows to map row → node */
        tv_visible_row_t vis[TV_MAX_VISIBLE];
        int total_vis = tv_build_visible(tv, vis, TV_MAX_VISIBLE);

        int vis_idx = clicked_row + tv->scroll_offset;
        if (vis_idx < 0 || vis_idx >= total_vis) return 0;

        int node_idx = vis[vis_idx].node_index;
        int depth    = vis[vis_idx].depth;
        treeview_node_t *node = &tv->nodes[node_idx];

        /* Check if click is on the expand icon area */
        int indent = TREEVIEW_PAD_X + depth * TREEVIEW_INDENT;
        if (lx >= indent && lx < indent + TREEVIEW_ICON_W && !node->is_leaf) {
            /* Toggle expand */
            node->expanded = !node->expanded;
            ctrl->dirty = 1;
            if (node->expanded && tv->on_expand) {
                tv->on_expand(node_idx, tv->expand_data);
            }
            return 1;
        }

        /* Click on label → select */
        tv->selected = node_idx;
        ctrl->dirty = 1;

        /* If it's a non-leaf and not expanded, expand it */
        if (!node->is_leaf && !node->expanded) {
            node->expanded = 1;
            if (tv->on_expand) {
                tv->on_expand(node_idx, tv->expand_data);
            }
        }

        if (tv->on_select) {
            tv->on_select(node_idx, tv->select_data);
        }
        return 1;
    }

    /* Scroll via key events */
    if (evt->type == GUI_EVENT_KEY_DOWN) {
        int max_scroll;
        tv_visible_row_t vis[TV_MAX_VISIBLE];
        int total_vis = tv_build_visible(tv, vis, TV_MAX_VISIBLE);
        max_scroll = total_vis - tv->visible_rows;
        if (max_scroll < 0) max_scroll = 0;

        if (evt->key_code == 0x48) {  /* Up */
            if (tv->scroll_offset > 0) {
                tv->scroll_offset--;
                ctrl->dirty = 1;
                return 1;
            }
        }
        if (evt->key_code == 0x50) {  /* Down */
            if (tv->scroll_offset < max_scroll) {
                tv->scroll_offset++;
                ctrl->dirty = 1;
                return 1;
            }
        }
    }

    return 0;
}

static void treeview_destroy_impl(control_t *ctrl) {
    (void)ctrl;
}

static const control_ops_t treeview_ops = {
    .draw    = treeview_draw,
    .event   = treeview_event,
    .destroy = treeview_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

treeview_t *treeview_create(int x, int y, int w, int h) {
    treeview_t *tv = (treeview_t *)malloc(sizeof(treeview_t));
    if (!tv) return (treeview_t *)0;

    /* Zero-init */
    {
        unsigned char *p = (unsigned char *)tv;
        unsigned int i;
        for (i = 0; i < sizeof(treeview_t); i++) p[i] = 0;
    }

    CONTROL_INIT(&tv->base, CONTROL_TREEVIEW, x, y, w, h, &treeview_ops);

    tv->node_count     = 0;
    tv->selected       = -1;
    tv->scroll_offset  = 0;
    tv->visible_rows   = 0;
    tv->on_select      = (void (*)(int, void *))0;
    tv->select_data    = (void *)0;
    tv->on_expand      = (void (*)(int, void *))0;
    tv->expand_data    = (void *)0;

    return tv;
}

int treeview_add_node(treeview_t *tv, const char *label,
                      int parent, int is_leaf) {
    if (!tv || tv->node_count >= TREEVIEW_MAX_NODES) return -1;

    int idx = tv->node_count;
    tv_strcpy(tv->nodes[idx].label, label, TREEVIEW_MAX_LABEL);
    tv->nodes[idx].parent      = parent;
    tv->nodes[idx].child_count = 0;
    tv->nodes[idx].expanded    = 0;
    tv->nodes[idx].is_leaf     = is_leaf;
    tv->nodes[idx].userdata    = (void *)0;

    /* Register as child of parent */
    if (parent >= 0 && parent < tv->node_count) {
        treeview_node_t *p = &tv->nodes[parent];
        if (p->child_count < TREEVIEW_MAX_CHILDREN) {
            p->children[p->child_count++] = idx;
        }
    }

    tv->node_count++;
    tv->base.dirty = 1;
    return idx;
}

void treeview_clear(treeview_t *tv) {
    if (!tv) return;
    tv->node_count    = 0;
    tv->selected      = -1;
    tv->scroll_offset = 0;
    tv->base.dirty    = 1;
}

void treeview_set_expanded(treeview_t *tv, int node, int expanded) {
    if (!tv || node < 0 || node >= tv->node_count) return;
    tv->nodes[node].expanded = expanded;
    tv->base.dirty = 1;
}

void treeview_set_selected(treeview_t *tv, int node) {
    if (!tv) return;
    tv->selected = node;
    tv->base.dirty = 1;
}

void treeview_set_on_select(treeview_t *tv,
                            void (*callback)(int node_index, void *userdata),
                            void *userdata) {
    if (!tv) return;
    tv->on_select  = callback;
    tv->select_data = userdata;
}

void treeview_set_on_expand(treeview_t *tv,
                            void (*callback)(int node_index, void *userdata),
                            void *userdata) {
    if (!tv) return;
    tv->on_expand  = callback;
    tv->expand_data = userdata;
}

void *treeview_get_node_userdata(treeview_t *tv, int node) {
    if (!tv || node < 0 || node >= tv->node_count) return (void *)0;
    return tv->nodes[node].userdata;
}

void treeview_set_node_userdata(treeview_t *tv, int node, void *userdata) {
    if (!tv || node < 0 || node >= tv->node_count) return;
    tv->nodes[node].userdata = userdata;
}

void treeview_destroy(treeview_t *tv) {
    if (!tv) return;
    if (tv->base.ops->destroy) {
        tv->base.ops->destroy(&tv->base);
    }
    free(tv);
}
