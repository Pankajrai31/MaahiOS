# System Programs

## Overview

System programs are special Ring 3 programs loaded as GRUB modules.
Unlike apps (.mex files loaded from disk), these are embedded in the boot image.

## Sysman (src/system/systemprograms/sysman/)
- **GRUB Module Index**: 0
- **PID**: 1 (first user process)
- **Role**: Boot orchestrator — loads all executives and system programs
- **Special**: Uses raw syscalls (no executive path exists yet when it starts)
- **Files**: sysman.c, sysman_entry.s, sysman_linker.ld

### Sysman Boot Sequence
1. Loads Log Executive (module 1)
2. Loads Cell Executive (module 2)
3. Loads Process Executive (module 3)
4. Loads Memory Executive (module 4)
5. Loads Disk Executive (module 5)
6. Loads FS Executive (module 6)
7. Loads GUI Executive (module 7)
8. Loads IO Executive (module 8)
9. Loads WM Executive (module 9)
10. Loads Orbit (module 10)
11. Loads Terminal (module 11)
12. Enters monitoring loop

## Orbit (src/system/systemprograms/orbit/)
- **GRUB Module Index**: 10
- **Role**: Desktop shell — Start menu, taskbar, app launcher
- **Features**: Icon loading, window list in taskbar, app launching via libmex
- **Files**: orbit.c, orbit_entry.s, orbit_linker.ld

## Terminal (src/system/systemprograms/terminal/)
- **GRUB Module Index**: 11
- **Role**: Console terminal — text input/output for console apps
- **Features**: SHM-based stdout from console apps, keyboard input
- **Files**: terminal.c, terminal_entry.s, terminal_linker.ld

## Future System Programs (planned)
- Security System Manager — OS-wide security policy enforcement

## Known Issues
*(Agents add issues here)*
