# Hardware Drivers Documentation

## Overview

MaahiOS includes drivers for:
- **BGA (Bochs Graphics Adapter)** - Primary graphics driver
- **VGA** - Text mode driver (80x25)
- **VBE** - VESA graphics (fallback)
- **Mode 13h** - Legacy 320x200 graphics
- **PS/2 Mouse** - Mouse input
- **PCI** - PCI configuration space access
- **USB/UHCI** - USB support (incomplete)

---

## bga.c / bga.h

**Purpose:** Bochs Graphics Adapter driver for high-resolution graphics (1024x768x32).

### Key Features
- Direct VBE I/O port programming (0x01CE/0x01CF)
- PCI BAR0 framebuffer detection
- Text rendering with embedded 8x16 font
- BMP image rendering with alpha transparency

### Constants (from bga.h)
```c
#define VBE_DISPI_IOPORT_INDEX   0x01CE
#define VBE_DISPI_IOPORT_DATA    0x01CF
#define VBE_DISPI_INDEX_XRES     0x1
#define VBE_DISPI_INDEX_YRES     0x2
#define VBE_DISPI_INDEX_BPP      0x3
#define VBE_DISPI_INDEX_ENABLE   0x4
```

### Global State
```c
static uint32_t *framebuffer = 0;
static uint16_t screen_width = 0;
static uint16_t screen_height = 0;
static uint16_t screen_bpp = 0;
static int cursor_x = 0;
static int cursor_y = 0;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `bga_is_available()` | Checks if BGA hardware exists |
| `bga_init(w, h, bpp)` | Initialize graphics mode |
| `bga_set_video_mode(w, h, bpp)` | Set resolution |
| `bga_get_framebuffer_addr()` | Get framebuffer from PCI BAR0 |
| `bga_clear(color)` | Fill screen with color |
| `bga_putpixel(x, y, color)` | Draw single pixel |
| `bga_fill_rect(x, y, w, h, color)` | Draw filled rectangle |
| `bga_draw_rect(x, y, w, h, color)` | Draw rectangle outline |
| `bga_print(str, fg, bg)` | Print text at cursor |
| `bga_print_at(x, y, str, fg, bg)` | Print text at position |
| `bga_draw_bmp(x, y, bmp_data)` | Draw BMP image |

### Issues Identified

1. **Incomplete Font Table (Lines 18-146)**
   - Not all ASCII characters are defined in `font_8x16[]`.
   - Missing: `#`, `$`, `%`, `&`, `'`, `(`, `)`, `*`, `+`, `,`, `-`, `.`, `/`, etc.
   - **Suggestion:** Complete the font table or use a generated font file.

2. **Duplicated PCI Read Function (Lines 212-216)**
   ```c
   static uint32_t pci_read_config(...)
   ```
   - Same function exists in `pci.c`.
   - **Suggestion:** Use the shared `pci.c` implementation.

3. **Fallback Framebuffer Address (Line 238)**
   ```c
   return 0xFD000000;  // QEMU Q35 typical address
   ```
   - **Issue:** May not work on all VM configurations.
   - **Suggestion:** Add error logging when using fallback.

4. **Alpha Stripping (Lines 300-301, 311)**
   ```c
   framebuffer[i] = color & 0x00FFFFFF;  // Strip alpha byte
   ```
   - BGA doesn't support alpha, but this could be documented.

5. **BMP Parser Assumptions (Lines 471-482)**
   - Assumes 32-bit BMP with specific header layout.
   - No support for 24-bit or paletted BMPs.
   - **Suggestion:** Add format validation and support more formats.

---

## vga.c

**Purpose:** VGA text mode driver (80x25 characters).

### Constants
```c
#define VGA_ADDR 0xB8000
#define VGA_WIDTH 80
#define VGA_HEIGHT 25
#define VGA_DEFAULT_ATTR 0x07  // Light gray on black
```

### Key Functions

| Function | Description |
|----------|-------------|
| `vga_clear()` | Clear screen |
| `vga_print(str)` | Print string |
| `vga_putchar(c)` | Print character |
| `vga_putint(num)` | Print integer |
| `vga_put_hex(val)` | Print hex value |
| `vga_set_color(fg, bg)` | Set text colors |
| `vga_set_cursor(x, y)` | Set cursor position |
| `vga_print_at(x, y, str)` | Print at position |
| `vga_draw_box(x, y, w, h)` | Draw ASCII box |
| `vga_draw_rect(x, y, w, h, color)` | Draw filled rectangle |

### Issues Identified

1. **No Scrolling (Lines 38-48)**
   - When text reaches bottom, it simply stops at the last line.
   - **Suggestion:** Implement screen scrolling.

2. **Buffer Overflow in `vga_putint` (Lines 76-100)**
   ```c
   char buffer[32];
   ```
   - No bounds checking on buffer index.
   - **Suggestion:** Add bounds checking.

---

## vbe.c

**Purpose:** VESA BIOS Extensions driver for high-resolution graphics.

### Key Functions

| Function | Description |
|----------|-------------|
| `vbe_init()` | Initialize from multiboot VBE info |
| `vbe_get_framebuffer_addr()` | Get framebuffer address |
| `vbe_get_framebuffer_size()` | Get buffer size |
| `vbe_clear(color)` | Clear screen |
| `vbe_putpixel(x, y, color)` | Draw pixel |
| `vbe_print(str, fg, bg)` | Print text |
| `vbe_emergency_text_mode()` | Switch to text mode for panic |

