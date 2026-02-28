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

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# LAYER 1: Drivers
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[1/17] Compiling Drivers...${NC}"

i686-elf-gcc -c "$SRC_DIR/drivers/vga/vga.c"                   -o "$BINARIES_DIR/vga.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/display/bga.c"               -o "$BINARIES_DIR/bga.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/display/bga_mouse.c"         -o "$BINARIES_DIR/bga_mouse.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/display/display.c"           -o "$BINARIES_DIR/display.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/mouse/mouse.c"               -o "$BINARIES_DIR/mouse.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/pci/pci.c"                   -o "$BINARIES_DIR/pci.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/ata/ata.c"             -o "$BINARIES_DIR/ata.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/disk/disk_subsystem.c" -o "$BINARIES_DIR/disk_subsystem.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/drive/iso9660/iso9660.c"     -o "$BINARIES_DIR/iso9660.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/rtc/rtc.c"                   -o "$BINARIES_DIR/rtc.o" $KFLAGS
i686-elf-gcc -c "$SRC_DIR/drivers/keyboard/keyboard.c"         -o "$BINARIES_DIR/keyboard.o" $KFLAGS
echo -e "${GREEN}âœ“ All drivers compiled${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# LAYER 2: Kernel Managers
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[2/17] Compiling Kernel Managers...${NC}"

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
echo -e "${GREEN}✓ All managers compiled${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# LINK KERNEL
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[3/17] Linking kernel...${NC}"

i686-elf-ld -T "$SRC_DIR/loader/linker/linker.ld" -o "$BUILD_DIR/kernel.bin" \
    "$BINARIES_DIR/boot.o" "$BINARIES_DIR/kernel.o" \
    "$BINARIES_DIR/vga.o" "$BINARIES_DIR/bga.o" "$BINARIES_DIR/bga_mouse.o" "$BINARIES_DIR/display.o" \
    "$BINARIES_DIR/mouse.o" "$BINARIES_DIR/pci.o" "$BINARIES_DIR/ata.o" "$BINARIES_DIR/iso9660.o" \
    "$BINARIES_DIR/disk_subsystem.o" "$BINARIES_DIR/rtc.o" "$BINARIES_DIR/keyboard.o" \
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
echo -e "${GREEN}âœ“ kernel.bin linked${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# BUILD SYSMAN (Ring 3 - PID 1)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[4/17] Building sysman + liblog + libcell + libprocess + libmemory...${NC}"

i686-elf-as "$SRC_DIR/system/systemprograms/sysman/sysman_entry.s" -o "$BINARIES_DIR/sysman_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/sysman/sysman.c" -o "$BINARIES_DIR/sysman.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/liblog/liblog.c" -o "$BINARIES_DIR/liblog.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libcell/libcell.c" -o "$BINARIES_DIR/libcell.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libprocess/libprocess.c" -o "$BINARIES_DIR/libprocess.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libmemory/libmemory.c" -o "$BINARIES_DIR/libmemory.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/executives/common/executive_queue.c" -o "$BINARIES_DIR/executive_queue.o" $UFLAGS

i686-elf-ld -T "$SRC_DIR/system/systemprograms/sysman/sysman_linker.ld" -o "$BUILD_DIR/sysman.elf" \
    "$BINARIES_DIR/sysman_entry.o" "$BINARIES_DIR/sysman.o" "$BINARIES_DIR/liblog.o" \
    "$BINARIES_DIR/libcell.o" "$BINARIES_DIR/libprocess.o" "$BINARIES_DIR/libmemory.o" \
    "$BINARIES_DIR/executive_queue.o"

i686-elf-objcopy -O binary "$BUILD_DIR/sysman.elf" "$BUILD_DIR/sysman.bin"
echo -e "${GREEN}âœ“ sysman.bin built (with liblog + libcell + libprocess + libmemory)${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# BUILD LOG EXECUTIVE (Ring 3 - Started by sysman)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[5/17] Building Log Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/logexecutive/log_executive_entry.s" -o "$BINARIES_DIR/log_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/logexecutive/logexecutive.c" -o "$BINARIES_DIR/logexecutive.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/executives/common/executive_queue.c" -o "$BINARIES_DIR/executive_queue.o" $UFLAGS

i686-elf-ld -T "$SRC_DIR/system/executives/logexecutive/log_executive_linker.ld" -o "$BUILD_DIR/logexec.elf" \
    "$BINARIES_DIR/log_executive_entry.o" "$BINARIES_DIR/logexecutive.o" "$BINARIES_DIR/executive_queue.o"

