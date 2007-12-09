/*
 * Copyright (c) 2005, Kohsuke Ohtani
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the author nor the names of any co-contributors 
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE AUTHOR AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE AUTHOR OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 */

/*
 * Reference:
 *
 *  Portable Formats Specification, Version 1.1
 *   - Tool Interface Standards (TIS)
 */

#ifndef _ELF_H
#define _ELF_H

/*
 * 32-bit data types
 */
typedef unsigned long	Elf32_Addr;
typedef unsigned short	Elf32_Half;
typedef unsigned long	Elf32_Off;
typedef signed long	Elf32_Sword;
typedef unsigned long	Elf32_Word;
typedef unsigned long	Elf32_Size;

/*
 * ELF header
 *
 * Figure 1-3.
 */
#define EI_NIDENT 16

typedef struct {
	unsigned char e_ident[EI_NIDENT];	/* File identification */
	Elf32_Half e_type;	/* File type */
	Elf32_Half e_machine;	/* Machine architecture */
	Elf32_Word e_version;	/* ELF format version */
	Elf32_Addr e_entry;	/* Entry point */
	Elf32_Off e_phoff;	/* Program header file offset */
	Elf32_Off e_shoff;	/* Section header file offset */
	Elf32_Word e_flags;	/* Architecture-specific flags */
	Elf32_Half e_ehsize;	/* Size of ELF header in bytes */
	Elf32_Half e_phentsize;	/* Size of program header entry */
	Elf32_Half e_phnum;	/* Number of program header entries */
	Elf32_Half e_shentsize;	/* Size of section header entry */
	Elf32_Half e_shnum;	/* Number of section header entries */
	Elf32_Half e_shstrndx;	/* Section name strings section */
} Elf32_Ehdr;


/*
 * e_ident[] identification indexes
 */
#define EI_MAG0		0
#define EI_MAG1		1
#define EI_MAG2		2
#define EI_MAG3		3
#define EI_CLASS	4
#define EI_DATA		5
#define EI_VERSION	6
#define EI_PAD		7

/*
 * magic number
 */
#define ELFMAG0		0x7f	/* e_ident[EI_MAG0] */
#define ELFMAG1		'E'	/* e_ident[EI_MAG1] */
#define ELFMAG2		'L'	/* e_ident[EI_MAG2] */
#define ELFMAG3		'F'	/* e_ident[EI_MAG3] */

/*
 * file class or capacity
 */
#define ELFCLASSNONE	0	/* Invalid class */
#define ELFCLASS32	1	/* 32-bit class */
#define ELFCLASS64	2	/* 64-bit class */

/*
 * data encoding
 */
#define ELFDATANONE	0
#define ELFDATA2LSB	1
#define ELFDATA2MSB	2

/*
 * object file types
 */
#define ET_NONE		0
#define ET_REL		1
#define ET_EXEC		2
#define ET_DYN		3
#define ET_CORE		4

#define ET_LOPROC	0xff00
#define ET_HIPROC	0xffff

/*
 * architecture
 */
#define EM_NONE		0
#define EM_M32		1
#define EM_SPARC	2
#define EM_386		3
#define EM_68K		4
#define EM_88K		5
#define EM_860		7
#define EM_MIPS		8
#define EM_MIPS_RS4_BE	10
#define EM_SPARC64	11
#define EM_PARISC	15
#define EM_PPC		20
#define EM_ARM		40

/* version - page 4-6 */

#define EV_NONE     	0
#define EV_CURRENT  	1

/* special section indexes - page 4-11, figure 4-7 */

#define SHN_UNDEF       0
#define SHN_LORESERVE   0xff00
#define SHN_LOPROC      0xff00
#define SHN_HIPROC      0xff1f
#define SHN_ABS         0xfff1
#define SHN_COMMON      0xfff2
#define SHN_HIRESERVE   0xffff

/*
 * Section header
 */
typedef struct {
	Elf32_Word sh_name;	/* Index to section name string */
	Elf32_Word sh_type;	/* Section type */
	Elf32_Word sh_flags;	/* Section flags */
	Elf32_Addr sh_addr;	/* Address in memory image */
	Elf32_Off sh_offset;	/* Offset in file */
	Elf32_Size sh_size;	/* Size in bytes */
	Elf32_Word sh_link;	/* Index of a related section */
	Elf32_Word sh_info;	/* Depends on section type */
	Elf32_Size sh_addralign;	/* Alignment in bytes */
	Elf32_Size sh_entsize;	/* Size of each entry in section */
} Elf32_Shdr;

