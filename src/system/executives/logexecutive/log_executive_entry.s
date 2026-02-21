# Log Executive Entry Point
# Sets up Ring 3 segments and calls exe_log_main()

.section .text
.global _start

_start:
    # CRITICAL: Set up Ring 3 data segments immediately
    # The ring3_switch() function leaves DS/ES/FS/GS as kernel (0x10)
    # Ring 3 code MUST use Ring 3 segments (0x23) to access memory
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    # Call Log Executive main function
    call exe_log_main
    
    # Should never return, but loop just in case
halt_loop:
    hlt
    jmp halt_loop
