# Storage Drivers

## Files
- `drivers/drive/ata/ata.c/.h` — ATA/IDE disk controller
- `drivers/drive/disk/disk.c/.h` — Disk abstraction
- `drivers/drive/disk/disk_subsystem.c/.h` — Disk subsystem initialization
- `drivers/drive/iso9660/iso9660.c/.h` — ISO 9660 CD-ROM filesystem
- `drivers/drive/mfs/mfs.c/.h` — MaahiOS native filesystem
- `drivers/drive/partition/partdrive.c/.h` — MBR partition table
- `drivers/drive/volume/voldrive.c/.h` — Volume/mount manager

## Architecture
```
Volume Manager
  └── Partition Driver (MBR)
       └── ATA Driver (hardware)
            └── PIO mode I/O

Filesystem Layer
  ├── ISO 9660 (read-only, CD-ROM)
  └── MFS (read-write, native)
```

## ATA Driver
- PIO mode (no DMA)
- Primary controller: I/O ports 0x1F0–0x1F7
- Supports LBA28 addressing
- IRQ 14 for command completion

## ISO 9660
- Read-only, used for boot CD
- Directory traversal, file reading
- Rock Ridge extensions not supported

## MFS (MaahiOS File System)
- Native read-write filesystem
- Used for hard disk volumes
- Simple block-based layout

## Known Issues
*(Agents add issues here)*
