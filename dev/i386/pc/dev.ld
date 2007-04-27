/* Linker script for driver */
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(driver_main)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS
{
	. = SIZEOF_HEADERS ;

	.text : {
		*(.text)
	} : text = 0x9090

	. = ALIGN(32);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	.kstrtab : {
		*(.kstrtab)
	} : text

	.ksymtab ALIGN(4) : {
		*(.ksymtab)
	} : text

	. = ALIGN(32);
	.data : {
		*(.data)
	} : data

	.bss . : {
		*(.bss)
		*(COMMON)
	}
	/DISCARD/ : { *(.comment .note) }
}
