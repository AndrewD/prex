/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * elf.c - ELF file format support
 */

#include <bootinfo.h>
#include <boot.h>
#include <elf.h>

extern int nr_img;		/* Number of images */

#ifdef DEBUG_BOOT_IMAGE
#define elf_dbg(x,y...) printk(x, ##y)
#else
#define elf_dbg(x,y...)
#endif

#define SHF_VALID	(SHF_ALLOC | SHF_EXECINSTR | SHF_ALLOC | SHF_WRITE)


static char *sect_addr[32];	/* Array of section address */

static int load_executable(char *img, struct img_info *info)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	u_int phys_base;
	int i;

	phys_base = load_base;
	ehdr = (Elf32_Ehdr *)img;
	phdr = (Elf32_Phdr *)((u_long)ehdr + ehdr->e_phoff);
	info->phys = load_base;
	phys_base = load_base;
	elf_dbg("phys addr=%x\n", phys_base);

	for (i = 0; i < ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type != PT_LOAD)
			continue;

		elf_dbg("p_flags=%x\n", (int)phdr->p_flags);
		elf_dbg("p_align=%x\n", (int)phdr->p_align);
		elf_dbg("p_paddr=%x\n", (int)phdr->p_paddr);

		if (i >= 2) {
			elf_dbg("skipping extra phdr\n");
			continue;
		}
		if (phdr->p_flags & PF_X) {
			/* Text */
			info->text = phdr->p_vaddr;
			info->text_size = phdr->p_memsz;
		} else {
			/* Data & BSS */
			info->data = phdr->p_vaddr;
			info->data_size = phdr->p_filesz;
			info->bss_size = phdr->p_memsz - phdr->p_filesz;
			load_base = phys_base + (info->data - info->text);
		}
		if (phdr->p_filesz > 0) {
			memcpy((char *)load_base, img + phdr->p_offset,
			       phdr->p_filesz);
			elf_dbg("load: offset=%x size=%x\n",
				load_base, (int)phdr->p_filesz);
		}
		if (!(phdr->p_flags & PF_X)) {
			if (info->bss_size > 0) {
				/* Zero fill BSS */
				memset((char *)load_base + info->data_size,
				       0, info->bss_size);
			}
			load_base += phdr->p_memsz;
		}
	}
	load_base = PAGE_ALIGN(load_base);
	info->size = load_base - info->phys;
	info->entry = ehdr->e_entry;
	elf_dbg("info size=%x entry=%x\n", info->size, info->entry);
	return 0;
}

static int relocate_section_rela(Elf32_Sym *sym_table, Elf32_Rela *rela,
			char *target_sect, int nr_reloc)
{
	Elf32_Sym *sym;
	Elf32_Addr sym_val;
	int i;

	for (i = 0; i < nr_reloc; i++) {
		sym = &sym_table[ELF32_R_SYM(rela->r_info)];
		if (sym->st_shndx != STN_UNDEF) {
			sym_val = (Elf32_Addr)sect_addr[sym->st_shndx]
				+ sym->st_value;
			if (relocate_rela(rela, sym_val, target_sect) != 0)
				return -1;
		} else if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
			elf_dbg("Undefined symbol for rela[%x]\n", i);
			return 0;
		} else
			elf_dbg("Undefined weak symbol for rela[%x]\n", i);
		rela++;
	}
	return 0;
}

static int relocate_section_rel(Elf32_Sym *sym_table, Elf32_Rel *rel,
			char *target_sect, int nr_reloc)
{
	Elf32_Sym *sym;
	Elf32_Addr sym_val;
	int i;

	for (i = 0; i < nr_reloc; i++) {
		sym = &sym_table[ELF32_R_SYM(rel->r_info)];
		if (sym->st_shndx != STN_UNDEF) {
			sym_val = (Elf32_Addr)sect_addr[sym->st_shndx]
				+ sym->st_value;
			if (relocate_rel(rel, sym_val, target_sect) != 0)
				return -1;
		} else if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
			printk("Undefined symbol for rel[%x] sym=%x\n", i, sym);
			return -1;
		} else
			elf_dbg("Undefined weak symbol for rel[%x]\n", i);
		rel++;
	}
	return 0;
}

static int relocate_section(char *img, Elf32_Shdr *shdr)
{
	Elf32_Sym *sym_table;
	char *target_sect;
	int nr_reloc;

	if (shdr->sh_entsize == 0)
		return 0;

	if ((target_sect = sect_addr[shdr->sh_info]) == 0)
		return -1;

	if ((sym_table = (Elf32_Sym *)sect_addr[shdr->sh_link]) == 0)
		return -1;

	nr_reloc = shdr->sh_size / shdr->sh_entsize;
	switch (shdr->sh_type) {
	case SHT_REL:
		return relocate_section_rel(sym_table,
				    (Elf32_Rel *)(img + shdr->sh_offset),
				    target_sect, nr_reloc);
		break;

	case SHT_RELA:
		return relocate_section_rela(sym_table,
				     (Elf32_Rela *)(img + shdr->sh_offset),
				     target_sect, nr_reloc);
		break;

	default:
		return -1;
		break;
	}
}

