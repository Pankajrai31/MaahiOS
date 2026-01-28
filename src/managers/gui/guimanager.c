#include "guimanager.h"
#include <stddef.h>

// Forward declarations for GFX abstraction layer
extern void gfx_fill_rect(int x, int y, int width, int height, uint32_t color);
extern void gfx_draw_rect(int x, int y, int width, int height, uint32_t color);
extern void gfx_draw_string(int x, int y, const char *text, uint32_t fg, uint32_t bg);

// Global control storage
static control_t g_controls[MAX_CONTROLS];
static int g_control_count = 0;
static int g_next_control_id = 1;

// Helper function to find control by ID
static control_t* find_control(int control_id) {
    for (int i = 0; i < g_control_count; i++) {
        if (g_controls[i].id == control_id) {
            return &g_controls[i];
        }
    }
    return NULL;
}

// Helper: Simple strlen
static int strlen_internal(const char *str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

// Helper: Simple strcpy
static void strcpy_internal(char *dest, const char *src) {
    int i = 0;
    while (src[i]) {
        dest[i] = src[i];
        i++;
    }
    dest[i] = '\0';
}

/**
 * Create a new control
 */
int control_create(int window_id, uint8_t type) {
    if (g_control_count >= MAX_CONTROLS) {
        return -1;  // No space
    }
    
    control_t *ctrl = &g_controls[g_control_count];
    g_control_count++;
    
    // Initialize control
    ctrl->id = g_next_control_id++;
    ctrl->type = type;
    ctrl->window_id = window_id;
    ctrl->parent_id = -1;  // No parent by default
    ctrl->visible = 1;
    ctrl->enabled = 1;
    ctrl->layout_type = LAYOUT_ABSOLUTE;
    
    // Default position and size
    ctrl->x = 0;
    ctrl->y = 0;
    ctrl->width = 100;
    ctrl->height = 20;
    ctrl->abs_x = 0;
    ctrl->abs_y = 0;
    
    // Default alignment
    ctrl->h_align = ALIGN_LEFT;
    ctrl->v_align = ALIGN_TOP;
    
    // Default margins/padding
    ctrl->margin_left = ctrl->margin_top = ctrl->margin_right = ctrl->margin_bottom = 0;
    ctrl->padding_left = ctrl->padding_top = ctrl->padding_right = ctrl->padding_bottom = 5;
    
    // Default colors
    ctrl->bg_color = 0xF0F0F0;  // Light gray background
    ctrl->fg_color = 0x000000;  // Black text
    ctrl->border_color = 0x808080;  // Gray border
    
    ctrl->text[0] = '\0';
    
    // Type-specific initialization
    switch (type) {
        case CONTROL_TYPE_PANEL:
            ctrl->data.panel.scrollable = 0;
            ctrl->data.panel.scroll_x = 0;
            ctrl->data.panel.scroll_y = 0;
            ctrl->data.panel.content_width = 0;
            ctrl->data.panel.content_height = 0;
            ctrl->data.panel.child_count = 0;
            ctrl->bg_color = 0xFFFFFF;  // White background for panels
            break;
            
        case CONTROL_TYPE_TABLE:
            ctrl->data.table.row_count = 0;
            ctrl->data.table.col_count = 0;
            ctrl->data.table.row_height = 20;
            ctrl->data.table.has_header = 0;
            ctrl->data.table.header_height = 25;
            for (int i = 0; i < 16; i++) {
                ctrl->data.table.col_widths[i] = 100;  // Default column width
            }
            break;
            
        case CONTROL_TYPE_TEXTBOX:
            ctrl->data.textbox.content[0] = '\0';
            ctrl->data.textbox.cursor_pos = 0;
            ctrl->data.textbox.max_length = 1023;
            ctrl->data.textbox.readonly = 0;
            ctrl->data.textbox.password = 0;
            ctrl->bg_color = 0xFFFFFF;  // White background
            ctrl->height = 25;
            break;
            
        case CONTROL_TYPE_BUTTON:
            ctrl->data.button.pressed = 0;
            ctrl->data.button.on_click = NULL;
            ctrl->bg_color = 0xE0E0E0;  // Light gray
            ctrl->height = 30;
            break;
            
        case CONTROL_TYPE_SCROLLBAR:
            ctrl->data.scrollbar.orientation = SCROLLBAR_VERTICAL;
            ctrl->data.scrollbar.min_value = 0;
            ctrl->data.scrollbar.max_value = 100;
            ctrl->data.scrollbar.current_value = 0;
            ctrl->data.scrollbar.thumb_size = 20;
            ctrl->data.scrollbar.thumb_pos = 0;
            ctrl->data.scrollbar.on_scroll = NULL;
            ctrl->width = 16;  // Standard scrollbar width
            ctrl->bg_color = 0xD0D0D0;
            break;
            
        case CONTROL_TYPE_LISTBOX:
            ctrl->data.listbox.item_count = 0;
            ctrl->data.listbox.selected_index = -1;
            ctrl->data.listbox.scroll_offset = 0;
            ctrl->data.listbox.visible_items = 10;
            ctrl->data.listbox.on_select = NULL;
            ctrl->bg_color = 0xFFFFFF;
            break;
            
        case CONTROL_TYPE_CHECKBOX:
            ctrl->data.checkbox.checked = 0;
            ctrl->data.checkbox.on_change = NULL;
            ctrl->width = 20;
            ctrl->height = 20;
            break;
    }
    
    return ctrl->id;
}

/**
 * Set control position
 */
int control_set_position(int control_id, int x, int y) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->x = x;
    ctrl->y = y;
    control_compute_layout(control_id);
    return 0;
}

