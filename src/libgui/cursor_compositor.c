/**
 * ==============================================
 *  MaahiOS Orbit Compositor Mouse Cursor Layer
 * ==============================================
 * 
 * Proper layered cursor rendering with save/restore
 * Prevents cursor from destroying UI elements underneath
 */

#include "../syscalls/user_syscalls.h"

// Cursor dimensions
#define CUR_W   12
#define CUR_H   18

// Backup buffer to restore background under cursor
static unsigned int cursor_backup[CUR_W * CUR_H];

// Last drawn position (-1 = not drawn yet)
static int cur_x = -1;
static int cur_y = -1;

/**
 * Save background pixels before drawing cursor
 */
static void cursor_backup_area(int x, int y) {
    int idx = 0;
    
    for (int iy = 0; iy < CUR_H; iy++) {
        for (int ix = 0; ix < CUR_W; ix++) {
            cursor_backup[idx++] = syscall_read_pixel(x + ix, y + iy);
        }
    }
}

/**
 * Restore background pixels (erase cursor)
 */
static void cursor_restore_area(int x, int y) {
    int idx = 0;
    
    for (int iy = 0; iy < CUR_H; iy++) {
        for (int ix = 0; ix < CUR_W; ix++) {
            // Draw single pixel - using fill_rect with 1x1 size
            syscall_fill_rect(x + ix, y + iy, 1, 1, cursor_backup[idx++]);
        }
    }
}

/**
 * Draw cursor shape (classic left-tilted triangle)
 * Triangle pattern (12x18 pixels):
 *   X = white, . = transparent, O = black outline
 */
static void cursor_draw_shape(int x, int y) {
    // Cursor pixel pattern: 1 = black outline, 2 = white fill, 0 = transparent
    static const unsigned char cursor_pattern[18][12] = {
        {1,0,0,0,0,0,0,0,0,0,0,0},  // Row 0: O
        {1,1,0,0,0,0,0,0,0,0,0,0},  // Row 1: OO
        {1,2,1,0,0,0,0,0,0,0,0,0},  // Row 2: OXO
        {1,2,2,1,0,0,0,0,0,0,0,0},  // Row 3: OXXO
        {1,2,2,2,1,0,0,0,0,0,0,0},  // Row 4: OXXXO
        {1,2,2,2,2,1,0,0,0,0,0,0},  // Row 5: OXXXXO
        {1,2,2,2,2,2,1,0,0,0,0,0},  // Row 6: OXXXXXO
        {1,2,2,2,2,2,2,1,0,0,0,0},  // Row 7: OXXXXXXO
        {1,2,2,2,2,2,2,2,1,0,0,0},  // Row 8: OXXXXXXXO
        {1,2,2,2,2,2,2,2,2,1,0,0},  // Row 9: OXXXXXXXXO
        {1,2,2,2,2,2,2,2,2,2,1,0},  // Row 10: OXXXXXXXXXO
        {1,2,2,2,2,2,1,1,1,1,1,1},  // Row 11: OXXXXXXOOOOOO
        {1,2,2,1,2,2,1,0,0,0,0,0},  // Row 12: OXXOXXO
        {1,2,1,0,1,2,2,1,0,0,0,0},  // Row 13: OXO OXXO
        {1,1,0,0,1,2,2,1,0,0,0,0},  // Row 14: OO  OXXO
        {1,0,0,0,0,1,2,2,1,0,0,0},  // Row 15: O    OXXO
        {0,0,0,0,0,1,2,2,1,0,0,0},  // Row 16:      OXXO
        {0,0,0,0,0,0,1,1,1,0,0,0},  // Row 17:       OOO
    };
    
    // Draw cursor pixel by pixel
    for (int iy = 0; iy < CUR_H; iy++) {
        for (int ix = 0; ix < CUR_W; ix++) {
            unsigned char pixel = cursor_pattern[iy][ix];
            
            if (pixel == 0) continue;  // Transparent - skip
            
            unsigned int color;
            if (pixel == 1) {
                color = 0x000000;  // Black outline
            } else {
                color = 0xFFFFFF;  // White fill
            }
            
            syscall_fill_rect(x + ix, y + iy, 1, 1, color);
        }
    }
}

/**
 * PUBLIC API: Draw cursor at (x, y)
 * Automatically saves/restores background
 */
void orbit_draw_cursor(int x, int y) {
    // Skip if cursor hasn't moved - prevents unnecessary flickering!
    if (x == cur_x && y == cur_y) {
        return;
    }
    
    // Restore old cursor background if previously drawn
    if (cur_x >= 0) {
        cursor_restore_area(cur_x, cur_y);
    }
    
    // Backup new background
    cursor_backup_area(x, y);
    
    // Draw the cursor on top
    cursor_draw_shape(x, y);
    
    // Remember current position
    cur_x = x;
    cur_y = y;
}

/**
 * Initialize cursor compositor
 * Call this once at startup
 */
void orbit_cursor_init(void) {
    cur_x = -1;
    cur_y = -1;
}
