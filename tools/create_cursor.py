#!/usr/bin/env python3
"""
Create a simple 16x16 cursor BMP for MaahiOS
"""

def create_cursor_bmp(filename):
    # BMP header for 16x16 24-bit image
    # File size = 54 (header) + 768 (16*16*3 bytes)
    file_size = 822  # 54 + 768
    
    # BMP File Header (14 bytes)
    bmp_header = bytearray([
        0x42, 0x4D,  # 'BM'
        file_size & 0xFF, (file_size >> 8) & 0xFF, (file_size >> 16) & 0xFF, (file_size >> 24) & 0xFF,  # File size
        0x00, 0x00,  # Reserved
        0x00, 0x00,  # Reserved
        0x36, 0x00, 0x00, 0x00,  # Offset to pixel data (54 bytes)
    ])
    
    # BMP Info Header (40 bytes)
    info_header = bytearray([
        0x28, 0x00, 0x00, 0x00,  # Header size (40)
        0x10, 0x00, 0x00, 0x00,  # Width (16)
        0x10, 0x00, 0x00, 0x00,  # Height (16)
        0x01, 0x00,  # Planes (1)
        0x18, 0x00,  # Bits per pixel (24)
        0x00, 0x00, 0x00, 0x00,  # Compression (none)
        0x00, 0x03, 0x00, 0x00,  # Image size (can be 0 for uncompressed)
        0x00, 0x00, 0x00, 0x00,  # X pixels per meter
        0x00, 0x00, 0x00, 0x00,  # Y pixels per meter
        0x00, 0x00, 0x00, 0x00,  # Colors used
        0x00, 0x00, 0x00, 0x00,  # Important colors
    ])
    
    # Create 16x16 pixel data (white arrow cursor on black background)
    # BMP is bottom-to-top, so row 0 is actually the bottom
    pixels = []
    for y in range(16):
        row = []
        for x in range(16):
            # Flip y coordinate (BMP is bottom-up)
            real_y = 15 - y
            
            # Draw arrow cursor shape (white on black)
            is_cursor = False
            
            # Vertical line (left edge)
            if x == 0 and real_y < 12:
                is_cursor = True
            # Horizontal line (top edge)
            elif real_y == 0 and x < 8:
                is_cursor = True
            # Diagonal fill
            elif x > 0 and x < 8 and real_y > 0 and real_y <= x:
                is_cursor = True
                
            if is_cursor:
                row.extend([0xFF, 0xFF, 0xFF])  # White (BGR format)
            else:
                row.extend([0x00, 0x00, 0x00])  # Black
        
        pixels.extend(row)
    
    # Write BMP file
    with open(filename, 'wb') as f:
        f.write(bmp_header)
        f.write(info_header)
        f.write(bytearray(pixels))
    
    print(f"Created {filename} (16x16 cursor)")

if __name__ == '__main__':
    create_cursor_bmp('cursor_icon.bmp')
