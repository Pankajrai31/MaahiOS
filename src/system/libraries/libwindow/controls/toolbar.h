/**
 * MaahiOS Window Library - Toolbar Control Header (Design System v2)
 *
 * Description:
 *   Horizontal toolbar strip with flat-style buttons matching the
 *   MaahiOS Design System v2 embossed chrome theme.
 *
 *   Visual layout:
 *     ┌──────────────────────────────────────────────────────┐
 *     │ [Btn1] [Btn2] | [Btn3] [Btn4]         │ raised bar  │
 *     └──────────────────────────────────────────────────────┘
 *
 *   Each item is a text button with hover highlight.
 *   Separators (|) can be inserted between groups.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef TOOLBAR_H
#define TOOLBAR_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * TOOLBAR CONSTANTS
 *===========================================================================*/

#define TOOLBAR_MAX_ITEMS    12     /* Maximum toolbar items                */
#define TOOLBAR_MAX_LABEL    16     /* Max label text per item              */
#define TOOLBAR_HEIGHT       28     /* Total toolbar height (pixels)        */
#define TOOLBAR_BTN_PAD_X     8     /* Horizontal padding per button        */
#define TOOLBAR_BTN_GAP       2     /* Gap between buttons                  */
#define TOOLBAR_SEP_WIDTH     8     /* Separator width (with gaps)          */

/*=============================================================================
 * TOOLBAR ITEM TYPES
 *===========================================================================*/

typedef enum {
    TOOLBAR_ITEM_BUTTON = 0,     /* Clickable button                      */
    TOOLBAR_ITEM_SEPARATOR,      /* Vertical separator line               */
} toolbar_item_type_t;

/*=============================================================================
 * TOOLBAR ITEM
 *===========================================================================*/

typedef struct {
    toolbar_item_type_t type;
    char label[TOOLBAR_MAX_LABEL];
    int  x;                       /* Computed X offset within toolbar      */
    int  width;                   /* Computed width                        */
    int  enabled;                 /* 1 = active, 0 = grayed out           */
    void (*on_click)(void *userdata);
    void *click_data;
} toolbar_item_t;

/*=============================================================================
 * TOOLBAR STRUCT
 *===========================================================================*/

typedef struct {
    control_t       base;         /* MUST be first member                  */
    toolbar_item_t  items[TOOLBAR_MAX_ITEMS];
    int             item_count;
    int             hover_index;  /* -1 = no hover                         */
    int             pressed_index;/* -1 = no press                         */
} toolbar_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * toolbar_create - Create a new toolbar control
 * @x:  X position relative to window content area
 * @y:  Y position relative to window content area
 * @w:  Total width
 *
 * Returns: Pointer to new toolbar, or NULL on failure.
 */
toolbar_t *toolbar_create(int x, int y, int w);

/**
 * toolbar_add_button - Add a button item
 * @tb:       Toolbar
 * @label:    Button text (copied)
 * @callback: Click handler
 * @userdata: Passed to callback
 *
 * Returns: Item index (0-based), or -1 if max items reached.
 */
int toolbar_add_button(toolbar_t *tb, const char *label,
                       void (*callback)(void *userdata), void *userdata);

/**
 * toolbar_add_separator - Add a vertical separator
 * @tb: Toolbar
 *
 * Returns: Item index, or -1 if max items reached.
 */
int toolbar_add_separator(toolbar_t *tb);

/**
 * toolbar_set_enabled - Enable or disable a toolbar item
 * @tb:      Toolbar
 * @index:   Item index
 * @enabled: 1 = enabled, 0 = disabled
 */
void toolbar_set_enabled(toolbar_t *tb, int index, int enabled);

/**
 * toolbar_destroy - Free toolbar resources
 * @tb: Toolbar to destroy
 */
void toolbar_destroy(toolbar_t *tb);

#endif /* TOOLBAR_H */
