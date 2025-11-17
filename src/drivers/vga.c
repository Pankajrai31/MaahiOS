#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_DEFAULT_ATTR 0x07  // Light gray on black

static volatile unsigned short *vga = (unsigned short*)VGA_ADDR;
static int vga_x = 0, vga_y = 0;
static unsigned char vga_current_attr = VGA_DEFAULT_ATTR;

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (vga_current_attr << 8) | ' ';
    }
    vga_x = 0;
    vga_y = 0;
}

void vga_set_color(unsigned char fg, unsigned char bg) {
    vga_current_attr = (bg << 4) | (fg & 0x0F);
}

void vga_draw_rect(int x, int y, int width, int height, unsigned char color) {
    // Create attribute: use color as background, same color as foreground for solid block
    unsigned char attr = (color << 4) | color;
    
    for (int row = y; row < y + height && row < VGA_HEIGHT; row++) {
        for (int col = x; col < x + width && col < VGA_WIDTH; col++) {
            int pos = row * VGA_WIDTH + col;
            vga[pos] = (attr << 8) | 0xDB;  // 0xDB = '█' filled block character
        }
    }
}

void vga_print(const char *s) {
    while (*s) {
        if (*s == '\n') {
            vga_x = 0;
            vga_y = vga_y + 1;
            if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
            s++;
            continue;
        }
        
        if (vga_x >= VGA_WIDTH) {
            vga_x = 0;
            vga_y = vga_y + 1;
            if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
        }
        
        int pos = vga_y * VGA_WIDTH + vga_x;
        vga[pos] = (vga_current_attr << 8) | (unsigned char)*s;
        vga_x = vga_x + 1;
        s++;
    }
}

void vga_putchar(char c) {
    if (c == '\n') {
        vga_x = 0;
        vga_y = vga_y + 1;
        if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
        return;
    }
    
    if (vga_x >= VGA_WIDTH) {
        vga_x = 0;
        vga_y = vga_y + 1;
        if (vga_y >= VGA_HEIGHT) vga_y = VGA_HEIGHT - 1;
    }
    
    int pos = vga_y * VGA_WIDTH + vga_x;
    vga[pos] = (vga_current_attr << 8) | (unsigned char)c;
    vga_x = vga_x + 1;
}

void vga_putint(int num) {
    char buffer[32];
    char *p = buffer;
    
    if (num == 0) {
        vga_putchar('0');
        return;
    }
    
    int n = (num < 0) ? -num : num;
    int count = 0;
    
    while (n > 0) {
        buffer[count++] = '0' + (n % 10);
        n = n / 10;
    }
    
    if (num < 0) {
        vga_putchar('-');
    }
    
    for (int i = count - 1; i >= 0; i--) {
        vga_putchar(buffer[i]);
    }
}

void vga_put_hex(unsigned int val) {
    char hex[9];
    hex[8] = '\0';
    for (int i = 7; i >= 0; i--) {
        unsigned int digit = val & 0xF;
        hex[i] = (digit < 10) ? ('0' + digit) : ('a' + digit - 10);
        val >>= 4;
    }
    vga_print(hex);
}

void vga_puts(const char *s) {
    vga_print(s);
}

void vga_set_cursor(int x, int y) {
    if (x >= 0 && x < VGA_WIDTH && y >= 0 && y < VGA_HEIGHT) {
        vga_x = x;
        vga_y = y;
    }
}

void vga_print_at(int x, int y, const char *s) {
    if (x < 0 || x >= VGA_WIDTH || y < 0 || y >= VGA_HEIGHT) return;
    
    int saved_x = vga_x, saved_y = vga_y;
    vga_x = x;
    vga_y = y;
    
    while (*s && vga_x < VGA_WIDTH) {
        if (*s == '\n') {
            break;  // Don't wrap in positioned output
        }
        int pos = vga_y * VGA_WIDTH + vga_x;
        vga[pos] = (vga_current_attr << 8) | (unsigned char)*s;
        vga_x++;
        s++;
    }
    
    vga_x = saved_x;
    vga_y = saved_y;
}

void vga_draw_box(int x, int y, int width, int height) {
    if (x < 0 || y < 0 || x + width > VGA_WIDTH || y + height > VGA_HEIGHT) return;
    
    // Draw top border
    for (int i = 0; i < width; i++) {
        int pos = y * VGA_WIDTH + (x + i);
        vga[pos] = (vga_current_attr << 8) | '=';
    }
    
    // Draw bottom border
    for (int i = 0; i < width; i++) {
        int pos = (y + height - 1) * VGA_WIDTH + (x + i);
        vga[pos] = (vga_current_attr << 8) | '=';
    }
    
    // Draw left and right borders
    for (int i = 1; i < height - 1; i++) {
        int left_pos = (y + i) * VGA_WIDTH + x;
        int right_pos = (y + i) * VGA_WIDTH + (x + width - 1);
        vga[left_pos] = (vga_current_attr << 8) | '|';
        vga[right_pos] = (vga_current_attr << 8) | '|';
    }
}
