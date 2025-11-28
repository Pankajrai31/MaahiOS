# GUI Library and Applications Documentation

## Overview

MaahiOS includes:
- **LibGUI** - User-space graphics library
- **Sysman** - System manager (PID 1)
- **Orbit** - Desktop shell (PID 2)

---

## libgui.h

**Purpose:** Main header for the GUI library.

### Color Constants
```c
#define GUI_COLOR_WHITE         0xFFFFFFFF
#define GUI_COLOR_BLACK         0xFF000000
#define GUI_COLOR_GRAY          0xFFC0C0C0
#define GUI_COLOR_DARK_GRAY     0xFF808080
#define GUI_COLOR_BLUE          0xFF0000FF
#define GUI_COLOR_RED           0xFFFF0000
#define GUI_COLOR_GREEN         0xFF00FF00
#define GUI_COLOR_YELLOW        0xFFFFFF00
#define GUI_COLOR_CYAN          0xFF00FFFF
#define GUI_COLOR_TEAL          0xFF008080
#define GUI_COLOR_NAVY          0xFF000080
```

### Structures

#### GUI_Window
```c
typedef struct {
    int x, y;
    int width, height;
    char title[64];
    uint32_t bg_color;
    uint32_t title_color;
    int visible;
} GUI_Window;
```

#### GUI_Button
```c
typedef struct {
    int x, y;
    int width, height;
    char text[32];
    uint32_t bg_color;
    uint32_t text_color;
    int pressed;
} GUI_Button;
```

### Issues Identified

1. **Alpha in Color Constants**
   - Colors include alpha (0xFF prefix) but BGA strips alpha.
   - **Suggestion:** Use RGB only or document alpha handling.

2. **Missing IconType Enum**
   - `icons.h` references `IconType` but it's not defined in `libgui.h`.

---

## draw.c

**Purpose:** Drawing primitive wrappers around syscalls.

### Functions
```c
void gui_draw_filled_rect(int x, int y, int width, int height, uint32_t color) {
    syscall_fill_rect(x, y, width, height, color);
}

void gui_draw_rect(int x, int y, int width, int height, uint32_t color) {
    syscall_draw_rect(x, y, width, height, color);
}

void gui_draw_text(int x, int y, const char *text, uint32_t fg, uint32_t bg) {
    syscall_print_at(x, y, text, fg, bg);
}

void gui_clear_screen(uint32_t color) {
    syscall_gfx_clear_color(color);
}
```

### Issues Identified

1. **Simple Wrappers Only**
   - Just wraps syscalls without added functionality.
   - **Suggestion:** Add clipping, validation, or buffering.

---

## window.c

**Purpose:** Window management with title bars and 3D effects.

### Window Pool
```c
static GUI_Window window_pool[8];
static int window_count = 0;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `gui_create_window(x, y, w, h, title, bg)` | Create window |
| `gui_draw_window(win)` | Draw complete window |
| `gui_draw_window_title_bar(win)` | Draw title bar with X button |
| `gui_free_window(win)` | Mark window as invisible |

### Window Appearance
- Navy blue title bar (Windows 98 style)
- White title text
- Red X close button
- 3D border effect

### Issues Identified

1. **Fixed Pool Size (Line 9)**
   ```c
   static GUI_Window window_pool[8];
   ```
   - Only 8 windows maximum.
   - **Suggestion:** Use dynamic allocation.

2. **No Click Detection**
   - No way to detect button clicks.
   - **Suggestion:** Add hit testing.

3. **No Z-Order**
   - Windows drawn in creation order.
   - **Suggestion:** Add z-order management.

---

## controls.c

**Purpose:** Button widgets with 3D effects.

### Button Pool
```c
static GUI_Button button_pool[32];
static int button_count = 0;
```

### Key Functions

| Function | Description |
|----------|-------------|
| `gui_create_button(x, y, w, h, text)` | Create button |
| `gui_draw_button(btn)` | Draw button with 3D effect |
| `gui_button(text, x, y)` | Simple button (no structure) |

### Button Appearance
```c
void gui_button(const char *text, int x, int y) {
    // Dark shadow
    gui_draw_filled_rect(x + 3, y + 3, 150, 40, 0x000510);
    // Blue button
    gui_draw_filled_rect(x, y, 150, 40, 0x003060);
    // Highlight
    gui_draw_filled_rect(x, y, 150, 2, 0x0055AA);
    // Cyan text
    syscall_print_at(x + 8, y + 12, text, 0xFF00FFFF, 0);
}
```

### Issues Identified

1. **Fixed Button Size (Line 61-74)**
   - `gui_button` uses hardcoded 150x40 size.
   - **Suggestion:** Take width/height parameters.

2. **No Click Handling**
   - Buttons are purely visual.
   - **Suggestion:** Add event system.

---

## cursor.c / cursor.h

**Purpose:** Mouse cursor rendering.

### Cursor Data
```c
#define CURSOR_WIDTH 10
#define CURSOR_HEIGHT 16

