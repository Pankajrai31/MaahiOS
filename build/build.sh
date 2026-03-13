#!/bin/bash

# MaahiOS Build Script
# Builds the kernel + sysman and creates bootable ISO

set -e

# Colors
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
RED='\033[0;31m'
NC='\033[0m' # No Color

# Cross-compiler path
export PATH="/usr/local/i686-elf/bin:$PATH"

# Directories
SRC_DIR="../src"
BUILD_DIR="."
BINARIES_DIR="./binaries"
ISODIR="./isodir"

# Common compiler flags
KFLAGS="-ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32"
UFLAGS="-ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32"

echo -e "${YELLOW}======================================${NC}"
echo -e "${YELLOW}MaahiOS Build System (Minimal)${NC}"
echo -e "${YELLOW}======================================${NC}"

# Ensure directories exist
mkdir -p "$BINARIES_DIR"
mkdir -p "$ISODIR/boot/grub"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# LAYER 1: Drivers
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[1/18] Compiling Drivers...${NC}"

i686-elf-gcc -c "$SRC_DIR/drivers/vga/vga.c"                   -o "$BINARIES_DIR/vga.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/display/bga.c"               -o "$BINARIES_DIR/bga.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/display/vbe.c"               -o "$BINARIES_DIR/vbe.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/display/display.c"           -o "$BINARIES_DIR/display.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/mouse/mouse.c"               -o "$BINARIES_DIR/mouse.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/pci/pci.c"                   -o "$BINARIES_DIR/pci.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/ata/ata.c"             -o "$BINARIES_DIR/ata.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/disk/disk.c"           -o "$BINARIES_DIR/disk.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/partition/partdrive.c" -o "$BINARIES_DIR/partdrive.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/mfs/mfs.c"            -o "$BINARIES_DIR/mfs.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/volume/voldrive.c"     -o "$BINARIES_DIR/voldrive.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/iso9660/iso9660.c"     -o "$BINARIES_DIR/iso9660.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/rtc/rtc.c"                   -o "$BINARIES_DIR/rtc.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/keyboard/keyboard.c"         -o "$BINARIES_DIR/keyboard.o" $KFLAGS
echo -e "${GREEN}Ã¢Å“â€œ All drivers compiled${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# LAYER 2: Kernel Managers
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[2/18] Compiling Kernel Managers...${NC}"

# Boot + core
i686-elf-as "$SRC_DIR/loader/boot.s" -o "$BINARIES_DIR/boot.o"
i686-elf-gcc -c "$SRC_DIR/loader/kernel.c"                              -o "$BINARIES_DIR/kernel.o" $KFLAGS -I"$SRC_DIR"

# CPU
i686-elf-gcc -c "$SRC_DIR/managers/gdt/gdt.c"                           -o "$BINARIES_DIR/gdt.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/interrupt/idt.c"                      -o "$BINARIES_DIR/idt.o" $KFLAGS
i686-elf-as "$SRC_DIR/managers/interrupt/interrupt_stubs.s"              -o "$BINARIES_DIR/interrupt_stubs.o"
i686-elf-gcc -c "$SRC_DIR/managers/interrupt/exception_handler.c"        -o "$BINARIES_DIR/exception_handler.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/irq/irq_manager.c"                   -o "$BINARIES_DIR/irq_manager.o" $KFLAGS

# Memory
i686-elf-gcc -c "$SRC_DIR/managers/memory/pmm.c"                        -o "$BINARIES_DIR/pmm.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/memory/paging.c"                     -o "$BINARIES_DIR/paging.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/memory/kheap.c"                      -o "$BINARIES_DIR/kheap.o" $KFLAGS

# Process + Scheduler
i686-elf-gcc -c "$SRC_DIR/managers/process/process_manager.c"           -o "$BINARIES_DIR/process_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/scheduler/scheduler.c"               -o "$BINARIES_DIR/scheduler.o" $KFLAGS
i686-elf-as "$SRC_DIR/managers/scheduler/switch_osdev.s"                -o "$BINARIES_DIR/switch.o"
i686-elf-gcc -c "$SRC_DIR/managers/timer/pit.c"                         -o "$BINARIES_DIR/pit.o" $KFLAGS

# Services
i686-elf-gcc -c "$SRC_DIR/managers/device/device_manager.c"             -o "$BINARIES_DIR/device_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/klog/klog.c"                         -o "$BINARIES_DIR/klog.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/shm/shm_manager.c"                   -o "$BINARIES_DIR/shm_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/cell/cell_manager.c"                  -o "$BINARIES_DIR/cell_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/grub_module/grub_module_manager.c"    -o "$BINARIES_DIR/grub_module_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/time/time_manager.c"                  -o "$BINARIES_DIR/time_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/ring3/ring3.c"                       -o "$BINARIES_DIR/ring3.o" $KFLAGS

# Syscall manager + 9 handler files
i686-elf-gcc -c "$SRC_DIR/managers/syscall/syscall_manager.c"            -o "$BINARIES_DIR/syscall_manager.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/core_handlers.c"     -o "$BINARIES_DIR/syscall_core_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/process_handlers.c"  -o "$BINARIES_DIR/syscall_process_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/memory_handlers.c"   -o "$BINARIES_DIR/syscall_memory_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/shm_handlers.c"      -o "$BINARIES_DIR/syscall_shm_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/cell_handlers.c"     -o "$BINARIES_DIR/syscall_cell_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/device_handlers.c"   -o "$BINARIES_DIR/syscall_device_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/module_handlers.c"   -o "$BINARIES_DIR/syscall_module_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/time_handlers.c"     -o "$BINARIES_DIR/syscall_time_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/debug_handlers.c"    -o "$BINARIES_DIR/syscall_debug_handlers.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/managers/syscall/handlers/fs_handlers.c"       -o "$BINARIES_DIR/syscall_fs_handlers.o" $KFLAGS
echo -e "${GREEN}âœ“ All managers compiled${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# LINK KERNEL
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[3/18] Linking kernel...${NC}"