i686-elf-objcopy -O binary "$BUILD_DIR/logexec.elf" "$BUILD_DIR/logexec.bin"
echo -e "${GREEN}âœ“ logexec.bin built${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# BUILD CELL EXECUTIVE (Ring 3 - Started by sysman after Log Executive)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[6/17] Building Cell Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/cellexecutive/cell_executive_entry.s" -o "$BINARIES_DIR/cell_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/cellexecutive/cellexecutive.c" -o "$BINARIES_DIR/cellexecutive.o" $UFLAGS
# executive_queue.o already compiled above, reuse it

i686-elf-ld -T "$SRC_DIR/system/executives/cellexecutive/cell_executive_linker.ld" -o "$BUILD_DIR/cellexec.elf" \
    "$BINARIES_DIR/cell_executive_entry.o" "$BINARIES_DIR/cellexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o"

i686-elf-objcopy -O binary "$BUILD_DIR/cellexec.elf" "$BUILD_DIR/cellexec.bin"
echo -e "${GREEN}âœ“ cellexec.bin built${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# BUILD PROCESS EXECUTIVE (Ring 3 - Started by sysman after Cell Executive)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[7/17] Building Process Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/processexecutive/process_executive_entry.s" -o "$BINARIES_DIR/process_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/processexecutive/process_executive.c" -o "$BINARIES_DIR/processexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above, reuse them

i686-elf-ld -T "$SRC_DIR/system/executives/processexecutive/process_executive_linker.ld" -o "$BUILD_DIR/procexec.elf" \
    "$BINARIES_DIR/process_executive_entry.o" "$BINARIES_DIR/processexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/procexec.elf" "$BUILD_DIR/procexec.bin"
echo -e "${GREEN}âœ“ procexec.bin built${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# BUILD MEMORY EXECUTIVE (Ring 3 - Started by sysman after Process Executive)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[8/17] Building Memory Executive...${NC}"

i686-elf-as "$SRC_DIR/system/executives/memoryexecutive/memory_executive_entry.s" -o "$BINARIES_DIR/memory_executive_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/executives/memoryexecutive/memory_executive.c" -o "$BINARIES_DIR/memoryexecutive.o" $UFLAGS
# executive_queue.o, liblog.o, libcell.o already compiled above, reuse them

i686-elf-ld -T "$SRC_DIR/system/executives/memoryexecutive/memory_executive_linker.ld" -o "$BUILD_DIR/memexec.elf" \
    "$BINARIES_DIR/memory_executive_entry.o" "$BINARIES_DIR/memoryexecutive.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" "$BINARIES_DIR/libcell.o"

i686-elf-objcopy -O binary "$BUILD_DIR/memexec.elf" "$BUILD_DIR/memexec.bin"
echo -e "${GREEN}âœ“ memexec.bin built${NC}"

# 
# BUILD DISK EXECUTIVE (Ring 3 - Started by sysman after Memory Executive)
# 
echo -e "\n${YELLOW}[9/17] Building Disk Executive...${NC}"

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
echo -e "\n${YELLOW}[10/17] Building FS Executive...${NC}"

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
echo -e "\n${YELLOW}[11/17] Building GUI Executive + libgui...${NC}"

# Compile libgui library files (shared by GUI Executive, Orbit, Terminal)
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/libgui.c"               -o "$BINARIES_DIR/libgui.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/printgui/printgui.c"    -o "$BINARIES_DIR/printgui.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/fonts/font8x16.c"       -o "$BINARIES_DIR/font8x16.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/keyboard/keyboard.c"    -o "$BINARIES_DIR/kbd.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libgui/console/console.c"     -o "$BINARIES_DIR/console.o" $UFLAGS

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
echo -e "\n${YELLOW}[12/17] Building I/O Executive + libio...${NC}"

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

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# BUILD ORBIT (Ring 3 - Desktop Shell, started by sysman)
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[13/17] Building Orbit...${NC}"

i686-elf-as "$SRC_DIR/system/systemprograms/orbit/orbit_entry.s" -o "$BINARIES_DIR/orbit_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/orbit/orbit.c" -o "$BINARIES_DIR/orbit.o" $UFLAGS
# liblog.o, libcell.o, libprocess.o, executive_queue.o already compiled above

i686-elf-ld -T "$SRC_DIR/system/systemprograms/orbit/orbit_linker.ld" -o "$BUILD_DIR/orbit.elf" \
    "$BINARIES_DIR/orbit_entry.o" "$BINARIES_DIR/orbit.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" \
    "$BINARIES_DIR/libcell.o" "$BINARIES_DIR/libprocess.o" \
    "$BINARIES_DIR/libgui.o" "$BINARIES_DIR/printgui.o" "$BINARIES_DIR/font8x16.o"

i686-elf-objcopy -O binary "$BUILD_DIR/orbit.elf" "$BUILD_DIR/orbit.bin"
echo -e "${GREEN} orbit.bin built${NC}"

# BUILD TERMINAL (Ring 3 - Command line, launched by Orbit)
echo -e "\n${YELLOW}[14/17] Building Terminal + Console Apps...${NC}"

