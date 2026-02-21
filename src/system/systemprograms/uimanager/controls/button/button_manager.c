/**
 * Button Manager - Handles all button operations
 * Reads button data from cells and renders them
 */

#include "button_manager.h"
#include "button_renderer.h"
#include "../../../../../Libraries/core/cells/cells.h"

/**
 * Check if point is inside rectangle
 */
static int is_point_in_rect(int px, int py, int x, int y, int w, int h) {
    return (px >= x && px < x + w && py >= y && py < y + h);
}

/**
 * Render all buttons from cell registry
 */
void button_manager_render_all(uint32_t *framebuffer, int screen_width,
                                int mouse_x, int mouse_y, int mouse_pressed) {
    /* Get control count from cells */
    int control_count = 0;
    if (maahi_cell_read_int("ui.controls.count", &control_count) != 0) {
        return;  /* No controls registered */
    }
    
    /* Render each button */
    for (int id = 1; id <= control_count; id++) {
        char key[128];
        int x, y, width, height;
        char label[64];
        
        /* Build key prefix: "ui.button.<id>." */
        key[0] = 'u'; key[1] = 'i'; key[2] = '.'; key[3] = 'b'; key[4] = 'u'; 
        key[5] = 't'; key[6] = 't'; key[7] = 'o'; key[8] = 'n'; key[9] = '.';
        int pos = 10;
        if (id >= 100) key[pos++] = '0' + (id / 100);
        if (id >= 10) key[pos++] = '0' + ((id / 10) % 10);
        key[pos++] = '0' + (id % 10);
        key[pos++] = '.';
        int base_pos = pos;
        
        /* Read button X */
        key[pos++] = 'x'; key[pos] = '\0';
        if (maahi_cell_read_int(key, &x) != 0) continue;
        
        /* Read button Y */
        key[base_pos] = 'y'; key[base_pos + 1] = '\0';
        if (maahi_cell_read_int(key, &y) != 0) continue;
        
        /* Read button width */
        key[base_pos] = 'w'; key[base_pos + 1] = 'i'; key[base_pos + 2] = 'd';
        key[base_pos + 3] = 't'; key[base_pos + 4] = 'h'; key[base_pos + 5] = '\0';
        if (maahi_cell_read_int(key, &width) != 0) continue;
        
        /* Read button height */
        key[base_pos] = 'h'; key[base_pos + 1] = 'e'; key[base_pos + 2] = 'i';
        key[base_pos + 3] = 'g'; key[base_pos + 4] = 'h'; key[base_pos + 5] = 't'; 
        key[base_pos + 6] = '\0';
        if (maahi_cell_read_int(key, &height) != 0) continue;
        
        /* Read button label */
        key[base_pos] = 'l'; key[base_pos + 1] = 'a'; key[base_pos + 2] = 'b';
        key[base_pos + 3] = 'e'; key[base_pos + 4] = 'l'; key[base_pos + 5] = '\0';
        size_t actual_size;
        if (maahi_cell_read(key, label, 64, &actual_size) != 0) {
            label[0] = '\0';
        }
        
        /* Determine hover and press state */
        int is_hover = is_point_in_rect(mouse_x, mouse_y, x, y, width, height);
        int is_pressed = is_hover && mouse_pressed;
        
        /* Render button */
        button_render_primary(framebuffer, screen_width, 
                             x, y, width, height, label, is_hover, is_pressed);
    }
}
