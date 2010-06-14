OUTPUT_FORMAT("elf32-powerpc")
OUTPUT_ARCH(powerpc)
ENTRY(_start)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS {
	. = 0x10000 + SIZEOF_HEADERS;

	.text . : {
		*(.text)
	}

	. = ALIGN(4);
	.rodata . : {
		*(.rodata)
		*(.rodata.*)
	}

	. = ALIGN(0x1000);
	.data . : {
		*(.data)
		*(.sdata)
		*(.sdata2)
	}

	. = ALIGN(4);
	.bss . : {
		*(.sbss)
		*(.bss)
		*(COMMON)
	}

	/DISCARD/ : { *(.comment .note) }
}
