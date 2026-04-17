# PCI and RTC Drivers

## PCI Bus (drivers/pci/)
- `pci.c/.h`
- Enumerates PCI devices at boot
- Used to discover: display adapter, NIC (E1000), IDE controller
- Configuration space access via I/O ports 0xCF8/0xCFC

## RTC (drivers/rtc/)
- `rtc.c/.h`
- Real-Time Clock — provides current date and time
- CMOS registers via ports 0x70/0x71
- Used by time_manager for wall-clock time

## VGA (drivers/vga/)
- `vga.c/.h`
- VGA text mode (80x25)
- Used only during early boot before display driver takes over
- Writes to video memory at 0xB8000

## Known Issues
*(Agents add issues here)*