i686-elf-ld -T "$SRC_DIR/loader/linker/linker.ld" -o "$BUILD_DIR/kernel.bin" \
    "$BINARIES_DIR/boot.o" "$BINARIES_DIR/kernel.o" \
    "$BINARIES_DIR/vga.o" "$BINARIES_DIR/bga.o" "$BINARIES_DIR/vbe.o" "$BINARIES_DIR/display.o" \
    "$BINARIES_DIR/mouse.o" "$BINARIES_DIR/pci.o" "$BINARIES_DIR/ata.o" "$BINARIES_DIR/iso9660.o" \
    "$BINARIES_DIR/disk.o" "$BINARIES_DIR/partdrive.o" "$BINARIES_DIR/mfs.o" "$BINARIES_DIR/voldrive.o" \
    "$BINARIES_DIR/rtc.o" "$BINARIES_DIR/keyboard.o" \
    "$BINARIES_DIR/gdt.o" "$BINARIES_DIR/idt.o" "$BINARIES_DIR/interrupt_stubs.o" \
    "$BINARIES_DIR/exception_handler.o" "$BINARIES_DIR/irq_manager.o" \
    "$BINARIES_DIR/pmm.o" "$BINARIES_DIR/paging.o" "$BINARIES_DIR/kheap.o" \
    "$BINARIES_DIR/process_manager.o" "$BINARIES_DIR/scheduler.o" "$BINARIES_DIR/switch.o" "$BINARIES_DIR/pit.o" \
    "$BINARIES_DIR/device_manager.o" "$BINARIES_DIR/klog.o" \
    "$BINARIES_DIR/shm_manager.o" "$BINARIES_DIR/cell_manager.o" \
    "$BINARIES_DIR/grub_module_manager.o" "$BINARIES_DIR/time_manager.o" "$BINARIES_DIR/ring3.o" \
    "$BINARIES_DIR/syscall_manager.o" \
    "$BINARIES_DIR/syscall_core_handlers.o" "$BINARIES_DIR/syscall_process_handlers.o" \
    "$BINARIES_DIR/syscall_memory_handlers.o" "$BINARIES_DIR/syscall_shm_handlers.o" \
    "$BINARIES_DIR/syscall_cell_handlers.o" "$BINARIES_DIR/syscall_device_handlers.o" \
    "$BINARIES_DIR/syscall_module_handlers.o" "$BINARIES_DIR/syscall_time_handlers.o" \
    "$BINARIES_DIR/syscall_debug_handlers.o" \
    "$BINARIES_DIR/syscall_fs_handlers.o"
echo -e "${GREEN}Ã¢Å“â€œ kernel.bin linked${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# BUILD SYSMAN (Ring 3 - PID 1)
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[4/18] Building shared libs + sysman entry (sysman.c deferred)...${NC}"

i686-elf-as "$SRC_DIR/system/systemprograms/sysman/sysman_entry.s" -o "$BINARIES_DIR/sysman_entry.o"
# NOTE: sysman.c compilation deferred to step 15.5 (needs module_bss_sizes.h)
i686-elf-gcc -c "$SRC_DIR/system/libraries/liblog/liblog.c" -o "$BINARIES_DIR/liblog.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libcell/libcell.c" -o "$BINARIES_DIR/libcell.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libprocess/libprocess.c" -o "$BINARIES_DIR/libprocess.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libmemory/libmemory.c" -o "$BINARIES_DIR/libmemory.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/executives/common/executive_queue.c" -o "$BINARIES_DIR/executive_queue.o" $UFLAGS


echo -e "${GREEN}Ã¢Å“â€œ Shared libs + sysman entry compiled (sysman.c deferred)${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# BUILD LOG EXECUTIVE (Ring 3 - Started by sysman)
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[5/18] Building Log Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/logexecutive/log_executive_entry.s" -o "$BINARIES_DIR/log_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/logexecutive/logexecutive.c" -o "$BINARIES_DIR/logexecutive.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/executives/common/executive_queue.c" -o "$BINARIES_DIR/executive_queue.o" $UFLAGS

i686-elf-ld -T "$SRC_DIR/system/executives/logexecutive/log_executive_linker.ld" -o "$BUILD_DIR/logexec.elf" \
    "$BINARIES_DIR/log_executive_entry.o" "$BINARIES_DIR/logexecutive.o" "$BINARIES_DIR/executive_queue.o"

i686-elf-objcopy -O binary "$BUILD_DIR/logexec.elf" "$BUILD_DIR/logexec.bin"
echo -e "${GREEN}Ã¢Å“â€œ logexec.bin built${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# BUILD CELL EXECUTIVE (Ring 3 - Started by sysman after Log Executive)
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[6/18] Building Cell Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/cellexecutive/cell_executive_entry.s" -o "$BINARIES_DIR/cell_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/cellexecutive/cellexecutive.c" -o "$BINARIES_DIR/cellexecutive.o" $UFLAGS
# executive_queue.o already compiled above, reuse it

i686-elf-ld -T "$SRC_DIR/system/executives/cellexecutive/cell_executive_linker.ld" -o "$BUILD_DIR/cellexec.elf" \
    "$BINARIES_DIR/cell_executive_entry.o" "$BINARIES_DIR/cellexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o"

i686-elf-objcopy -O binary "$BUILD_DIR/cellexec.elf" "$BUILD_DIR/cellexec.bin"
echo -e "${GREEN}Ã¢Å“â€œ cellexec.bin built${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# BUILD PROCESS EXECUTIVE (Ring 3 - Started by sysman after Cell Executive)
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[7/18] Building Process Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/processexecutive/process_executive_entry.s" -o "$BINARIES_DIR/process_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/processexecutive/process_executive.c" -o "$BINARIES_DIR/processexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above, reuse them

i686-elf-ld -T "$SRC_DIR/system/executives/processexecutive/process_executive_linker.ld" -o "$BUILD_DIR/procexec.elf" \
    "$BINARIES_DIR/process_executive_entry.o" "$BINARIES_DIR/processexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/procexec.elf" "$BUILD_DIR/procexec.bin"
echo -e "${GREEN}Ã¢Å“â€œ procexec.bin built${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# BUILD MEMORY EXECUTIVE (Ring 3 - Started by sysman after Process Executive)
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[8/18] Building Memory Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/memoryexecutive/memory_executive_entry.s" -o "$BINARIES_DIR/memory_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/memoryexecutive/memory_executive.c" -o "$BINARIES_DIR/memoryexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above, reuse them

i686-elf-ld -T "$SRC_DIR/system/executives/memoryexecutive/memory_executive_linker.ld" -o "$BUILD_DIR/memexec.elf" \
    "$BINARIES_DIR/memory_executive_entry.o" "$BINARIES_DIR/memoryexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/memexec.elf" "$BUILD_DIR/memexec.bin"
echo -e "${GREEN}Ã¢Å“â€œ memexec.bin built${NC}"

# 
# BUILD DISK EXECUTIVE (Ring 3 - Started by sysman after Memory Executive)
# 
echo -e "\n${YELLOW}[9/18] Building Disk Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/diskexecutive/disk_executive_entry.s" -o "$BINARIES_DIR/disk_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/diskexecutive/disk_executive.c" -o "$BINARIES_DIR/diskexecutive.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libdisk/libdisk.c" -o "$BINARIES_DIR/libdisk.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above, reuse them

i686-elf-ld -T "$SRC_DIR/system/executives/diskexecutive/disk_executive_linker.ld" -o "$BUILD_DIR/diskexec.elf" \
    "$BINARIES_DIR/disk_executive_entry.o" "$BINARIES_DIR/diskexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/diskexec.elf" "$BUILD_DIR/diskexec.bin"
echo -e "${GREEN} diskexec.bin built${NC}"

# 
# BUILD FS EXECUTIVE (Ring 3 - Started by sysman after Disk Executive)
# 
echo -e "\n${YELLOW}[10/18] Building FS Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/fsexecutive/fs_executive_entry.s" -o "$BINARIES_DIR/fs_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/fsexecutive/fs_executive.c" -o "$BINARIES_DIR/fsexecutive.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libfs/libfs.c" -o "$BINARIES_DIR/libfs.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above, reuse them

i686-elf-ld -T "$SRC_DIR/system/executives/fsexecutive/fs_executive_linker.ld" -o "$BUILD_DIR/fsexec.elf" \
    "$BINARIES_DIR/fs_executive_entry.o" "$BINARIES_DIR/fsexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/fsexec.elf" "$BUILD_DIR/fsexec.bin"
echo -e "${GREEN} fsexec.bin built${NC}"

# BUILD GUI EXECUTIVE + LIBGUI (Ring 3 - Started by sysman after FS Executive)
echo -e "\n${YELLOW}[11/18] Building GUI Executive + libgui...${NC}"

# Compile libgui library files (shared by GUI Executive, Orbit, Terminal)
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/libgui.c"               -o "$BINARIES_DIR/libgui.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/printgui/printgui.c"    -o "$BINARIES_DIR/printgui.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/fonts/font8x16.c"       -o "$BINARIES_DIR/font8x16.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/keyboard/keyboard.c"    -o "$BINARIES_DIR/kbd.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/console/console.c"     -o "$BINARIES_DIR/console.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/fonts/libfont.c"       -o "$BINARIES_DIR/libfont.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libbmp/libbmp.c"              -o "$BINARIES_DIR/libbmp.o" $UFLAGS

# Compile GUI Executive
i686-elf-as "$SRC_DIR/system/executives/guiexecutive/gui_executive_entry.s" -o "$BINARIES_DIR/gui_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/guiexecutive/gui_executive.c"   -o "$BINARIES_DIR/guiexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above

i686-elf-ld -T "$SRC_DIR/system/executives/guiexecutive/gui_executive_linker.ld" -o "$BUILD_DIR/guiexec.elf" \
    "$BINARIES_DIR/gui_executive_entry.o" "$BINARIES_DIR/guiexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/guiexec.elf" "$BUILD_DIR/guiexec.bin"
echo -e "${GREEN} guiexec.bin + libgui built${NC}"

# BUILD I/O EXECUTIVE + LIBIO (Ring 3 - Started by sysman after GUI Executive)
echo -e "\n${YELLOW}[12/18] Building I/O Executive + libio...${NC}"

