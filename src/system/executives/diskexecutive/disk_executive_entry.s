# Disk Executive Entry Point
.section .text
.global _start

_start:
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    call exe_disk_main
    
halt_loop:
    hlt
    jmp halt_loop
