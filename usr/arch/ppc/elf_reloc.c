/*-
 * Copyright (c) 2006, Kohsuke Ohtani
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
#include <sys/elf.h>
#include <sys/syslog.h>

int
relocate_rel(Elf32_Rel *rel, Elf32_Addr sym_val, char *target_sect)
{

	/* printf("Invalid relocation type\n"); */
	return -1;
}

int
relocate_rela(Elf32_Rela *rela, Elf32_Addr sym_val, char *target_sect)
{
	Elf32_Addr *where;
	uint32_t val;

	where = (Elf32_Addr *)(target_sect + rela->r_offset);
	val = sym_val + rela->r_addend;

	switch (ELF32_R_TYPE(rela->r_info)) {
	case R_PPC_NONE:
		break;
	case R_PPC_ADDR32:
		/* printf("R_PPC_ADDR32\n"); */
		*(uint32_t *)where = val;
		break;
	case R_PPC_ADDR16_LO:
		/* printf("R_PPC_ADDR16_LO\n"); */
		*(uint16_t *)where = val;
		break;
	case R_PPC_ADDR16_HI:
		/* printf("R_PPC_ADDR16_HI\n"); */
		*(uint16_t *)where = (val >> 16);
		break;
	case R_PPC_ADDR16_HA:
		/* printf("R_PPC_ADDR16_HA\n"); */
		*(uint16_t *)where = (val + 0x8000) >> 16;
		break;
	case R_PPC_REL24:
		/* printf("R_PPC_REL24\n"); */
		*(uint32_t *)where =
			(*(uint32_t *)where & ~0x03fffffc)
			| ((val - (uint32_t)where)
			   & 0x03fffffc);
		break;
	case R_PPC_REL32:
		/* printf("R_PPC_REL32\n"); */
		*(uint32_t *)where = val - (uint32_t)where;
		break;
	default:
		/* printf("Unkown relocation type=%d\n",
			ELF32_R_TYPE(rela->r_info)); */
#ifdef DEBUG
		syslog(LOG_ERR, "relocation fail type=%d\n", ELF32_R_TYPE(rela->r_info));
#endif
		return -1;
	}
	return 0;
}
