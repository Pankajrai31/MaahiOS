#ifndef GUIMANAGER_H
#define GUIMANAGER_H

#include <stdint.h>

// Control types
#define CONTROL_TYPE_LABEL      1
#define CONTROL_TYPE_BUTTON     2
#define CONTROL_TYPE_TEXTBOX    3
#define CONTROL_TYPE_PANEL      4
#define CONTROL_TYPE_TABLE      5
#define CONTROL_TYPE_SCROLLBAR  6
#define CONTROL_TYPE_CHECKBOX   7
#define CONTROL_TYPE_LISTBOX    8

// Layout types
#define LAYOUT_ABSOLUTE  0  // Fixed x, y positioning
#define LAYOUT_RELATIVE  1  // Relative to parent
#define LAYOUT_FLOW      2  // Auto-flow (like HTML flow)
#define LAYOUT_GRID      3  // Grid layout

// Alignment
#define ALIGN_LEFT    0
#define ALIGN_CENTER  1
#define ALIGN_RIGHT   2
#define ALIGN_TOP     0
#define ALIGN_MIDDLE  1
#define ALIGN_BOTTOM  2

// Scrollbar orientation
#define SCROLLBAR_VERTICAL    0
#define SCROLLBAR_HORIZONTAL  1

// Maximum controls per window/container
#define MAX_CONTROLS 256

// Control structure
typedef struct control_t {
    uint8_t type;           // Control type (CONTROL_TYPE_*)
    uint8_t visible;        // Is control visible
    uint8_t enabled;        // Is control enabled
    uint8_t layout_type;    // Layout type
    
    int id;                 // Unique control ID
    int parent_id;          // Parent control ID (-1 for window root)
    int window_id;          // Window this control belongs to
    
    // Position and size
    int x, y;               // Position (absolute or relative based on layout)
    int width, height;      // Size
    
    // Computed absolute position (for rendering)
    int abs_x, abs_y;
    
    // Alignment
    uint8_t h_align;        // Horizontal alignment
    uint8_t v_align;        // Vertical alignment
    
    // Margins and padding
    int margin_left, margin_top, margin_right, margin_bottom;
    int padding_left, padding_top, padding_right, padding_bottom;
    
    // Colors
    uint32_t bg_color;
    uint32_t fg_color;
    uint32_t border_color;
    
    // Text
    char text[256];
    
    // Type-specific data
    union {
        // Button
        struct {
            uint8_t pressed;
            void (*on_click)(int control_id);
        } button;
        
        // Textbox
        struct {
            char content[1024];
            int cursor_pos;
            int max_length;
            uint8_t readonly;
            uint8_t password;
        } textbox;
        
        // Panel/Container
        struct {
            uint8_t scrollable;
            int scroll_x, scroll_y;
            int content_width, content_height;
            int child_count;
            int children[64];  // Child control IDs
        } panel;
        
        // Table
        struct {
            int row_count;
            int col_count;
            int row_height;
            int col_widths[16];  // Width of each column
            char *cells[256];     // Pointers to cell text (row * col_count + col)
            int header_height;
            uint8_t has_header;
        } table;
        
        // Scrollbar
        struct {
            uint8_t orientation;
            int min_value;
            int max_value;
            int current_value;
            int thumb_size;
            int thumb_pos;
            void (*on_scroll)(int control_id, int value);
        } scrollbar;
        
        // Checkbox
        struct {
            uint8_t checked;
            void (*on_change)(int control_id, uint8_t checked);
        } checkbox;
        
        // Listbox
        struct {
            int item_count;
            int selected_index;
            int scroll_offset;
            int visible_items;
            char items[32][128];  // Up to 32 items, 128 chars each
            void (*on_select)(int control_id, int index);
        } listbox;
    } data;
    
} control_t;

// Control management functions
int control_create(int window_id, uint8_t type);
int control_set_position(int control_id, int x, int y);
int control_set_size(int control_id, int width, int height);
int control_set_text(int control_id, const char *text);
int control_set_parent(int control_id, int parent_id);
int control_set_layout(int control_id, uint8_t layout_type);
int control_set_margins(int control_id, int left, int top, int right, int bottom);
int control_set_padding(int control_id, int left, int top, int right, int bottom);
int control_set_colors(int control_id, uint32_t bg, uint32_t fg, uint32_t border);
int control_set_alignment(int control_id, uint8_t h_align, uint8_t v_align);

// Control query functions
control_t* control_get(int control_id);
int control_get_child_count(int control_id);
int control_get_child(int control_id, int index);

// Layout functions
void control_compute_layout(int control_id);
void control_reflow(int parent_id);

// Panel-specific functions
int panel_add_child(int panel_id, int child_id);
int panel_set_scrollable(int panel_id, uint8_t scrollable);
int panel_scroll_to(int panel_id, int x, int y);

// Table-specific functions
int table_set_dimensions(int table_id, int rows, int cols);
int table_set_cell(int table_id, int row, int col, const char *text);
int table_set_column_width(int table_id, int col, int width);
int table_set_header(int table_id, uint8_t has_header);

// Textbox-specific functions
int textbox_set_content(int textbox_id, const char *content);
const char* textbox_get_content(int textbox_id);
int textbox_set_readonly(int textbox_id, uint8_t readonly);

// Scrollbar-specific functions
int scrollbar_set_range(int scrollbar_id, int min, int max);
int scrollbar_set_value(int scrollbar_id, int value);
int scrollbar_get_value(int scrollbar_id);

// Rendering functions
void control_render(int control_id);
void control_render_tree(int root_id);

// Event handling
int control_handle_click(int control_id, int mouse_x, int mouse_y);
int control_handle_key(int control_id, char key);

#endif // GUIMANAGER_H
