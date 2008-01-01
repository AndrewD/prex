/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * machdep.c - machine-dependent routines
 */

#include <kernel.h>
#include <cpu.h>

/*
 * Setup pages.
 * This reserves some kernel pages which includes a page for
 * kernel page directory, interrupt stack, boot stack, etc.
 */
static void
page_setup(void)
{
	struct mem_map *mem;
	int i;

	/*
	 * Find empty slot, and set reserved pages
	 */
	for (i = 0; i < NRESMEM; i++) {
		mem = &boot_info->reserved[i];
		if (mem->size == 0) {
			mem->start = RESERVED_BASE;
			mem->size = (RESERVED_MAX - RESERVED_BASE);
			break;
		}
	}
	ASSERT(i != NRESMEM);
}

/*
 * Cause an i386 machine reset.
 */
void
machine_reset(void)
{
	int i;

	/*
	 * Try to do keyboard reset.
	 */
	cli();
	outb(0xfe, 0x64);
	for (i = 0; i < 10000; i++)
		outb(0, 0x80);

	/*
	 * Do cpu reset.
	 */
	cpu_reset();
	/* NOTREACHED */
}

/*
 * Machine-dependent startup code
 */
void
machine_init(void)
{

	cpu_init();

#ifdef CONFIG_GDB
	gdb_init();
#endif
	page_setup();
	interrupt_init();
}
