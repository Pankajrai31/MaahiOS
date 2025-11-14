; boot.asm - Multiboot bootloader entry point for MaahiOS

; Multiboot header constants
MBALIGN     equ  1 << 0              ; align loaded modules on page boundaries
MEMINFO     equ  1 << 1              ; provide memory map
FLAGS       equ  MBALIGN | MEMINFO   ; multiboot 'flag' field
MAGIC       equ  0x1BADB002          ; magic number for bootloader to find header
CHECKSUM    equ -(MAGIC + FLAGS)     ; checksum to prove we are multiboot

; Multiboot header
section .multiboot
align 4
    dd MAGIC
    dd FLAGS
    dd CHECKSUM

; Reserve stack space
section .bss
align 16
stack_bottom:
    resb 16384  ; 16 KiB
stack_top:

; Entry point
section .text
global _start:function (_start.end - _start)
_start:
    ; Set up the stack
    mov esp, stack_top

    ; Call the kernel main function
    extern kernel_main
    call kernel_main

    ; If kernel_main returns, halt the CPU
    cli
.hang:
    hlt
    jmp .hang
.end:
