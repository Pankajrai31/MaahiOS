.section .text
.extern sysman_main_c
.global sysman_main
.type sysman_main, @function

/* 
 * Ring 3 Entry Point
 * 
 * When Ring 3 is entered via IRET, execution jumps directly here.
 * Data segments (DS/ES/FS/GS) are already set to 0x23 by ring3_switch()
 */
sysman_main:
    /* Set up stack frame and call C code */
    push %ebp
    mov %esp, %ebp
    and $-16, %esp
    
    call sysman_main_c
    
    /* Halt when done */
    hlt
    jmp .
