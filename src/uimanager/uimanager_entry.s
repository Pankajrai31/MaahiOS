/* UIManager Entry Point */
.global _start

.section .text
_start:
    /* DEBUG: Print marker immediately */
    pushl %eax
    pushl %edx
    movl $0x3F8, %edx
    movb $'[', %al
    outb %al, %dx
    movb $'U', %al
    outb %al, %dx
    movb $'I', %al
    outb %al, %dx
    movb $']', %al
    outb %al, %dx
    movb $0x0A, %al  /* newline */
    outb %al, %dx
    popl %edx
    popl %eax
    
    /* Call C main function */
    call uimanager_main_c
    
    /* Should never return, but if it does, infinite loop */
hang:
    hlt
    jmp hang