/**
 * Set control size
 */
int control_set_size(int control_id, int width, int height) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->width = width;
    ctrl->height = height;
    return 0;
}

/**
 * Set control text
 */
int control_set_text(int control_id, const char *text) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    int len = strlen_internal(text);
    if (len > 255) len = 255;
    
    for (int i = 0; i < len; i++) {
        ctrl->text[i] = text[i];
    }
    ctrl->text[len] = '\0';
    
    return 0;
}

/**
 * Set control parent
 */
int control_set_parent(int control_id, int parent_id) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->parent_id = parent_id;
    
    // Add to parent's children list if parent is a panel
    if (parent_id >= 0) {
        control_t *parent = find_control(parent_id);
        if (parent && parent->type == CONTROL_TYPE_PANEL) {
            panel_add_child(parent_id, control_id);
        }
    }
    
    control_compute_layout(control_id);
    return 0;
}

/**
 * Set layout type
 */
int control_set_layout(int control_id, uint8_t layout_type) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->layout_type = layout_type;
    control_compute_layout(control_id);
    return 0;
}

/**
 * Set margins
 */
int control_set_margins(int control_id, int left, int top, int right, int bottom) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->margin_left = left;
    ctrl->margin_top = top;
    ctrl->margin_right = right;
    ctrl->margin_bottom = bottom;
    control_compute_layout(control_id);
    return 0;
}

/**
 * Set padding
 */
int control_set_padding(int control_id, int left, int top, int right, int bottom) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->padding_left = left;
    ctrl->padding_top = top;
    ctrl->padding_right = right;
    ctrl->padding_bottom = bottom;
    return 0;
}

/**
 * Set colors
 */
int control_set_colors(int control_id, uint32_t bg, uint32_t fg, uint32_t border) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->bg_color = bg;
    ctrl->fg_color = fg;
    ctrl->border_color = border;
    return 0;
}

/**
 * Set alignment
 */
int control_set_alignment(int control_id, uint8_t h_align, uint8_t v_align) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return -1;
    
    ctrl->h_align = h_align;
    ctrl->v_align = v_align;
    control_compute_layout(control_id);
    return 0;
}

/**
 * Get control by ID
 */
control_t* control_get(int control_id) {
    return find_control(control_id);
}

/**
 * Compute absolute position based on layout type and parent
 */
