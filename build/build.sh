#!/bin/bash

# MaahiOS Build Script
# Builds the kernel and creates bootable ISO

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

echo -e "${YELLOW}======================================${NC}"
echo -e "${YELLOW}MaahiOS Build System${NC}"
echo -e "${YELLOW}======================================${NC}"

# Ensure binaries directory exists
mkdir -p "$BINARIES_DIR"
mkdir -p "$ISODIR/boot/grub"

echo -e "\n${YELLOW}[1/5] Assembling boot.s...${NC}"
i686-elf-as "$SRC_DIR/loader/boot.s" -o "$BINARIES_DIR/boot.o"
echo -e "${GREEN}✓ boot.o created${NC}"

echo -e "\n${YELLOW}[2/5] Compiling kernel.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/loader/kernel.c" -o "$BINARIES_DIR/kernel.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32 -I"$SRC_DIR"
echo -e "${GREEN}✓ kernel.o created${NC}"

echo -e "\n${YELLOW}[2b/5] Compiling vga.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/vga.c" -o "$BINARIES_DIR/vga.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ vga.o created${NC}"

echo -e "\n${YELLOW}[2b2/5] Compiling bga.c (BGA graphics driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/bga.c" -o "$BINARIES_DIR/bga.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ bga.o created${NC}"

echo -e "\n${YELLOW}[2b3/5] Compiling bga_mouse.c (BGA hardware cursor driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/bga_mouse.c" -o "$BINARIES_DIR/bga_mouse.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ bga_mouse.o created${NC}"

echo -e "\n${YELLOW}[2b4b/5] Compiling gfx.c (Graphics abstraction layer)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/gfx.c" -o "$BINARIES_DIR/gfx.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ gfx.o created${NC}"

echo -e "\n${YELLOW}[2b5/5] Compiling mouse.c (PS/2 mouse driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/mouse.c" -o "$BINARIES_DIR/mouse.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ mouse.o created${NC}"

echo -e "\n${YELLOW}[2b6/5] Compiling pci.c (PCI access)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/pci.c" -o "$BINARIES_DIR/pci.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ pci.o created${NC}"

echo -e "\n${YELLOW}[2b7/5] Compiling usb.c (USB HID driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/usb.c" -o "$BINARIES_DIR/usb.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ usb.o created${NC}"

echo -e "\n${YELLOW}[2b8/5] Compiling ata.c (ATA/ATAPI disk driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/disk/ata.c" -o "$BINARIES_DIR/ata.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ ata.o created${NC}"

echo -e "\n${YELLOW}[2b9/5] Compiling disk_subsystem.c (Disk subsystem)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/disk/disk_subsystem.c" -o "$BINARIES_DIR/disk_subsystem.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ disk_subsystem.o created${NC}"

echo -e "\n${YELLOW}[2b9b/5] Compiling iso9660.c (ISO filesystem driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/disk/iso9660.c" -o "$BINARIES_DIR/iso9660.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ iso9660.o created${NC}"

echo -e "\n${YELLOW}[2b10/5] Compiling guimanager.c (GUI manager - UI controls framework)...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/gui/guimanager.c" -o "$BINARIES_DIR/guimanager.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ guimanager.o created${NC}"

echo -e "\n${YELLOW}[2c/5] Compiling gdt.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/gdt/gdt.c" -o "$BINARIES_DIR/gdt.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ gdt.o created${NC}"

echo -e "\n${YELLOW}[2d/5] Compiling idt.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/interrupt/idt.c" -o "$BINARIES_DIR/idt.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ idt.o created${NC}"

echo -e "\n${YELLOW}[2d2/5] Assembling interrupt_stubs.s...${NC}"
i686-elf-as "$SRC_DIR/managers/interrupt/interrupt_stubs.s" -o "$BINARIES_DIR/interrupt_stubs.o"
echo -e "${GREEN}✓ interrupt_stubs.o created${NC}"

echo -e "\n${YELLOW}[2d3/5] Compiling exception_handler.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/interrupt/exception_handler.c" -o "$BINARIES_DIR/exception_handler.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ exception_handler.o created${NC}"

