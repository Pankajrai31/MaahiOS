/**
 * MaahiOS Window Library - Table Control Implementation (Design System v2)
 *
 * Description:
 *   Renders a data table with chrome header, alternating row colors,
 *   sunken content well, and optional row selection highlight.
 *
 *   Header:  chrome bg, raised bevel, dark text (FONT_SMALL)
 *   Rows:    alternating white / light tint, dark text (FONT_SMALL)
 *   Selected row: accent bg, inverse text
 *   Border:  2px sunken bevel (dark top-left, light bottom-right)
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#include "table.h"
#include "../surface.h"
#include "../theme.h"

/* Forward declaration — user-space malloc/free */
extern void *malloc(unsigned int size);
extern void  free(void *ptr);

/*=============================================================================
 * THEME COLORS FOR TABLE
 *===========================================================================*/

#define TBL_HEADER_BG       THEME_CHROME
#define TBL_HEADER_FG       THEME_TEXT
#define TBL_ROW_BG_EVEN     THEME_SURFACE           /* White                */
#define TBL_ROW_BG_ODD      0x00F0F1F6              /* Very light blue-gray */
#define TBL_ROW_FG          THEME_TEXT
#define TBL_SELECT_BG       THEME_ACCENT
#define TBL_SELECT_FG       THEME_TEXT_INVERSE
#define TBL_GRID_COLOR      THEME_CHROME_DARK        /* Column separator     */
#define TBL_BEVEL_LIGHT     THEME_BEVEL_LIGHT
#define TBL_BEVEL_DARK      THEME_BEVEL_DARK
#define TBL_EMPTY_FG        THEME_TEXT_SECONDARY

/*=============================================================================
 * INTERNAL: STRING HELPERS
 *===========================================================================*/

static int tbl_strlen(const char *s) {
    int n = 0;
    while (s[n]) n++;
    return n;
}

static void tbl_strcpy(char *dst, const char *src, int max) {
    int i;
    for (i = 0; i < max - 1 && src && src[i]; i++)
        dst[i] = src[i];
    dst[i] = '\0';
}

/*=============================================================================
 * VTABLE FUNCTIONS
 *===========================================================================*/

