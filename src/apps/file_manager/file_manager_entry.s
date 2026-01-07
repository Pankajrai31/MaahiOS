# File Manager Entry Point (Ring 3)
# Assembly entry that calls C function

.section .text
.global _start

_start:
    # Set up Ring 3 data segments
    # Ring 3 code MUST use Ring 3 segments (0x23) to access memory
    movw $0x23, %ax
    movw %ax, %ds
    movw %ax, %es
    movw %ax, %fs
    movw %ax, %gs
    
    # Call C function
    call file_manager_main_c
    
    # If file_manager_main_c returns, loop forever
hang:
    jmp hang