echo -e "\n${YELLOW}[2e/5] Compiling ring3.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/ring3/ring3.c" -o "$BINARIES_DIR/ring3.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ ring3.o created${NC}"

echo -e "\n${YELLOW}[2h/5] Compiling syscall modules...${NC}"
# Compile syscall dispatcher
i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/syscall_dispatcher.c" -o "$BINARIES_DIR/syscall_dispatcher.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_dispatcher.o created${NC}"

# Compile process syscalls
i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/process.c" -o "$BINARIES_DIR/syscall_process.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_process.o created${NC}"

# Compile memory syscalls
i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/memory.c" -o "$BINARIES_DIR/syscall_memory.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_memory.o created${NC}"

# Compile UI syscalls (split into components)
i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui.c" -o "$BINARIES_DIR/syscall_ui.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui.o created${NC}"

# Compile UI component syscalls
i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui/text_vga.c" -o "$BINARIES_DIR/syscall_ui_text_vga.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui_text_vga.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui/graphics.c" -o "$BINARIES_DIR/syscall_ui_graphics.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui_graphics.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui/window.c" -o "$BINARIES_DIR/syscall_ui_window.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui_window.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui/controls.c" -o "$BINARIES_DIR/syscall_ui_controls.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui_controls.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui/control_framework.c" -o "$BINARIES_DIR/syscall_ui_control_framework.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui_control_framework.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/ui/mouse_input.c" -o "$BINARIES_DIR/syscall_ui_mouse_input.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_ui_mouse_input.o created${NC}"

# Compile I/O syscalls
i686-elf-gcc -c "$SRC_DIR/system/syscalls/kernel/io.c" -o "$BINARIES_DIR/syscall_io.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_io.o created${NC}"

# Compile user syscalls (split into components)
i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/io.c" -o "$BINARIES_DIR/user_syscall_io.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_io.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/memory.c" -o "$BINARIES_DIR/user_syscall_memory.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_memory.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/process.c" -o "$BINARIES_DIR/user_syscall_process.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_process.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/graphics.c" -o "$BINARIES_DIR/user_syscall_graphics.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_graphics.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/mouse.c" -o "$BINARIES_DIR/user_syscall_mouse.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_mouse.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/window.c" -o "$BINARIES_DIR/user_syscall_window.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_window.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/controls.c" -o "$BINARIES_DIR/user_syscall_controls.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_controls.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/ui/iso_fs.c" -o "$BINARIES_DIR/user_syscall_iso_fs.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscall_iso_fs.o created${NC}"


echo -e "\n${YELLOW}[2i/5] Compiling pmm.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/memory/pmm.c" -o "$BINARIES_DIR/pmm.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ pmm.o created${NC}"

echo -e "\n${YELLOW}[2j/5] Compiling paging.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/memory/paging.c" -o "$BINARIES_DIR/paging.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ paging.o created${NC}"

echo -e "\n${YELLOW}[2j2/5] Compiling kheap.c (kernel heap)...${NC}"
i686-elf-gcc -c "$SRC_DIR/lib/kheap.c" -o "$BINARIES_DIR/kheap.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ kheap.o created${NC}"

echo -e "\n${YELLOW}[2j3/5] Compiling process_manager.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/process/process_manager.c" -o "$BINARIES_DIR/process_manager.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ process_manager.o created${NC}"

echo -e "\n${YELLOW}[2k/5] Compiling pit.c (timer)...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/timer/pit.c" -o "$BINARIES_DIR/pit.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ pit.o created${NC}"

echo -e "\n${YELLOW}[2l/5] Compiling scheduler.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/scheduler/scheduler.c" -o "$BINARIES_DIR/scheduler.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ scheduler.o created${NC}"

echo -e "\n${YELLOW}[2m/5] Assembling switch_osdev.s (context switch - OSDev approach)...${NC}"
i686-elf-as "$SRC_DIR/managers/scheduler/switch_osdev.s" -o "$BINARIES_DIR/switch.o"
echo -e "${GREEN}✓ switch.o created (OSDev approach)${NC}"

