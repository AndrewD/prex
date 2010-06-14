/*
 * Linker script for kernel
 */
INCLUDE config.ld
OUTPUT_FORMAT("elf32-i386", "elf32-i386", "elf32-i386")
OUTPUT_ARCH(i386)
ENTRY(kernel_start)
PHDRS
{
	text PT_LOAD FILEHDR PHDRS;
	data PT_LOAD;
}
SECTIONS
{
	. = CONFIG_KERNEL_TEXT + SIZEOF_HEADERS;

	.text : {
		*(.text)
	} : text = 0x9090

	. = ALIGN(32);
	.rodata : {
		*(.rodata)
		*(.rodata.*)
	} : text

	. = ALIGN(32);
	.data : {
		*(.data)
	} : data

	__bss = .;
	.bss .    : {
		*(.bss)
		*(COMMON)
	}
	. = ALIGN(32 / 8);
	__end = .;

	/DISCARD/ : { *(.comment .note) }
}
