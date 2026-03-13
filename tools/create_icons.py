#!/usr/bin/env python3
"""
Simple BMP Icon Creator for MaahiOS
Creates 32x32 pixel BMP files for desktop icons.
Output directory: ../src/images/icons/
"""

import struct
import os

OUTPUT_DIR = os.path.join(os.path.dirname(__file__), '..', 'src', 'images', 'icons')

def create_bmp(filename, width, height, pixels):
    """
    Create a simple 32-bit BMP file
    pixels: list of (R, G, B) tuples, row by row, bottom to top
    """
    os.makedirs(OUTPUT_DIR, exist_ok=True)
    filepath = os.path.join(OUTPUT_DIR, filename)

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
    with open(filepath, 'wb') as f:
        f.write(bmp_header)
        f.write(dib_header)
        
        # Write pixels (BMP stores bottom-to-top, BGRA format)
        for (r, g, b) in pixels:
            # Make black (0, 0, 0) transparent, everything else opaque
            alpha = 0 if (r == 0 and g == 0 and b == 0) else 255
            f.write(struct.pack('BBBB', b, g, r, alpha))  # BGRA
    
    print(f"  Created {filepath}")


def create_terminal_icon():
    """Terminal icon — dark prompt window with '>_' cursor."""
    W, H = 32, 32
    pixels = []
    # Colors
    FRAME = (100, 120, 180)     # Blueish frame
    TITLE = (60, 80, 140)       # Darker titlebar
    BG    = (30, 30, 50)        # Dark terminal background
    TXT   = (0, 220, 120)      # Green terminal text
    BK    = (0, 0, 0)          # Black = transparent

    for y in range(H):
        for x in range(W):
            # Outer margin (transparent)
            if x < 3 or x >= 29 or y < 3 or y >= 29:
                pixels.append(BK)
            # Frame border
            elif x == 3 or x == 28 or y == 3 or y == 28:
                pixels.append(FRAME)
            # Titlebar (row 4-7)
            elif 4 <= y <= 7:
                # Close/min/max dots
                if y == 5 or y == 6:
                    if 5 <= x <= 6:   pixels.append((220, 80, 80))   # Red close
                    elif 8 <= x <= 9: pixels.append((220, 200, 60))  # Yellow min
                    elif 11 <= x <= 12: pixels.append((80, 200, 80)) # Green max
                    else: pixels.append(TITLE)
                else:
                    pixels.append(TITLE)
            # Terminal body
            elif 8 <= y <= 27 and 4 <= x <= 27:
                # Draw ">_" prompt
                # '>' at row 12-18, col 7-12
                if y == 14 and 7 <= x <= 10: pixels.append(TXT)    # Top of >
                elif y == 16 and 7 <= x <= 12: pixels.append(TXT)  # Middle of >
                elif y == 18 and 7 <= x <= 10: pixels.append(TXT)  # Bottom of >
                # '_' cursor blinking at row 18, col 14-19
                elif y == 18 and 14 <= x <= 19: pixels.append(TXT)
                else: pixels.append(BG)
            else:
                pixels.append(BK)

    create_bmp('TERMINAL.BMP', W, H, pixels)


def create_hellogui_icon():
    """HelloGUI icon — a small window with 'Hi' inside."""
    W, H = 32, 32
    pixels = []
    FRAME = (43, 91, 181)       # Accent blue
    TITLE = (43, 91, 181)
    WIN_BG = (240, 242, 248)    # Light chrome
    TXT   = (26, 26, 46)        # Dark text
    BK    = (0, 0, 0)

    for y in range(H):
        for x in range(W):
            if x < 4 or x >= 28 or y < 5 or y >= 27:
                pixels.append(BK)
            elif x == 4 or x == 27 or y == 5 or y == 26:
                pixels.append(FRAME)
            elif 6 <= y <= 9 and 5 <= x <= 26:
                # Titlebar with close button
                if y == 7 and 23 <= x <= 25: pixels.append((220, 80, 80))
                else: pixels.append(TITLE)
            elif 10 <= y <= 25 and 5 <= x <= 26:
                # Content — draw "Hi" text
                # 'H' at col 9-13, row 14-20
                if 14 <= y <= 20 and (x == 9 or x == 13):
                    pixels.append(TXT)
                elif y == 17 and 10 <= x <= 12:
                    pixels.append(TXT)
                # 'i' at col 16-16, row 14-20 with dot at 12
                elif y == 12 and x == 17: pixels.append(TXT)
                elif 14 <= y <= 20 and x == 17: pixels.append(TXT)
                else: pixels.append(WIN_BG)
            else:
                pixels.append(BK)

    create_bmp('HELLOGUI.BMP', W, H, pixels)


