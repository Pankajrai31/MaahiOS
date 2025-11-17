/* Task 3: Progress Bar / Counter - Kernel version */

#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80

static volatile unsigned short *vga = (unsigned short*)VGA_ADDR;

void task3_main() {
    int counter = 0;
    
    while (1) {
        /* Calculate progress bar length */
        int bar_length = (counter % 20);
        
        /* Print at position in Box 3 (row 23, col 55) */
        unsigned char attr = 11;  /* Light Cyan */
        int pos = 23 * VGA_WIDTH + 55;
        
        /* Draw progress bar */
        for (int i = 0; i < bar_length; i++) {
            vga[pos + i] = (attr << 8) | '#';
        }
        for (int i = bar_length; i < 20; i++) {
            vga[pos + i] = (attr << 8) | '-';
        }
        
        counter++;
        
        /* Yield to other tasks */
        for (volatile int j = 0; j < 120000; j++);
    }
}