echo -e "\n${YELLOW}[2n/5] Compiling irq_manager.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/irq/irq_manager.c" -o "$BINARIES_DIR/irq_manager.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ irq_manager.o created${NC}"

echo -e "\n${YELLOW}[2o/5] Compiling klog.c (kernel logger manager)...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/klog/klog.c" -o "$BINARIES_DIR/klog.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ klog.o created${NC}"

echo -e "\n${YELLOW}[2o2/5] Compiling test_dummy.c (test file)...${NC}"
i686-elf-gcc -c "$SRC_DIR/lib/test_dummy.c" -o "$BINARIES_DIR/test_dummy.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ test_dummy.o created${NC}"

echo -e "\n${YELLOW}[2p/5] Compiling keyboard.c (keyboard driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/keyboard.c" -o "$BINARIES_DIR/keyboard.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ keyboard.o created${NC}"

echo -e "\n${YELLOW}[2q/5] Compiling font_manager.c (TrueType font rendering)...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/gui/font/font_manager.c" -o "$BINARIES_DIR/font_manager.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ font_manager.o created${NC}"

echo -e "\n${YELLOW}[2r/5] Compiling windows_mgmt.c (Window management system)...${NC}"
i686-elf-gcc -c "$SRC_DIR/managers/gui/windows/windows_mgmt.c" -o "$BINARIES_DIR/windows_mgmt.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ windows_mgmt.o created${NC}"

echo -e "\n${YELLOW}[3/5] Linking kernel...${NC}"
# keyboard.o ENABLED
i686-elf-ld -T "$SRC_DIR/loader/linker/linker.ld" -o "$BUILD_DIR/kernel.bin" \
    "$BINARIES_DIR/boot.o" "$BINARIES_DIR/kernel.o" "$BINARIES_DIR/vga.o" "$BINARIES_DIR/bga.o" "$BINARIES_DIR/bga_mouse.o" "$BINARIES_DIR/gfx.o" "$BINARIES_DIR/mouse.o" "$BINARIES_DIR/pci.o" "$BINARIES_DIR/usb.o" "$BINARIES_DIR/ata.o" "$BINARIES_DIR/iso9660.o" "$BINARIES_DIR/disk_subsystem.o" "$BINARIES_DIR/guimanager.o" "$BINARIES_DIR/gdt.o" "$BINARIES_DIR/idt.o" "$BINARIES_DIR/interrupt_stubs.o" "$BINARIES_DIR/exception_handler.o" "$BINARIES_DIR/ring3.o" "$BINARIES_DIR/syscall_dispatcher.o" "$BINARIES_DIR/syscall_process.o" "$BINARIES_DIR/syscall_memory.o" "$BINARIES_DIR/syscall_ui.o" "$BINARIES_DIR/syscall_ui_text_vga.o" "$BINARIES_DIR/syscall_ui_graphics.o" "$BINARIES_DIR/syscall_ui_window.o" "$BINARIES_DIR/syscall_ui_controls.o" "$BINARIES_DIR/syscall_ui_control_framework.o" "$BINARIES_DIR/syscall_ui_mouse_input.o" "$BINARIES_DIR/syscall_io.o" "$BINARIES_DIR/pmm.o" "$BINARIES_DIR/paging.o" "$BINARIES_DIR/kheap.o" "$BINARIES_DIR/process_manager.o" "$BINARIES_DIR/pit.o" "$BINARIES_DIR/scheduler.o" "$BINARIES_DIR/switch.o" "$BINARIES_DIR/irq_manager.o" "$BINARIES_DIR/klog.o" "$BINARIES_DIR/keyboard.o" "$BINARIES_DIR/font_manager.o" "$BINARIES_DIR/windows_mgmt.o"
echo -e "${GREEN}✓ kernel.bin created${NC}"

echo -e "\n${YELLOW}[4a/7] Building sysman (Ring 3 System Manager)...${NC}"
# Assemble sysman entry
i686-elf-as "$SRC_DIR/sysman/sysman_entry.s" -o "$BINARIES_DIR/sysman_entry.o"
echo -e "${GREEN}✓ sysman_entry.o created${NC}"

