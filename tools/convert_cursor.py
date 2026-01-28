#!/usr/bin/env python3
"""
Convert cursor.bmp to C header file for MaahiOS
Reads from: src/images/icons/cursor.bmp
Outputs to: src/libgui/cursor_data.h
"""

import os

def bmp_to_c_array(bmp_filename, array_name):
    """Convert BMP file to C byte array"""
    with open(bmp_filename, 'rb') as f:
        bmp_data = f.read()
    
    output = f"const unsigned char {array_name}[{len(bmp_data)}] = {{\n    "
    
    for i, byte in enumerate(bmp_data):
        output += f"0x{byte:02X}"
        if i < len(bmp_data) - 1:
            output += ","
            if (i + 1) % 12 == 0:
                output += "\n    "
            else:
                output += " "
    
    output += "\n};\n"
    return output

def main():
    # Input: src/images/icons/cursor.bmp
    cursor_bmp = '../src/images/icons/cursor.bmp'
    # Output: src/libgui/cursor_data.h
    output_file = '../src/libgui/cursor_data.h'
    
    if not os.path.exists(cursor_bmp):
        print(f"Error: {cursor_bmp} not found!")
        print("Run create_cursor_bmp.py first to generate cursor.bmp")
        return
    
    header = """#ifndef CURSOR_DATA_H
#define CURSOR_DATA_H

#include <stdint.h>

/**
 * Cursor BMP data (12x18 pixels with transparency)
 * Generated from src/images/icons/cursor.bmp
 * Magenta (0xFF00FF) = transparent
 */

"""
    
    footer = """
#endif // CURSOR_DATA_H
"""
    
    content = header
    content += bmp_to_c_array(cursor_bmp, 'cursor_bmp_data')
    content += footer
    
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"✓ Generated {output_file}")
    print(f"✓ Cursor data embedded from {cursor_bmp}")

if __name__ == '__main__':
    main()
