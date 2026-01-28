/* Linker script for Disk Manager (Ring 3 process) */

ENTRY(_start)

SECTIONS
{
    /* Start at 5MB mark (after file_manager at 4MB) */
    . = 0x00500000;
    
    .text : {
        *(.text)
    }
    
    .data : {
        *(.data)
        *(.rodata)
    }
    
    .bss : {
        *(.bss)
    }
}
