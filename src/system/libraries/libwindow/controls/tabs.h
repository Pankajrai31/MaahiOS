/**
 * MaahiOS Window Library - Tab Control Header (Design System v2)
 *
 * Description:
 *   Tabbed container control for switching between panels of content.
 *   Each tab has a label and an array of child controls. Only the
 *   active tab's children are drawn and receive events.
 *
 *   Visual layout:
 *     ┌──────┬──────┬──────┐
 *     │ Tab1 │ Tab2 │ Tab3 │          ← raised chrome tab strip
 *     ├──────┴──────┴──────┤
 *     │                     │          ← sunken content area
 *     │   [active tab's     │
 *     │    controls here]   │
 *     └─────────────────────┘
 *
 *   Active tab header is white (surface color) with no bottom border,
 *   merging into the content area. Inactive tabs are chrome.
 *
 * Usage:
 *   tabs_t *tabs = tabs_create(0, 0, 480, 320);
 *   int tab0 = tabs_add_tab(tabs, "Overview");
 *   int tab1 = tabs_add_tab(tabs, "Packets");
 *   tabs_add_child(tabs, tab0, &my_label->base);
 *   tabs_add_child(tabs, tab1, &my_table->base);
 *   window_add_control(win, &tabs->base);
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef TABS_H
#define TABS_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * TAB CONSTANTS
 *===========================================================================*/

#define TABS_MAX_TABS        8       /* Maximum number of tabs               */
#define TABS_MAX_CHILDREN    32      /* Max controls per tab                 */
#define TABS_MAX_LABEL       24      /* Max tab label text length            */
#define TABS_HEADER_H        26      /* Tab header strip height (pixels)     */
#define TABS_PAD             6       /* Padding around tab label text        */
#define TABS_BORDER          2       /* Sunken border width for content area */

/*=============================================================================
 * TAB DEFINITION
 *===========================================================================*/

typedef struct {
    char       label[TABS_MAX_LABEL];
    control_t *children[TABS_MAX_CHILDREN];
    int        child_count;
} tab_def_t;

/*=============================================================================
 * TABS CONTROL STRUCT
 *===========================================================================*/

typedef struct {
    control_t   base;                   /* MUST be first member             */
    tab_def_t   tabs[TABS_MAX_TABS];
    int         tab_count;
    int         active_tab;             /* Currently visible tab index      */
    int         hover_tab;              /* Tab being hovered, -1 = none     */

    /* Content area (below tab headers), auto-computed */
    int         content_x;
    int         content_y;
    int         content_w;
    int         content_h;

    /* Callback for tab change */
    void      (*on_tab_change)(int tab_index, void *userdata);
    void       *tab_change_data;
} tabs_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * tabs_create - Create a new tab container control
 * @x:      X position relative to window content area
 * @y:      Y position relative to window content area
 * @w:      Total width (including border)
 * @h:      Total height (including tab headers + border)
 *
 * Returns: Pointer to new tabs control, or NULL on failure.
 */
tabs_t *tabs_create(int x, int y, int w, int h);

/**
 * tabs_add_tab - Add a new tab
 * @tabs:   Tab control
 * @label:  Tab header label text (copied)
 *
 * Returns: Tab index (0-based), or -1 if max tabs reached.
 */
int tabs_add_tab(tabs_t *tabs, const char *label);

/**
 * tabs_add_child - Add a control to a specific tab
 * @tabs:       Tab control
 * @tab_index:  Tab to add the child to
 * @child:      Control to add (position relative to tab content area)
 *
 * Returns: 0 on success, -1 on failure.
 */
int tabs_add_child(tabs_t *tabs, int tab_index, control_t *child);

/**
 * tabs_set_active - Switch to a specific tab
 * @tabs:       Tab control
 * @tab_index:  Tab to activate
 */
void tabs_set_active(tabs_t *tabs, int tab_index);

/**
 * tabs_get_active - Get the currently active tab index
 * @tabs: Tab control
 * Returns: Active tab index
 */
int tabs_get_active(tabs_t *tabs);

/**
 * tabs_set_on_tab_change - Set callback for tab switch events
 * @tabs:      Tab control
 * @callback:  Called when active tab changes, with new tab index
 * @userdata:  Passed to callback
 */
void tabs_set_on_tab_change(tabs_t *tabs,
                            void (*callback)(int tab_index, void *userdata),
                            void *userdata);

/**
 * tabs_destroy - Free tab control and all child controls
 * @tabs: Tab control to destroy
 */
void tabs_destroy(tabs_t *tabs);

#endif /* TABS_H */