def create_default_icon():
    """Default/fallback icon — simple gear/cog shape."""
    W, H = 32, 32
    pixels = []
    COG = (150, 160, 190)
    BK = (0, 0, 0)

    cx, cy = 16, 16
    for y in range(H):
        for x in range(W):
            dx = x - cx
            dy = y - cy
            dist_sq = dx*dx + dy*dy
            # Outer ring: radius 10-12
            if 81 <= dist_sq <= 144:
                # Teeth at 4 compass points + diagonals
                angle_region = False
                if abs(dx) <= 2: angle_region = True   # Top/bottom teeth
                if abs(dy) <= 2: angle_region = True   # Left/right teeth
                if abs(abs(dx) - abs(dy)) <= 2: angle_region = True  # Diagonals
                if dist_sq <= 100 or angle_region:
                    pixels.append(COG)
                else:
                    pixels.append(BK)
            # Inner circle: radius 4-5
            elif 16 <= dist_sq <= 36:
                pixels.append(COG)
            # Center hole
            elif dist_sq < 9:
                pixels.append(BK)
            # Middle ring fill
            elif 36 < dist_sq < 81:
                pixels.append(COG)
            else:
                pixels.append(BK)

    create_bmp('DEFAULT.BMP', W, H, pixels)


# Keep original icons for reference
def create_process_icon():
    width, height = 32, 32
    pixels = []
    for y in range(height):
        for x in range(width):
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))
                else:
                    pixels.append((0, 0, 255))
            else:
                pixels.append((0, 0, 0))
    create_bmp('process_icon.bmp', width, height, pixels)


def create_disk_icon():
    width, height = 32, 32
    pixels = []
    for y in range(height):
        for x in range(width):
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))
                else:
                    pixels.append((255, 0, 0))
            else:
                pixels.append((0, 0, 0))
    create_bmp('disk_icon.bmp', width, height, pixels)


def create_files_icon():
    width, height = 32, 32
    pixels = []
    for y in range(height):
        for x in range(width):
            if (6 <= y < 10) and (6 <= x < 14):
                if x == 6 or x == 13 or y == 6:
                    pixels.append((139, 69, 19))
                else:
                    pixels.append((255, 165, 0))
            elif (10 <= y < 26) and (6 <= x < 26):
                if x == 6 or x == 25 or y == 10 or y == 25:
                    pixels.append((139, 69, 19))
                else:
                    pixels.append((255, 215, 0))
            else:
                pixels.append((0, 0, 0))
    create_bmp('file_icon.bmp', width, height, pixels)


def create_notebook_icon():
    width, height = 32, 32
    pixels = []
    for y in range(height):
        for x in range(width):
            if (8 <= x < 24) and (8 <= y < 24):
                if x == 8 or x == 23 or y == 8 or y == 23:
                    pixels.append((255, 255, 255))
                else:
                    pixels.append((0, 255, 0))
            else:
                pixels.append((0, 0, 0))
    create_bmp('notebook_icon.bmp', width, height, pixels)