# Compile sysman C code (position-independent)
i686-elf-gcc -c "$SRC_DIR/sysman/sysman.c" -o "$BINARIES_DIR/sysman.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ sysman.o created (position-independent)${NC}"

# Compile user syscalls (position-independent) - STUB FILE
i686-elf-gcc -c "$SRC_DIR/system/syscalls/user/user_syscalls.c" -o "$BINARIES_DIR/user_syscalls.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ user_syscalls.o created (position-independent stub)${NC}"

# Compile Libraries (MaahiOS Application API)
echo -e "\n${YELLOW}Compiling Libraries (App API library components)...${NC}"

# GUI components
i686-elf-gcc -c "$SRC_DIR/Libraries/gui/window/window.c" -o "$BINARIES_DIR/lib_window.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_window.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/button/button.c" -o "$BINARIES_DIR/lib_button.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_button.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/label/label.c" -o "$BINARIES_DIR/lib_label.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_label.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/icon/icon.c" -o "$BINARIES_DIR/lib_icon.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_icon.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/list/list.c" -o "$BINARIES_DIR/lib_list.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_list.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/graphics/graphics.c" -o "$BINARIES_DIR/lib_graphics.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_graphics.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/text/text.c" -o "$BINARIES_DIR/lib_text.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_text.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/event/event.c" -o "$BINARIES_DIR/lib_event.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_event.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/gui/menu/menu.c" -o "$BINARIES_DIR/lib_menu.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_menu.o created${NC}"

# Core components
i686-elf-gcc -c "$SRC_DIR/Libraries/process/process.c" -o "$BINARIES_DIR/lib_process.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_process.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/filesystem/filesystem.c" -o "$BINARIES_DIR/lib_filesystem.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_filesystem.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/Libraries/debug/debug.c" -o "$BINARIES_DIR/lib_debug.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ lib_debug.o created${NC}"

# Link sysman as ELF (using Libraries + split user syscalls)
# Link sysman as ELF (using Libraries + split user syscalls)
i686-elf-ld -T "$SRC_DIR/sysman_linker.ld" -o "$BUILD_DIR/sysman.elf" \
    "$BINARIES_DIR/sysman_entry.o" "$BINARIES_DIR/sysman.o" \
    "$BINARIES_DIR/lib_window.o" "$BINARIES_DIR/lib_button.o" "$BINARIES_DIR/lib_label.o" "$BINARIES_DIR/lib_icon.o" "$BINARIES_DIR/lib_list.o" \
    "$BINARIES_DIR/lib_graphics.o" "$BINARIES_DIR/lib_text.o" "$BINARIES_DIR/lib_event.o" "$BINARIES_DIR/lib_menu.o" \
    "$BINARIES_DIR/lib_process.o" "$BINARIES_DIR/lib_filesystem.o" "$BINARIES_DIR/lib_debug.o" \
    "$BINARIES_DIR/user_syscall_io.o" "$BINARIES_DIR/user_syscall_memory.o" "$BINARIES_DIR/user_syscall_process.o" \
    "$BINARIES_DIR/user_syscall_graphics.o" "$BINARIES_DIR/user_syscall_mouse.o" "$BINARIES_DIR/user_syscall_window.o" \
    "$BINARIES_DIR/user_syscall_controls.o" "$BINARIES_DIR/user_syscall_iso_fs.o"
echo -e "${GREEN}✓ sysman.elf created (with Libraries + split syscalls)${NC}"

# Convert ELF to flat binary for position-independence
i686-elf-objcopy -O binary "$BUILD_DIR/sysman.elf" "$BUILD_DIR/sysman.bin"
echo -e "${GREEN}✓ sysman.bin created${NC}"

echo -e "\n${YELLOW}[4b/7] Building orbit (Ring 3 Shell)...${NC}"
# Assemble orbit entry
i686-elf-as "$SRC_DIR/orbit/orbit_entry.s" -o "$BINARIES_DIR/orbit_entry.o"
echo -e "${GREEN}✓ orbit_entry.o created${NC}"

# Compile orbit C code (position-independent)
i686-elf-gcc -c "$SRC_DIR/orbit/orbit.c" -o "$BINARIES_DIR/orbit.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ orbit.o created (position-independent)${NC}"

