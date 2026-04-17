# Input Drivers (Keyboard, Mouse)

## Files
- `drivers/keyboard/keyboard.c/.h` — PS/2 keyboard
- `drivers/mouse/mouse.c/.h` — PS/2 mouse

## Keyboard
- PS/2 scancode set 1
- IRQ 1 (remapped to 33)
- Ring buffer for key events
- Registered with device_manager

## Mouse
- PS/2 mouse protocol
- IRQ 12 (remapped to 44)
- Reports: x delta, y delta, button state
- Registered with device_manager

## Data Flow
Driver → device_manager → SYS_DEV_READ → IO Executive → SHM ring → libio → app

## Known Issues
*(Agents add issues here)*
