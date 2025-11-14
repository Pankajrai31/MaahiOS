#include "../syscalls/user_syscalls.h"

/* Simple 5x7 font for drawing text */
/* Each character is 5 pixels wide, 7 pixels tall */
static const unsigned char font_H[] = {
    0b10001,
    0b10001,
    0b10001,
    0b11111,
    0b10001,
    0b10001,
    0b10001
};

static const unsigned char font_e[] = {
    0b00000,
    0b00000,
    0b01110,
    0b10001,
    0b11111,
    0b10000,
    0b01110
};

static const unsigned char font_l[] = {
    0b01000,
    0b01000,
    0b01000,
    0b01000,
    0b01000,
    0b01000,
    0b01110
};

static const unsigned char font_o[] = {
    0b00000,
    0b00000,
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

static const unsigned char font_space[] = {
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000,
    0b00000
};

static const unsigned char font_M[] = {
    0b10001,
    0b11011,
    0b10101,
    0b10101,
    0b10001,
    0b10001,
    0b10001
};

static const unsigned char font_a[] = {
    0b00000,
    0b00000,
    0b01110,
    0b00001,
    0b01111,
    0b10001,
    0b01111
};

static const unsigned char font_h[] = {
    0b10000,
    0b10000,
    0b10110,
    0b11001,
    0b10001,
    0b10001,
    0b10001
};

static const unsigned char font_i[] = {
    0b00100,
    0b00000,
    0b01100,
    0b00100,
    0b00100,
    0b00100,
    0b01110
};

static const unsigned char font_O[] = {
    0b01110,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b10001,
    0b01110
};

static const unsigned char font_S[] = {
    0b01111,
    0b10000,
    0b10000,
    0b01110,
    0b00001,
    0b00001,
    0b11110
};

/* Draw a character at position (x, y) with color */
static void draw_char(int x, int y, const unsigned char* font, unsigned char color) {
    for (int row = 0; row < 7; row++) {
        unsigned char line = font[row];
        for (int col = 0; col < 5; col++) {
            if (line & (1 << (4 - col))) {
                syscall_put_pixel(x + col, y + row, color);
            }
        }
    }
}

/* Ring 3 system manager main function */
void sysman_main_c(void) {
    /* Clear screen to black */
    syscall_clear_gfx(0);
    
    /* Draw colorful gradient background */
    for (int y = 0; y < 50; y++) {
        for (int x = 0; x < 320; x++) {
            unsigned char color = (x + y) / 4;
            syscall_put_pixel(x, y, color);
        }
    }
    
    /* Draw "Hello from MaahiOS" */
    int start_x = 40;
    int start_y = 80;
    int char_spacing = 6;
    
    /* "Hello" in white */
    draw_char(start_x + 0*char_spacing, start_y, font_H, 15);
    draw_char(start_x + 1*char_spacing, start_y, font_e, 15);
    draw_char(start_x + 2*char_spacing, start_y, font_l, 15);
    draw_char(start_x + 3*char_spacing, start_y, font_l, 15);
    draw_char(start_x + 4*char_spacing, start_y, font_o, 15);
    
    /* " from " */
    draw_char(start_x + 6*char_spacing, start_y, font_space, 15);
    
    /* "MaahiOS" in cyan */
    int line2_x = start_x + 30;
    int line2_y = start_y + 20;
    draw_char(line2_x + 0*char_spacing, line2_y, font_M, 11);
    draw_char(line2_x + 1*char_spacing, line2_y, font_a, 11);
    draw_char(line2_x + 2*char_spacing, line2_y, font_a, 11);
    draw_char(line2_x + 3*char_spacing, line2_y, font_h, 11);
    draw_char(line2_x + 4*char_spacing, line2_y, font_i, 11);
    draw_char(line2_x + 5*char_spacing, line2_y, font_O, 11);
    draw_char(line2_x + 6*char_spacing, line2_y, font_S, 11);
    
    /* Draw some colorful boxes */
    for (int i = 0; i < 10; i++) {
        int box_x = 20 + i * 28;
        int box_y = 150;
        unsigned char color = i * 2 + 4;
        
        for (int y = 0; y < 30; y++) {
            for (int x = 0; x < 25; x++) {
                syscall_put_pixel(box_x + x, box_y + y, color);
            }
        }
    }
    
    /* White border around screen */
    for (int x = 0; x < 320; x++) {
        syscall_put_pixel(x, 0, 15);
        syscall_put_pixel(x, 199, 15);
    }
    for (int y = 0; y < 200; y++) {
        syscall_put_pixel(0, y, 15);
        syscall_put_pixel(319, y, 15);
    }
    
    /* Infinite loop */
    while(1) {
        /* Idle in Ring 3 */
    }
}
