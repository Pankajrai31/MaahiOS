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
    /* DEBUG: Print immediately on entry */
    pushl %eax
    pushl %edx
    movl $0x3F8, %edx
    movb $'[', %al
    outb %al, %dx
    movb $'S', %al
    outb %al, %dx
    movb $'Y', %al
    outb %al, %dx
    movb $'S', %al
    outb %al, %dx
    movb $']', %al
    outb %al, %dx
    movb $'\n', %al
    outb %al, %dx
    popl %edx
    popl %eax
    
    /* Set up Ring 3 data segments */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    /* Set up stack frame and call C code */
    push %ebp
    mov %esp, %ebp
    and $-16, %esp
    
    call sysman_main_c
    
    /* Halt when done */
    hlt
    jmp .
