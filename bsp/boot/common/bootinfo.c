/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * bootinfo.c - Boot information
 */

#include <boot.h>
#include <sys/bootinfo.h>
#include <machine/syspage.h>

/*
 * Boot information holds various platform dependent data that
 * can be referred by kernel/drivers.
 */
struct bootinfo *const bootinfo = (struct bootinfo *)kvtop(BOOTINFO);


#if defined(DEBUG) && defined(DEBUG_BOOTINFO)
static void
print_module(struct module *m)
{

	printf("%lx %lx %x %lx %lx %x %x %x %s\n",
	       m->entry, m->phys, m->size,
	       m->text, m->data, m->textsz,
	       m->datasz, m->bsssz, m->name);
}

void
dump_bootinfo(void)
{
	static const char strtype[][9] = \
		{ "", "USABLE", "MEMHOLE", "RESERVED", "BOOTDISK" };
	struct module *m;
	struct bootinfo *bi = bootinfo;
	int i;

	printf("[Boot information]\n");

	printf("nr_rams=%d\n", bi->nr_rams);
	for (i = 0; i < bi->nr_rams; i++) {
		if (bi->ram[i].type != 0) {
			printf("ram[%d]:  base=%lx size=%x type=%s\n", i,
			       bi->ram[i].base,
			       bi->ram[i].size,
			       strtype[bi->ram[i].type]);
		}
	}

	printf("bootdisk: base=%lx size=%x\n",
	       bi->bootdisk.base,
	       bi->bootdisk.size);

	printf("entry    phys     size     text     data     textsz   datasz   bsssz    module\n");
	printf("-------- -------- -------- -------- -------- -------- -------- -------- ------\n");
	print_module(&bi->kernel);
	print_module(&bi->driver);

	m = (struct module *)&bi->tasks[0];
	for (i = 0; i < bi->nr_tasks; i++, m++)
		print_module(m);
}
#else
void
dump_bootinfo(void)
{
}
#endif
