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
i686-elf-as "$SRC_DIR/boot.s" -o "$BINARIES_DIR/boot.o"
echo -e "${GREEN}✓ boot.o created${NC}"

echo -e "\n${YELLOW}[2/5] Compiling kernel.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/kernel.c" -o "$BINARIES_DIR/kernel.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ kernel.o created${NC}"

echo -e "\n${YELLOW}[2b/5] Compiling vga.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/vga.c" -o "$BINARIES_DIR/vga.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ vga.o created${NC}"

echo -e "\n${YELLOW}[2b2/5] Compiling graphics.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/graphics.c" -o "$BINARIES_DIR/graphics.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ graphics.o created${NC}"

echo -e "\n${YELLOW}[2b3/5] Compiling vbe.c (VBE graphics)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/vbe.c" -o "$BINARIES_DIR/vbe.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ vbe.o created${NC}"

echo -e "\n${YELLOW}[2b4/5] Compiling bga.c (BGA graphics driver)...${NC}"
i686-elf-gcc -c "$SRC_DIR/drivers/bga.c" -o "$BINARIES_DIR/bga.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ bga.o created${NC}"

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

echo -e "\n${YELLOW}[2h/5] Compiling syscall_handler.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/syscalls/syscall_handler.c" -o "$BINARIES_DIR/syscall_handler.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ syscall_handler.o created${NC}"

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

echo -e "\n${YELLOW}[3/5] Linking kernel...${NC}"
i686-elf-ld -T "$SRC_DIR/linker.ld" -o "$BUILD_DIR/kernel.bin" \
    "$BINARIES_DIR/boot.o" "$BINARIES_DIR/kernel.o" "$BINARIES_DIR/vga.o" "$BINARIES_DIR/graphics.o" "$BINARIES_DIR/vbe.o" "$BINARIES_DIR/bga.o" "$BINARIES_DIR/mouse.o" "$BINARIES_DIR/pci.o" "$BINARIES_DIR/usb.o" "$BINARIES_DIR/gdt.o" "$BINARIES_DIR/idt.o" "$BINARIES_DIR/interrupt_stubs.o" "$BINARIES_DIR/exception_handler.o" "$BINARIES_DIR/ring3.o" "$BINARIES_DIR/syscall_handler.o" "$BINARIES_DIR/pmm.o" "$BINARIES_DIR/paging.o" "$BINARIES_DIR/kheap.o" "$BINARIES_DIR/process_manager.o" "$BINARIES_DIR/pit.o" "$BINARIES_DIR/scheduler.o" "$BINARIES_DIR/switch.o" "$BINARIES_DIR/irq_manager.o"
echo -e "${GREEN}✓ kernel.bin created${NC}"

echo -e "\n${YELLOW}[4a/7] Building sysman (Ring 3 System Manager)...${NC}"
# Assemble sysman entry
i686-elf-as "$SRC_DIR/sysman/sysman_entry.s" -o "$BINARIES_DIR/sysman_entry.o"
echo -e "${GREEN}✓ sysman_entry.o created${NC}"

# Compile sysman C code (position-independent)
i686-elf-gcc -c "$SRC_DIR/sysman/sysman.c" -o "$BINARIES_DIR/sysman.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ sysman.o created (position-independent)${NC}"

# Compile user syscalls (position-independent)
i686-elf-gcc -c "$SRC_DIR/syscalls/user_syscalls.c" -o "$BINARIES_DIR/user_syscalls.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ user_syscalls.o created (position-independent)${NC}"

# Compile LibGUI for sysman (position-independent)
i686-elf-gcc -c "$SRC_DIR/libgui/draw.c" -o "$BINARIES_DIR/sysman_gui_draw.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ sysman_gui_draw.o created${NC}"

# Link sysman as ELF (with libgui support)
i686-elf-ld -T "$SRC_DIR/sysman_linker.ld" -o "$BUILD_DIR/sysman.elf" \
    "$BINARIES_DIR/sysman_entry.o" "$BINARIES_DIR/sysman.o" "$BINARIES_DIR/user_syscalls.o" "$BINARIES_DIR/sysman_gui_draw.o"
echo -e "${GREEN}✓ sysman.elf created (with libgui)${NC}"

