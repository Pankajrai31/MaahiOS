# Orbit Entry Point (Ring 3)
# Assembly entry that calls C main function

.section .text
.global _start

_start:
    # Call C function
    call orbit_main_c
    
    # If orbit_main_c returns (shouldn't happen), loop forever
hang:
    jmp hang
