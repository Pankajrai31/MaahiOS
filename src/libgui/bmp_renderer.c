/**
 * BMP Renderer for MaahiOS
 * Converts 24-bit BMP images (BGR format) to 32-bit RGB framebuffer
 */

#include <stdint.h>

/**
 * BMP File Header (14 bytes)
 */
typedef struct __attribute__((packed)) {
    uint16_t signature;      // 0x4D42 ('BM')
    uint32_t file_size;
    uint16_t reserved1;
    uint16_t reserved2;
    uint32_t data_offset;    // Offset to pixel data
} BMP_FileHeader;

/**
 * BMP Info Header (40 bytes - BITMAPINFOHEADER)
 */
typedef struct __attribute__((packed)) {
    uint32_t header_size;    // 40
    int32_t width;
    int32_t height;          // Positive = bottom-up, negative = top-down
    uint16_t planes;         // Always 1
    uint16_t bits_per_pixel; // 24 for RGB, 32 for RGBA
    uint32_t compression;    // 0 = uncompressed
    uint32_t image_size;
    int32_t x_pixels_per_m;
    int32_t y_pixels_per_m;
    uint32_t colors_used;
    uint32_t colors_important;
} BMP_InfoHeader;

/**
 * Draw BMP image to back buffer
 * 
 * @param x X position on screen
 * @param y Y position on screen
 * @param bmp_data Pointer to BMP file data (starts with 'BM' signature)
 * @param back_buffer Pointer to framebuffer (32-bit RGB)
 * @param screen_width Screen width in pixels
 * @param screen_height Screen height in pixels
 */
void bmp_draw_to_buffer(int x, int y, const unsigned char *bmp_data, 
                        uint32_t *back_buffer, int screen_width, int screen_height) {
    int width, height, bpp, is_bottom_up, bytes_per_pixel, row_size;
    const unsigned char *pixel_data;
    int row, col, src_row, dest_y, dest_x, pixel_offset, buffer_offset;
    unsigned char r, g, b;
    uint32_t rgb_pixel;
    
    // Parse BMP headers
    const BMP_FileHeader *file_header = (const BMP_FileHeader *)bmp_data;
    const BMP_InfoHeader *info_header = (const BMP_InfoHeader *)(bmp_data + sizeof(BMP_FileHeader));
    
    // Verify it's a valid BMP
    if (file_header->signature != 0x4D42) {  // 'BM'
        return;  // Not a BMP file
    }
    
    // Get image properties
    width = info_header->width;
    height = info_header->height;
    bpp = info_header->bits_per_pixel;
    is_bottom_up = (height > 0);
    
    // Handle negative height (top-down BMPs)
    if (height < 0) {
        height = -height;
    }
    
    // Support 24-bit and 32-bit uncompressed BMPs
    if ((bpp != 24 && bpp != 32) || info_header->compression != 0) {
        return;
    }
    
    // Get pixel data pointer
    pixel_data = bmp_data + file_header->data_offset;
    
    // BMP rows are padded to 4-byte boundaries
    bytes_per_pixel = bpp / 8;
    row_size = ((width * bytes_per_pixel + 3) / 4) * 4;
    
    // Draw pixels
    for (row = 0; row < height; row++) {
        const unsigned char *row_data;
        
        // Calculate source row (BMPs are stored bottom-up by default)
        src_row = is_bottom_up ? (height - 1 - row) : row;
        row_data = pixel_data + (src_row * row_size);
        
        // Calculate destination row on screen
        dest_y = y + row;
        if (dest_y < 0 || dest_y >= screen_height) {
            continue;  // Skip rows outside screen
        }
        
        // Draw pixels in this row
        for (col = 0; col < width; col++) {
            dest_x = x + col;
            if (dest_x < 0 || dest_x >= screen_width) {
                continue;  // Skip pixels outside screen
            }
            
            // Read BGR(A) pixel from BMP (24-bit or 32-bit)
            pixel_offset = col * bytes_per_pixel;
            b = row_data[pixel_offset + 0];
            g = row_data[pixel_offset + 1];
            r = row_data[pixel_offset + 2];
            
            // For 32-bit BMPs, check alpha channel (skip if fully transparent)
            if (bytes_per_pixel == 4) {
                unsigned char a = row_data[pixel_offset + 3];
                if (a == 0) {
                    continue;  // Skip fully transparent pixels
                }
            }
            
            // Convert BGR to RGB and write to framebuffer (32-bit)
            // Format: 0x00RRGGBB
            rgb_pixel = (r << 16) | (g << 8) | b;
            
            // Write to back buffer
            buffer_offset = dest_y * screen_width + dest_x;
            back_buffer[buffer_offset] = rgb_pixel;
        }
    }
}