static const unsigned char cursor_bitmap[16] = {
    0x80, // 1.......
    0xC0, // 11......
    0xE0, // 111.....
    // ... arrow shape
};
```

### Key Functions

| Function | Description |
|----------|-------------|
| `gui_draw_cursor()` | Draw cursor at current position |
| `gui_update_cursor()` | Redraw cursor |

### Current Implementation
- Simple white rectangle with black border
- Bitmap cursor data defined but not used

### Issues Identified

1. **Bitmap Not Used (Lines 18-36)**
   - `cursor_bitmap` array defined but `gui_draw_cursor()` draws rectangle.
   - **Suggestion:** Implement bitmap rendering.

2. **No Background Restore**
   - Old cursor position not properly erased.
   - **Suggestion:** Save/restore background.

---

## bmp.c / bmp.h

**Purpose:** BMP image loading and rendering.

### Key Function
```c
void bmp_draw_embedded(int x, int y, const uint8_t *bmp_data) {
    if (!bmp_data) return;
    syscall_draw_bmp(x, y, (uint32_t)bmp_data);
}
```

### Notes
- Actual parsing done in kernel's `bga_draw_bmp()`.
- Only supports 32-bit BMPs with alpha.

### Issues Identified

1. **No User-Side Validation**
   - BMP data passed directly to kernel.
   - **Suggestion:** Add basic format check.

---

## icons.c / icons.h

**Purpose:** Icon rendering with colored squares.

### Icon Types
```c
typedef enum {
    ICON_PROCESS,
    ICON_DISK,
    ICON_FILES,
    ICON_NOTEBOOK
} IconType;  // Note: This should be in libgui.h
```

### Key Functions

| Function | Description |
|----------|-------------|
| `icon_draw(type, x, y)` | Draw 48x48 icon |
| `icon_draw_with_label(type, x, y, label)` | Draw icon with text |

### Icon Colors
| Type | Color |
|------|-------|
| ICON_PROCESS | Blue |
| ICON_DISK | Red |
| ICON_FILES | Yellow |
| ICON_NOTEBOOK | Green |

### Issues Identified

1. **IconType Not in libgui.h**
   - `icons.h` includes `libgui.h` and uses undeclared type.
   - **Suggestion:** Define IconType in libgui.h.

---

## sysman.c / sysman_entry.s

**Purpose:** System Manager - first Ring 3 process (PID 1).

### Entry Point (sysman_entry.s)
```assembly
sysman_main:
    movw $0x23, %ax    # Set Ring 3 data segments
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    call sysman_main_c
    hlt
