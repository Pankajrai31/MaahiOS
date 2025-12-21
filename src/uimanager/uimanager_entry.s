/* UIManager Entry Point */
.global _start

.section .text
_start:
    /* Call C main function */
    call uimanager_main_c
    
    /* Should never return, but if it does, infinite loop */
hang:
    hlt
    jmp hang