# Link orbit (with Libraries library and split user syscalls)
i686-elf-ld -T "$SRC_DIR/orbit/orbit_linker.ld" -o "$BUILD_DIR/orbit.elf" \
    "$BINARIES_DIR/orbit_entry.o" "$BINARIES_DIR/orbit.o" \
    "$BINARIES_DIR/user_syscall_io.o" "$BINARIES_DIR/user_syscall_memory.o" "$BINARIES_DIR/user_syscall_process.o" \
    "$BINARIES_DIR/user_syscall_graphics.o" "$BINARIES_DIR/user_syscall_mouse.o" "$BINARIES_DIR/user_syscall_window.o" \
    "$BINARIES_DIR/user_syscall_controls.o" "$BINARIES_DIR/user_syscall_iso_fs.o" \
    "$BINARIES_DIR/lib_window.o" "$BINARIES_DIR/lib_button.o" "$BINARIES_DIR/lib_label.o" "$BINARIES_DIR/lib_icon.o" "$BINARIES_DIR/lib_list.o" \
    "$BINARIES_DIR/lib_graphics.o" "$BINARIES_DIR/lib_text.o" "$BINARIES_DIR/lib_event.o" "$BINARIES_DIR/lib_menu.o" \
    "$BINARIES_DIR/lib_process.o" "$BINARIES_DIR/lib_filesystem.o" "$BINARIES_DIR/lib_debug.o"
echo -e "${GREEN}✓ orbit.elf created (with Libraries and split syscalls)${NC}"

# Convert ELF to flat binary
i686-elf-objcopy -O binary "$BUILD_DIR/orbit.elf" "$BUILD_DIR/orbit.bin"
echo -e "${GREEN}✓ orbit.bin created${NC}"

echo -e "\n${YELLOW}[4c/7] Building UIManager (Window Server)...${NC}"
# Compile cursor data
i686-elf-gcc -c "$SRC_DIR/libgui/cursor_data.c" -o "$BINARIES_DIR/cursor_data.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ cursor_data.o created${NC}"

# Compile BMP renderer (needed for cursor)
i686-elf-gcc -c "$SRC_DIR/libgui/bmp_renderer.c" -o "$BINARIES_DIR/bmp_renderer.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ bmp_renderer.o created${NC}"

# Compile themed button renderer
i686-elf-gcc -c "$SRC_DIR/uimanager/render/button/button_render.c" -o "$BINARIES_DIR/button_render.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ button_render.o created (themed button renderer)${NC}"

# Compile mouse cursor handler
i686-elf-gcc -c "$SRC_DIR/uimanager/events/mouse_cursor.c" -o "$BINARIES_DIR/mouse_cursor.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ mouse_cursor.o created (mouse cursor handler)${NC}"

# Assemble UIManager entry
i686-elf-as "$SRC_DIR/uimanager/uimanager_entry.s" -o "$BINARIES_DIR/uimanager_entry.o" --32
echo -e "${GREEN}✓ uimanager_entry.o created${NC}"

# Compile UIManager C code (position-independent)
i686-elf-gcc -c "$SRC_DIR/uimanager/uimanager.c" -o "$BINARIES_DIR/uimanager.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ uimanager.o created (position-independent)${NC}"

# Link UIManager - MINIMAL VERSION with proper mouse cursor module and split syscalls
i686-elf-ld -T "$SRC_DIR/uimanager/uimanager_linker.ld" -o "$BUILD_DIR/uimanager.elf" \
    "$BINARIES_DIR/uimanager_entry.o" "$BINARIES_DIR/uimanager.o" \
    "$BINARIES_DIR/mouse_cursor.o" "$BINARIES_DIR/cursor_data.o" \
    "$BINARIES_DIR/bmp_renderer.o" \
    "$BINARIES_DIR/user_syscall_io.o" "$BINARIES_DIR/user_syscall_memory.o" "$BINARIES_DIR/user_syscall_process.o" \
    "$BINARIES_DIR/user_syscall_graphics.o" "$BINARIES_DIR/user_syscall_mouse.o" "$BINARIES_DIR/user_syscall_window.o" \
    "$BINARIES_DIR/user_syscall_controls.o" "$BINARIES_DIR/user_syscall_iso_fs.o"