static void table_draw(control_t *ctrl, surface_t *surf) {
    table_t *tbl = (table_t *)ctrl;
    int x = ctrl->x;
    int y = ctrl->y;
    int w = ctrl->width;
    int h = ctrl->height;

    /* ---- Sunken border (2px) ---- */
    /* Outer: dark top/left, light bottom/right */
    surface_draw_hline(surf, x, y, w, TBL_BEVEL_DARK);
    surface_draw_hline(surf, x + 1, y + 1, w - 2, TBL_BEVEL_DARK);
    surface_draw_vline(surf, x, y, h, TBL_BEVEL_DARK);
    surface_draw_vline(surf, x + 1, y + 1, h - 2, TBL_BEVEL_DARK);
    surface_draw_hline(surf, x, y + h - 1, w, TBL_BEVEL_LIGHT);
    surface_draw_hline(surf, x + 1, y + h - 2, w - 2, TBL_BEVEL_LIGHT);
    surface_draw_vline(surf, x + w - 1, y, h, TBL_BEVEL_LIGHT);
    surface_draw_vline(surf, x + w - 2, y + 1, h - 2, TBL_BEVEL_LIGHT);

    int bw = TABLE_BORDER_W;       /* 2 */
    int inner_x = x + bw;
    int inner_y = y + bw;
    int inner_w = w - bw * 2;
    int inner_h = h - bw * 2;

    /* ---- Header row ---- */
    int hdr_y = inner_y;
    int hdr_h = TABLE_HEADER_H;

    /* Header background */
    surface_fill_rect(surf, inner_x, hdr_y, inner_w, hdr_h, TBL_HEADER_BG);

    /* Header bottom edge (raised) */
    surface_draw_hline(surf, inner_x, hdr_y, inner_w, TBL_BEVEL_LIGHT);
    surface_draw_hline(surf, inner_x, hdr_y + hdr_h - 1, inner_w, TBL_BEVEL_DARK);

    /* Draw column headers */
    {
        int cx = inner_x;
        int i;
        for (i = 0; i < tbl->col_count; i++) {
            int cw = tbl->columns[i].width;
            if (cx + cw > inner_x + inner_w) cw = inner_x + inner_w - cx;
            if (cw <= 0) break;

            /* Draw header text */
            const char *title = tbl->columns[i].title;
            int tw = surface_measure_text(title, FONT_SMALL);
            int text_x;
            if (tbl->columns[i].align == TABLE_ALIGN_RIGHT)
                text_x = cx + cw - TABLE_PAD_X - tw;
            else if (tbl->columns[i].align == TABLE_ALIGN_CENTER)
                text_x = cx + (cw - tw) / 2;
            else
                text_x = cx + TABLE_PAD_X;

            int text_y = hdr_y + (hdr_h - surface_text_height(FONT_SMALL)) / 2;
            if (text_y < hdr_y + 1) text_y = hdr_y + 1;
            surface_draw_text(surf, text_x, text_y, title,
                              FONT_SMALL, TBL_HEADER_FG);

            /* Column separator line */
            if (i < tbl->col_count - 1) {
                surface_draw_vline(surf, cx + cw - 1, hdr_y + 2,
                                   hdr_h - 4, TBL_GRID_COLOR);
            }
            cx += cw;
        }
    }

    /* ---- Data rows area ---- */
    int data_y = hdr_y + hdr_h;
    int data_h = inner_h - hdr_h;
    if (data_h < 0) data_h = 0;

    /* Compute visible rows */
    tbl->visible_rows = data_h / TABLE_ROW_H;
    if (tbl->visible_rows < 0) tbl->visible_rows = 0;

    /* Clamp scroll offset */
    int max_scroll = tbl->row_count - tbl->visible_rows;
    if (max_scroll < 0) max_scroll = 0;
    if (tbl->scroll_offset > max_scroll) tbl->scroll_offset = max_scroll;
    if (tbl->scroll_offset < 0) tbl->scroll_offset = 0;

    /* Fill data area background (surface white) */
    surface_fill_rect(surf, inner_x, data_y, inner_w, data_h, TBL_ROW_BG_EVEN);

    /* Draw visible rows */
    {
        int r;
        for (r = 0; r < tbl->visible_rows; r++) {
            int data_row = r + tbl->scroll_offset;
            if (data_row >= tbl->row_count) break;

            int ry = data_y + r * TABLE_ROW_H;

            /* Row background */
            uint32_t row_bg, row_fg;
            if (data_row == tbl->selected_row) {
                row_bg = TBL_SELECT_BG;
                row_fg = TBL_SELECT_FG;
            } else if (data_row & 1) {
                row_bg = TBL_ROW_BG_ODD;
                row_fg = TBL_ROW_FG;
            } else {
                row_bg = TBL_ROW_BG_EVEN;
                row_fg = TBL_ROW_FG;
            }
            surface_fill_rect(surf, inner_x, ry, inner_w, TABLE_ROW_H, row_bg);

            /* Draw cells */
            int cx = inner_x;
            int c;
            for (c = 0; c < tbl->col_count; c++) {
                int cw = tbl->columns[c].width;
                if (cx + cw > inner_x + inner_w) cw = inner_x + inner_w - cx;
                if (cw <= 0) break;

                const char *text = tbl->cells[data_row][c];
                if (text[0] != '\0') {
                    int tw = surface_measure_text(text, FONT_SMALL);
                    int text_x;
                    if (tbl->columns[c].align == TABLE_ALIGN_RIGHT)
                        text_x = cx + cw - TABLE_PAD_X - tw;
                    else if (tbl->columns[c].align == TABLE_ALIGN_CENTER)
                        text_x = cx + (cw - tw) / 2;
                    else
                        text_x = cx + TABLE_PAD_X;

                    int text_y = ry + (TABLE_ROW_H - surface_text_height(FONT_SMALL)) / 2;
                    if (text_y < ry) text_y = ry;
                    surface_draw_text(surf, text_x, text_y, text,
                                      FONT_SMALL, row_fg);
                }

                /* Column separator */
                if (c < tbl->col_count - 1) {
                    surface_draw_vline(surf, cx + cw - 1, ry, TABLE_ROW_H,
                                       TBL_GRID_COLOR);
                }
                cx += cw;
            }
        }
    }

    /* ---- Scrollbar (right edge of data area) ---- */
    if (tbl->row_count > tbl->visible_rows && tbl->visible_rows > 0) {
        int sb_x = inner_x + inner_w - TABLE_SCROLLBAR_W;
        int sb_y = data_y;
        int sb_w = TABLE_SCROLLBAR_W;
        int sb_h = data_h;

        /* Scrollbar track background */
        surface_fill_rect(surf, sb_x, sb_y, sb_w, sb_h, THEME_CHROME_LIGHT);

        /* Track bevel: sunken look */
        surface_draw_vline(surf, sb_x, sb_y, sb_h, TBL_BEVEL_DARK);
        surface_draw_vline(surf, sb_x + sb_w - 1, sb_y, sb_h, TBL_BEVEL_LIGHT);

        /* Compute thumb size and position */
        int total_rows = tbl->row_count;
        int thumb_h = sb_h * tbl->visible_rows / total_rows;
        if (thumb_h < 16) thumb_h = 16;
        if (thumb_h > sb_h) thumb_h = sb_h;

        int max_scroll = total_rows - tbl->visible_rows;
        int thumb_y = sb_y;
        if (max_scroll > 0) {
            thumb_y = sb_y + (sb_h - thumb_h) * tbl->scroll_offset / max_scroll;
        }

        /* Thumb: raised chrome rectangle */
        surface_fill_rect(surf, sb_x + 1, thumb_y, sb_w - 2, thumb_h, THEME_CHROME);
        surface_draw_hline(surf, sb_x + 1, thumb_y, sb_w - 2, TBL_BEVEL_LIGHT);
        surface_draw_hline(surf, sb_x + 1, thumb_y + thumb_h - 1, sb_w - 2, TBL_BEVEL_DARK);
        surface_draw_vline(surf, sb_x + 1, thumb_y, thumb_h, TBL_BEVEL_LIGHT);
        surface_draw_vline(surf, sb_x + sb_w - 2, thumb_y, thumb_h, TBL_BEVEL_DARK);

        /* Grip lines in center of thumb (3 horizontal lines) */
        int grip_y = thumb_y + thumb_h / 2 - 3;
        int grip_x = sb_x + 3;
        int grip_w = sb_w - 6;
        if (thumb_h >= 20) {
            surface_draw_hline(surf, grip_x, grip_y,     grip_w, TBL_BEVEL_DARK);
            surface_draw_hline(surf, grip_x, grip_y + 1, grip_w, TBL_BEVEL_LIGHT);
            surface_draw_hline(surf, grip_x, grip_y + 3, grip_w, TBL_BEVEL_DARK);
            surface_draw_hline(surf, grip_x, grip_y + 4, grip_w, TBL_BEVEL_LIGHT);
            surface_draw_hline(surf, grip_x, grip_y + 6, grip_w, TBL_BEVEL_DARK);
            surface_draw_hline(surf, grip_x, grip_y + 7, grip_w, TBL_BEVEL_LIGHT);
        }
    }

    /* If no rows, show "(empty)" placeholder */
    if (tbl->row_count == 0) {
        const char *empty_text = "(empty)";
        int tw = surface_measure_text(empty_text, FONT_SMALL);
        int text_x = inner_x + (inner_w - tw) / 2;
        int text_y = data_y + (data_h - surface_text_height(FONT_SMALL)) / 2;
        if (text_y < data_y) text_y = data_y;
        surface_draw_text(surf, text_x, text_y, empty_text,
                          FONT_SMALL, TBL_EMPTY_FG);
    }
}