# Compile libio library file (shared by Terminal and any app using device input)
i686-elf-gcc -c "$SRC_DIR/system/libraries/libio/libio.c" -o "$BINARIES_DIR/libio.o" $UFLAGS

# Compile I/O Executive
i686-elf-as "$SRC_DIR/system/executives/ioexecutive/io_executive_entry.s" -o "$BINARIES_DIR/io_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/ioexecutive/io_executive.c" -o "$BINARIES_DIR/ioexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above

i686-elf-ld -T "$SRC_DIR/system/executives/ioexecutive/io_executive_linker.ld" -o "$BUILD_DIR/ioexec.elf" \
    "$BINARIES_DIR/io_executive_entry.o" "$BINARIES_DIR/ioexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/ioexec.elf" "$BUILD_DIR/ioexec.bin"
echo -e "${GREEN} ioexec.bin + libio built${NC}"

# BUILD WM EXECUTIVE (Ring 3 - Window Manager, started by sysman after I/O Executive)
echo -e "\n${YELLOW}[13/18] Building WM Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/wmexecutive/wm_executive_entry.s" -o "$BINARIES_DIR/wm_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/wmexecutive/wm_executive.c" -o "$BINARIES_DIR/wmexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above

i686-elf-ld -T "$SRC_DIR/system/executives/wmexecutive/wm_executive_linker.ld" -o "$BUILD_DIR/wmexec.elf" \
    "$BINARIES_DIR/wm_executive_entry.o" "$BINARIES_DIR/wmexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o" \
    "$BINARIES_DIR/font8x16.o"

i686-elf-objcopy -O binary "$BUILD_DIR/wmexec.elf" "$BUILD_DIR/wmexec.bin"
echo -e "${GREEN} wmexec.bin built${NC}"

# Compile libconsole (stdout pipe for console non-interactive apps)
i686-elf-gcc -c "$SRC_DIR/system/libraries/libconsole/libconsole.c" -o "$BINARIES_DIR/libconsole.o" $UFLAGS

# Compile libwindow (GUI window library with controls)
echo -e "\n${YELLOW}    Compiling libwindow...${NC}"
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/malloc.c"             -o "$BINARIES_DIR/libwindow_malloc.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/surface.c"            -o "$BINARIES_DIR/libwindow_surface.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/label.c"     -o "$BINARIES_DIR/libwindow_label.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/button.c"    -o "$BINARIES_DIR/libwindow_button.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/dialog.c"    -o "$BINARIES_DIR/libwindow_dialog.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/table.c"     -o "$BINARIES_DIR/libwindow_table.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/toolbar.c"   -o "$BINARIES_DIR/libwindow_toolbar.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/statusbar.c" -o "$BINARIES_DIR/libwindow_statusbar.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/menubar.c"   -o "$BINARIES_DIR/libwindow_menubar.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/treeview.c"  -o "$BINARIES_DIR/libwindow_treeview.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/textarea.c"  -o "$BINARIES_DIR/libwindow_textarea.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/controls/radiogroup.c" -o "$BINARIES_DIR/libwindow_radiogroup.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwindow/libwindow.c"          -o "$BINARIES_DIR/libwindow.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libwm/libwm.c"                    -o "$BINARIES_DIR/libwm.o" $UFLAGS
echo -e "${GREEN}âœ“ libwindow + libwm compiled${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# BUILD ORBIT (Ring 3 - Desktop Shell, started by sysman)
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[14/18] Building Orbit...${NC}"

i686-elf-as "$SRC_DIR/system/systemprograms/orbit/orbit_entry.s" -o "$BINARIES_DIR/orbit_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/orbit/orbit.c" -o "$BINARIES_DIR/orbit.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libmex/libmex.c" -o "$BINARIES_DIR/libmex.o" $UFLAGS
# liblog.o, libcell.o, libprocess.o, libfs.o, executive_queue.o already compiled above

i686-elf-ld -T "$SRC_DIR/system/systemprograms/orbit/orbit_linker.ld" -o "$BUILD_DIR/orbit.elf" \
    "$BINARIES_DIR/orbit_entry.o" "$BINARIES_DIR/orbit.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" \
    "$BINARIES_DIR/libcell.o" "$BINARIES_DIR/libprocess.o" \
    "$BINARIES_DIR/libfs.o" "$BINARIES_DIR/libmex.o" \
    "$BINARIES_DIR/libgui.o" "$BINARIES_DIR/printgui.o" "$BINARIES_DIR/font8x16.o" \
    "$BINARIES_DIR/libfont.o" "$BINARIES_DIR/libbmp.o" "$BINARIES_DIR/libio.o"

i686-elf-objcopy -O binary "$BUILD_DIR/orbit.elf" "$BUILD_DIR/orbit.bin"
echo -e "${GREEN} orbit.bin built${NC}"

# BUILD TERMINAL (Ring 3 - Windowed command line, launched by Orbit)
echo -e "\n${YELLOW}[15/18] Building Terminal...${NC}"

i686-elf-as "$SRC_DIR/system/systemprograms/terminal/terminal_entry.s" -o "$BINARIES_DIR/terminal_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/terminal.c" -o "$BINARIES_DIR/terminal.o" $UFLAGS
# liblog.o, libfs.o, libcell.o, libprocess.o, libmex.o, executive_queue.o already compiled above
# libwindow.o, libwindow_surface.o, libwindow_label.o, libwindow_button.o, libwindow_malloc.o compiled above

