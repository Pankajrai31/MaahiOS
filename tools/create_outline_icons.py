#!/usr/bin/env python3
"""
Create SVG-style outline icons for MaahiOS
Clean, minimal icons with white outlines on transparent background
"""

import struct
import os

def create_bmp_32bit(width, height, pixels):
    """
    Create a 32-bit BMP file with alpha channel
    pixels: list of (R, G, B, A) tuples, row by row from top to bottom
    """
    # BMP stores rows bottom-up, so reverse
    rows = []
    for y in range(height):
        row = pixels[y * width : (y + 1) * width]
        rows.append(row)
    rows.reverse()
    
    # Flatten to pixel data (BGRA format)
    pixel_data = bytearray()
    for row in rows:
        for r, g, b, a in row:
            pixel_data.extend([b, g, r, a])  # BGRA order
    
    # BMP File Header (14 bytes)
    file_size = 54 + len(pixel_data)
    file_header = struct.pack('<2sIHHI',
        b'BM',           # Signature
        file_size,       # File size
        0,               # Reserved1
        0,               # Reserved2
        54               # Pixel data offset
    )
    
    # BMP Info Header (40 bytes)
    info_header = struct.pack('<IIIHHIIIIII',
        40,              # Header size
        width,           # Width
        height,          # Height (positive = bottom-up)
        1,               # Planes
        32,              # Bits per pixel
        0,               # Compression (0 = none)
        len(pixel_data), # Image size
        2835,            # X pixels per meter
        2835,            # Y pixels per meter
        0,               # Colors used
        0                # Important colors
    )
    
    return file_header + info_header + bytes(pixel_data)


def draw_line(pixels, width, x0, y0, x1, y1, color):
    """Draw a line using Bresenham's algorithm"""
    dx = abs(x1 - x0)
    dy = abs(y1 - y0)
    sx = 1 if x0 < x1 else -1
    sy = 1 if y0 < y1 else -1
    err = dx - dy
    
    while True:
        if 0 <= x0 < width and 0 <= y0 < len(pixels) // width:
            pixels[y0 * width + x0] = color
        if x0 == x1 and y0 == y1:
            break
        e2 = 2 * err
        if e2 > -dy:
            err -= dy
            x0 += sx
        if e2 < dx:
            err += dx
            y0 += sy


def draw_rect(pixels, width, x, y, w, h, color, thickness=1):
    """Draw a rectangle outline"""
    for t in range(thickness):
        # Top
        draw_line(pixels, width, x, y + t, x + w - 1, y + t, color)
        # Bottom
        draw_line(pixels, width, x, y + h - 1 - t, x + w - 1, y + h - 1 - t, color)
        # Left
        draw_line(pixels, width, x + t, y, x + t, y + h - 1, color)
        # Right
        draw_line(pixels, width, x + w - 1 - t, y, x + w - 1 - t, y + h - 1, color)


def draw_filled_rect(pixels, width, x, y, w, h, color):
    """Draw a filled rectangle"""
    for py in range(y, y + h):
        for px in range(x, x + w):
            if 0 <= px < width and 0 <= py < len(pixels) // width:
                pixels[py * width + px] = color


def create_folder_icon_32x32():
    """
    Create a clean folder icon (32x32) - SVG style outline
    White outline on transparent background
    """
    size = 32
    pixels = [(0, 0, 0, 0)] * (size * size)  # Transparent
    
    white = (255, 255, 255, 255)
    
    # Folder shape:
    #   ___________
    #  /   \       |
    # |_____________|
    # |             |
    # |             |
    # |_____________|
    
    # Main body outline (bottom part)
    # Top-left corner at (3, 10), size 26x16
    draw_rect(pixels, size, 3, 10, 26, 16, white, 2)
    
    # Tab on top (folder tab)
    # Small tab from (3, 6) to (12, 10)
    draw_line(pixels, size, 3, 10, 3, 6, white)
    draw_line(pixels, size, 4, 10, 4, 7, white)
    draw_line(pixels, size, 3, 6, 12, 6, white)
    draw_line(pixels, size, 4, 7, 11, 7, white)
    draw_line(pixels, size, 12, 6, 14, 10, white)
    draw_line(pixels, size, 11, 7, 13, 10, white)
    
    return create_bmp_32bit(size, size, pixels)


def create_file_icon_32x32():
    """
    Create a clean file/document icon (32x32) - SVG style outline
    White outline on transparent background
    """
    size = 32
    pixels = [(0, 0, 0, 0)] * (size * size)  # Transparent
    
    white = (255, 255, 255, 255)
    
    # File shape with folded corner:
    #  ________
    # |   ___/ |
    # |  |___| |
    # |        |
    # |  ___   |
    # | |___| |
    # |________|
    
    # Main body (6, 4) to (25, 27) = 20x24
    # Left edge
    draw_line(pixels, size, 6, 4, 6, 27, white)
    draw_line(pixels, size, 7, 4, 7, 27, white)
    # Bottom edge
    draw_line(pixels, size, 6, 26, 25, 26, white)
    draw_line(pixels, size, 6, 27, 25, 27, white)
    # Right edge (below fold)
    draw_line(pixels, size, 24, 10, 24, 27, white)
    draw_line(pixels, size, 25, 10, 25, 27, white)
    # Top edge (before fold)
    draw_line(pixels, size, 6, 4, 18, 4, white)
    draw_line(pixels, size, 6, 5, 18, 5, white)
    
    # Folded corner (diagonal from (18,4) to (24,10))
    draw_line(pixels, size, 18, 4, 24, 10, white)
    draw_line(pixels, size, 18, 5, 25, 10, white)
    draw_line(pixels, size, 19, 4, 25, 10, white)
    
    # Inner fold line (shows the fold)
    draw_line(pixels, size, 18, 4, 18, 10, white)
    draw_line(pixels, size, 18, 10, 24, 10, white)
    
    # Text lines inside (to indicate document content)
    draw_line(pixels, size, 10, 14, 21, 14, white)
    draw_line(pixels, size, 10, 17, 21, 17, white)
    draw_line(pixels, size, 10, 20, 17, 20, white)
    
    return create_bmp_32bit(size, size, pixels)


