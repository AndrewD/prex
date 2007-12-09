OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(_start)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS
{
	. = SIZEOF_HEADERS;

	__text = .;
	.text : {
		*(.text)
		*(.glue_7)
		*(.glue_7t)
	} : text = 0xff
	__etext = .;

	. = ALIGN(4);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	. = ALIGN(4);
	.data : {
		*(.data)
	} : data

	__edata = .;

	. = ALIGN(4);
	__bss = . ;
	.bss .  : {
		*(.bss)
		*(COMMON)
	}

	/DISCARD/ : { *(.comment .note) }
}
