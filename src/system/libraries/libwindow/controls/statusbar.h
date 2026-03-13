/**
 * MaahiOS Window Library - StatusBar Control Header (Design System v2)
 *
 * Description:
 *   Dark horizontal status strip at the bottom of a window.
 *   Displays up to 4 named panels with text values.
 *
 *   Visual layout:
 *     ┌──────────────────────────────────────────────────────┐
 *     │ Panel0: value  │ Panel1: value  │ Panel2: value      │  dark bg
 *     └──────────────────────────────────────────────────────┘
 *
 *   Background: THEME_PRIMARY_DARK (#131A22)
 *   Text: white (THEME_TEXT_INVERSE)
 *   Separator: subtle vertical line
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef STATUSBAR_H
#define STATUSBAR_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * STATUSBAR CONSTANTS
 *===========================================================================*/

#define STATUSBAR_MAX_PANELS  6     /* Maximum panels                       */
#define STATUSBAR_MAX_TEXT    48     /* Max text per panel                   */
#define STATUSBAR_HEIGHT      22    /* Total statusbar height (pixels)      */
#define STATUSBAR_PAD_X        8    /* Horizontal padding per panel         */

/*=============================================================================
 * STATUSBAR PANEL
 *===========================================================================*/

typedef struct {
    char text[STATUSBAR_MAX_TEXT];
    int  width;                     /* 0 = auto-size, >0 = fixed width      */
} statusbar_panel_t;

/*=============================================================================
 * STATUSBAR STRUCT
 *===========================================================================*/

typedef struct {
    control_t          base;        /* MUST be first member                 */
    statusbar_panel_t  panels[STATUSBAR_MAX_PANELS];
    int                panel_count;
} statusbar_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * statusbar_create - Create a new statusbar control
 * @x:  X position relative to window content area
 * @y:  Y position relative to window content area
 * @w:  Total width
 *
 * Returns: Pointer to new statusbar, or NULL on failure.
 */
statusbar_t *statusbar_create(int x, int y, int w);

/**
 * statusbar_add_panel - Add a panel to the statusbar
 * @sb:    StatusBar
 * @text:  Initial text (copied)
 * @width: Panel width (0 = auto-expand to fill remaining space)
 *
 * Returns: Panel index (0-based), or -1 if max panels reached.
 */
int statusbar_add_panel(statusbar_t *sb, const char *text, int width);

/**
 * statusbar_set_text - Update a panel's text
 * @sb:    StatusBar
 * @index: Panel index
 * @text:  New text (copied)
 */
void statusbar_set_text(statusbar_t *sb, int index, const char *text);

/**
 * statusbar_destroy - Free statusbar resources
 * @sb: StatusBar to destroy
 */
void statusbar_destroy(statusbar_t *sb);

#endif /* STATUSBAR_H */
