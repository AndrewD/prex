/*
 * Linker script for kernel
 */
INCLUDE config.ld
OUTPUT_FORMAT("elf32-powerpc", "elf32-powerpc", "elf32-powerpc")
OUTPUT_ARCH(powerpc)
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
		*(.sdata)
		*(.sdata2)
	} : data

	__bss = .;
	.bss .    : {
		*(.sbss)
		*(.bss)
		*(COMMON)
	}
	. = ALIGN(32 / 8);
	__end = .;

	/DISCARD/ : { *(.comment .note) }
}
