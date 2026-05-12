/* NexusOS kernel linker script.
 *
 * The bootloader loads the kernel image at physical address 0x10000
 * and jumps to it in 32-bit protected mode. kernel_entry.asm provides
 * the _start symbol, which must end up at the very beginning of the
 * output (first object on the ld command line).
 */

OUTPUT_FORMAT("elf64-x86-64")
ENTRY(_start)

SECTIONS
{
    . = 0x10000;

    .text ALIGN(16) : {
        *(.text._start)
        *(.text)
        *(.text.*)
    }

    .rodata ALIGN(16) : {
        *(.rodata)
        *(.rodata.*)
    }

    .data ALIGN(16) : {
        *(.data)
        *(.data.*)
    }

    .bss ALIGN(16) : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    }

    /DISCARD/ : {
        *(.comment)
        *(.note*)
        *(.eh_frame*)
    }
}
