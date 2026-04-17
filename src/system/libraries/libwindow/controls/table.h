/**
 * MaahiOS Window Library - Table Control Header (Design System v2)
 *
 * Description:
 *   Reusable data table control with columns, rows, header bar,
 *   alternating row colors, and scrolling.  Follows the MaahiOS
 *   Design System v2 embossed chrome theme.
 *
 *   Visual layout:
 *     ┌─── sunken border ─────────────────────────────┐
 *     │  Col 0       │  Col 1       │  Col 2          │  ← chrome header
 *     ├──────────────┼──────────────┼─────────────────┤
 *     │  cell(0,0)   │  cell(0,1)   │  cell(0,2)      │  ← white row
 *     │  cell(1,0)   │  cell(1,1)   │  cell(1,2)      │  ← tinted row
 *     │  cell(2,0)   │  cell(2,1)   │  cell(2,2)      │  ← white row
 *     └──────────────┴──────────────┴─────────────────┘
 *
 *   The header uses chrome bg with bevel edges (raised).
 *   Data rows alternate between THEME_SURFACE and a light tint.
 *   Selected row (if any) uses THEME_ACCENT bg + inverse text.
 *
 * Usage:
 *   table_t *tbl = table_create(10, 10, 400, 200);
 *   table_add_column(tbl, "PID", 60, TABLE_ALIGN_RIGHT);
 *   table_add_column(tbl, "Name", 140, TABLE_ALIGN_LEFT);
 *   table_add_column(tbl, "Status", 100, TABLE_ALIGN_LEFT);
 *   table_set_cell(tbl, 0, 0, "1");
 *   table_set_cell(tbl, 0, 1, "sysman");
 *   table_set_cell(tbl, 0, 2, "Running");
 *   window_add_control(win, &tbl->base);
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef TABLE_H
#define TABLE_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * TABLE CONSTANTS
 *===========================================================================*/

#define TABLE_MAX_COLUMNS    8      /* Maximum columns per table            */
#define TABLE_MAX_ROWS       128    /* Maximum data rows                    */
#define TABLE_MAX_CELL_TEXT  32     /* Max text length per cell             */
#define TABLE_MAX_COL_TITLE  24     /* Max column header text length        */

#define TABLE_HEADER_H       22     /* Header row height (pixels)           */
#define TABLE_ROW_H          20     /* Data row height (pixels)             */
#define TABLE_PAD_X           6     /* Horizontal cell padding              */
#define TABLE_BORDER_W        2     /* Sunken border width                  */
#define TABLE_SCROLLBAR_W    14     /* Scrollbar width (pixels)             */

/*=============================================================================
 * TABLE ALIGNMENT
 *===========================================================================*/

typedef enum {
    TABLE_ALIGN_LEFT = 0,
    TABLE_ALIGN_CENTER,
    TABLE_ALIGN_RIGHT,
} table_align_t;

/*=============================================================================
 * TABLE COLUMN DEFINITION
 *===========================================================================*/

typedef struct {
    char          title[TABLE_MAX_COL_TITLE]; /* Header text               */
    int           width;                       /* Column width in pixels    */
    table_align_t align;                       /* Text alignment            */
} table_column_t;

/*=============================================================================
 * TABLE STRUCT
 *===========================================================================*/

typedef struct {
    control_t      base;                /* MUST be first member             */

    /* Columns */
    table_column_t columns[TABLE_MAX_COLUMNS];
    int            col_count;

    /* Cell data — [row][col] */
    char           cells[TABLE_MAX_ROWS][TABLE_MAX_COLUMNS][TABLE_MAX_CELL_TEXT];
    int            row_count;

    /* Scroll */
    int            scroll_offset;       /* First visible row index          */
    int            visible_rows;        /* Computed from height             */

    /* Selection */
    int            selected_row;        /* -1 = no selection                */

    /* Callbacks */
    void         (*on_row_click)(int row, void *userdata);
    void          *click_data;
} table_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * table_create - Create a new table control
 * @x:      X position relative to window content area
 * @y:      Y position relative to window content area
 * @w:      Total width (including border)
 * @h:      Total height (including header + border)
 *
 * Returns: Pointer to new table, or NULL on failure.
 */
table_t *table_create(int x, int y, int w, int h);

/**
 * table_add_column - Add a column to the table
 * @tbl:    Table
 * @title:  Column header text (copied)
 * @width:  Column width in pixels
 * @align:  Text alignment for this column
 *
 * Returns: Column index (0-based), or -1 if max columns reached.
 */
int table_add_column(table_t *tbl, const char *title, int width,
                     table_align_t align);

/**
 * table_set_cell - Set the text content of a cell
 * @tbl:    Table
 * @row:    Row index (0-based)
 * @col:    Column index (0-based)
 * @text:   Cell text (copied, max TABLE_MAX_CELL_TEXT-1 chars)
 */
void table_set_cell(table_t *tbl, int row, int col, const char *text);

/**
 * table_set_row_count - Set the number of data rows
 * @tbl:    Table
 * @count:  Number of rows (0 to TABLE_MAX_ROWS)
 *
 * Existing cell data beyond new count is untouched but not displayed.
 */
void table_set_row_count(table_t *tbl, int count);

/**
 * table_clear - Clear all rows (set row_count to 0)
 * @tbl: Table
 */
void table_clear(table_t *tbl);

/**
 * table_set_on_row_click - Set callback for row click events
 * @tbl:      Table
 * @callback: Called with row index when a data row is clicked
 * @userdata: Passed to callback
 */
void table_set_on_row_click(table_t *tbl,
                            void (*callback)(int row, void *userdata),
                            void *userdata);

/**
 * table_destroy - Free table resources
 * @tbl: Table to destroy
 */
void table_destroy(table_t *tbl);

#endif /* TABLE_H */
