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
    
    /* CRITICAL: Set up Ring 3 data segments */
    /* Ring 3 code MUST use Ring 3 segments (0x23) to access memory */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    /* Debug: segments set */
    pushl %eax
    pushl %edx
    movl $0x3F8, %edx
    movb $'S', %al
    outb %al, %dx
    movb $'E', %al
    outb %al, %dx
    movb $'G', %al
    outb %al, %dx
    movb $0x0A, %al
    outb %al, %dx
    popl %edx
    popl %eax
    
    /* Debug: About to call C function */
    pushl %eax
    pushl %edx
    movl $0x3F8, %edx
    movb $'[', %al
    outb %al, %dx
    movb $'_', %al
    outb %al, %dx
    movb $'S', %al
    outb %al, %dx
    movb $'T', %al
    outb %al, %dx
    movb $'A', %al
    outb %al, %dx
    movb $'R', %al
    outb %al, %dx
    movb $'T', %al
    outb %al, %dx
    movb $']', %al
    outb %al, %dx
    movb $0x0A, %al
    outb %al, %dx
    popl %edx
    popl %eax
    
    /* Call C main function */
    call uimanager_main_c
    
    /* Debug: returned from C */
    pushl %eax
    pushl %edx
    movl $0x3F8, %edx
    movb $'R', %al
    outb %al, %dx
    movb $'E', %al
    outb %al, %dx
    movb $'T', %al
    outb %al, %dx
    movb $0x0A, %al
    outb %al, %dx
    popl %edx
    popl %eax
    
    /* Should never return, but if it does, infinite loop */
hang:
    hlt
    jmp hang
