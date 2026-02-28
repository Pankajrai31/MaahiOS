/*
 * mex_app.ld - Standard linker script for MaahiOS .mex applications
 *
 * All .mex apps link at 0x10000000. Per-process page tables
 * ensure each process has its own mapping at this address.
 *
 * Build: i686-elf-ld -T mex_app.ld -o app.elf app.o
 * Then:  i686-elf-objcopy -O binary app.elf app.bin
 * Then:  python mex_pack.py app.bin app.mex --name "app" --type app --flags console
 */

ENTRY(_start)

SECTIONS
{
    /* All .mex apps load at this virtual address */
    . = 0x10000000;

    .text ALIGN(4) :
    {
        *(.text._start)     /* Entry point first */
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(4) :
    {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(4) :
    {
        *(.data)
        *(.data.*)
    }

    /* BSS must be last loadable section */
    .bss ALIGN(4) :
    {
        __bss_start = .;
        *(.bss)
        *(.bss.*)
        *(COMMON)
        __bss_end = .;
    }

    /* Export sizes for mex_pack.py --bss-size */
    __bss_size = __bss_end - __bss_start;

    /DISCARD/ :
    {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
