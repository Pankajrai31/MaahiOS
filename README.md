# MaahiOS

A simple operating system built from the ground up. This project demonstrates the fundamentals of OS development, including bootloading, kernel initialization, and basic VGA text output.

## Features

- **Multiboot Compliant**: Boots with GRUB bootloader
- **32-bit x86 Kernel**: Written in C and Assembly
- **VGA Text Mode**: Basic text output functionality
- **Minimal Design**: Clean, educational codebase

## Project Structure

```
MaahiOS/
├── boot.asm       # Bootloader entry point (Assembly)
├── kernel.c       # Kernel main code (C)
├── linker.ld      # Linker script
├── Makefile       # Build system
└── README.md      # This file
```

## Prerequisites

To build and run MaahiOS, you need:

- **NASM** (Netwide Assembler) - for assembling boot.asm
- **GCC** (GNU Compiler Collection) - for compiling C code
- **LD** (GNU Linker) - for linking object files
- **QEMU** (optional) - for testing the OS in an emulator
- **GRUB** (optional) - for creating bootable ISO images

### Installing Prerequisites on Ubuntu/Debian:
```bash
sudo apt-get update
sudo apt-get install build-essential nasm qemu-system-x86 grub-pc-bin xorriso
```

### Installing Prerequisites on Fedora/RHEL:
```bash
sudo dnf install gcc nasm qemu-system-x86 grub2-tools xorriso
```

### Installing Prerequisites on Arch Linux:
```bash
sudo pacman -S base-devel nasm qemu grub xorriso
```

## Building

To build the kernel binary:

```bash
make
```

This will produce `maahios.bin`, a multiboot-compliant kernel binary.

To create a bootable ISO image:

```bash
make iso
```

This will create `maahios.iso` that can be burned to a CD or USB drive.

## Running

### Run in QEMU (Quick Test)

The easiest way to test MaahiOS is using QEMU:

```bash
make run
```

Or run the ISO image:

```bash
make run-iso
```

### Run on Real Hardware

1. Build the ISO: `make iso`
2. Write the ISO to a USB drive:
   ```bash
   sudo dd if=maahios.iso of=/dev/sdX bs=4M status=progress
   ```
   (Replace `/dev/sdX` with your USB drive)
3. Boot from the USB drive

**Warning**: Be very careful with the `dd` command. Using the wrong device can erase your hard drive!

## Cleaning

To remove all build artifacts:

```bash
make clean
```

## How It Works

1. **Bootloader**: When the computer starts, GRUB loads the multiboot header from `boot.asm`
2. **Initialization**: The bootloader jumps to `_start`, which sets up the stack
3. **Kernel Entry**: Control is passed to `kernel_main()` in `kernel.c`
4. **Display**: The kernel initializes VGA text mode and displays a welcome message
5. **Halt**: The CPU is halted, waiting for interrupts

## Development

This is a minimal OS kernel intended for educational purposes. Future enhancements could include:

- Interrupt handling (IDT)
- Keyboard input
- Memory management
- Process scheduling
- File system support
- Device drivers

## License

This project is provided as-is for educational purposes.

## Contributing

Contributions are welcome! Feel free to submit pull requests or open issues.

## References

- [OSDev Wiki](https://wiki.osdev.org/) - Comprehensive OS development resource
- [Multiboot Specification](https://www.gnu.org/software/grub/manual/multiboot/multiboot.html)
- [Intel x86 Documentation](https://www.intel.com/content/www/us/en/developer/articles/technical/intel-sdm.html)