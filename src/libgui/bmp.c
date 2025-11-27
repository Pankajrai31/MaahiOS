#include "bmp.h"
#include "../syscalls/user_syscalls.h"

/**
 * Simple BMP loader for MaahiOS
 * Loads BMP from embedded data and draws via syscall
 */

void bmp_draw_embedded(int x, int y, const uint8_t *bmp_data) {
    if (!bmp_data) return;
    
    // Just pass the BMP data to kernel syscall
    // Kernel will parse and draw it efficiently
    syscall_draw_bmp(x, y, (uint32_t)bmp_data);
}
