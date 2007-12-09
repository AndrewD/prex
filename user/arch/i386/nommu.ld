OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(_start)
SECTIONS
{
	. = SIZEOF_HEADERS ;

	.text : {
		*(.text)
	} = 0x9090
        _etext = .;
	PROVIDE (etext = .);

	. = ALIGN(32);

	.rodata : {
		*(.rodata)
		*(.rodata.*)
	}

	. = ALIGN(32);
	.data : {
		*(.data)
	}

        _edata = .;
        PROVIDE(edata = .);
	__bss_start = .;

	.bss . : {
		*(.bss)
		*(COMMON)
	}
	_end = .;
	PROVIDE (end = .);

	/DISCARD/ : {
		*(.comment .note .eh_frame)
	}
}