# Convert ELF to flat binary for position-independence
i686-elf-objcopy -O binary "$BUILD_DIR/sysman.elf" "$BUILD_DIR/sysman.bin"
echo -e "${GREEN}✓ sysman.bin created${NC}"

echo -e "\n${YELLOW}[4b/7] Building orbit (Ring 3 Shell)...${NC}"
# Assemble orbit entry
i686-elf-as "$SRC_DIR/orbit/orbit_entry.s" -o "$BINARIES_DIR/orbit_entry.o"
echo -e "${GREEN}✓ orbit_entry.o created${NC}"

# Compile LibGUI (position-independent)
i686-elf-gcc -c "$SRC_DIR/libgui/draw.c" -o "$BINARIES_DIR/gui_draw.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ gui_draw.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/libgui/window.c" -o "$BINARIES_DIR/gui_window.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ gui_window.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/libgui/controls.c" -o "$BINARIES_DIR/gui_controls.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ gui_controls.o created${NC}"

i686-elf-gcc -c "$SRC_DIR/libgui/cursor.c" -o "$BINARIES_DIR/gui_cursor.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ gui_cursor.o created${NC}"

# Compile orbit C code (position-independent)
i686-elf-gcc -c "$SRC_DIR/orbit/orbit.c" -o "$BINARIES_DIR/orbit.o" \
    -ffreestanding -fno-stack-protector -fPIC -m32
echo -e "${GREEN}✓ orbit.o created (position-independent)${NC}"

# Link orbit with LibGUI (uses syscalls only)
i686-elf-ld -T "$SRC_DIR/orbit/orbit_linker.ld" -o "$BUILD_DIR/orbit.elf" \
    "$BINARIES_DIR/orbit_entry.o" "$BINARIES_DIR/orbit.o" \
    "$BINARIES_DIR/gui_draw.o" "$BINARIES_DIR/gui_window.o" \
    "$BINARIES_DIR/gui_controls.o" "$BINARIES_DIR/gui_cursor.o" \
    "$BINARIES_DIR/user_syscalls.o"
echo -e "${GREEN}✓ orbit.elf created (with mouse support)${NC}"

# Convert ELF to flat binary
i686-elf-objcopy -O binary "$BUILD_DIR/orbit.elf" "$BUILD_DIR/orbit.bin"
echo -e "${GREEN}✓ orbit.bin created${NC}"

echo -e "\n${YELLOW}[5/7] Copying files to ISO directory...${NC}"
cp "$BUILD_DIR/kernel.bin" "$ISODIR/boot/kernel.bin"
echo -e "${GREEN}✓ kernel.bin copied${NC}"

cp "$BUILD_DIR/sysman.bin" "$ISODIR/boot/sysman.bin"
echo -e "${GREEN}✓ sysman.bin copied${NC}"

cp "$BUILD_DIR/orbit.bin" "$ISODIR/boot/orbit.bin"
echo -e "${GREEN}✓ orbit.bin copied${NC}"

echo -e "\n${YELLOW}[6/7] Creating grub.cfg...${NC}"
cat > "$ISODIR/boot/grub/grub.cfg" << 'EOF'
set default=0
set timeout=0

menuentry "MaahiOS" {
    multiboot /boot/kernel.bin
    module /boot/sysman.bin
    module /boot/orbit.bin
}
EOF
echo -e "${GREEN}✓ grub.cfg created (kernel + sysman + orbit)${NC}"

echo -e "\n${YELLOW}[7/7] Creating ISO image...${NC}"

grub-mkrescue -o "$BUILD_DIR/boot.iso" "$ISODIR" 2>&1
echo -e "${GREEN}✓ boot.iso created${NC}"

# Clean up .o files from build root
rm -f "$BINARIES_DIR"/*.o
echo -e "${GREEN}✓ Object files cleaned${NC}"

echo -e "\n${YELLOW}======================================${NC}"
echo -e "${GREEN}Build Complete!${NC}"
echo -e "${GREEN}ISO Image: $BUILD_DIR/boot.iso${NC}"
echo -e "${YELLOW}======================================${NC}"
echo ""
echo -e "${YELLOW}To test in QEMU (with default PS/2 mouse):${NC}"
echo "  qemu-system-i386 -cdrom $BUILD_DIR/boot.iso -serial stdio"
