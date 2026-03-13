/**
 * MaahiOS Window Library - MenuBar Control Header (Design System v2)
 *
 * Description:
 *   Horizontal menu bar with dropdown menus. Modular — apps configure
 *   menus and items at runtime.
 *
 *   Visual layout (closed):
 *     ┌──────────────────────────────────────────────────────┐
 *     │  File   Edit   View   Help                 chrome bg │
 *     └──────────────────────────────────────────────────────┘
 *
 *   Visual layout (dropdown open):
 *     ┌──────────────────────────────────────────────────────┐
 *     │ [File]  Edit   View   Help                           │
 *     ├─────────┐                                            │
 *     │ New     │                                            │
 *     │ Open    │                                            │
 *     │─────────│                                            │
 *     │ Save    │                                            │
 *     └─────────┘                                            │
 *
 *   When open, the control expands its height to cover the dropdown,
 *   capturing mouse events via back-to-front dispatch.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef MENUBAR_H
#define MENUBAR_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * MENUBAR CONSTANTS
 *===========================================================================*/

#define MENUBAR_MAX_MENUS       8      /* Max top-level menus               */
#define MENUBAR_MAX_ITEMS      12      /* Max items per menu                */
#define MENUBAR_MAX_LABEL      20      /* Max label text                    */
#define MENUBAR_HEIGHT         22      /* Height of the menu bar strip      */
#define MENUBAR_ITEM_PAD_X     10      /* Horizontal padding per menu label */
#define MENUBAR_DROP_ITEM_H    22      /* Dropdown item height              */
#define MENUBAR_DROP_PAD        3      /* Dropdown inner padding            */
#define MENUBAR_DROP_W        140      /* Dropdown width                    */

/*=============================================================================
 * MENUBAR ITEM (inside a dropdown)
 *===========================================================================*/

typedef struct {
    char     label[MENUBAR_MAX_LABEL];
    int      enabled;
    int      separator_before;         /* 1 = draw separator line above    */
    void   (*on_click)(void *userdata);
    void    *click_data;
} menubar_item_t;

/*=============================================================================
 * MENUBAR MENU (a top-level entry with dropdown)
 *===========================================================================*/

typedef struct {
    char           label[MENUBAR_MAX_LABEL];
    menubar_item_t items[MENUBAR_MAX_ITEMS];
    int            item_count;
    int            x;                  /* Computed X offset in menubar      */
    int            width;              /* Computed width of label area      */
} menubar_menu_t;

/*=============================================================================
 * MENUBAR STRUCT
 *===========================================================================*/

typedef struct {
    control_t      base;               /* MUST be first member              */
    menubar_menu_t menus[MENUBAR_MAX_MENUS];
    int            menu_count;
    int            open_menu;          /* -1 = no dropdown open             */
    int            hover_menu;         /* -1 = no menu label hovered        */
    int            hover_item;         /* -1 = no dropdown item hovered     */
    int            bar_width;          /* Total width of the bar            */
    int            saved_height;       /* Original height when no dropdown  */
} menubar_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * menubar_create - Create a new menubar control
 * @x:  X position relative to window content area
 * @y:  Y position relative to window content area
 * @w:  Total width
 *
 * Returns: Pointer to new menubar, or NULL on failure.
 */
menubar_t *menubar_create(int x, int y, int w);

/**
 * menubar_add_menu - Add a top-level menu
 * @mb:    MenuBar
 * @label: Menu name (e.g., "File", "Edit")
 *
 * Returns: Menu index (0-based), or -1 if max menus reached.
 */
int menubar_add_menu(menubar_t *mb, const char *label);

/**
 * menubar_add_item - Add an item to a menu's dropdown
 * @mb:         MenuBar
 * @menu_index: Menu to add item to
 * @label:      Item text (e.g., "New", "Open...")
 * @callback:   Click handler (NULL = no action)
 * @userdata:   Passed to callback
 *
 * Returns: Item index within the menu, or -1 if full.
 */
int menubar_add_item(menubar_t *mb, int menu_index, const char *label,
                     void (*callback)(void *userdata), void *userdata);

/**
 * menubar_add_separator - Add a separator line before the next item
 * @mb:         MenuBar
 * @menu_index: Menu to add separator to
 *
 * Sets a flag so the NEXT item added will have a separator above it.
 * Call this before menubar_add_item() for the item that follows.
 */
void menubar_add_separator(menubar_t *mb, int menu_index);

/**
 * menubar_set_item_enabled - Enable or disable a menu item
 * @mb:         MenuBar
 * @menu_index: Menu index
 * @item_index: Item index within menu
 * @enabled:    1 = enabled, 0 = disabled (grayed)
 */
void menubar_set_item_enabled(menubar_t *mb, int menu_index,
                              int item_index, int enabled);

/**
 * menubar_close - Force-close any open dropdown
 * @mb: MenuBar
 */
void menubar_close(menubar_t *mb);

/**
 * menubar_destroy - Free menubar resources
 * @mb: MenuBar to destroy
 */
void menubar_destroy(menubar_t *mb);

#endif /* MENUBAR_H */
