/* Task 1: ABCD-J Color Cycler - Kernel version */

#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80

static volatile unsigned short *vga = (unsigned short*)VGA_ADDR;

void task1_main() {
    const char *letters = "ABCDEFGHIJ";
    unsigned char colors[] = {10, 12, 9};  /* Green, Red, Light Blue */
    int color_index = 0;
    int letter_index = 0;
    
    while (1) {
        unsigned char attr = colors[color_index];
        
        /* Clear previous (10 spaces at position) */
        int pos = 5 * VGA_WIDTH + 10;
        for (int k = 0; k < 10; k++) {
            vga[pos + k] = (attr << 8) | ' ';
        }
        
        /* Print letters */
        for (int i = 0; i <= letter_index && i < 10; i++) {
            vga[pos + i] = (attr << 8) | letters[i];
        }
        
        /* Update indices */
        letter_index++;
        if (letter_index >= 10) {
            letter_index = 0;
            color_index = (color_index + 1) % 3;
        }
        
        /* Yield to other tasks */
        for (volatile int j = 0; j < 100000; j++);
    }
}