static int table_event(control_t *ctrl, gui_event_t *evt) {
    table_t *tbl = (table_t *)ctrl;
    if (!ctrl->enabled) return 0;

    if (evt->type == GUI_EVENT_MOUSE_DOWN && evt->mouse_button == 0) {
        /* Left click — determine which row was clicked */
        int local_y = evt->mouse_y - ctrl->y;
        int bw = TABLE_BORDER_W;
        int data_start = bw + TABLE_HEADER_H;

        if (local_y >= data_start) {
            int row_offset = (local_y - data_start) / TABLE_ROW_H;
            int clicked_row = row_offset + tbl->scroll_offset;
            if (clicked_row >= 0 && clicked_row < tbl->row_count) {
                tbl->selected_row = clicked_row;
                ctrl->dirty = 1;
                if (tbl->on_row_click) {
                    tbl->on_row_click(clicked_row, tbl->click_data);
                }
                return 1;
            }
        }
    }

    /* Scroll via key events */
    if (evt->type == GUI_EVENT_KEY_DOWN) {
        int max_scroll = tbl->row_count - tbl->visible_rows;
        if (max_scroll < 0) max_scroll = 0;

        if (evt->key_code == 0x48) {  /* Up arrow scancode */
            if (tbl->scroll_offset > 0) {
                tbl->scroll_offset--;
                ctrl->dirty = 1;
                return 1;
            }
            /* Move selection up */
            if (tbl->selected_row > 0) {
                tbl->selected_row--;
                /* Scroll to keep selection visible */
                if (tbl->selected_row < tbl->scroll_offset)
                    tbl->scroll_offset = tbl->selected_row;
                ctrl->dirty = 1;
                return 1;
            }
        }
        if (evt->key_code == 0x50) {  /* Down arrow scancode */
            /* Move selection down */
            if (tbl->selected_row < tbl->row_count - 1) {
                tbl->selected_row++;
                /* Scroll to keep selection visible */
                int last_vis = tbl->scroll_offset + tbl->visible_rows - 1;
                if (tbl->selected_row > last_vis) {
                    tbl->scroll_offset = tbl->selected_row - tbl->visible_rows + 1;
                    if (tbl->scroll_offset > max_scroll) tbl->scroll_offset = max_scroll;
                }
                ctrl->dirty = 1;
                return 1;
            }
        }

        if (evt->key_code == 0x49) {  /* Page Up scancode */
            int page = tbl->visible_rows > 1 ? tbl->visible_rows - 1 : 1;
            if (tbl->scroll_offset > 0) {
                tbl->scroll_offset -= page;
                if (tbl->scroll_offset < 0) tbl->scroll_offset = 0;
                /* Move selection with page */
                if (tbl->selected_row >= 0) {
                    tbl->selected_row -= page;
                    if (tbl->selected_row < 0) tbl->selected_row = 0;
                }
                ctrl->dirty = 1;
                return 1;
            }
        }
        if (evt->key_code == 0x51) {  /* Page Down scancode */
            int page = tbl->visible_rows > 1 ? tbl->visible_rows - 1 : 1;
            if (tbl->scroll_offset < max_scroll) {
                tbl->scroll_offset += page;
                if (tbl->scroll_offset > max_scroll) tbl->scroll_offset = max_scroll;
                /* Move selection with page */
                if (tbl->selected_row >= 0) {
                    tbl->selected_row += page;
                    if (tbl->selected_row >= tbl->row_count)
                        tbl->selected_row = tbl->row_count - 1;
                }
                ctrl->dirty = 1;
                return 1;
            }
        }
        if (evt->key_code == 0x47) {  /* Home scancode */
            tbl->scroll_offset = 0;
            tbl->selected_row = 0;
            ctrl->dirty = 1;
            return 1;
        }
        if (evt->key_code == 0x4F) {  /* End scancode */
            tbl->scroll_offset = max_scroll;
            tbl->selected_row = tbl->row_count - 1;
            ctrl->dirty = 1;
            return 1;
        }
    }

    return 0;
}

