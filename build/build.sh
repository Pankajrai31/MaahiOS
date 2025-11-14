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

echo -e "\n${YELLOW}[2e1/5] Assembling ring3_switch.s...${NC}"
i686-elf-as "$SRC_DIR/managers/ring3/ring3_switch.s" -o "$BINARIES_DIR/ring3_switch.o"
echo -e "${GREEN}✓ ring3_switch.o created${NC}"

echo -e "\n${YELLOW}[2e2/5] Assembling sysman_entry.s...${NC}"
i686-elf-as "$SRC_DIR/sysman/sysman_entry.s" -o "$BINARIES_DIR/sysman_entry.o"
echo -e "${GREEN}✓ sysman_entry.o created${NC}"

echo -e "\n${YELLOW}[2f/5] Compiling sysman.c...${NC}"
i686-elf-gcc -c "$SRC_DIR/sysman/sysman.c" -o "$BINARIES_DIR/sysman.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ sysman.o created (will be linked separately)${NC}"

echo -e "\n${YELLOW}[2g/5] Compiling user_syscalls.c (OSDev inline asm)...${NC}"
i686-elf-gcc -c "$SRC_DIR/syscalls/user_syscalls.c" -o "$BINARIES_DIR/user_syscalls.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ user_syscalls.o created${NC}"

echo -e "\n${YELLOW}[2g1/5] Compiling heap.c (user library)...${NC}"
i686-elf-gcc -c "$SRC_DIR/lib/heap.c" -o "$BINARIES_DIR/heap.o" \
    -ffreestanding -fno-stack-protector -fno-pic -fno-pie -m32
echo -e "${GREEN}✓ heap.o created${NC}"

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

echo -e "\n${YELLOW}[3/5] Linking kernel (WITHOUT sysman)...${NC}"
i686-elf-ld -T "$SRC_DIR/linker.ld" -o "$BUILD_DIR/kernel.bin" \
    "$BINARIES_DIR/boot.o" "$BINARIES_DIR/kernel.o" "$BINARIES_DIR/vga.o" "$BINARIES_DIR/graphics.o" "$BINARIES_DIR/gdt.o" "$BINARIES_DIR/idt.o" "$BINARIES_DIR/interrupt_stubs.o" "$BINARIES_DIR/exception_handler.o" "$BINARIES_DIR/ring3.o" "$BINARIES_DIR/ring3_switch.o" "$BINARIES_DIR/user_syscalls.o" "$BINARIES_DIR/syscall_handler.o" "$BINARIES_DIR/pmm.o" "$BINARIES_DIR/paging.o"
echo -e "${GREEN}✓ kernel.bin created${NC}"

echo -e "\n${YELLOW}[3b/5] Linking sysman.bin (separate user-mode binary)...${NC}"
i686-elf-ld -T "$SRC_DIR/sysman_linker.ld" -o "$BUILD_DIR/sysman.elf" \
    "$BINARIES_DIR/sysman_entry.o" "$BINARIES_DIR/sysman.o" "$BINARIES_DIR/user_syscalls.o" "$BINARIES_DIR/heap.o"
echo -e "${GREEN}✓ sysman.elf created${NC}"

echo -e "\n${YELLOW}[3c/5] Creating flat binary from ELF...${NC}"
i686-elf-objcopy -O binary "$BUILD_DIR/sysman.elf" "$BUILD_DIR/sysman.bin"
echo -e "${GREEN}✓ sysman.bin (flat binary) created${NC}"

echo -e "\n${YELLOW}[4/5] Copying binaries to ISO directory...${NC}"
cp "$BUILD_DIR/kernel.bin" "$ISODIR/boot/kernel.bin"
cp "$BUILD_DIR/sysman.bin" "$ISODIR/boot/sysman.bin"
echo -e "${GREEN}✓ kernel.bin and sysman.bin copied to ISO directory${NC}"

echo -e "\n${YELLOW}[5/5] Creating grub.cfg...${NC}"
cat > "$ISODIR/boot/grub/grub.cfg" << 'EOF'
set default=0
set timeout=0

menuentry "MaahiOS" {
    multiboot /boot/kernel.bin
    module /boot/sysman.bin
}
EOF
echo -e "${GREEN}✓ grub.cfg created${NC}"

echo -e "\n${YELLOW}[6/5] Creating ISO image...${NC}"

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
echo -e "${YELLOW}To test in QEMU:${NC}"
echo "  qemu-system-i386 -cdrom $BUILD_DIR/boot.iso -serial stdio"
