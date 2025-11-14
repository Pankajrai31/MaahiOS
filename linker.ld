/* linker.ld - Linker script for MaahiOS kernel */

/* Entry point of the kernel */
ENTRY(_start)

/* Define the sections */
SECTIONS
{
    /* Start at 1 MiB, the standard location for kernels */
    . = 1M;

    /* Multiboot header comes first */
    .multiboot :
    {
        *(.multiboot)
    }

    /* Followed by the text (code) section */
    .text ALIGN(4K) : 
    {
        *(.text)
    }

    /* Read-only data */
    .rodata ALIGN(4K) : 
    {
        *(.rodata)
    }

    /* Read-write data (initialized) */
    .data ALIGN(4K) : 
    {
        *(.data)
    }

    /* Read-write data (uninitialized) and stack */
    .bss ALIGN(4K) : 
    {
        *(COMMON)
        *(.bss)
    }
}
