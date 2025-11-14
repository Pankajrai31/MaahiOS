# Makefile for MaahiOS

# Toolchain
AS = nasm
CC = gcc
LD = ld

# Flags
ASFLAGS = -f elf32
CFLAGS = -m32 -std=gnu99 -ffreestanding -O2 -Wall -Wextra -fno-pie -fno-stack-protector
LDFLAGS = -m elf_i386 -T linker.ld -nostdlib

# Source files
ASM_SOURCES = boot.asm
C_SOURCES = kernel.c

# Object files
ASM_OBJECTS = $(ASM_SOURCES:.asm=.o)
C_OBJECTS = $(C_SOURCES:.c=.o)
OBJECTS = $(ASM_OBJECTS) $(C_OBJECTS)

# Output
KERNEL = maahios.bin

# Default target
all: $(KERNEL)

# Link the kernel
$(KERNEL): $(OBJECTS)
	$(LD) $(LDFLAGS) -o $@ $(OBJECTS)

# Compile assembly files
%.o: %.asm
	$(AS) $(ASFLAGS) $< -o $@

# Compile C files
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# Create bootable ISO (requires grub-mkrescue)
iso: $(KERNEL)
	mkdir -p isodir/boot/grub
	cp $(KERNEL) isodir/boot/$(KERNEL)
	echo 'set timeout=0' > isodir/boot/grub/grub.cfg
	echo 'set default=0' >> isodir/boot/grub/grub.cfg
	echo '' >> isodir/boot/grub/grub.cfg
	echo 'menuentry "MaahiOS" {' >> isodir/boot/grub/grub.cfg
	echo '	multiboot /boot/$(KERNEL)' >> isodir/boot/grub/grub.cfg
	echo '	boot' >> isodir/boot/grub/grub.cfg
	echo '}' >> isodir/boot/grub/grub.cfg
	grub-mkrescue -o maahios.iso isodir

# Run in QEMU emulator
run: $(KERNEL)
	qemu-system-i386 -kernel $(KERNEL)

# Run ISO in QEMU
run-iso: iso
	qemu-system-i386 -cdrom maahios.iso

# Clean build artifacts
clean:
	rm -f $(OBJECTS) $(KERNEL) maahios.iso
	rm -rf isodir

# Phony targets
.PHONY: all iso run run-iso clean