echo -e "${GREEN}✓ uimanager.elf created (minimal with mouse cursor module and split syscalls)${NC}"

# Convert ELF to flat binary
i686-elf-objcopy -O binary "$BUILD_DIR/uimanager.elf" "$BUILD_DIR/uimanager.bin"
echo -e "${GREEN}✓ uimanager.bin created${NC}"

echo -e "\n${YELLOW}[4d/7] Building File Manager (Application)...${NC}"
# Assemble file_manager entry
i686-elf-as "$SRC_DIR/apps/file_manager/file_manager_entry.s" -o "$BINARIES_DIR/file_manager_entry.o"
echo -e "${GREEN}✓ file_manager_entry.o created${NC}"

# Compile file_manager C code (position-independent)
i686-elf-gcc -c "$SRC_DIR/apps/file_manager/file_manager.c" -o "$BINARIES_DIR/file_manager.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ file_manager.o created (position-independent)${NC}"

# Link file_manager (reuse split user_syscalls and Libraries)
i686-elf-ld -T "$SRC_DIR/apps/file_manager/file_manager_linker.ld" -o "$BUILD_DIR/file_manager.elf" \
    "$BINARIES_DIR/file_manager_entry.o" "$BINARIES_DIR/file_manager.o" \
    "$BINARIES_DIR/user_syscall_io.o" "$BINARIES_DIR/user_syscall_memory.o" "$BINARIES_DIR/user_syscall_process.o" \
    "$BINARIES_DIR/user_syscall_graphics.o" "$BINARIES_DIR/user_syscall_mouse.o" "$BINARIES_DIR/user_syscall_window.o" \
    "$BINARIES_DIR/user_syscall_controls.o" "$BINARIES_DIR/user_syscall_iso_fs.o" \
    "$BINARIES_DIR/lib_window.o" "$BINARIES_DIR/lib_button.o" "$BINARIES_DIR/lib_label.o" "$BINARIES_DIR/lib_icon.o" "$BINARIES_DIR/lib_list.o" \
    "$BINARIES_DIR/lib_graphics.o" "$BINARIES_DIR/lib_text.o" "$BINARIES_DIR/lib_event.o" "$BINARIES_DIR/lib_menu.o" \
    "$BINARIES_DIR/lib_process.o" "$BINARIES_DIR/lib_filesystem.o" "$BINARIES_DIR/lib_debug.o"
echo -e "${GREEN}✓ file_manager.elf created (with split syscalls and Libraries)${NC}"

# Convert ELF to flat binary
i686-elf-objcopy -O binary "$BUILD_DIR/file_manager.elf" "$BUILD_DIR/file_manager.bin"
echo -e "${GREEN}✓ file_manager.bin created${NC}"

# Build disk_manager.bin (ring 3 process)
echo -e "\n${YELLOW}Building disk_manager.bin...${NC}"
i686-elf-as "$SRC_DIR/apps/disk_manager/disk_manager_entry.s" -o "$BINARIES_DIR/disk_manager_entry.o" --32
echo -e "${GREEN}✓ disk_manager_entry.o created${NC}"

# Compile disk_manager.c
i686-elf-gcc -c "$SRC_DIR/apps/disk_manager/disk_manager.c" -o "$BINARIES_DIR/disk_manager.o" \
    -std=gnu99 -ffreestanding -O2 -Wall -Wextra -nostdlib -nodefaultlibs \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ disk_manager.o created (position-independent)${NC}"

# Link disk_manager (reuse split user_syscalls)
i686-elf-ld -T "$SRC_DIR/apps/disk_manager/disk_manager_linker.ld" -o "$BUILD_DIR/disk_manager.elf" \
    "$BINARIES_DIR/disk_manager_entry.o" "$BINARIES_DIR/disk_manager.o" \
    "$BINARIES_DIR/user_syscall_io.o" "$BINARIES_DIR/user_syscall_memory.o" "$BINARIES_DIR/user_syscall_process.o" \
    "$BINARIES_DIR/user_syscall_graphics.o" "$BINARIES_DIR/user_syscall_mouse.o" "$BINARIES_DIR/user_syscall_window.o" \
    "$BINARIES_DIR/user_syscall_controls.o" "$BINARIES_DIR/user_syscall_iso_fs.o"
