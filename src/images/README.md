# Images and Icons

This directory contains all BMP image files used in MaahiOS.

## Directory Structure

```
src/images/
└── icons/              # BMP icon files (source)
    ├── cursor.bmp      # 12x18 arrow cursor with transparency
    ├── file_icon.bmp   # 32x32 file icon
    ├── disk_icon.bmp   # 32x32 disk/drive icon
    ├── notebook_icon.bmp # 32x32 notebook icon
    └── process_icon.bmp  # 32x32 process icon
```

## Workflow

### 1. Create/Edit BMP Files
BMP files in this directory are the **source files**. Edit them with any image editor.

**Requirements:**
- 24-bit uncompressed BMP format
- For cursors/icons with transparency: use magenta (0xFF00FF) as transparent color
- Cursor: 12x18 pixels
- Icons: 32x32 pixels

### 2. Convert to C Headers
After creating or modifying BMP files, convert them to C arrays:

```bash
cd tools/

# Convert cursor
python convert_cursor.py
# Output: src/libgui/cursor_data.h

# Convert desktop icons
python bmp_to_c.py
# Output: src/libgui/embedded_icons.h
```

### 3. Use in Code
Include the generated headers in your C code:

```c
#include "../libgui/cursor_data.h"
#include "../libgui/bmp_renderer.h"

// Draw cursor with transparency
bmp_draw_transparent(x, y, cursor_bmp_data, back_buffer, 
                    screen_width, screen_height);
```

## Tools

- `tools/create_cursor_bmp.py` - Generate cursor.bmp from pattern array
- `tools/convert_cursor.py` - Convert cursor.bmp to cursor_data.h
- `tools/bmp_to_c.py` - Convert all icons to embedded_icons.h
- `tools/create_icons.py` - Generate icon BMP files

## BMP Renderer

The BMP renderer (`src/libgui/bmp_renderer.c`) handles:
- 24-bit BGR to 32-bit RGB conversion
- Bottom-up and top-down BMP formats
- Transparency support (magenta = 0xFF00FF)
- Bounds checking and clipping

## Notes

- **Do not edit** generated `.h` files directly - they will be overwritten
- Always regenerate headers after modifying BMP files
- BMP files use BGR pixel order, framebuffer uses RGB
- All BMPs must be uncompressed (compression = 0)
