ENTRY(Reset_Handler)

MEMORY {
    sram  : ORIGIN = 0x20000000, LENGTH = 20K
    flash : ORIGIN = 0x08000000, LENGTH = 128K
}

SECTIONS {
    .vectors : {
        *(.isr_vector)
    } > flash

    .text : {
        *(.text*)
        *(.rodata*)

        . = ALIGN(4);

        _sidata = ABSOLUTE(.);
    } > flash

    .data : {
        _sdata = ABSOLUTE(.);

        *(.data)

        . = ALIGN(4);

        _edata = ABSOLUTE(.);
    } > sram AT > flash

    .bss : {
        _sbss = ABSOLUTE(.);
        *(.bss)

        . = ALIGN(4);

        _ebss = ABSOLUTE(.);
    } > sram

    .stack : {
        _sstack = ABSOLUTE(.);

        . = . + 2K;

        _estack = ABSOLUTE(.);
    } > sram

    . = ALIGN(8);

    _sheap = ABSOLUTE(.);
    _eheap = ABSOLUTE(0x20000000 + 20K);

    /DISCARD/ : {
        *(.comment)
    }
}