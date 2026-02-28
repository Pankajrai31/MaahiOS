/* Sysman Linker Script */
OUTPUT_FORMAT("elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(sysman_main)

SECTIONS
{
    /* All user processes load at this virtual address.
     * Per-process page directories ensure each process has its own mapping. */
    . = 0x10000000;

    .text : {
        *(.text*)
    }

    .rodata : {
        *(.rodata*)
    }

    .data : {
        *(.data*)
    }

    .bss : {
        __bss_start = .;
        *(COMMON)
        *(.bss*)
        __bss_end = .;
    }
}