void control_compute_layout(int control_id) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl) return;
    
    // Start with relative position
    int x = ctrl->x + ctrl->margin_left;
    int y = ctrl->y + ctrl->margin_top;
    
    // If has parent, compute relative to parent
    if (ctrl->parent_id >= 0) {
        control_t *parent = find_control(ctrl->parent_id);
        if (parent) {
            x += parent->abs_x + parent->padding_left;
            y += parent->abs_y + parent->padding_top;
            
            // Apply scrolling if parent is scrollable panel
            if (parent->type == CONTROL_TYPE_PANEL && parent->data.panel.scrollable) {
                x -= parent->data.panel.scroll_x;
                y -= parent->data.panel.scroll_y;
            }
        }
    }
    
    ctrl->abs_x = x;
    ctrl->abs_y = y;
    
    // Recursively update children if this is a panel
    if (ctrl->type == CONTROL_TYPE_PANEL) {
        for (int i = 0; i < ctrl->data.panel.child_count; i++) {
            control_compute_layout(ctrl->data.panel.children[i]);
        }
    }
}

/**
 * Reflow children in a container
 */
void control_reflow(int parent_id) {
    control_t *parent = find_control(parent_id);
    if (!parent || parent->type != CONTROL_TYPE_PANEL) return;
    
    int current_y = 0;
    
    for (int i = 0; i < parent->data.panel.child_count; i++) {
        control_t *child = find_control(parent->data.panel.children[i]);
        if (child && child->visible) {
            if (child->layout_type == LAYOUT_FLOW) {
                child->y = current_y;
                current_y += child->height + child->margin_top + child->margin_bottom;
            }
            control_compute_layout(child->id);
        }
    }
    
    // Update content size
    parent->data.panel.content_height = current_y;
}

// ==================== PANEL FUNCTIONS ====================

int panel_add_child(int panel_id, int child_id) {
    control_t *panel = find_control(panel_id);
    if (!panel || panel->type != CONTROL_TYPE_PANEL) return -1;
    if (panel->data.panel.child_count >= 64) return -1;
    
    panel->data.panel.children[panel->data.panel.child_count++] = child_id;
    control_compute_layout(child_id);
    return 0;
}

int panel_set_scrollable(int panel_id, uint8_t scrollable) {
    control_t *panel = find_control(panel_id);
    if (!panel || panel->type != CONTROL_TYPE_PANEL) return -1;
    
    panel->data.panel.scrollable = scrollable;
    return 0;
}

int panel_scroll_to(int panel_id, int x, int y) {
    control_t *panel = find_control(panel_id);
    if (!panel || panel->type != CONTROL_TYPE_PANEL) return -1;
    
    panel->data.panel.scroll_x = x;
    panel->data.panel.scroll_y = y;
    
    // Recompute layout for all children
    for (int i = 0; i < panel->data.panel.child_count; i++) {
        control_compute_layout(panel->data.panel.children[i]);
    }
    
    return 0;
}

// ==================== TABLE FUNCTIONS ====================

int table_set_dimensions(int table_id, int rows, int cols) {
    control_t *table = find_control(table_id);
    if (!table || table->type != CONTROL_TYPE_TABLE) return -1;
    if (rows > 16 || cols > 16) return -1;
    
    table->data.table.row_count = rows;
    table->data.table.col_count = cols;
    
    // Auto-size table
    int total_width = 0;
    for (int i = 0; i < cols; i++) {
        total_width += table->data.table.col_widths[i];
    }
    table->width = total_width + 2;  // +2 for borders
    
    int total_height = table->data.table.row_height * rows;
    if (table->data.table.has_header) {
        total_height += table->data.table.header_height;
    }
    table->height = total_height + 2;
    
    return 0;
}

int table_set_cell(int table_id, int row, int col, const char *text) {
    control_t *table = find_control(table_id);
    if (!table || table->type != CONTROL_TYPE_TABLE) return -1;
    if (row >= table->data.table.row_count || col >= table->data.table.col_count) return -1;
    
    // For now, we'll store in control text (simplified)
    // In a full implementation, you'd allocate memory for each cell
    return 0;
}

int table_set_column_width(int table_id, int col, int width) {
    control_t *table = find_control(table_id);
    if (!table || table->type != CONTROL_TYPE_TABLE) return -1;
    if (col >= 16) return -1;
    
    table->data.table.col_widths[col] = width;
    return table_set_dimensions(table_id, table->data.table.row_count, table->data.table.col_count);
}

