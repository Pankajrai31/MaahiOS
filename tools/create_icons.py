#!/usr/bin/env python3
"""
Simple BMP Icon Creator for MaahiOS
Creates 32x32 pixel BMP files for desktop icons
"""

import struct

def create_bmp(filename, width, height, pixels):
    """
    Create a simple 32-bit BMP file
    pixels: list of (R, G, B) tuples, row by row, bottom to top
    """
    # BMP Header (14 bytes)
    file_size = 54 + (width * height * 4)  # Header + pixel data
    bmp_header = struct.pack('<2sIHHI', 
        b'BM',           # Signature
        file_size,       # File size
        0,               # Reserved
        0,               # Reserved
        54               # Pixel data offset
    )
    
    # DIB Header (40 bytes - BITMAPINFOHEADER)
    dib_header = struct.pack('<IiiHHIIiiII',
        40,              # DIB header size
        width,           # Width
        height,          # Height (positive = bottom-up)
        1,               # Color planes
        32,              # Bits per pixel (RGBA)
        0,               # Compression (none)
        width * height * 4,  # Image size
        2835,            # Horizontal resolution (pixels/meter)
        2835,            # Vertical resolution
        0,               # Colors in palette
        0                # Important colors
    )
    
    # Write BMP file
    with open(filename, 'wb') as f:
        f.write(bmp_header)
        f.write(dib_header)
        
        # Write pixels (BMP stores bottom-to-top, BGRA format)
        for (r, g, b) in pixels:
            # Make black (0, 0, 0) transparent, everything else opaque
            alpha = 0 if (r == 0 and g == 0 and b == 0) else 255
            f.write(struct.pack('BBBB', b, g, r, alpha))  # BGRA
    
    print(f"Created {filename}")


# Process Manager Icon (32x32) - Blue square
def create_process_icon():
    width, height = 32, 32
    pixels = []
    
    for y in range(height):
        for x in range(width):
            # Draw a simple blue square with border
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))  # White border
                else:
                    pixels.append((0, 0, 255))      # Blue fill
            else:
                pixels.append((0, 0, 0))            # Black (transparent)
    
    create_bmp('../libraries/icons/process.bmp', width, height, pixels)


# Disk Manager Icon (32x32) - Red square
def create_disk_icon():
    width, height = 32, 32
    pixels = []
    
    for y in range(height):
        for x in range(width):
            # Draw a simple red square with border
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))  # White border
                else:
                    pixels.append((255, 0, 0))      # Red fill
            else:
                pixels.append((0, 0, 0))            # Black (transparent)
    
    create_bmp('../libraries/icons/disk.bmp', width, height, pixels)


# File Explorer Icon (32x32) - Yellow square
def create_files_icon():
    width, height = 32, 32
    pixels = []
    
    for y in range(height):
        for x in range(width):
            # Draw a simple yellow square with border
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))  # White border
                else:
                    pixels.append((255, 255, 0))    # Yellow fill
            else:
                pixels.append((0, 0, 0))            # Black (transparent)
    
    create_bmp('../libraries/icons/files.bmp', width, height, pixels)


# Notebook Icon (32x32) - Green square
def create_notebook_icon():
    width, height = 32, 32
    pixels = []
    
    for y in range(height):
        for x in range(width):
            # Draw a simple green square with border
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))  # White border
                else:
                    pixels.append((0, 255, 0))      # Green fill
            else:
                pixels.append((0, 0, 0))            # Black (transparent)
    
    create_bmp('../libraries/icons/notebook.bmp', width, height, pixels)


if __name__ == '__main__':
    print("Creating MaahiOS Icons...")
    create_process_icon()
    create_disk_icon()
    create_files_icon()
    create_notebook_icon()
    print("Done! Icons created in libraries/icons/")