static int load_relocatable(char *img, struct img_info *info)
{
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr;
	u_long sect_base, bss_base;
	int i;

	ehdr = (Elf32_Ehdr *)img;
	shdr = (Elf32_Shdr *)((u_long)ehdr + ehdr->e_shoff);
	bss_base = 0;
	info->phys = load_base;
	elf_dbg("phys addr=%x\n", load_base);

	/* Copy sections */
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		sect_addr[i] = 0;
		if (shdr->sh_type == SHT_PROGBITS) {

			elf_dbg("sh_addr=%x\n", shdr->sh_addr);
			elf_dbg("sh_size=%x\n", shdr->sh_size);
			elf_dbg("sh_offset=%x\n", shdr->sh_offset);
			elf_dbg("sh_flags=%x\n", shdr->sh_flags);

			switch (shdr->sh_flags & SHF_VALID) {
			case (SHF_ALLOC | SHF_EXECINSTR):
				/* Text */
				info->text = (u_long)phys_to_virt(load_base);
				break;
			case (SHF_ALLOC | SHF_WRITE):
				/* Data */
				if (info->data == 0)
					info->data =
						(u_long)phys_to_virt(load_base + shdr->sh_addr);
				break;
			case SHF_ALLOC:
				/* rodata */
				/* Note: rodata is treated as text. */
				break;
			default:
				continue;
			}
			sect_base = load_base + shdr->sh_addr;
			memcpy((char *)sect_base, img + shdr->sh_offset,
			       shdr->sh_size);
			elf_dbg("load: offset=%x size=%x\n",
				sect_base, (int)shdr->sh_size);

			sect_addr[i] = (char *)sect_base;
		} else if (shdr->sh_type == SHT_NOBITS) {
			/* BSS */
			info->bss_size = shdr->sh_size;
			sect_base = load_base + shdr->sh_addr;
			bss_base = sect_base;

			/* Zero fill BSS */
			memset((char *)sect_base, 0, shdr->sh_size);

			sect_addr[i] = (char *)sect_base;
		} else if (shdr->sh_type == SHT_SYMTAB) {
			/* Symbol table */
			sect_addr[i] = img + shdr->sh_offset;
		}
	}
	info->text_size = info->data - info->text;
	info->data_size = bss_base - info->data;

	load_base = bss_base + info->bss_size;
	load_base = PAGE_ALIGN(load_base);

	elf_dbg("info load_base=%x info->text=%x\n",
		load_base, info->text);
	info->size = load_base - (u_long)virt_to_phys(info->text);
	info->entry = (u_long)phys_to_virt(ehdr->e_entry + info->phys);
	elf_dbg("info size=%x entry=%x\n", info->size, info->entry);

	/* Process relocation */
	shdr = (Elf32_Shdr *)((u_long)ehdr + ehdr->e_shoff);
	for (i = 0; i < ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) {
			if (relocate_section(img, shdr) != 0)
				return -1;
		}
	}
	return 0;
}

/*
 * Load the program from specified ELF image data stored in memory.
 * The boot information is filled after loading the program.
 */
int elf_load(char *img, struct img_info *info)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;

	elf_dbg("\nelf_load\n");

	ehdr = (Elf32_Ehdr *)img;

	/*  Check ELF header */
	if ((ehdr->e_ident[EI_MAG0] != ELFMAG0) ||
	    (ehdr->e_ident[EI_MAG1] != ELFMAG1) ||
	    (ehdr->e_ident[EI_MAG2] != ELFMAG2) ||
	    (ehdr->e_ident[EI_MAG3] != ELFMAG3))
		return -1;

	phdr = (Elf32_Phdr *)((u_long)ehdr + ehdr->e_ehsize);

	if (nr_img == 0) {
		/*  Initialize the load address */
		load_base = (u_long)virt_to_phys(phdr->p_paddr);
		if (load_base == 0)
			return -1;
		elf_dbg("kernel base=%x\n", load_base);
		load_start = load_base;
	}
	else if (nr_img == 1) {
		/*  Second image => Driver */
		elf_dbg("driver base=%x\n", load_base);
	}
	else {
		/* Other images => Boot tasks */
		elf_dbg("task base=%x\n", load_base);
	}

	switch (ehdr->e_type) {
	case ET_EXEC:
		if (load_executable(img, info) != 0)
			return -1;
		break;
	case ET_REL:
		if (load_relocatable(img, info) != 0)
			return -1;
		break;
	default:
		elf_dbg("Unsupported file type\n");
		return -1;
	}
	nr_img++;
	return 0;
}