int table_set_header(int table_id, uint8_t has_header) {
    control_t *table = find_control(table_id);
    if (!table || table->type != CONTROL_TYPE_TABLE) return -1;
    
    table->data.table.has_header = has_header;
    return 0;
}

// ==================== TEXTBOX FUNCTIONS ====================

int textbox_set_content(int textbox_id, const char *content) {
    control_t *textbox = find_control(textbox_id);
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) return -1;
    
    int len = strlen_internal(content);
    if (len > textbox->data.textbox.max_length) len = textbox->data.textbox.max_length;
    
    for (int i = 0; i < len; i++) {
        textbox->data.textbox.content[i] = content[i];
    }
    textbox->data.textbox.content[len] = '\0';
    textbox->data.textbox.cursor_pos = len;
    
    return 0;
}

const char* textbox_get_content(int textbox_id) {
    control_t *textbox = find_control(textbox_id);
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) return NULL;
    
    return textbox->data.textbox.content;
}

int textbox_set_readonly(int textbox_id, uint8_t readonly) {
    control_t *textbox = find_control(textbox_id);
    if (!textbox || textbox->type != CONTROL_TYPE_TEXTBOX) return -1;
    
    textbox->data.textbox.readonly = readonly;
    return 0;
}

// ==================== SCROLLBAR FUNCTIONS ====================

int scrollbar_set_range(int scrollbar_id, int min, int max) {
    control_t *scrollbar = find_control(scrollbar_id);
    if (!scrollbar || scrollbar->type != CONTROL_TYPE_SCROLLBAR) return -1;
    
    scrollbar->data.scrollbar.min_value = min;
    scrollbar->data.scrollbar.max_value = max;
    return 0;
}

int scrollbar_set_value(int scrollbar_id, int value) {
    control_t *scrollbar = find_control(scrollbar_id);
    if (!scrollbar || scrollbar->type != CONTROL_TYPE_SCROLLBAR) return -1;
    
    if (value < scrollbar->data.scrollbar.min_value) value = scrollbar->data.scrollbar.min_value;
    if (value > scrollbar->data.scrollbar.max_value) value = scrollbar->data.scrollbar.max_value;
    
    scrollbar->data.scrollbar.current_value = value;
    
    // Update thumb position
    int range = scrollbar->data.scrollbar.max_value - scrollbar->data.scrollbar.min_value;
    if (range > 0) {
        int track_size = (scrollbar->data.scrollbar.orientation == SCROLLBAR_VERTICAL) 
                        ? scrollbar->height : scrollbar->width;
        track_size -= scrollbar->data.scrollbar.thumb_size;
        scrollbar->data.scrollbar.thumb_pos = (value * track_size) / range;
    }
    
    // Call callback if set
    if (scrollbar->data.scrollbar.on_scroll) {
        scrollbar->data.scrollbar.on_scroll(scrollbar_id, value);
    }
    
    return 0;
}

int scrollbar_get_value(int scrollbar_id) {
    control_t *scrollbar = find_control(scrollbar_id);
    if (!scrollbar || scrollbar->type != CONTROL_TYPE_SCROLLBAR) return 0;
    
    return scrollbar->data.scrollbar.current_value;
}

// ==================== RENDERING FUNCTIONS ====================

/**
 * Render a label control
 */
static void render_label(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    // Draw text
    gfx_draw_string(ctrl->abs_x, ctrl->abs_y, ctrl->text, ctrl->fg_color, ctrl->bg_color);
}

/**
 * Render a button control
 */
static void render_button(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    uint32_t bg = ctrl->data.button.pressed ? 0xC0C0C0 : ctrl->bg_color;
    
    // Draw button background
    gfx_fill_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, bg);
    
    // Draw border
    gfx_draw_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->border_color);
    
    // Draw text centered
    int text_len = strlen_internal(ctrl->text);
    int text_x = ctrl->abs_x + (ctrl->width - text_len * 8) / 2;
    int text_y = ctrl->abs_y + (ctrl->height - 16) / 2;
    gfx_draw_string(text_x, text_y, ctrl->text, ctrl->fg_color, bg);
}

/**
 * Render a textbox control
 */
