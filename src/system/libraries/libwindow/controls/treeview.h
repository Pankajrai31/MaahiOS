/**
 * MaahiOS Window Library - TreeView Control Header (Design System v2)
 *
 * Description:
 *   Hierarchical tree control with expandable/collapsible nodes.
 *   Each node has a label, optional children, and expand/collapse state.
 *
 *   Visual layout:
 *     ┌─── sunken border ─────────────────┐
 *     │ ▸ Drive C:                        │
 *     │   ▸ BOOT                          │
 *     │   ▸ ICONS                         │
 *     │ ▾ Drive D:                        │
 *     │   ▸ docs                          │
 *     │     readme.txt                    │
 *     └───────────────────────────────────┘
 *
 *   Nodes use indentation to show hierarchy depth.
 *   ▸ = collapsed (has children), ▾ = expanded, no icon = leaf.
 *   Selected node highlighted with accent color.
 *
 * Author: MaahiOS Team
 * Date: March 2026
 */

#ifndef TREEVIEW_H
#define TREEVIEW_H

#include "control.h"
#include <stdint.h>

/*=============================================================================
 * TREEVIEW CONSTANTS
 *===========================================================================*/

#define TREEVIEW_MAX_NODES     64     /* Maximum total nodes                */
#define TREEVIEW_MAX_LABEL     32     /* Max label text per node            */
#define TREEVIEW_MAX_CHILDREN  16     /* Max direct children per node       */
#define TREEVIEW_ROW_H         20     /* Row height per visible node        */
#define TREEVIEW_INDENT        16     /* Pixels of indent per depth level   */
#define TREEVIEW_ICON_W        12     /* Width of expand/collapse icon area */
#define TREEVIEW_PAD_X          4     /* Left padding                       */
#define TREEVIEW_BORDER_W       2     /* Sunken border width                */

/*=============================================================================
 * TREEVIEW NODE
 *===========================================================================*/

typedef struct {
    char     label[TREEVIEW_MAX_LABEL];
    int      parent;                   /* Parent node index, -1 for root   */
    int      children[TREEVIEW_MAX_CHILDREN];
    int      child_count;
    int      expanded;                 /* 1 = children visible             */
    int      is_leaf;                  /* 1 = no expand icon               */
    void    *userdata;                 /* App-specific data                */
} treeview_node_t;

/*=============================================================================
 * TREEVIEW STRUCT
 *===========================================================================*/

typedef struct {
    control_t        base;             /* MUST be first member              */
    treeview_node_t  nodes[TREEVIEW_MAX_NODES];
    int              node_count;
    int              selected;         /* Selected node index, -1 = none   */
    int              scroll_offset;    /* First visible row index          */
    int              visible_rows;     /* Computed from height             */

    /* Callback when a node is selected */
    void           (*on_select)(int node_index, void *userdata);
    void            *select_data;

    /* Callback when a node is expanded (for lazy-loading children) */
    void           (*on_expand)(int node_index, void *userdata);
    void            *expand_data;
} treeview_t;

/*=============================================================================
 * API
 *===========================================================================*/

/**
 * treeview_create - Create a new treeview control
 * @x:  X position relative to window content area
 * @y:  Y position relative to window content area
 * @w:  Total width (including border)
 * @h:  Total height (including border)
 *
 * Returns: Pointer to new treeview, or NULL on failure.
 */
treeview_t *treeview_create(int x, int y, int w, int h);

/**
 * treeview_add_node - Add a node to the tree
 * @tv:      TreeView
 * @label:   Node text (copied)
 * @parent:  Parent node index (-1 for root-level)
 * @is_leaf: 1 = leaf node (no expand icon), 0 = can have children
 *
 * Returns: Node index (0-based), or -1 if max nodes reached.
 */
int treeview_add_node(treeview_t *tv, const char *label,
                      int parent, int is_leaf);

/**
 * treeview_clear - Remove all nodes
 * @tv: TreeView
 */
void treeview_clear(treeview_t *tv);

/**
 * treeview_set_expanded - Expand or collapse a node
 * @tv:       TreeView
 * @node:     Node index
 * @expanded: 1 = expanded, 0 = collapsed
 */
void treeview_set_expanded(treeview_t *tv, int node, int expanded);

/**
 * treeview_set_selected - Set the selected node
 * @tv:   TreeView
 * @node: Node index (-1 = deselect)
 */
void treeview_set_selected(treeview_t *tv, int node);

/**
 * treeview_set_on_select - Set selection callback
 * @tv:       TreeView
 * @callback: Called with node index when user selects a node
 * @userdata: Passed to callback
 */
void treeview_set_on_select(treeview_t *tv,
                            void (*callback)(int node_index, void *userdata),
                            void *userdata);

/**
 * treeview_set_on_expand - Set expand callback (for lazy-loading)
 * @tv:       TreeView
 * @callback: Called with node index when user expands a node
 * @userdata: Passed to callback
 */
void treeview_set_on_expand(treeview_t *tv,
                            void (*callback)(int node_index, void *userdata),
                            void *userdata);

/**
 * treeview_get_node_userdata - Get app-specific data for a node
 * @tv:   TreeView
 * @node: Node index
 *
 * Returns: The userdata pointer, or NULL if invalid.
 */
void *treeview_get_node_userdata(treeview_t *tv, int node);

/**
 * treeview_set_node_userdata - Set app-specific data for a node
 * @tv:       TreeView
 * @node:     Node index
 * @userdata: Data to store
 */
void treeview_set_node_userdata(treeview_t *tv, int node, void *userdata);

/**
 * treeview_destroy - Free treeview resources
 * @tv: TreeView to destroy
 */
void treeview_destroy(treeview_t *tv);

#endif /* TREEVIEW_H */