/*
 * Section type
 */
#define SHT_NULL    	0
#define SHT_PROGBITS    1
#define SHT_SYMTAB  	2
#define SHT_STRTAB  	3
#define SHT_RELA    	4
#define SHT_HASH    	5
#define SHT_DYNAMIC 	6
#define SHT_NOTE    	7
#define SHT_NOBITS  	8
#define SHT_REL     	9
#define SHT_SHLIB   	10
#define SHT_DYNSYM  	11
#define SHT_NUM     	12
#define SHT_LOPROC  	0x70000000
#define SHT_HIPROC  	0x7fffffff
#define SHT_LOUSER  	0x80000000
#define SHT_HIUSER  	0xffffffff

/*
 * Section flags
 */
#define SHF_WRITE   	0x1
#define SHF_ALLOC   	0x2
#define SHF_EXECINSTR   0x4
#define SHF_MASKPROC    0xf0000000


/*
 * Program header
 */
typedef struct {
	Elf32_Word p_type;	/* Entry type. */
	Elf32_Off p_offset;	/* File offset of contents. */
	Elf32_Addr p_vaddr;	/* Virtual address in memory image. */
	Elf32_Addr p_paddr;	/* Physical address. */
	Elf32_Size p_filesz;	/* Size of contents in file. */
	Elf32_Size p_memsz;	/* Size of contents in memory. */
	Elf32_Word p_flags;	/* Access permission flags. */
	Elf32_Size p_align;	/* Alignment in memory and file. */
} Elf32_Phdr;

/*
 * Segment type
 */
#define PT_NULL     	0
#define PT_LOAD     	1
#define PT_DYNAMIC  	2
#define PT_INTERP   	3
#define PT_NOTE     	4
#define PT_SHLIB    	5
#define PT_PHDR     	6

#define PT_LOPROC   	0x70000000
#define PT_HIPROC   	0x7fffffff

/*
 * Permission flag
 */
#define PF_X        	0x1
#define PF_W        	0x2
#define PF_R        	0x4


/*
 * Relocation
 */
typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
} Elf32_Rel;

typedef struct {
	Elf32_Addr r_offset;
	Elf32_Word r_info;
	Elf32_Sword r_addend;
} Elf32_Rela;

/*
 * for r_info
 */
#define ELF32_R_SYM(i)   ((i) >> 8)
#define ELF32_R_TYPE(i)  ((unsigned char)(i))
#define ELF32_R_INFO(s,t) (((s) << 8) + (unsigned char)(t))

/* Symbol table index */
#define STN_UNDEF	0		/* undefined */

/*
 * Relocation type for i386
 */
#define R_386_NONE	0
#define R_386_32	1
#define R_386_PC32	2
#define R_386_GOT32	3
#define R_386_PLT32	4
#define R_386_COPY	5
#define R_386_GLOB_DAT	6
#define R_386_JMP_SLOT	7
#define R_386_RELATIVE	8
#define R_386_GOTOFF	9
#define R_386_GOTPC	10
#define R_386_NUM	11

/*
 * Relocation type for arm
 */
#define	R_ARM_NONE	0
#define	R_ARM_PC24	1
#define	R_ARM_ABS32	2
#define	R_ARM_PLT32	27

/*
 * Symbol
 */
typedef struct {
	Elf32_Word st_name;
	Elf32_Addr st_value;
	Elf32_Word st_size;
	unsigned char st_info;
	unsigned char st_other;
	Elf32_Half st_shndx;
} Elf32_Sym;


/* Symbol table index */
#define STN_UNDEF	0	/* undefined */

/*
 * for st_info
 */

#define STB_LOCAL	0
#define STB_GLOBAL	1
#define STB_WEAK	2

#define STT_NOTYPE	0
#define STT_OBJECT	1
#define STT_FUNC	2
#define STT_SECTION	3
#define STT_FILE	4
#define STT_COMMON	5
#define STT_TLS		6

#define ELF32_ST_BIND(x)	((x) >> 4)
#define ELF32_ST_TYPE(x)	(((unsigned int)x) & 0xf)

#endif	/* !_ELF_H */