static void render_textbox(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    // Draw background
    gfx_fill_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->bg_color);
    
    // Draw border
    gfx_draw_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->border_color);
    
    // Draw content
    int text_x = ctrl->abs_x + ctrl->padding_left;
    int text_y = ctrl->abs_y + ctrl->padding_top;
    
    if (ctrl->data.textbox.password) {
        // Show asterisks for password
        char masked[128];
        int len = strlen_internal(ctrl->data.textbox.content);
        if (len > 127) len = 127;
        for (int i = 0; i < len; i++) masked[i] = '*';
        masked[len] = '\0';
        gfx_draw_string(text_x, text_y, masked, ctrl->fg_color, ctrl->bg_color);
    } else {
        gfx_draw_string(text_x, text_y, ctrl->data.textbox.content, ctrl->fg_color, ctrl->bg_color);
    }
    
    // Draw cursor if enabled
    if (ctrl->enabled) {
        int cursor_x = text_x + ctrl->data.textbox.cursor_pos * 8;
        gfx_draw_rect(cursor_x, text_y, 2, 16, ctrl->fg_color);
    }
}

/**
 * Render a panel control
 */
static void render_panel(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    // Draw background
    gfx_fill_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->bg_color);
    
    // Draw border
    gfx_draw_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->border_color);
    
    // Render all children
    for (int i = 0; i < ctrl->data.panel.child_count; i++) {
        control_render(ctrl->data.panel.children[i]);
    }
    
    // Draw scrollbars if needed
    if (ctrl->data.panel.scrollable) {
        // Vertical scrollbar
        if (ctrl->data.panel.content_height > ctrl->height) {
            int scrollbar_x = ctrl->abs_x + ctrl->width - 16;
            int scrollbar_y = ctrl->abs_y;
            int scrollbar_h = ctrl->height;
            
            // Scrollbar background
            gfx_fill_rect(scrollbar_x, scrollbar_y, 16, scrollbar_h, 0xD0D0D0);
            
            // Thumb
            int thumb_h = (ctrl->height * ctrl->height) / ctrl->data.panel.content_height;
            if (thumb_h < 20) thumb_h = 20;
            int thumb_y = scrollbar_y + (ctrl->data.panel.scroll_y * (scrollbar_h - thumb_h)) / 
                         (ctrl->data.panel.content_height - ctrl->height);
            gfx_fill_rect(scrollbar_x + 2, thumb_y, 12, thumb_h, 0x808080);
        }
        
        // Horizontal scrollbar
        if (ctrl->data.panel.content_width > ctrl->width) {
            int scrollbar_x = ctrl->abs_x;
            int scrollbar_y = ctrl->abs_y + ctrl->height - 16;
            int scrollbar_w = ctrl->width;
            
            // Scrollbar background
            gfx_fill_rect(scrollbar_x, scrollbar_y, scrollbar_w, 16, 0xD0D0D0);
            
            // Thumb
            int thumb_w = (ctrl->width * ctrl->width) / ctrl->data.panel.content_width;
            if (thumb_w < 20) thumb_w = 20;
            int thumb_x = scrollbar_x + (ctrl->data.panel.scroll_x * (scrollbar_w - thumb_w)) / 
                         (ctrl->data.panel.content_width - ctrl->width);
            gfx_fill_rect(thumb_x, scrollbar_y + 2, thumb_w, 12, 0x808080);
        }
    }
}

/**
 * Render a table control
 */