def create_procexp_icon():
    """Process Explorer icon — monitor/chart with teal accent, matching theme."""
    W, H = 32, 32
    pixels = []
    # Colors from theme
    FRAME = (43, 91, 181)       # THEME_ACCENT blue
    TITLE = (27, 63, 139)       # Darker blue titlebar
    BG    = (255, 255, 255)     # White content
    TEAL  = (30, 138, 101)      # THEME_TEAL
    BAR1  = (43, 91, 181)       # Blue bar
    BAR2  = (40, 167, 69)       # Green bar (THEME_SUCCESS)
    BAR3  = (232, 163, 23)      # Yellow bar (THEME_WARNING)
    GRID  = (216, 219, 232)     # Chrome grid lines
    BK    = (0, 0, 0)

    for y in range(H):
        for x in range(W):
            # Outer margin (transparent)
            if x < 3 or x >= 29 or y < 3 or y >= 29:
                pixels.append(BK)
            # Frame border
            elif x == 3 or x == 28 or y == 3 or y == 28:
                pixels.append(FRAME)
            # Titlebar (row 4-7)
            elif 4 <= y <= 7 and 4 <= x <= 27:
                # Window dots
                if y == 5 or y == 6:
                    if 5 <= x <= 6:   pixels.append((220, 80, 80))
                    elif 8 <= x <= 9: pixels.append((220, 200, 60))
                    elif 11 <= x <= 12: pixels.append((80, 200, 80))
                    else: pixels.append(TITLE)
                else:
                    pixels.append(TITLE)
            # Content area (row 8-27, col 4-27)
            elif 8 <= y <= 27 and 4 <= x <= 27:
                # Grid lines for chart effect
                is_grid = (y == 14 or y == 20) and 5 <= x <= 26
                is_vgrid = (x == 10 or x == 16 or x == 22) and 9 <= y <= 26

                # Bar chart at bottom
                bar_y_start_1 = 16  # Tall blue bar (col 6-8)
                bar_y_start_2 = 19  # Medium green bar (col 12-14)
                bar_y_start_3 = 22  # Short yellow bar (col 18-20)
                bar_y_start_4 = 14  # Tallest teal bar (col 24-26)

                if 6 <= x <= 8 and bar_y_start_1 <= y <= 26:
                    pixels.append(BAR1)
                elif 12 <= x <= 14 and bar_y_start_2 <= y <= 26:
                    pixels.append(BAR2)
                elif 18 <= x <= 20 and bar_y_start_3 <= y <= 26:
                    pixels.append(BAR3)
                elif 24 <= x <= 26 and bar_y_start_4 <= y <= 26:
                    pixels.append(TEAL)
                # Header row indicator
                elif y == 9 and 5 <= x <= 26:
                    pixels.append(GRID)
                elif is_grid or is_vgrid:
                    pixels.append(GRID)
                else:
                    pixels.append(BG)
            else:
                pixels.append(BK)

    create_bmp('PROCEXP.BMP', W, H, pixels)


def create_diskexp_icon():
    """Disk Explorer icon — hard drive with partition bars, themed."""
    W, H = 32, 32
    pixels = []
    FRAME = (43, 91, 181)       # THEME_ACCENT blue
    BODY  = (216, 219, 232)     # Chrome body
    DARK  = (148, 152, 172)     # Dark chrome edge
    LIGHT = (244, 245, 250)     # Light chrome highlight
    BLUE  = (43, 91, 181)       # Primary partition
    GREEN = (40, 167, 69)       # Data partition
    TEAL  = (30, 138, 101)      # Status indicator
    PLATTER = (176, 180, 198)   # Disk platter area
    BK    = (0, 0, 0)

    for y in range(H):
        for x in range(W):
            # Outer margin
            if x < 3 or x >= 29 or y < 4 or y >= 28:
                pixels.append(BK)
            # Top rounded edge of drive
            elif y == 4 and (x < 5 or x >= 27):
                pixels.append(BK)
            elif y == 4 and 5 <= x <= 26:
                pixels.append(DARK)
            # Drive body border
            elif (x == 3 and 5 <= y <= 27) or (x == 28 and 5 <= y <= 27):
                pixels.append(DARK)
            elif y == 27 and 4 <= x <= 27:
                pixels.append(DARK)
            # Top highlight line
            elif y == 5 and 4 <= x <= 27:
                pixels.append(LIGHT)
            # Left highlight
            elif x == 4 and 5 <= y <= 26:
                pixels.append(LIGHT)
            # Bottom/right shadow
            elif (x == 27 and 5 <= y <= 26):
                pixels.append(DARK)
            elif y == 26 and 4 <= x <= 27:
                pixels.append(DARK)
            # Platter area (round disk impression)
            elif 6 <= y <= 18 and 5 <= x <= 26:
                cx, cy = 15, 12
                dx, dy = x - cx, y - cy
                dist_sq = dx*dx + dy*dy
                if dist_sq <= 49:  # r=7 outer platter
                    if dist_sq <= 4:  # r=2 center hub
                        pixels.append(DARK)
                    elif dist_sq <= 9:  # r=3 center ring
                        pixels.append(LIGHT)
                    else:
                        pixels.append(PLATTER)
                else:
                    pixels.append(BODY)
            # Partition bar section at bottom
            elif 20 <= y <= 24 and 5 <= x <= 26:
                bar_width = 22  # total width for bars
                # 3 colored segments showing partitions
                if 6 <= x <= 13:       # Primary (blue)
                    if y == 20 or y == 24:
                        pixels.append(DARK)
                    else:
                        pixels.append(BLUE)
                elif 14 <= x <= 19:    # Data (green)
                    if y == 20 or y == 24:
                        pixels.append(DARK)
                    else:
                        pixels.append(GREEN)
                elif 20 <= x <= 25:    # System (teal)
                    if y == 20 or y == 24:
                        pixels.append(DARK)
                    else:
                        pixels.append(TEAL)
                else:
                    pixels.append(BODY)
            # LED indicator
            elif y == 19 and 5 <= x <= 26:
                pixels.append(BODY)
            # Rest of body
            elif 5 <= y <= 25 and 5 <= x <= 26:
                pixels.append(BODY)
            else:
                pixels.append(BK)

    create_bmp('DISKEXP.BMP', W, H, pixels)