/**
 * Draw BMP with transparency (magenta = 0xFF00FF is transparent)
 * Useful for cursors and icons with transparency
 */
void bmp_draw_transparent(int x, int y, const unsigned char *bmp_data,
                          uint32_t *back_buffer, int screen_width, int screen_height) {
    int width, height, bpp, is_bottom_up, bytes_per_pixel, row_size;
    const unsigned char *pixel_data;
    int row, col, src_row, dest_y, dest_x, pixel_offset, buffer_offset;
    unsigned char r, g, b;
    uint32_t rgb_pixel;
    const unsigned char *row_data;
    
    const BMP_FileHeader *file_header = (const BMP_FileHeader *)bmp_data;
    const BMP_InfoHeader *info_header = (const BMP_InfoHeader *)(bmp_data + sizeof(BMP_FileHeader));
    
    if (file_header->signature != 0x4D42) return;
    
    width = info_header->width;
    height = info_header->height;
    bpp = info_header->bits_per_pixel;
    is_bottom_up = (height > 0);
    
    if (height < 0) height = -height;
    if ((bpp != 24 && bpp != 32) || info_header->compression != 0) return;
    
    pixel_data = bmp_data + file_header->data_offset;
    bytes_per_pixel = bpp / 8;
    row_size = ((width * bytes_per_pixel + 3) / 4) * 4;
    
    for (row = 0; row < height; row++) {
        src_row = is_bottom_up ? (height - 1 - row) : row;
        row_data = pixel_data + (src_row * row_size);
        dest_y = y + row;
        
        if (dest_y < 0 || dest_y >= screen_height) continue;
        
        for (col = 0; col < width; col++) {
            dest_x = x + col;
            if (dest_x < 0 || dest_x >= screen_width) continue;
            
            pixel_offset = col * bytes_per_pixel;
            b = row_data[pixel_offset + 0];
            g = row_data[pixel_offset + 1];
            r = row_data[pixel_offset + 2];
            // For 32-bit: BGRA format, alpha at offset 3 (ignored)
            
            // Check if pixel is magenta (transparency key)
            if (r == 0xFF && g == 0x00 && b == 0xFF) {
                continue;  // Skip transparent pixels
            }
            
            rgb_pixel = (r << 16) | (g << 8) | b;
            buffer_offset = dest_y * screen_width + dest_x;
            back_buffer[buffer_offset] = rgb_pixel;
        }
    }
}

/**
 * Get BMP dimensions without drawing
 * Returns 1 if valid BMP, 0 otherwise
 */
int bmp_get_dimensions(const unsigned char *bmp_data, int *width, int *height) {
    const BMP_FileHeader *file_header = (const BMP_FileHeader *)bmp_data;
    const BMP_InfoHeader *info_header = (const BMP_InfoHeader *)(bmp_data + sizeof(BMP_FileHeader));
    
    if (file_header->signature != 0x4D42) {
        return 0;  // Not a BMP
    }
    
    *width = info_header->width;
    *height = info_header->height;
    
    if (*height < 0) {
        *height = -*height;
    }
    
    return 1;
}
