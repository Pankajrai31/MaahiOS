#define VIDEO_MEMORY 0xA0000
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200

static unsigned char *video_mem = (unsigned char*)VIDEO_MEMORY;

// VGA register ports
#define VGA_MISC_WRITE 0x3C2
#define VGA_SEQ_INDEX 0x3C4
#define VGA_SEQ_DATA 0x3C5
#define VGA_GC_INDEX 0x3CE
#define VGA_GC_DATA 0x3CF
#define VGA_CRTC_INDEX 0x3D4
#define VGA_CRTC_DATA 0x3D5
#define VGA_AC_INDEX 0x3C0
#define VGA_AC_WRITE 0x3C0
#define VGA_AC_READ 0x3C1

static inline void outb(unsigned short port, unsigned char val) {
    asm volatile("outb %0, %1" : : "a"(val), "Nd"(port));
}

static inline unsigned char inb(unsigned short port) {
    unsigned char ret;
    asm volatile("inb %1, %0" : "=a"(ret) : "Nd"(port));
    return ret;
}

// Switch to Mode 13h (320x200, 256 colors) using direct VGA register programming
void graphics_mode_13h(void) {
    unsigned char seq_regs[] = {0x03, 0x01, 0x0F, 0x00, 0x0E};
    unsigned char crtc_regs[] = {0x5F, 0x4F, 0x50, 0x82, 0x54, 0x80, 0xBF, 0x1F,
                                  0x00, 0x41, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
                                  0x9C, 0x0E, 0x8F, 0x28, 0x40, 0x96, 0xB9, 0xA3, 0xFF};
    unsigned char gc_regs[] = {0x00, 0x00, 0x00, 0x00, 0x00, 0x40, 0x05, 0x0F, 0xFF};
    unsigned char ac_regs[] = {0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
                                0x08, 0x09, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F,
                                0x41, 0x00, 0x0F, 0x00, 0x00};
    
    // Misc Output Register
    outb(VGA_MISC_WRITE, 0x63);
    
    // Sequencer Registers
    for (int i = 0; i < 5; i++) {
        outb(VGA_SEQ_INDEX, i);
        outb(VGA_SEQ_DATA, seq_regs[i]);
    }
    
    // CRTC Registers - unlock them first
    outb(VGA_CRTC_INDEX, 0x03);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) | 0x80);
    outb(VGA_CRTC_INDEX, 0x11);
    outb(VGA_CRTC_DATA, inb(VGA_CRTC_DATA) & ~0x80);
    
    // Make sure they remain unlocked
    crtc_regs[0x03] |= 0x80;
    crtc_regs[0x11] &= ~0x80;
    
    for (int i = 0; i < 25; i++) {
        outb(VGA_CRTC_INDEX, i);
        outb(VGA_CRTC_DATA, crtc_regs[i]);
    }
    
    // Graphics Controller Registers
    for (int i = 0; i < 9; i++) {
        outb(VGA_GC_INDEX, i);
        outb(VGA_GC_DATA, gc_regs[i]);
    }
    
    // Attribute Controller Registers
    inb(0x3DA); // Reset attribute controller flip-flop
    for (int i = 0; i < 21; i++) {
        outb(VGA_AC_INDEX, i);
        outb(VGA_AC_WRITE, ac_regs[i]);
    }
    outb(VGA_AC_INDEX, 0x20); // Enable video
}

// Put a pixel at (x, y) with color
void put_pixel(int x, int y, unsigned char color) {
    if (x >= 0 && x < SCREEN_WIDTH && y >= 0 && y < SCREEN_HEIGHT) {
        video_mem[y * SCREEN_WIDTH + x] = color;
    }
}

// Draw a filled rectangle
void draw_rect_filled(int x, int y, int width, int height, unsigned char color) {
    for (int row = 0; row < height; row++) {
        for (int col = 0; col < width; col++) {
            put_pixel(x + col, y + row, color);
        }
    }
}

// Draw a line (simple horizontal/vertical)
void draw_line(int x1, int y1, int x2, int y2, unsigned char color) {
    if (y1 == y2) { // Horizontal line
        int start = (x1 < x2) ? x1 : x2;
        int end = (x1 < x2) ? x2 : x1;
        for (int x = start; x <= end; x++) {
            put_pixel(x, y1, color);
        }
    } else if (x1 == x2) { // Vertical line
        int start = (y1 < y2) ? y1 : y2;
        int end = (y1 < y2) ? y2 : y1;
        for (int y = start; y <= end; y++) {
            put_pixel(x1, y, color);
        }
    }
}

// Clear screen to color
void clear_screen(unsigned char color) {
    for (int i = 0; i < SCREEN_WIDTH * SCREEN_HEIGHT; i++) {
        video_mem[i] = color;
    }
}