def create_wordwrite_icon():
    """WordWrite icon — document with lines of text and a pen/pencil, themed."""
    W, H = 32, 32
    pixels = []
    FRAME = (43, 91, 181)       # THEME_ACCENT blue
    DOC_BG = (255, 255, 255)    # White document
    DOC_BORDER = (148, 152, 172)  # Dark chrome border
    LINE_COLOR = (26, 26, 46)   # THEME_TEXT — text lines
    LINE_FAINT = (180, 183, 198)  # Faint ruled line
    PEN_BODY = (43, 91, 181)    # Accent blue pen body
    PEN_TIP = (232, 163, 23)    # Warning yellow pen tip
    FOLD = (216, 219, 232)      # Chrome fold corner
    BK = (0, 0, 0)

    for y in range(H):
        for x in range(W):
            # Document area: x=3..22, y=2..28 with folded corner at top-right
            in_fold = (x >= 18 and y <= 7) and (x + y < 25) is False
            fold_region = x >= 18 and y <= 7 and (x - 18 + 2) + y <= 7

            if x < 3 or x >= 23 or y < 2 or y >= 29:
                # Outside document — check if on the pen
                # Pen diagonal from (22,14) to (28,26)
                pen_dx = x - 21
                pen_dy = y - 14
                # Pen runs diagonally, ~45 degrees
                on_pen = (pen_dx >= 0 and pen_dy >= 0 and
                         abs(pen_dx - pen_dy) <= 1 and
                         pen_dx <= 8 and pen_dy <= 12)
                if on_pen and pen_dy <= 9:
                    pixels.append(PEN_BODY)
                elif on_pen and pen_dy <= 12:
                    pixels.append(PEN_TIP)
                else:
                    pixels.append(BK)
            # Fold corner triangle (top-right)
            elif fold_region:
                pixels.append(FOLD)
            # Document border
            elif x == 3 or (x == 22 and not fold_region) or y == 2 or y == 28:
                pixels.append(DOC_BORDER)
            elif y == 2 and x >= 18:
                pixels.append(DOC_BORDER)
            # Text lines in the document
            elif 4 <= x <= 20 and 5 <= y <= 25:
                # Ruled lines every 4 rows
                is_rule = (y % 4 == 0) and 5 <= x <= 20
                # Simulated text blocks
                has_text = False
                if y == 6 and 5 <= x <= 18: has_text = True
                if y == 10 and 5 <= x <= 16: has_text = True
                if y == 14 and 5 <= x <= 19: has_text = True
                if y == 18 and 5 <= x <= 14: has_text = True
                if y == 22 and 5 <= x <= 17: has_text = True

                if has_text:
                    # Draw text as short dashes with gaps
                    segment = (x - 5) % 4
                    if segment < 3:
                        pixels.append(LINE_COLOR)
                    else:
                        pixels.append(DOC_BG)
                elif is_rule:
                    pixels.append(LINE_FAINT)
                else:
                    pixels.append(DOC_BG)
            else:
                pixels.append(DOC_BG)

    create_bmp('WORDWRT.BMP', W, H, pixels)


