.section .text
.extern sysman_main_c
.global sysman_main
.type sysman_main, @function

/* 
 * Ring 3 Entry Point
 * 
 * When Ring 3 is entered via IRET, execution jumps directly here.
 * CRITICAL: ring3_switch() leaves DS/ES/FS/GS as kernel (0x10)
 * Ring 3 code MUST set them to user segments (0x23) immediately
 */
sysman_main:
    /* Set up Ring 3 data segments */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    /* Zero .bss section using direct addresses.
     * Sysman is linked at 0x10000000 with per-process page directory.
     * __bss_start/__bss_end are absolute runtime virtual addresses. */
    movl $__bss_start, %edi
    movl $__bss_end, %ecx
    subl %edi, %ecx
    shrl $2, %ecx                               /* count in dwords */
    xorl %eax, %eax
    rep stosl
    
    /* Set up stack frame and call C code */
    push %ebp
    mov %esp, %ebp
    and $-16, %esp
    
    call sysman_main_c
    
    /* Halt when done */
    hlt
    jmp .
