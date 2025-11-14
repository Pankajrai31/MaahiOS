#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_ATTR 0x07

static volatile unsigned short *vga = (unsigned short*)VGA_ADDR;
static int vga_x = 0, vga_y = 0;

void vga_clear(void) {
    for (int i = 0; i < VGA_WIDTH * VGA_HEIGHT; i++) {
        vga[i] = (VGA_ATTR << 8) | ' ';
    }
    vga_x = 0;
    vga_y = 0;
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
        vga[pos] = (VGA_ATTR << 8) | (unsigned char)*s;
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
    vga[pos] = (VGA_ATTR << 8) | (unsigned char)c;
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