def create_window_icon_32x32():
    """
    Create a generic window icon (32x32) - SVG style outline
    For use as default window icon
    """
    size = 32
    pixels = [(0, 0, 0, 0)] * (size * size)  # Transparent
    
    white = (255, 255, 255, 255)
    
    # Window shape:
    #  _______________
    # |_[_]_[_]_[_]__|  <- title bar with buttons
    # |              |
    # |              |
    # |______________|
    
    # Main window frame
    draw_rect(pixels, size, 3, 4, 26, 24, white, 2)
    
    # Title bar separator
    draw_line(pixels, size, 3, 10, 28, 10, white)
    draw_line(pixels, size, 3, 11, 28, 11, white)
    
    # Window buttons (3 small squares in title bar)
    draw_rect(pixels, size, 19, 6, 3, 3, white, 1)
    draw_rect(pixels, size, 23, 6, 3, 3, white, 1)
    
    return create_bmp_32bit(size, size, pixels)


def create_folder_icon_16x16():
    """
    Create a small folder icon (16x16) for list view
    """
    size = 16
    pixels = [(0, 0, 0, 0)] * (size * size)  # Transparent
    
    white = (255, 255, 255, 255)
    
    # Compact folder shape
    # Main body
    draw_rect(pixels, size, 1, 5, 14, 9, white, 1)
    
    # Tab
    draw_line(pixels, size, 1, 5, 1, 3, white)
    draw_line(pixels, size, 1, 3, 6, 3, white)
    draw_line(pixels, size, 6, 3, 7, 5, white)
    
    return create_bmp_32bit(size, size, pixels)


def create_file_icon_16x16():
    """
    Create a small file icon (16x16) for list view
    """
    size = 16
    pixels = [(0, 0, 0, 0)] * (size * size)  # Transparent
    
    white = (255, 255, 255, 255)
    
    # Compact file shape with fold
    # Main body
    draw_line(pixels, size, 3, 2, 3, 13, white)  # Left
    draw_line(pixels, size, 3, 13, 12, 13, white)  # Bottom
    draw_line(pixels, size, 12, 6, 12, 13, white)  # Right
    draw_line(pixels, size, 3, 2, 9, 2, white)   # Top
    
    # Folded corner
    draw_line(pixels, size, 9, 2, 12, 6, white)
    draw_line(pixels, size, 9, 2, 9, 6, white)
    draw_line(pixels, size, 9, 6, 12, 6, white)
    
    # Text lines
    draw_line(pixels, size, 5, 8, 10, 8, white)
    draw_line(pixels, size, 5, 10, 9, 10, white)
    
    return create_bmp_32bit(size, size, pixels)


def main():
    # Output directory
    icons_dir = os.path.join(os.path.dirname(__file__), '..', 'src', 'images', 'icons')
    os.makedirs(icons_dir, exist_ok=True)
    
    # Create 32x32 icons
    print("Creating 32x32 folder icon...")
    folder_32 = create_folder_icon_32x32()
    with open(os.path.join(icons_dir, 'folder_32.bmp'), 'wb') as f:
        f.write(folder_32)
    
    print("Creating 32x32 file icon...")
    file_32 = create_file_icon_32x32()
    with open(os.path.join(icons_dir, 'file_32.bmp'), 'wb') as f:
        f.write(file_32)
    
    print("Creating 32x32 window icon...")
    window_32 = create_window_icon_32x32()
    with open(os.path.join(icons_dir, 'window_32.bmp'), 'wb') as f:
        f.write(window_32)
    
    # Create 16x16 icons for list view
    print("Creating 16x16 folder icon...")
    folder_16 = create_folder_icon_16x16()
    with open(os.path.join(icons_dir, 'folder_16.bmp'), 'wb') as f:
        f.write(folder_16)
    
    print("Creating 16x16 file icon...")
    file_16 = create_file_icon_16x16()
    with open(os.path.join(icons_dir, 'file_16.bmp'), 'wb') as f:
        f.write(file_16)
    
    print(f"\nAll icons created in {icons_dir}")
    print("Files created:")
    print("  - folder_32.bmp (32x32 folder icon)")
    print("  - file_32.bmp (32x32 file icon)")
    print("  - window_32.bmp (32x32 generic window icon)")
    print("  - folder_16.bmp (16x16 folder icon for lists)")
    print("  - file_16.bmp (16x16 file icon for lists)")


if __name__ == '__main__':
    main()
