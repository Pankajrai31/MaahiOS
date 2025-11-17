/* Task 2: Cricket Random Selector - Kernel version */

#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80

static volatile unsigned short *vga = (unsigned short*)VGA_ADDR;
static unsigned int seed2 = 987654321;

unsigned int rand_simple2() {
    seed2 = seed2 * 1103515245 + 12345;
    return (seed2 / 65536) % 32768;
}

void task2_main() {
    const char *cricket_shots[] = {
        "Six!    ",
        "Four!   ",
        "1 Run   ",
        "Catch!  ",
        "Sweep   ",
        "Drive   ",
        "Pull    ",
        "Cut     ",
        "Bouncer ",
        "Wicket! "
    };
    
    while (1) {
        /* Generate random number 0-9 */
        int shot_num = rand_simple2() % 10;
        
        /* Print at position in Box 2 (row 14, col 30) */
        unsigned char attr = 14;  /* Yellow */
        int pos = 14 * VGA_WIDTH + 30;
        
        const char *shot = cricket_shots[shot_num];
        for (int i = 0; shot[i] != '\0'; i++) {
            vga[pos + i] = (attr << 8) | shot[i];
        }
        
        /* Yield to other tasks */
        for (volatile int j = 0; j < 150000; j++);
    }
}