```

### Main Function (sysman.c)
```c
void sysman_main_c(void) {
    // Clear screen
    gui_clear_screen(0x000000);
    
    // Draw loading box
    gui_draw_filled_rect(404, 364, 480, 80, 0x0000AA);  // Shadow
    gui_draw_filled_rect(400, 360, 480, 80, 0x808080);  // Box
    gui_draw_text(480, 395, "Starting Orbit...", 0x000000, 0);
    gui_draw_text(850, 730, "MaahiOS v0.1", 0xFFFFFF, 0);
    
    // Get and start orbit
    unsigned int orbit_addr = syscall_get_orbit_address();
    int orbit_pid = syscall_create_process(orbit_addr);
    
    // Sysman continues as background process
    gui_clear_screen(0x000000);
    gui_draw_text(10, 10, "Sysman running (PID 1)", 0x00FF00, 0);
    while(1) { __asm__ volatile("hlt"); }
}
```

### Issues Identified

1. **Blocking After Orbit Start (Lines 37-41)**
   - Sysman just halts after starting orbit.
   - **Suggestion:** Implement proper message loop.

2. **No Error Recovery**
   - If orbit fails to start, sysman shows error and halts.

---

## orbit.c / orbit_entry.s / orbit_linker.ld

**Purpose:** Desktop Shell - second Ring 3 process (PID 2).

### Entry Point (orbit_entry.s)
```assembly
_start:
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    call orbit_main_c
hang:
    jmp hang
```

### Linker Script (orbit_linker.ld)
```
ENTRY(_start)
. = 0x00300000;  # Start at 3MB
.text : { *(.text) }
.data : { *(.data) *(.rodata) }
.bss : { *(.bss) }
```

### Main Function (orbit.c)

#### Initialization
```c
void orbit_main_c(void) {
    // Check PIC masks
    unsigned int pic_mask = syscall_get_pic_mask();
    
    // Clear and draw buttons
    gui_clear_screen(0x001020);
    gui_button("Process Manager", 20, 20);
    gui_button("Disk Manager", 20, 90);
    gui_button("File Explorer", 20, 160);
    gui_button("Notebook", 20, 230);
    
    gui_draw_text(300, 40, "MaahiOS Desktop - Move your mouse!", 0xFFFF00, 0);
```

#### Main Loop
```c
    while(1) {
        // Delay
        for (volatile int i = 0; i < 1000; i++);
        
        // Get mouse position
        int x = syscall_mouse_get_x();
        int y = syscall_mouse_get_y();
        
        // Workaround: Poll if IRQ12 stopped
        if (irq == last_irq_count) {
            polls_since_irq++;
            if (polls_since_irq > 2) {
                syscall_poll_mouse();
            }
        }
        
        // Clear and redraw display
        gui_draw_filled_rect(300, 100, 400, 140, 0x001020);
        gui_draw_text(300, 110, "Mouse: X=", 0xFFFFFF, 0);
        gui_draw_text(420, 110, x_buf, 0x00FF00, 0);
        
        // Draw cursor
        syscall_fill_rect(x, y, 12, 18, 0x000000);
        syscall_fill_rect(x+1, y+1, 10, 16, 0xFFFFFF);
    }
}
```

### Helper Function
```c
static void int_to_str(int num, char *buf) {
    // Integer to string conversion
    // Handles negative numbers
}
```

### Issues Identified

1. **Direct Memory Access (Lines 103-105)**
   ```c
   volatile int *kernel_mouse_x = (volatile int *)0x00118040;
   volatile int *kernel_mouse_y = (volatile int *)0x00118044;
   volatile int *kernel_irq_total = (volatile int *)0x0011804C;
   ```
   - Ring 3 code accessing kernel memory.
   - **Issue:** Security violation (works because PAGE_USER set).
   - **Suggestion:** Use syscalls only.

2. **Busy Wait Loop (Line 97)**
   ```c
   for (volatile int i = 0; i < 1000; i++);
   ```
   - Wastes CPU cycles.
   - **Suggestion:** Use proper sleep syscall.

3. **Cursor Flicker (Lines 168-176)**
   - Erases and redraws cursor every iteration.
   - **Suggestion:** Only redraw when position changes.

4. **No Button Click Handling**
   - Buttons are purely decorative.
   - **Suggestion:** Implement mouse click detection.

5. **Hardcoded Screen Coordinates**
   - Many magic numbers for positions.
   - **Suggestion:** Use constants or layout calculations.

6. **QEMU PS/2 Workaround (Lines 112-118)**
   ```c
   // QEMU bug: IRQ12 stops firing randomly
   if (polls_since_irq > 2) {
       syscall_poll_mouse();
   }
   ```
   - Documents QEMU-specific issue.
   - Acceptable workaround but should be removed for hardware.
