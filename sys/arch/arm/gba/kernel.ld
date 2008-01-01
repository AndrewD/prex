OUTPUT_FORMAT("elf32-littlearm", "elf32-bigarm", "elf32-littlearm")
OUTPUT_ARCH(arm)
ENTRY(kernel_start)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS
{
	. = 0x02000000 + SIZEOF_HEADERS;

	.text : {
		*(.text)
		*(.glue_7)
		*(.glue_7t)
	} : text = 0xff

	. = ALIGN(4);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	.data ALIGN(4) : {
		*(.data)
	} : data = 0xff

	. = ALIGN(4);
	__bss = . ;
	.bss .    : {
		*(.bss)
		*(COMMON)
	}
	. = ALIGN(4);
	__end = . ;

	. = 0x3000000;
	__iwram = .;
	.iwram : {
		*(.iwram)
	} = 0xff

	/DISCARD/ : { *(.comment .note) }
}