static void table_destroy_impl(control_t *ctrl) {
    (void)ctrl;
    /* No dynamic resources beyond the struct itself */
}

static const control_ops_t table_ops = {
    .draw    = table_draw,
    .event   = table_event,
    .destroy = table_destroy_impl,
};

/*=============================================================================
 * PUBLIC API
 *===========================================================================*/

table_t *table_create(int x, int y, int w, int h) {
    table_t *tbl = (table_t *)malloc(sizeof(table_t));
    if (!tbl) return (table_t *)0;

    /* Zero-initialize all fields */
    {
        unsigned char *p = (unsigned char *)tbl;
        unsigned int i;
        for (i = 0; i < sizeof(table_t); i++) p[i] = 0;
    }

    /* Initialize base control */
    CONTROL_INIT(&tbl->base, CONTROL_TABLE, x, y, w, h, &table_ops);

    /* Table-specific defaults */
    tbl->col_count      = 0;
    tbl->row_count      = 0;
    tbl->scroll_offset  = 0;
    tbl->visible_rows   = 0;
    tbl->selected_row   = -1;
    tbl->on_row_click   = (void (*)(int, void *))0;
    tbl->click_data     = (void *)0;

    return tbl;
}

int table_add_column(table_t *tbl, const char *title, int width,
                     table_align_t align) {
    if (!tbl || tbl->col_count >= TABLE_MAX_COLUMNS) return -1;

    int idx = tbl->col_count;
    tbl_strcpy(tbl->columns[idx].title, title, TABLE_MAX_COL_TITLE);
    tbl->columns[idx].width = width;
    tbl->columns[idx].align = align;
    tbl->col_count++;
    tbl->base.dirty = 1;
    return idx;
}

void table_set_cell(table_t *tbl, int row, int col, const char *text) {
    if (!tbl) return;
    if (row < 0 || row >= TABLE_MAX_ROWS) return;
    if (col < 0 || col >= tbl->col_count) return;

    tbl_strcpy(tbl->cells[row][col], text, TABLE_MAX_CELL_TEXT);

    /* Auto-expand row count if needed */
    if (row >= tbl->row_count) {
        tbl->row_count = row + 1;
    }
    tbl->base.dirty = 1;
}

void table_set_row_count(table_t *tbl, int count) {
    if (!tbl) return;
    if (count < 0) count = 0;
    if (count > TABLE_MAX_ROWS) count = TABLE_MAX_ROWS;
    tbl->row_count = count;
    /* Reset selection if out of bounds */
    if (tbl->selected_row >= count) {
        tbl->selected_row = -1;
    }
    tbl->base.dirty = 1;
}

void table_clear(table_t *tbl) {
    if (!tbl) return;
    tbl->row_count = 0;
    tbl->scroll_offset = 0;
    tbl->selected_row = -1;
    tbl->base.dirty = 1;
}

void table_set_on_row_click(table_t *tbl,
                            void (*callback)(int row, void *userdata),
                            void *userdata) {
    if (!tbl) return;
    tbl->on_row_click = callback;
    tbl->click_data   = userdata;
}

void table_destroy(table_t *tbl) {
    if (!tbl) return;
    if (tbl->base.ops->destroy) {
        tbl->base.ops->destroy(&tbl->base);
    }
    free(tbl);
}