i686-elf-ld -T "$SRC_DIR/system/systemprograms/terminal/terminal_linker.ld" -o "$BUILD_DIR/terminal.elf" \
    "$BINARIES_DIR/terminal_entry.o" "$BINARIES_DIR/terminal.o" \
    "$BINARIES_DIR/libwindow.o" "$BINARIES_DIR/libwindow_surface.o" \
    "$BINARIES_DIR/libwindow_label.o" "$BINARIES_DIR/libwindow_button.o" \
    "$BINARIES_DIR/libwindow_dialog.o" "$BINARIES_DIR/libwindow_malloc.o" "$BINARIES_DIR/libwm.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" \
    "$BINARIES_DIR/libfs.o" "$BINARIES_DIR/libcell.o" \
    "$BINARIES_DIR/libmex.o" "$BINARIES_DIR/libprocess.o" "$BINARIES_DIR/libmemory.o" \
    "$BINARIES_DIR/libgui.o" "$BINARIES_DIR/printgui.o" "$BINARIES_DIR/font8x16.o" "$BINARIES_DIR/kbd.o" "$BINARIES_DIR/console.o" "$BINARIES_DIR/libio.o" \
    "$BINARIES_DIR/libfont.o"

i686-elf-objcopy -O binary "$BUILD_DIR/terminal.elf" "$BUILD_DIR/terminal.bin"
echo -e "${GREEN} terminal.bin built${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# GENERATE MODULE BSS SIZES + BUILD SYSMAN (deferred from step 4)
# Sysman needs BSS sizes from all module ELFs, which are now all built.
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[15.5/18] Generating module BSS sizes + building sysman...${NC}"

BSS_HEADER="$BUILD_DIR/module_bss_sizes.h"
echo "/* Auto-generated by build.sh â€” DO NOT EDIT */" > "$BSS_HEADER"
echo "#ifndef MODULE_BSS_SIZES_H" >> "$BSS_HEADER"
echo "#define MODULE_BSS_SIZES_H" >> "$BSS_HEADER"
echo "" >> "$BSS_HEADER"

for entry in \
    "logexec:LOGEXEC" \
    "cellexec:CELLEXEC" \
    "procexec:PROCEXEC" \
    "memexec:MEMEXEC" \
    "diskexec:DISKEXEC" \
    "fsexec:FSEXEC" \
    "guiexec:GUIEXEC" \
    "ioexec:IOEXEC" \
    "wmexec:WMEXEC" \
    "orbit:ORBIT" \
    "terminal:TERMINAL"; do
    elf_name="${entry%%:*}"
    macro="${entry##*:}"
    bss_val=$(i686-elf-size "$BUILD_DIR/${elf_name}.elf" | tail -1 | awk '{print $3}')
    echo "#define MOD_BSS_${macro} ${bss_val}U" >> "$BSS_HEADER"
    echo "  ${elf_name}: BSS = ${bss_val} bytes"
done

echo "" >> "$BSS_HEADER"
echo "#endif /* MODULE_BSS_SIZES_H */" >> "$BSS_HEADER"
echo -e "${GREEN}âœ“ module_bss_sizes.h generated${NC}"

# Now compile sysman.c (needs module_bss_sizes.h via -I)
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/sysman/sysman.c" -o "$BINARIES_DIR/sysman.o" $UFLAGS -I "$BUILD_DIR"

i686-elf-ld -T "$SRC_DIR/system/systemprograms/sysman/sysman_linker.ld" -o "$BUILD_DIR/sysman.elf" \
    "$BINARIES_DIR/sysman_entry.o" "$BINARIES_DIR/sysman.o" "$BINARIES_DIR/liblog.o" \
    "$BINARIES_DIR/libcell.o" "$BINARIES_DIR/libprocess.o" "$BINARIES_DIR/libmemory.o" \
    "$BINARIES_DIR/executive_queue.o"

i686-elf-objcopy -O binary "$BUILD_DIR/sysman.elf" "$BUILD_DIR/sysman.bin"
echo -e "${GREEN}âœ“ sysman.bin built with BSS-aware module sizes${NC}"


# BUILD MEX APPS (standalone .mex applications on the ISO filesystem)
echo -e "\n${YELLOW}[16/18] Building MEX apps...${NC}"

# Common MEX entry point (reused by all apps)
i686-elf-gcc -c "$SRC_DIR/apps/mex_entry.s" -o "$BINARIES_DIR/mex_entry.o" $UFLAGS

# Common libs for GUI-based MEX apps
MEX_GUI_LIBS="$BINARIES_DIR/executive_queue.o $BINARIES_DIR/liblog.o $BINARIES_DIR/libcell.o \
    $BINARIES_DIR/libgui.o $BINARIES_DIR/printgui.o $BINARIES_DIR/font8x16.o \
    $BINARIES_DIR/kbd.o $BINARIES_DIR/console.o $BINARIES_DIR/libio.o \
    $BINARIES_DIR/libfont.o"