echo -e "${GREEN}✓ disk_manager.elf created (with split syscalls)${NC}"

# Convert ELF to flat binary
i686-elf-objcopy -O binary "$BUILD_DIR/disk_manager.elf" "$BUILD_DIR/disk_manager.bin"
echo -e "${GREEN}✓ disk_manager.bin created${NC}"

echo -e "\n${YELLOW}[5/7] Copying files to ISO directory...${NC}"
cp "$BUILD_DIR/kernel.bin" "$ISODIR/boot/kernel.bin"
echo -e "${GREEN}✓ kernel.bin copied${NC}"

cp "$BUILD_DIR/sysman.bin" "$ISODIR/boot/sysman.bin"
echo -e "${GREEN}✓ sysman.bin copied${NC}"

cp "$BUILD_DIR/uimanager.bin" "$ISODIR/boot/uimanager.bin"
echo -e "${GREEN}✓ uimanager.bin copied${NC}"

cp "$BUILD_DIR/orbit.bin" "$ISODIR/boot/orbit.bin"
echo -e "${GREEN}✓ orbit.bin copied${NC}"

cp "$BUILD_DIR/file_manager.bin" "$ISODIR/boot/file_manager.bin"
echo -e "${GREEN}✓ file_manager.bin copied${NC}"

cp "$BUILD_DIR/disk_manager.bin" "$ISODIR/boot/disk_manager.bin"
echo -e "${GREEN}✓ disk_manager.bin copied${NC}"

# Copy icons to ISO for runtime loading
mkdir -p "$ISODIR/icons"
cp "$SRC_DIR/images/icons/folder_32.bmp" "$ISODIR/icons/folder_32.bmp"
cp "$SRC_DIR/images/icons/file_32.bmp" "$ISODIR/icons/file_32.bmp"
cp "$SRC_DIR/images/icons/window_32.bmp" "$ISODIR/icons/window_32.bmp"
cp "$SRC_DIR/images/icons/folder_16.bmp" "$ISODIR/icons/folder_16.bmp"
cp "$SRC_DIR/images/icons/file_16.bmp" "$ISODIR/icons/file_16.bmp"
echo -e "${GREEN}✓ Icons copied to /icons/ (folder_32, file_32, window_32, folder_16, file_16)${NC}"

echo -e "\n${YELLOW}[6/7] Creating GRUB configuration...${NC}"

echo -e "\n${YELLOW}[6/7] Creating grub.cfg...${NC}"
cat > "$ISODIR/boot/grub/grub.cfg" << 'EOF'
set default=0
set timeout=0

menuentry "MaahiOS" {
    multiboot /boot/kernel.bin
    module /boot/sysman.bin
    module /boot/uimanager.bin
    module /boot/orbit.bin
    module /boot/file_manager.bin
    module /boot/disk_manager.bin
}
EOF
echo -e "${GREEN}✓ grub.cfg created (kernel + sysman + uimanager + orbit + file_manager + disk_manager)${NC}"

echo -e "\n${YELLOW}[7/7] Creating ISO image...${NC}"

grub-mkrescue -o "$BUILD_DIR/boot.iso" "$ISODIR" 2>&1
echo -e "${GREEN}✓ boot.iso created${NC}"

# Clean up .o files from build root - disabled for debugging
# rm -f "$BINARIES_DIR"/*.o
echo -e "${GREEN}✓ Object files kept for debugging${NC}"

echo -e "\n${YELLOW}======================================${NC}"
echo -e "${GREEN}Build Complete!${NC}"
echo -e "${GREEN}ISO Image: $BUILD_DIR/boot.iso${NC}"
echo -e "${YELLOW}======================================${NC}"
echo ""
echo -e "${YELLOW}To test in QEMU (with default PS/2 mouse):${NC}"
echo "  qemu-system-i386 -cdrom $BUILD_DIR/boot.iso -serial stdio"
