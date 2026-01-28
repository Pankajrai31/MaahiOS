#!/usr/bin/env python3
"""
Convert BMP files to C byte arrays for embedding in MaahiOS
"""

import os

def bmp_to_c_array(bmp_filename, array_name):
    """Convert BMP file to C byte array"""
    with open(bmp_filename, 'rb') as f:
        bmp_data = f.read()
    
    output = f"// {array_name} - Embedded BMP data\n"
    output += f"const uint8_t {array_name}[{len(bmp_data)}] = {{\n    "
    
    for i, byte in enumerate(bmp_data):
        output += f"0x{byte:02X}"
        if i < len(bmp_data) - 1:
            output += ","
            if (i + 1) % 12 == 0:
                output += "\n    "
            else:
                output += " "
    
    output += "\n};\n\n"
    return output

def main():
    # Icons are now in src/images/icons directory
    icons_dir = '../src/images/icons'
    # Output C header to src/libgui
    output_dir = '../src/libgui'
    os.makedirs(output_dir, exist_ok=True)
    output_file = os.path.join(output_dir, 'embedded_icons.h')
    
    header = """#ifndef EMBEDDED_ICONS_H
#define EMBEDDED_ICONS_H

#include <stdint.h>

/**
 * Embedded BMP icon data for MaahiOS desktop
 * Generated from src/images/icons/*.bmp files
 */

"""
    
    footer = """#endif // EMBEDDED_ICONS_H
"""
    
    content = header
    
    # Convert each BMP file
    icons = {
        'file_icon.bmp': 'icon_file_bmp',
        'process_icon.bmp': 'icon_process_bmp',
        'disk_icon.bmp': 'icon_disk_bmp',
        'notebook_icon.bmp': 'icon_notebook_bmp'
    }
    
    for filename, array_name in icons.items():
        filepath = os.path.join(icons_dir, filename)
        if os.path.exists(filepath):
            print(f"Converting {filename}...")
            content += bmp_to_c_array(filepath, array_name)
        else:
            print(f"Warning: {filepath} not found, skipping...")
    
    content += footer
    
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"\nGenerated {output_file}")
    print("Icons embedded and ready to use!")

if __name__ == '__main__':
    main()
