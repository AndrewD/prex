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

#include <boot.h>
#include <prex/bootinfo.h>
#include <sys/elf.h>
#include <platform.h>

extern int nr_img;		/* number of images */

#define SHF_VALID	(SHF_ALLOC | SHF_EXECINSTR | SHF_ALLOC | SHF_WRITE)

static char *sect_addr[32];	/* array of section address */
static struct module *km = NULL; /* kernel image info */
int nr_km = 0;               /* kernel image info */

static int
load_executable(char *img, struct module *m)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;
	Elf32_Shdr *shdrs, *shdr;
	char *shstrtab;
	u_long phys_base;
	int i;

	phys_base = load_base;
	ehdr = (Elf32_Ehdr *)img;
	phdr = (Elf32_Phdr *)((u_long)ehdr + ehdr->e_phoff);
	m->phys = load_base;
	phys_base = load_base;
	elf_print("phys addr=%x\n", phys_base);

	/* find ksyms */
	shdrs = (Elf32_Shdr *)((u_long)ehdr + ehdr->e_shoff);
	shstrtab = img + shdrs[ehdr->e_shstrndx].sh_offset;
	for (i = 0, shdr = shdrs; i < ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_PROGBITS
		    && (shdr->sh_flags & SHF_VALID) == SHF_ALLOC
		    && strncmp(shstrtab + shdr->sh_name, ".ksymtab", 9) == 0)
		{
			m->ksym = shdr->sh_addr;
			m->ksymsz = shdr->sh_size;
			break;
		}
	}

	for (i = 0; i < (int)ehdr->e_phnum; i++, phdr++) {
		if (phdr->p_type != PT_LOAD)
			continue;

		elf_print("p_flags=%x\n", (int)phdr->p_flags);
		elf_print("p_align=%x\n", (int)phdr->p_align);
		elf_print("p_paddr=%x\n", (int)phdr->p_paddr);

		if (i >= 2) {
			elf_print("skipping extra phdr\n");
			continue;
		}
		if (phdr->p_flags & PF_X) {
			/* Text */
			m->text = phdr->p_vaddr;
			m->textsz = (size_t)phdr->p_memsz;
		} else {
			/* Data & BSS */
			m->data = phdr->p_vaddr;
			m->datasz = (size_t)phdr->p_filesz;
			m->bsssz =
				(size_t)(phdr->p_memsz - phdr->p_filesz);
			m->bss = m->data + m->datasz;
			load_base = phys_base + (m->data - m->text);
		}
		if (phdr->p_filesz > 0) {
			memcpy((char *)load_base, img + phdr->p_offset,
			       (size_t)phdr->p_filesz);
			elf_print("load: offset=%x size=%x\n",
				load_base, (int)phdr->p_filesz);
		}
		if (!(phdr->p_flags & PF_X)) {
			if (m->bsssz > 0) {
				/* Zero fill BSS */
				memset((char *)load_base + m->datasz,
				       0, m->bsssz);
			}
			load_base += phdr->p_memsz;
		}
	}
	/* workaround for data/bss size is 0 */
	if (m->data == 0)
		load_base = phys_base + m->textsz;

	load_base = PAGE_ALIGN(load_base);
	m->size = (size_t)(load_base - m->phys);
	m->entry = ehdr->e_entry;
	elf_print("module size=%x entry=%x\n", m->size, m->entry);
	return 0;
}

static int resolve_symbol(const char *name)
{
	struct kernel_symbol *ksym;
	struct module *m;
	int i;

	for (i = nr_km, m = km; i > 0; i--, m++) {
		int nr_ksym = m->ksymsz / sizeof(struct kernel_symbol);
		ksym = (struct kernel_symbol *)m->ksym;
		for (; nr_ksym > 0; nr_ksym--, ksym++) {
                        if (strncmp(name, ksym->name, 20) == 0)
                                return ksym->value;
                }
        }
        return 0;
}

