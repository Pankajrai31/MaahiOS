.section .text
.extern sysman_main_c
.global sysman_main
.type sysman_main, @function

/* 
 * Ring 3 Entry Point
 * 
 * When Ring 3 is entered via IRET, execution jumps directly here.
 * This is the very first code that runs in user mode.
 */
sysman_main:
    /* Set up Ring 3 data segments (IRET only loads CS and SS) */
    mov $0x23, %ax
    mov %ax, %ds
    mov %ax, %es
    mov %ax, %fs
    mov %ax, %gs
    
    /* Set up stack frame and call C code */
    push %ebp
    mov %esp, %ebp
    and $-16, %esp
    
    call sysman_main_c
    
    /* Halt when done */
    hlt
    jmp .
