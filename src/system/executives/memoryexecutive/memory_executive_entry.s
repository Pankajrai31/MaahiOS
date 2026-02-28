# Memory Executive Entry Point
# Sets up Ring 3 segments, zeros .bss, calls exe_memory_main()

.section .text
.global _start

_start:
    # CRITICAL: Set up Ring 3 data segments immediately
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    # Zero .bss section
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    shrl $2, %ecx
    xorl %eax, %eax
    rep stosl
    
    # Call Memory Executive main function
    call exe_memory_main
    
    # Should never return, but loop just in case
halt_loop:
    hlt
    jmp halt_loop