static int
relocate_section_rela(Elf32_Sym *sym_table, Elf32_Rela *rela,
		      char *target_sect,  const char *strtab, int nr_reloc)
{
	Elf32_Sym *sym;
	Elf32_Addr sym_val;
	int i;

	for (i = 0; i < nr_reloc; i++, rela++) {
		sym = &sym_table[ELF32_R_SYM(rela->r_info)];

		if (sym->st_shndx != STN_UNDEF) {
			sym_val = (Elf32_Addr)sect_addr[sym->st_shndx]
				+ sym->st_value;
		} else {
			sym_val = resolve_symbol(strtab + sym->st_name);

			if (sym_val)
				elf_print("Resolved symbol \"%s\": %x\n",
					  strtab + sym->st_name, sym_val);
			else if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
				printk("Undefined symbol \"%s\"\n",
				       strtab + sym->st_name);
				return -1; /* fatal */
			} else {
				elf_print("Undefined weak symbol \"%s\"\n",
					  strtab + sym->st_name);
				continue; /* don't relocate */
			}
		}
		if (relocate_rela(rela, sym_val, target_sect) != 0)
			return -1;
	}
	return 0;
}

static int
relocate_section_rel(Elf32_Sym *sym_table, Elf32_Rel *rel,
		     char *target_sect,  const char *strtab, int nr_reloc)
{
	Elf32_Sym *sym;
	Elf32_Addr sym_val;
	int i;

	for (i = 0; i < nr_reloc; i++, rel++) {
		sym = &sym_table[ELF32_R_SYM(rel->r_info)];

		if (sym->st_shndx != STN_UNDEF) {
			sym_val = (Elf32_Addr)sect_addr[sym->st_shndx]
				+ sym->st_value;
		} else {
			sym_val = resolve_symbol(strtab + sym->st_name);

			if (sym_val)
				elf_print("Resolved symbol \"%s\": %x\n",
					  strtab + sym->st_name, sym_val);
			else if (ELF32_ST_BIND(sym->st_info) != STB_WEAK) {
				printk("Undefined symbol \"%s\"\n",
				       strtab + sym->st_name);
				return -1; /* fatal */
			} else {
				elf_print("Undefined weak symbol \"%s\"\n",
					  strtab + sym->st_name);
				continue; /* don't relocate */
			}
		}
		if (relocate_rel(rel, sym_val, target_sect) != 0)
			return -1;
	}
	return 0;
}

static int
relocate_section(char *img, Elf32_Shdr *shdr, const char *strtab)
{
	Elf32_Sym *sym_table;
	char *target_sect;
	int nr_reloc, err;

	if (shdr->sh_entsize == 0)
		return 0;
	if ((target_sect = sect_addr[shdr->sh_info]) == 0)
		return -1;
	if ((sym_table = (Elf32_Sym *)sect_addr[shdr->sh_link]) == 0)
		return -1;

	nr_reloc = (int)(shdr->sh_size / shdr->sh_entsize);
	switch (shdr->sh_type) {
	case SHT_REL:
		err = relocate_section_rel(
			sym_table,
			(Elf32_Rel *)(img + shdr->sh_offset),
			target_sect, strtab, nr_reloc);
		break;

	case SHT_RELA:
		err = relocate_section_rela(
			sym_table,
			(Elf32_Rela *)(img + shdr->sh_offset),
			target_sect, strtab, nr_reloc);
		break;

	default:
		err = -1;
		break;
	}
	return err;
}