### Issues Identified

1. **Duplicated Font Table**
   - Same `font_8x16[]` array duplicated from `bga.c`.
   - **Suggestion:** Share font data between drivers.

2. **Incomplete Emergency Text Mode (Lines 373-396)**
   - May not work correctly on all hardware.
   - **Suggestion:** Document limitations.

---

## graphics.c

**Purpose:** Legacy Mode 13h graphics (320x200, 256 colors).

### Constants
```c
#define VIDEO_MEMORY 0xA0000
#define SCREEN_WIDTH 320
#define SCREEN_HEIGHT 200
```

### Key Functions

| Function | Description |
|----------|-------------|
| `graphics_mode_13h()` | Switch to Mode 13h |
| `put_pixel(x, y, color)` | Draw pixel |
| `draw_rect_filled(x, y, w, h, color)` | Draw filled rectangle |
| `draw_line(x1, y1, x2, y2, color)` | Draw line |
| `clear_screen(color)` | Clear screen |

### Issues Identified

1. **Limited Line Drawing (Lines 96-110)**
   - Only supports horizontal and vertical lines.
   - **Suggestion:** Implement Bresenham's line algorithm.

2. **Duplicated I/O Functions**
   - `outb()` and `inb()` duplicated again.

---

## mouse.c / mouse.h

**Purpose:** PS/2 mouse driver with IRQ12 interrupt handling.

### Constants
```c
#define PS2_DATA    0x60
#define PS2_STATUS  0x64
#define PS2_CMD     0x64
#define STATUS_OBF  0x01
#define STATUS_IBF  0x02
#define STATUS_AUX  0x20  // Mouse data in output buffer
```

### Global State
```c
volatile int mouse_x = 320;
volatile int mouse_y = 240;
volatile int irq_total = 0;
```

### Ring Buffer
```c
#define MOUSE_BUF_SIZE 128
static mouse_packet_t ring[MOUSE_BUF_SIZE];
static volatile int head = 0;
static volatile int tail = 0;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `mouse_init()` | Initialize PS/2 mouse |
| `mouse_handler()` | IRQ12 handler (atomic) |
| `mouse_get_x()` | Get X position |
| `mouse_get_y()` | Get Y position |
| `mouse_get_buttons()` | Get button state |
| `mouse_drain_buffer()` | Flush PS/2 buffer |

### Issues Identified

1. **Hardcoded Screen Bounds (Lines 153-155)**
   ```c
   if (mouse_x > 1023) mouse_x = 1023;
   if (mouse_y > 767)  mouse_y = 767;
   ```
   - **Suggestion:** Use screen size from BGA driver.

2. **Double EOI (Lines 204-205)**
   ```c
   outb(0xA0, 0x20);
   outb(0x20, 0x20);
   ```
   - EOI sent in handler AND in interrupt stub.
   - **Suggestion:** Send EOI only once (in stub).

3. **Unused `mouse_read()` Function (Lines 209-216)**
   - Static function never called externally.
   - **Suggestion:** Expose via header or remove.

4. **Unimplemented `mouse_set_bounds()` (Lines 249-251)**
   ```c
   void mouse_set_bounds(int width, int height) {
       // Bounds are hardcoded in push_packet for now (1024x768)
   }
   ```
   - **Suggestion:** Implement properly.

---

## pci.c / pci.h

**Purpose:** PCI configuration space access via I/O ports 0xCF8/0xCFC.

### Key Functions

| Function | Description |
|----------|-------------|
| `pci_config_read_byte(bus, slot, func, offset)` | Read byte |
| `pci_config_read_word(bus, slot, func, offset)` | Read word |
| `pci_config_read_dword(bus, slot, func, offset)` | Read dword |
| `pci_config_write_byte(bus, slot, func, offset, value)` | Write byte |
| `pci_config_write_word(bus, slot, func, offset, value)` | Write word |
| `pci_config_write_dword(bus, slot, func, offset, value)` | Write dword |

### Issues Identified

1. **Duplicated I/O Functions (Lines 4-12)**
   - **Suggestion:** Share with other drivers.

---

## usb.c / usb.h / uhci.h

**Purpose:** USB UHCI driver for tablet/mouse support (incomplete).

### Status
- Basic UHCI controller detection implemented
- Port reset and enable implemented
- **Not fully functional** - device enumeration not implemented

### Key Functions

| Function | Description |
|----------|-------------|
| `usb_init()` | Initialize USB subsystem |
| `usb_detect_devices()` | Check for devices |
| `usb_is_tablet_present()` | Check for tablet |
| `usb_tablet_get_report(x, y, buttons)` | Get tablet input |

### Issues Identified

1. **Incomplete Implementation**
   - `usb_tablet_get_report()` just returns last known values.
   - No actual USB transfers implemented.
   - **Suggestion:** Mark as TODO or remove until completed.

2. **PCI Bus Overflow (Line 70)**
   ```c
   for (uint16_t bus = 0; bus < 256; bus++)
   ```
   - Uses 16-bit loop variable but PCI bus is 8-bit.
   - Works but wasteful.

3. **Hardcoded Vendor/Product IDs (Lines 188-189)**
   ```c
   g_tablet_device.vendor_id = 0x0627;  // QEMU tablet vendor ID
   g_tablet_device.product_id = 0x0001;
   ```
   - **Suggestion:** Get from actual device enumeration.