static void render_table(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    int x = ctrl->abs_x;
    int y = ctrl->abs_y;
    
    // Draw table background
    gfx_fill_rect(x, y, ctrl->width, ctrl->height, ctrl->bg_color);
    
    // Draw border
    gfx_draw_rect(x, y, ctrl->width, ctrl->height, ctrl->border_color);
    
    int current_y = y + 1;
    
    // Draw header if present
    if (ctrl->data.table.has_header) {
        gfx_fill_rect(x + 1, current_y, ctrl->width - 2, 
                         ctrl->data.table.header_height, 0xE0E0E0);
        
        int current_x = x + 1;
        for (int col = 0; col < ctrl->data.table.col_count; col++) {
            // Draw column separator
            if (col > 0) {
                gfx_draw_rect(current_x, current_y, 1, 
                                ctrl->data.table.header_height, ctrl->border_color);
            }
            current_x += ctrl->data.table.col_widths[col];
        }
        
        current_y += ctrl->data.table.header_height;
        // Draw horizontal line after header
        gfx_draw_rect(x + 1, current_y, ctrl->width - 2, 1, ctrl->border_color);
        current_y++;
    }
    
    // Draw rows
    for (int row = 0; row < ctrl->data.table.row_count; row++) {
        int current_x = x + 1;
        
        for (int col = 0; col < ctrl->data.table.col_count; col++) {
            // Draw column separator
            if (col > 0) {
                gfx_draw_rect(current_x, current_y, 1, 
                                ctrl->data.table.row_height, ctrl->border_color);
            }
            
            current_x += ctrl->data.table.col_widths[col];
        }
        
        current_y += ctrl->data.table.row_height;
        
        // Draw horizontal line
        if (row < ctrl->data.table.row_count - 1) {
            gfx_draw_rect(x + 1, current_y, ctrl->width - 2, 1, ctrl->border_color);
        }
    }
}

/**
 * Render a scrollbar control
 */
static void render_scrollbar(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    // Draw track
    gfx_fill_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->bg_color);
    gfx_draw_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->border_color);
    
    // Draw thumb
    if (ctrl->data.scrollbar.orientation == SCROLLBAR_VERTICAL) {
        int thumb_x = ctrl->abs_x + 2;
        int thumb_y = ctrl->abs_y + ctrl->data.scrollbar.thumb_pos;
        gfx_fill_rect(thumb_x, thumb_y, ctrl->width - 4, 
                         ctrl->data.scrollbar.thumb_size, 0x606060);
    } else {
        int thumb_x = ctrl->abs_x + ctrl->data.scrollbar.thumb_pos;
        int thumb_y = ctrl->abs_y + 2;
        gfx_fill_rect(thumb_x, thumb_y, ctrl->data.scrollbar.thumb_size, 
                         ctrl->height - 4, 0x606060);
    }
}

/**
 * Render a checkbox control
 */
static void render_checkbox(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    // Draw checkbox box
    gfx_fill_rect(ctrl->abs_x, ctrl->abs_y, 20, 20, ctrl->bg_color);
    gfx_draw_rect(ctrl->abs_x, ctrl->abs_y, 20, 20, ctrl->border_color);
    
    // Draw check mark if checked
    if (ctrl->data.checkbox.checked) {
        gfx_draw_rect(ctrl->abs_x + 5, ctrl->abs_y + 5, 10, 10, 0x000000);
        gfx_fill_rect(ctrl->abs_x + 6, ctrl->abs_y + 6, 8, 8, 0x000000);
    }
    
    // Draw label
    if (ctrl->text[0]) {
        gfx_draw_string(ctrl->abs_x + 25, ctrl->abs_y + 2, ctrl->text, 
                        ctrl->fg_color, 0xFFFFFF);
    }
}

/**
 * Render a listbox control
 */
static void render_listbox(control_t *ctrl) {
    if (!ctrl->visible) return;
    
    // Draw background and border
    gfx_fill_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->bg_color);
    gfx_draw_rect(ctrl->abs_x, ctrl->abs_y, ctrl->width, ctrl->height, ctrl->border_color);
    
    // Draw items
    int y = ctrl->abs_y + 2;
    int item_height = 20;
    
    for (int i = ctrl->data.listbox.scroll_offset; 
         i < ctrl->data.listbox.item_count && i < ctrl->data.listbox.scroll_offset + ctrl->data.listbox.visible_items; 
         i++) {
        
        // Highlight selected item
        if (i == ctrl->data.listbox.selected_index) {
            gfx_fill_rect(ctrl->abs_x + 2, y, ctrl->width - 4, item_height, 0x0078D7);
            gfx_draw_string(ctrl->abs_x + 5, y + 2, ctrl->data.listbox.items[i], 
                           0xFFFFFF, 0x0078D7);
        } else {
            gfx_draw_string(ctrl->abs_x + 5, y + 2, ctrl->data.listbox.items[i], 
                           ctrl->fg_color, ctrl->bg_color);
        }
        
        y += item_height;
    }
}