# Common libs for libwindow-based MEX apps (GUI apps using windowed controls)
MEX_WINDOW_LIBS="$BINARIES_DIR/libwindow.o $BINARIES_DIR/libwindow_surface.o \
    $BINARIES_DIR/libwindow_label.o $BINARIES_DIR/libwindow_button.o \
    $BINARIES_DIR/libwindow_dialog.o $BINARIES_DIR/libwindow_table.o \
    $BINARIES_DIR/libwindow_toolbar.o $BINARIES_DIR/libwindow_statusbar.o \
    $BINARIES_DIR/libwindow_menubar.o $BINARIES_DIR/libwindow_treeview.o \
    $BINARIES_DIR/libwindow_textarea.o $BINARIES_DIR/libwindow_radiogroup.o \
    $BINARIES_DIR/libwindow_malloc.o $BINARIES_DIR/libwm.o \
    $BINARIES_DIR/libmemory.o \
    $BINARIES_DIR/executive_queue.o $BINARIES_DIR/liblog.o $BINARIES_DIR/libcell.o \
    $BINARIES_DIR/libgui.o $BINARIES_DIR/printgui.o $BINARIES_DIR/font8x16.o \
    $BINARIES_DIR/kbd.o $BINARIES_DIR/console.o $BINARIES_DIR/libio.o \
    $BINARIES_DIR/libfont.o"

# Common libs for console non-interactive MEX apps (stdout SHM pipe)
MEX_CONSOLE_LIBS="$BINARIES_DIR/libconsole.o $BINARIES_DIR/libcell.o \
    $BINARIES_DIR/liblog.o $BINARIES_DIR/executive_queue.o"

# --- hello.mex ---
i686-elf-gcc -c "$SRC_DIR/apps/hello/hello.c" -o "$BINARIES_DIR/hello.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/hello.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/hello.o" "$BINARIES_DIR/liblog.o"
i686-elf-objcopy -O binary "$BUILD_DIR/hello.elf" "$BUILD_DIR/hello.bin"

# --- procman.mex (console non-interactive) ---
i686-elf-gcc -c "$SRC_DIR/apps/procman/procman.c" -o "$BINARIES_DIR/procman.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/procman.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/procman.o" \
    "$BINARIES_DIR/libprocess.o" $MEX_CONSOLE_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/procman.elf" "$BUILD_DIR/procman.bin"

# --- diskman.mex (console non-interactive) ---
i686-elf-gcc -c "$SRC_DIR/apps/diskman/diskman.c" -o "$BINARIES_DIR/diskman.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/diskman.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/diskman.o" \
    "$BINARIES_DIR/libdisk.o" "$BINARIES_DIR/libfs.o" $MEX_CONSOLE_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/diskman.elf" "$BUILD_DIR/diskman.bin"

# --- fileman.mex (console non-interactive) ---
i686-elf-gcc -c "$SRC_DIR/apps/fileman/fileman.c" -o "$BINARIES_DIR/fileman.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/fileman.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/fileman.o" \
    "$BINARIES_DIR/libfs.o" $MEX_CONSOLE_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/fileman.elf" "$BUILD_DIR/fileman.bin"

# --- boxdrop.mex ---
i686-elf-gcc -c "$SRC_DIR/apps/boxdrop/boxdrop.c" -o "$BINARIES_DIR/boxdrop.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/boxdrop.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/boxdrop.o" \
    "$BINARIES_DIR/libprocess.o" $MEX_GUI_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/boxdrop.elf" "$BUILD_DIR/boxdrop.bin"

# --- sysinfo.mex (console non-interactive) ---
i686-elf-gcc -c "$SRC_DIR/apps/sysinfo/sysinfo.c" -o "$BINARIES_DIR/sysinfo.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/sysinfo.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/sysinfo.o" \
    "$BINARIES_DIR/libprocess.o" $MEX_CONSOLE_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/sysinfo.elf" "$BUILD_DIR/sysinfo.bin"

# --- shutdown.mex (console non-interactive) ---
i686-elf-gcc -c "$SRC_DIR/apps/shutdown/shutdown.c" -o "$BINARIES_DIR/shutdown.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/shutdown.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/shutdown.o" \
    "$BINARIES_DIR/libprocess.o" $MEX_CONSOLE_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/shutdown.elf" "$BUILD_DIR/shutdown.bin"

# --- hellogui.mex (windowed GUI app) ---
i686-elf-gcc -c "$SRC_DIR/apps/hellogui/hellogui.c" -o "$BINARIES_DIR/hellogui.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/hellogui.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/hellogui.o" \
    $MEX_WINDOW_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/hellogui.elf" "$BUILD_DIR/hellogui.bin"

# --- procexp.mex (windowed GUI app - Process Explorer) ---
i686-elf-gcc -c "$SRC_DIR/apps/procexp/procexp.c" -o "$BINARIES_DIR/procexp.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/procexp.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/procexp.o" \
    "$BINARIES_DIR/libprocess.o" $MEX_WINDOW_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/procexp.elf" "$BUILD_DIR/procexp.bin"

# --- diskexp.mex (windowed GUI app - Disk Explorer) ---
i686-elf-gcc -c "$SRC_DIR/apps/diskexp/diskexp.c" -o "$BINARIES_DIR/diskexp.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/diskexp.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/diskexp.o" \
    "$BINARIES_DIR/libdisk.o" "$BINARIES_DIR/libfs.o" $MEX_WINDOW_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/diskexp.elf" "$BUILD_DIR/diskexp.bin"

# --- wordwrit.mex (windowed GUI app - WordWrite Text Editor) ---
i686-elf-gcc -c "$SRC_DIR/apps/wordwrite/wordwrite.c" -o "$BINARIES_DIR/wordwrite.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/wordwrit.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/wordwrite.o" \
    "$BINARIES_DIR/libfs.o" $MEX_WINDOW_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/wordwrit.elf" "$BUILD_DIR/wordwrit.bin"

