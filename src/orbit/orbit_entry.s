# Orbit Entry Point (Ring 3)
# Assembly entry that calls C function

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
    
    # Call C function
    call orbit_main_c
    
    # If orbit_main_c returns (shouldn't happen), loop forever
hang:
    jmp hang