/**
 * Render a single control
 */
void control_render(int control_id) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl || !ctrl->visible) return;
    
    switch (ctrl->type) {
        case CONTROL_TYPE_LABEL:
            render_label(ctrl);
            break;
        case CONTROL_TYPE_BUTTON:
            render_button(ctrl);
            break;
        case CONTROL_TYPE_TEXTBOX:
            render_textbox(ctrl);
            break;
        case CONTROL_TYPE_PANEL:
            render_panel(ctrl);
            break;
        case CONTROL_TYPE_TABLE:
            render_table(ctrl);
            break;
        case CONTROL_TYPE_SCROLLBAR:
            render_scrollbar(ctrl);
            break;
        case CONTROL_TYPE_CHECKBOX:
            render_checkbox(ctrl);
            break;
        case CONTROL_TYPE_LISTBOX:
            render_listbox(ctrl);
            break;
    }
}

/**
 * Render entire control tree starting from root
 */
void control_render_tree(int root_id) {
    control_t *root = find_control(root_id);
    if (!root) return;
    
    // Render root
    control_render(root_id);
    
    // If it's a panel, children are already rendered in render_panel
    // For other types, we'd need to handle child rendering here
}

/**
 * Handle mouse click on control
 */
int control_handle_click(int control_id, int mouse_x, int mouse_y) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl || !ctrl->visible || !ctrl->enabled) return 0;
    
    // Check if click is within control bounds
    if (mouse_x < ctrl->abs_x || mouse_x > ctrl->abs_x + ctrl->width ||
        mouse_y < ctrl->abs_y || mouse_y > ctrl->abs_y + ctrl->height) {
        return 0;
    }
    
    // Handle based on control type
    switch (ctrl->type) {
        case CONTROL_TYPE_BUTTON:
            ctrl->data.button.pressed = 1;
            if (ctrl->data.button.on_click) {
                ctrl->data.button.on_click(control_id);
            }
            return 1;
            
        case CONTROL_TYPE_CHECKBOX:
            ctrl->data.checkbox.checked = !ctrl->data.checkbox.checked;
            if (ctrl->data.checkbox.on_change) {
                ctrl->data.checkbox.on_change(control_id, ctrl->data.checkbox.checked);
            }
            return 1;
            
        case CONTROL_TYPE_LISTBOX: {
            int rel_y = mouse_y - ctrl->abs_y - 2;
            int item_height = 20;
            int clicked_index = ctrl->data.listbox.scroll_offset + (rel_y / item_height);
            
            if (clicked_index >= 0 && clicked_index < ctrl->data.listbox.item_count) {
                ctrl->data.listbox.selected_index = clicked_index;
                if (ctrl->data.listbox.on_select) {
                    ctrl->data.listbox.on_select(control_id, clicked_index);
                }
                return 1;
            }
            break;
        }
        
        case CONTROL_TYPE_PANEL:
            // Check children
            for (int i = 0; i < ctrl->data.panel.child_count; i++) {
                if (control_handle_click(ctrl->data.panel.children[i], mouse_x, mouse_y)) {
                    return 1;
                }
            }
            break;
    }
    
    return 0;
}

/**
 * Handle keyboard input for control
 */
int control_handle_key(int control_id, char key) {
    control_t *ctrl = find_control(control_id);
    if (!ctrl || !ctrl->visible || !ctrl->enabled) return 0;
    
    if (ctrl->type == CONTROL_TYPE_TEXTBOX && !ctrl->data.textbox.readonly) {
        if (key == '\b') {  // Backspace
            if (ctrl->data.textbox.cursor_pos > 0) {
                ctrl->data.textbox.cursor_pos--;
                ctrl->data.textbox.content[ctrl->data.textbox.cursor_pos] = '\0';
            }
        } else if (key >= 32 && key < 127) {  // Printable character
            int len = strlen_internal(ctrl->data.textbox.content);
            if (len < ctrl->data.textbox.max_length) {
                ctrl->data.textbox.content[len] = key;
                ctrl->data.textbox.content[len + 1] = '\0';
                ctrl->data.textbox.cursor_pos++;
            }
        }
        return 1;
    }
    
    return 0;
}