static int
load_relocatable(char *img, struct module *m)
{
	Elf32_Ehdr *ehdr;
	Elf32_Shdr *shdr, *shdrs;
	u_long sect_base;
	char *strtab = NULL, *shstrtab;
	int i;

	ehdr = (Elf32_Ehdr *)img;
	shdrs = (Elf32_Shdr *)((u_long)ehdr + ehdr->e_shoff);
	m->phys = load_base;
	elf_print("phys addr=%x\n", load_base);

	shstrtab = img + shdrs[ehdr->e_shstrndx].sh_offset;

	/* Copy sections */
	for (i = 0, shdr = shdrs; i < (int)ehdr->e_shnum; i++, shdr++) {
		sect_addr[i] = 0;
		if (shdr->sh_type == SHT_PROGBITS) {

			elf_print("sh_addr=%x\n", shdr->sh_addr);
			elf_print("sh_size=%x\n", shdr->sh_size);
			elf_print("sh_offset=%x\n", shdr->sh_offset);
			elf_print("sh_flags=%x\n", shdr->sh_flags);

			switch (shdr->sh_flags & SHF_VALID) {
			case (SHF_ALLOC | SHF_EXECINSTR):
				/* Text */
				m->text = (u_long)phys_to_virt(load_base);
				break;
			case (SHF_ALLOC | SHF_WRITE):
				/* Data */
				if (m->data == 0)
					m->data =
						(u_long)phys_to_virt(load_base + shdr->sh_addr);
				break;
			case SHF_ALLOC:
				/* rodata & kstrtab */
				/* Note: rodata is treated as text. */
				if (strncmp(shstrtab + shdr->sh_name,
					    ".ksymtab", 9) == 0)
				{
					m->ksym =
						(u_long)phys_to_virt(load_base + shdr->sh_addr);
					m->ksymsz = shdr->sh_size;
				}
				break;
			default:
				continue;
			}
			sect_base = load_base + shdr->sh_addr;
			memcpy((char *)sect_base, img + shdr->sh_offset,
			       (size_t)shdr->sh_size);
			elf_print("load: offset=%x size=%x\n",
				sect_base, (int)shdr->sh_size);

			sect_addr[i] = (char *)sect_base;
		} else if (shdr->sh_type == SHT_NOBITS) {
			/* BSS, SBSS, etc. */
			sect_base = load_base + shdr->sh_addr;
			if (m->bss == 0) {
				m->bss = sect_base;
				m->bsssz = (size_t)shdr->sh_size;
			} else
				m->bsssz += (size_t)shdr->sh_size;

			/* Zero fill uninitialised sections */
			memset((char *)sect_base, 0, (size_t)shdr->sh_size);

			sect_addr[i] = (char *)sect_base;
		} else if (shdr->sh_type == SHT_SYMTAB) {
			/* Symbol table */
			sect_addr[i] = img + shdr->sh_offset;
			strtab = img + shdrs[shdr->sh_link].sh_offset;
		}
	}
	m->textsz = (size_t)(m->data - m->text);
	m->datasz = (size_t)(m->bss - m->data);

	load_base = m->bss + m->bsssz;
	load_base = PAGE_ALIGN(load_base);

	elf_print("module load_base=%x text=%x\n", load_base, m->text);
	m->size = (size_t)(load_base - (u_long)virt_to_phys(m->text));
	m->entry = (u_long)phys_to_virt(ehdr->e_entry + m->phys);
	elf_print("module size=%x entry=%x\n", m->size, m->entry);

	/* Process relocation */
	for (i = 0, shdr = shdrs; i < (int)ehdr->e_shnum; i++, shdr++) {
		if (shdr->sh_type == SHT_REL || shdr->sh_type == SHT_RELA) {
			if (relocate_section(img, shdr, strtab) != 0)
				return -1;
		}
	}
	return 0;
}

/*
 * Load the program from specified ELF image data stored in memory.
 * The boot information is filled after loading the program.
 */
int
elf_load(char *img, struct module *m)
{
	Elf32_Ehdr *ehdr;
	Elf32_Phdr *phdr;

	elf_print("\nelf_load\n");

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
		load_base = (u_long)phys_to_virt(phdr->p_paddr);
		if (load_base == 0)
			return -1;
		elf_print("kernel base=%x\n", load_base);
		load_start = load_base;
		km = m;	/* REVISIT: bit of a hack */
	}
	else if (nr_img == 1) {
		/*  Second image => Driver */
		elf_print("driver base=%x\n", load_base);
	}
	else {
		/* Other images => Boot tasks */
		elf_print("task base=%x\n", load_base);
	}

	switch (ehdr->e_type) {
	case ET_EXEC:
		if (load_executable(img, m) != 0)
			return -1;
		break;
	case ET_REL:
		if (load_relocatable(img, m) != 0)
			return -1;
		break;
	default:
		elf_print("Unsupported file type\n");
		return -1;
	}
	nr_img++;
	if (m->ksym != 0)
		nr_km = nr_img;
	return 0;
}
