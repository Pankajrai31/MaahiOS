#!/usr/bin/env python3
"""
Create 12x18 cursor BMP for MaahiOS (matches current cursor design)
Uses magenta (0xFF00FF) as transparency key
"""

def create_cursor_bmp(filename):
    width = 12
    height = 18
    
    # BMP needs row padding to 4-byte boundary
    # 12 pixels * 3 bytes = 36 bytes (already 4-byte aligned)
    row_size = width * 3
    image_size = row_size * height
    file_size = 54 + image_size
    
    # BMP File Header (14 bytes)
    bmp_header = bytearray([
        0x42, 0x4D,  # 'BM'
        file_size & 0xFF, (file_size >> 8) & 0xFF, 
        (file_size >> 16) & 0xFF, (file_size >> 24) & 0xFF,
        0x00, 0x00,  # Reserved
        0x00, 0x00,  # Reserved
        0x36, 0x00, 0x00, 0x00,  # Offset to pixel data (54 bytes)
    ])
    
    # BMP Info Header (40 bytes)
    info_header = bytearray([
        0x28, 0x00, 0x00, 0x00,  # Header size (40)
        width & 0xFF, (width >> 8) & 0xFF, 0x00, 0x00,  # Width
        height & 0xFF, (height >> 8) & 0xFF, 0x00, 0x00,  # Height
        0x01, 0x00,  # Planes (1)
        0x18, 0x00,  # Bits per pixel (24)
        0x00, 0x00, 0x00, 0x00,  # Compression (none)
        image_size & 0xFF, (image_size >> 8) & 0xFF, 
        (image_size >> 16) & 0xFF, 0x00,  # Image size
        0x00, 0x00, 0x00, 0x00,  # X pixels per meter
        0x00, 0x00, 0x00, 0x00,  # Y pixels per meter
        0x00, 0x00, 0x00, 0x00,  # Colors used
        0x00, 0x00, 0x00, 0x00,  # Important colors
    ])
    
    # Cursor pattern matching uimanager.c design
    # 0 = transparent (magenta), 1 = black, 2 = white
    pattern = [
        [1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],  # Row 0
        [1, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [1, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0, 0],
        [1, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0, 0],
        [1, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0, 0],
        [1, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0, 0],
        [1, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0, 0],
        [1, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0, 0],
        [1, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0, 0],
        [1, 2, 2, 2, 2, 2, 2, 2, 2, 1, 0, 0],
        [1, 2, 2, 2, 2, 2, 1, 1, 1, 1, 1, 0],  # Row 10
        [1, 2, 2, 1, 2, 2, 1, 0, 0, 0, 0, 0],
        [1, 2, 1, 0, 1, 2, 2, 1, 0, 0, 0, 0],
        [1, 1, 0, 0, 1, 2, 2, 1, 0, 0, 0, 0],
        [1, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0],
        [0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0, 0],
        [0, 0, 0, 0, 0, 0, 1, 2, 2, 1, 0, 0],
        [0, 0, 0, 0, 0, 0, 1, 1, 1, 1, 0, 0],  # Row 17
    ]
    
    # Create pixel data (BMP is bottom-up)
    pixels = []
    for y in range(height):
        row = []
        # BMP is bottom-up, so flip Y
        real_y = height - 1 - y
        
        for x in range(width):
            pixel_type = pattern[real_y][x]
            
            if pixel_type == 0:  # Transparent
                row.extend([0xFF, 0x00, 0xFF])  # Magenta (BGR)
            elif pixel_type == 1:  # Black
                row.extend([0x00, 0x00, 0x00])
            else:  # White (2)
                row.extend([0xFF, 0xFF, 0xFF])
        
        pixels.extend(row)
    
    # Write BMP file
    with open(filename, 'wb') as f:
        f.write(bmp_header)
        f.write(info_header)
        f.write(bytearray(pixels))
    
    print(f"Created {filename} ({width}x{height} cursor with transparency)")

if __name__ == '__main__':
    create_cursor_bmp('cursor.bmp')
