# Display Drivers

## Files
- `drivers/display/display.c/.h` — Display abstraction layer
- `drivers/display/bga.c/.h` — Bochs Graphics Adapter
- `drivers/display/vbe.c/.h` — VESA BIOS Extensions

## Purpose
Manages the graphical framebuffer. Supports BGA (QEMU/Bochs) and 
VBE (real hardware). Provides a linear framebuffer for pixel-level drawing.

## Resolution
- Default: 1024x768x32bpp (configurable via BGA registers)

## Device Interface
Registered with device_manager as display device.
- `ioctl(DISPLAY_FLIP)` — flip the framebuffer (called by gui_flip())

## Known Issues
*(Agents add issues here)*