# --- logexp.mex (windowed GUI app - Log Explorer) ---
i686-elf-gcc -c "$SRC_DIR/apps/logexp/logexp.c" -o "$BINARIES_DIR/logexp.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/logexp.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/logexp.o" \
    $MEX_WINDOW_LIBS
i686-elf-objcopy -O binary "$BUILD_DIR/logexp.elf" "$BUILD_DIR/logexp.bin"

# Pack all MEX files
MEX_PACK="python3 ../tools/mex_pack.py"
MEX_PACK_ALT="/c/Users/panrai/AppData/Local/Programs/Python/Launcher/py.exe ../tools/mex_pack.py"

$MEX_PACK "$BUILD_DIR/hello.bin" "$BUILD_DIR/hello.mex" --name "hello" --type app --flags console --elf "$BUILD_DIR/hello.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/hello.bin" "$BUILD_DIR/hello.mex" --name "hello" --type app --flags console --elf "$BUILD_DIR/hello.elf" -v

$MEX_PACK "$BUILD_DIR/procman.bin" "$BUILD_DIR/procman.mex" --name "procman" --type app --flags console --elf "$BUILD_DIR/procman.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/procman.bin" "$BUILD_DIR/procman.mex" --name "procman" --type app --flags console --elf "$BUILD_DIR/procman.elf" -v

$MEX_PACK "$BUILD_DIR/diskman.bin" "$BUILD_DIR/diskman.mex" --name "diskman" --type app --flags console --elf "$BUILD_DIR/diskman.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/diskman.bin" "$BUILD_DIR/diskman.mex" --name "diskman" --type app --flags console --elf "$BUILD_DIR/diskman.elf" -v

$MEX_PACK "$BUILD_DIR/fileman.bin" "$BUILD_DIR/fileman.mex" --name "fileman" --type app --flags console --elf "$BUILD_DIR/fileman.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/fileman.bin" "$BUILD_DIR/fileman.mex" --name "fileman" --type app --flags console --elf "$BUILD_DIR/fileman.elf" -v

$MEX_PACK "$BUILD_DIR/boxdrop.bin" "$BUILD_DIR/boxdrop.mex" --name "boxdrop" --type app --flags gui --elf "$BUILD_DIR/boxdrop.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/boxdrop.bin" "$BUILD_DIR/boxdrop.mex" --name "boxdrop" --type app --flags gui --elf "$BUILD_DIR/boxdrop.elf" -v

$MEX_PACK "$BUILD_DIR/sysinfo.bin" "$BUILD_DIR/sysinfo.mex" --name "sysinfo" --type app --flags console --elf "$BUILD_DIR/sysinfo.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/sysinfo.bin" "$BUILD_DIR/sysinfo.mex" --name "sysinfo" --type app --flags console --elf "$BUILD_DIR/sysinfo.elf" -v

$MEX_PACK "$BUILD_DIR/shutdown.bin" "$BUILD_DIR/shutdown.mex" --name "shutdown" --type app --flags console --elf "$BUILD_DIR/shutdown.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/shutdown.bin" "$BUILD_DIR/shutdown.mex" --name "shutdown" --type app --flags console --elf "$BUILD_DIR/shutdown.elf" -v

$MEX_PACK "$BUILD_DIR/hellogui.bin" "$BUILD_DIR/hellogui.mex" --name "hellogui" --type app --flags gui --elf "$BUILD_DIR/hellogui.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/hellogui.bin" "$BUILD_DIR/hellogui.mex" --name "hellogui" --type app --flags gui --elf "$BUILD_DIR/hellogui.elf" -v

$MEX_PACK "$BUILD_DIR/procexp.bin" "$BUILD_DIR/procexp.mex" --name "procexp" --type app --flags gui --elf "$BUILD_DIR/procexp.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/procexp.bin" "$BUILD_DIR/procexp.mex" --name "procexp" --type app --flags gui --elf "$BUILD_DIR/procexp.elf" -v

$MEX_PACK "$BUILD_DIR/diskexp.bin" "$BUILD_DIR/diskexp.mex" --name "diskexp" --type app --flags gui --elf "$BUILD_DIR/diskexp.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/diskexp.bin" "$BUILD_DIR/diskexp.mex" --name "diskexp" --type app --flags gui --elf "$BUILD_DIR/diskexp.elf" -v

$MEX_PACK "$BUILD_DIR/wordwrit.bin" "$BUILD_DIR/wordwrit.mex" --name "wordwrit" --type app --flags gui --elf "$BUILD_DIR/wordwrit.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/wordwrit.bin" "$BUILD_DIR/wordwrit.mex" --name "wordwrit" --type app --flags gui --elf "$BUILD_DIR/wordwrit.elf" -v

$MEX_PACK "$BUILD_DIR/logexp.bin" "$BUILD_DIR/logexp.mex" --name "logexp" --type app --flags gui --elf "$BUILD_DIR/logexp.elf" -v || \
    $MEX_PACK_ALT "$BUILD_DIR/logexp.bin" "$BUILD_DIR/logexp.mex" --name "logexp" --type app --flags gui --elf "$BUILD_DIR/logexp.elf" -v

echo -e "${GREEN}\xE2\x9C\x93 MEX apps built (hello, procman, diskman, fileman, boxdrop, sysinfo, shutdown, hellogui, procexp, diskexp, wordwrit, logexp)${NC}"

# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
# CREATE ISO
# Ã¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢ÂÃ¢â€¢Â
echo -e "\n${YELLOW}[17/18] Creating ISO...${NC}"

cp "$BUILD_DIR/kernel.bin" "$ISODIR/boot/kernel.bin"
cp "$BUILD_DIR/sysman.bin" "$ISODIR/boot/sysman.bin"
cp "$BUILD_DIR/logexec.bin" "$ISODIR/boot/logexec.bin"
cp "$BUILD_DIR/cellexec.bin" "$ISODIR/boot/cellexec.bin"
cp "$BUILD_DIR/procexec.bin" "$ISODIR/boot/procexec.bin"
cp "$BUILD_DIR/memexec.bin" "$ISODIR/boot/memexec.bin"
cp "$BUILD_DIR/diskexec.bin" "$ISODIR/boot/diskexec.bin"
cp "$BUILD_DIR/fsexec.bin" "$ISODIR/boot/fsexec.bin"
cp "$BUILD_DIR/guiexec.bin" "$ISODIR/boot/guiexec.bin"
cp "$BUILD_DIR/ioexec.bin" "$ISODIR/boot/ioexec.bin"
cp "$BUILD_DIR/wmexec.bin" "$ISODIR/boot/wmexec.bin"
cp "$BUILD_DIR/orbit.bin" "$ISODIR/boot/orbit.bin"
cp "$BUILD_DIR/terminal.bin" "$ISODIR/boot/terminal.bin"
cp "$BUILD_DIR/hello.mex" "$ISODIR/hello.mex"
cp "$BUILD_DIR/procman.mex" "$ISODIR/procman.mex"
cp "$BUILD_DIR/diskman.mex" "$ISODIR/diskman.mex"
cp "$BUILD_DIR/fileman.mex" "$ISODIR/fileman.mex"
cp "$BUILD_DIR/boxdrop.mex" "$ISODIR/boxdrop.mex"
cp "$BUILD_DIR/sysinfo.mex" "$ISODIR/sysinfo.mex"
cp "$BUILD_DIR/shutdown.mex" "$ISODIR/shutdown.mex"
cp "$BUILD_DIR/hellogui.mex" "$ISODIR/hellogui.mex"
cp "$BUILD_DIR/procexp.mex" "$ISODIR/procexp.mex"
cp "$BUILD_DIR/diskexp.mex" "$ISODIR/diskexp.mex"
cp "$BUILD_DIR/wordwrit.mex" "$ISODIR/wordwrit.mex"
cp "$BUILD_DIR/logexp.mex" "$ISODIR/logexp.mex"

# Copy icon BMP files to ISO root
for icon in "$SRC_DIR/images/icons/TERMINAL.BMP" "$SRC_DIR/images/icons/HELLOGUI.BMP" "$SRC_DIR/images/icons/DEFAULT.BMP" "$SRC_DIR/images/icons/PROCEXP.BMP" "$SRC_DIR/images/icons/DISKEXP.BMP" "$SRC_DIR/images/icons/WORDWRT.BMP" "$SRC_DIR/images/icons/LOGEXP.BMP"; do
    if [ -f "$icon" ]; then
        cp "$icon" "$ISODIR/"
    fi
done
echo -e "${GREEN}\xE2\x9C\x93 Icon BMPs copied to ISO${NC}"

cat > "$ISODIR/boot/grub/grub.cfg" << 'EOF'
set default=0
set timeout=0

menuentry "MaahiOS" {
    multiboot /boot/kernel.bin
    module /boot/sysman.bin
    module /boot/logexec.bin
    module /boot/cellexec.bin
    module /boot/procexec.bin
    module /boot/memexec.bin
    module /boot/diskexec.bin
    module /boot/fsexec.bin
    module /boot/guiexec.bin
    module /boot/ioexec.bin
    module /boot/wmexec.bin
    module /boot/orbit.bin
    module /boot/terminal.bin
}
EOF

echo -e "\n${YELLOW}[18/18] Building ISO image...${NC}"

if command -v grub-mkrescue &> /dev/null; then
    grub-mkrescue -o "$BUILD_DIR/boot.iso" "$ISODIR" 2>&1
elif command -v xorrisofs &> /dev/null; then
    xorrisofs -R -J \
        -b boot/grub/i386-pc/eltorito.img \
        -no-emul-boot -boot-load-size 4 -boot-info-table \
        -o "$BUILD_DIR/boot.iso" "$ISODIR" 2>&1
else
    echo -e "${RED}ERROR: Neither grub-mkrescue nor xorrisofs found!${NC}"
    exit 1
fi
echo -e "${GREEN}✓ boot.iso created${NC}"

# ── Step 18: Clean up intermediate build artifacts ──
echo -e "\n${YELLOW}[18/18] Cleaning intermediate .elf and .bin files...${NC}"
rm -f "$BUILD_DIR"/*.elf "$BUILD_DIR"/*.bin
echo -e "${GREEN}✓ Removed .elf and .bin files (rebuilt each build)${NC}"

echo -e "\n${YELLOW}======================================${NC}"
echo -e "${GREEN}Build Complete! (Kernel + Sysman + Log + Cell + Process + Memory + Disk + FS + GUI + I/O + WM Executives + Orbit + Terminal)${NC}"
echo -e "${GREEN}ISO: $BUILD_DIR/boot.iso${NC}"
echo -e "${YELLOW}======================================${NC}"