i686-elf-as "$SRC_DIR/system/systemprograms/terminal/terminal_entry.s" -o "$BINARIES_DIR/terminal_entry.o"
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/terminal.c" -o "$BINARIES_DIR/terminal.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/libmex/libmex.c" -o "$BINARIES_DIR/libmex.o" $UFLAGS
# Console apps
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/apps/app_shutdown.c" -o "$BINARIES_DIR/app_shutdown.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/apps/app_restart.c" -o "$BINARIES_DIR/app_restart.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/apps/app_procman.c" -o "$BINARIES_DIR/app_procman.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/apps/app_diskman.c" -o "$BINARIES_DIR/app_diskman.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/systemprograms/terminal/apps/app_fileman.c" -o "$BINARIES_DIR/app_fileman.o" $UFLAGS
# liblog.o, libfs.o, libcell.o, libprocess.o, libdisk.o, executive_queue.o already compiled above

i686-elf-ld -T "$SRC_DIR/system/systemprograms/terminal/terminal_linker.ld" -o "$BUILD_DIR/terminal.elf" \
    "$BINARIES_DIR/terminal_entry.o" "$BINARIES_DIR/terminal.o" \
    "$BINARIES_DIR/app_shutdown.o" "$BINARIES_DIR/app_restart.o" \
    "$BINARIES_DIR/app_procman.o" "$BINARIES_DIR/app_diskman.o" "$BINARIES_DIR/app_fileman.o" \
    "$BINARIES_DIR/executive_queue.o" "$BINARIES_DIR/liblog.o" \
    "$BINARIES_DIR/libfs.o" "$BINARIES_DIR/libcell.o" \
    "$BINARIES_DIR/libmex.o" "$BINARIES_DIR/libprocess.o" "$BINARIES_DIR/libdisk.o" \
    "$BINARIES_DIR/libgui.o" "$BINARIES_DIR/printgui.o" "$BINARIES_DIR/font8x16.o" "$BINARIES_DIR/kbd.o" "$BINARIES_DIR/console.o" "$BINARIES_DIR/libio.o"

i686-elf-objcopy -O binary "$BUILD_DIR/terminal.elf" "$BUILD_DIR/terminal.bin"
echo -e "${GREEN} terminal.bin built${NC}"

# BUILD HELLO.MEX (first .mex test application)
echo -e "\n${YELLOW}[15/17] Building hello.mex...${NC}"

i686-elf-gcc -c "$SRC_DIR/apps/mex_entry.s" -o "$BINARIES_DIR/mex_entry.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/apps/hello/hello.c" -o "$BINARIES_DIR/hello.o" $UFLAGS
i686-elf-gcc -c "$SRC_DIR/system/libraries/liblog/liblog.c" -o "$BINARIES_DIR/hello_liblog.o" $UFLAGS
i686-elf-ld -T "$SRC_DIR/apps/mex_app.ld" -o "$BUILD_DIR/hello.elf" \
    "$BINARIES_DIR/mex_entry.o" "$BINARIES_DIR/hello.o" "$BINARIES_DIR/hello_liblog.o"
i686-elf-objcopy -O binary "$BUILD_DIR/hello.elf" "$BUILD_DIR/hello.bin"
python3 ../tools/mex_pack.py "$BUILD_DIR/hello.bin" "$BUILD_DIR/hello.mex" \
    --name "hello" --type app --flags console -v || \
    /c/Users/panrai/AppData/Local/Programs/Python/Launcher/py.exe ../tools/mex_pack.py "$BUILD_DIR/hello.bin" "$BUILD_DIR/hello.mex" \
    --name "hello" --type app --flags console -v
echo -e "${GREEN} hello.mex built${NC}"

# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
# CREATE ISO
# â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•â•
echo -e "\n${YELLOW}[16/17] Creating ISO...${NC}"

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
cp "$BUILD_DIR/orbit.bin" "$ISODIR/boot/orbit.bin"
cp "$BUILD_DIR/terminal.bin" "$ISODIR/boot/terminal.bin"
cp "$BUILD_DIR/hello.mex" "$ISODIR/hello.mex"

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
    module /boot/orbit.bin
    module /boot/terminal.bin
}
EOF

echo -e "\n${YELLOW}[17/17] Building ISO image...${NC}"

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
echo -e "${GREEN}âœ“ boot.iso created${NC}"

echo -e "\n${YELLOW}======================================${NC}"
echo -e "${GREEN}Build Complete! (Kernel + Sysman + Log + Cell + Process + Memory + Disk + FS + GUI + I/O Executives + Orbit + Terminal)${NC}"
echo -e "${GREEN}ISO: $BUILD_DIR/boot.iso${NC}"
echo -e "${YELLOW}======================================${NC}"
