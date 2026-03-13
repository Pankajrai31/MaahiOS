/**
 * MaahiOS Window Library - Radio Group Control Header (Design System v2)
 *
 * Description:
 *   A horizontal row of radio buttons with labels.  Exactly one
 *   option is selected at a time.  Clicking an unselected option
 *   fires the on_change callback.
 *
 *   Visual layout (horizontal):
 *     (●) All   ( ) Debug   ( ) Info   ( ) Warning   ( ) Error
 *
 *   Each radio is a 12px circle with 4px inner dot when selected.
 *   Uses Design System v2 theme colors:
 *     - Ring:     THEME_BEVEL_DARK (unselected), THEME_ACCENT (selected)
 *     - Dot:      THEME_ACCENT (selected)
 *     - Label:    THEME_TEXT
 *     - Bg:       transparent (inherits parent)
 *
 * Usage:
 *   radiogroup_t *rg = radiogroup_create(0, 30, 500, 24);
 *   radiogroup_add_option(rg, "All");
 *   radiogroup_add_option(rg, "Debug");
 *   radiogroup_add_option(rg, "Info");
 *   radiogroup_set_selected(rg, 0);
 *   radiogroup_set_on_change(rg, my_callback, NULL);
 *   window_add_control(win, &rg->base);
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef RADIOGROUP_H
#define RADIOGROUP_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * RADIO GROUP CONSTANTS
 *===========================================================================*/

#define RADIOGROUP_MAX_OPTIONS   8    /* Maximum radio options per group     */
#define RADIOGROUP_MAX_LABEL    24    /* Max label text length               */
#define RADIOGROUP_CIRCLE_R      6    /* Outer circle radius (pixels)        */
#define RADIOGROUP_DOT_R         3    /* Inner dot radius when selected      */
#define RADIOGROUP_ITEM_PAD     16    /* Padding between items (pixels)      */
#define RADIOGROUP_TEXT_PAD      4    /* Gap between circle and label text   */

/*=============================================================================
 * RADIO GROUP STRUCT
 *===========================================================================*/

typedef struct {
    control_t   base;                    /* MUST be first member             */

    /* Options */
    char        labels[RADIOGROUP_MAX_OPTIONS][RADIOGROUP_MAX_LABEL];
    int         option_count;

    /* State */
    int         selected;                /* Currently selected index (0-based) */
    int         hover;                   /* Hovered option index (-1 = none) */

    /* Cached item positions (x offset of each option start) */
    int         item_x[RADIOGROUP_MAX_OPTIONS];
    int         item_w[RADIOGROUP_MAX_OPTIONS];

    /* Callback */
    void      (*on_change)(int selected, void *userdata);
    void       *change_data;
} radiogroup_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * radiogroup_create - Create a new radio group control
 * @x:  X position relative to window content area
 * @y:  Y position relative to window content area
 * @w:  Total width
 * @h:  Total height (typically 24px)
 *
 * Returns: Pointer to new radio group, or NULL on failure.
 */
radiogroup_t *radiogroup_create(int x, int y, int w, int h);

/**
 * radiogroup_add_option - Add a radio option with label text
 * @rg:    Radio group
 * @label: Label text (copied, max RADIOGROUP_MAX_LABEL-1 chars)
 *
 * Returns: Option index (0-based), or -1 on failure.
 */
int radiogroup_add_option(radiogroup_t *rg, const char *label);

/**
 * radiogroup_set_selected - Set the selected option
 * @rg:    Radio group
 * @index: Option index to select (0-based)
 */
void radiogroup_set_selected(radiogroup_t *rg, int index);

/**
 * radiogroup_get_selected - Get the currently selected option index
 * @rg: Radio group
 *
 * Returns: Selected index (0-based), or -1 if none.
 */
int radiogroup_get_selected(radiogroup_t *rg);

/**
 * radiogroup_set_on_change - Set callback for selection changes
 * @rg:       Radio group
 * @callback: Called with new selected index when selection changes
 * @userdata: Passed to callback
 */
void radiogroup_set_on_change(radiogroup_t *rg,
                              void (*callback)(int selected, void *userdata),
                              void *userdata);

/**
 * radiogroup_destroy - Free radio group resources
 * @rg: Radio group to destroy
 */
void radiogroup_destroy(radiogroup_t *rg);

#endif /* RADIOGROUP_H */