def create_logexp_icon():
    """Log Explorer icon — scrolling list/terminal with magnifying glass."""
    W, H = 32, 32
    pixels = []
    FRAME = (43, 91, 181)         # THEME_ACCENT blue
    BG = (255, 255, 255)          # White document bg
    BORDER = (148, 152, 172)      # Dark chrome border
    LINE_INFO = (30, 138, 101)    # THEME_TEAL — info lines
    LINE_WARN = (232, 163, 23)    # Warning yellow
    LINE_ERR = (200, 50, 50)      # Red error
    LINE_DBG = (148, 152, 172)    # Gray debug
    GLASS_RING = (43, 91, 181)    # Accent blue lens ring
    GLASS_BG = (208, 220, 248)    # Light blue lens interior
    BK = (0, 0, 0)

    for y in range(H):
        for x in range(W):
            # Document area: x=2..21, y=2..28
            in_doc = (2 <= x <= 21 and 2 <= y <= 28)

            # Magnifying glass: circle center at (24, 22), radius 6
            # Handle from (27, 25) to (30, 28)
            gcx, gcy, gr = 24, 20, 6
            dx = x - gcx
            dy = y - gcy
            dist_sq = dx * dx + dy * dy
            in_glass_ring = ((gr - 1) * (gr - 1) <= dist_sq <= gr * gr)
            in_glass_fill = (dist_sq < (gr - 1) * (gr - 1))
            # handle: diagonal line from bottom-right of circle
            in_handle = (28 <= x <= 30 and 24 <= y <= 28 and
                         abs((x - 28) - (y - 24)) <= 1)

            if in_glass_ring:
                pixels.append(GLASS_RING)
            elif in_glass_fill:
                pixels.append(GLASS_BG)
            elif in_handle:
                pixels.append(BORDER)
            elif not in_doc:
                pixels.append(BK)
            # Document border
            elif x == 2 or x == 21 or y == 2 or y == 28:
                pixels.append(BORDER)
            # Log lines inside document
            elif 4 <= x <= 19 and 4 <= y <= 26:
                row = (y - 4) // 2
                is_text_row = ((y - 4) % 2 == 0)
                if is_text_row and x <= 18:
                    # Different colored lines for different log levels
                    if row == 0 and 4 <= x <= 16:
                        pixels.append(LINE_INFO)
                    elif row == 1 and 4 <= x <= 14:
                        pixels.append(LINE_DBG)
                    elif row == 2 and 4 <= x <= 17:
                        pixels.append(LINE_INFO)
                    elif row == 3 and 4 <= x <= 12:
                        pixels.append(LINE_WARN)
                    elif row == 4 and 4 <= x <= 18:
                        pixels.append(LINE_INFO)
                    elif row == 5 and 4 <= x <= 15:
                        pixels.append(LINE_ERR)
                    elif row == 6 and 4 <= x <= 16:
                        pixels.append(LINE_DBG)
                    elif row == 7 and 4 <= x <= 13:
                        pixels.append(LINE_INFO)
                    elif row == 8 and 4 <= x <= 17:
                        pixels.append(LINE_INFO)
                    elif row == 9 and 4 <= x <= 14:
                        pixels.append(LINE_WARN)
                    elif row == 10 and 4 <= x <= 16:
                        pixels.append(LINE_ERR)
                    elif row == 11 and 4 <= x <= 18:
                        pixels.append(LINE_DBG)
                    else:
                        pixels.append(BG)
                else:
                    pixels.append(BG)
            else:
                pixels.append(BG)

    create_bmp('LOGEXP.BMP', W, H, pixels)


if __name__ == '__main__':
    print("Creating MaahiOS Icons...")
    create_terminal_icon()
    create_hellogui_icon()
    create_default_icon()
    create_process_icon()
    create_disk_icon()
    create_files_icon()
    create_notebook_icon()
    create_procexp_icon()
    create_diskexp_icon()
    create_wordwrite_icon()
    create_logexp_icon()
    print(f"Done! Icons created in {OUTPUT_DIR}")
