/* UIManager Entry Point */
.global _start

.section .text
_start:
    /* Immediate debug output */
    pushl $msg_entry
    call syscall_puts
    addl $4, %esp
    
    /* CRITICAL: Set up Ring 3 data segments */
    /* Ring 3 code MUST use Ring 3 segments (0x23) to access memory */
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    pushl $msg_segments
    call syscall_puts
    addl $4, %esp
    
    /* Use the stack already set up by process_create */
    /* No need to allocate a new stack via syscall */
    
    /* Call C main function */
    call uimanager_main_c
    
    /* Should never return, but if it does, infinite loop */
hang:
    hlt
    jmp hang

.section .rodata
msg_entry:
    .asciz "[UIMAN_ASM] Entry point reached!\n"
msg_segments:
    .asciz "[UIMAN_ASM] Segments configured\n"
