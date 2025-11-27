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
    icons_dir = '../libraries/icons'
    output_file = '../src/orbit/embedded_icons.h'
    
    header = """#ifndef EMBEDDED_ICONS_H
#define EMBEDDED_ICONS_H

#include <stdint.h>

/**
 * Embedded BMP icon data for MaahiOS desktop
 * Generated from libraries/icons/*.bmp files
 */

"""
    
    footer = """#endif // EMBEDDED_ICONS_H
"""
    
    content = header
    
    # Convert each BMP file
    icons = {
        'process.bmp': 'icon_process_bmp',
        'disk.bmp': 'icon_disk_bmp',
        'files.bmp': 'icon_files_bmp',
        'notebook.bmp': 'icon_notebook_bmp'
    }
    
    for filename, array_name in icons.items():
        filepath = os.path.join(icons_dir, filename)
        if os.path.exists(filepath):
            print(f"Converting {filename}...")
            content += bmp_to_c_array(filepath, array_name)
        else:
            print(f"Warning: {filepath} not found")
    
    content += footer
    
    with open(output_file, 'w') as f:
        f.write(content)
    
    print(f"\nGenerated {output_file}")
    print("Icons embedded and ready to use!")

if __name__ == '__main__':
    main()
