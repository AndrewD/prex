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
 * main.c - Boot loader main module.
 */

#include <boot.h>
#include <machdep.h>
#include <sys/bootinfo.h>
#include "load.h"

typedef void (*entry_t)(void);

/*
 * Loader main routine
 *
 * We assume that the following machine state has
 * been already set before this routine.
 *	- CPU is initialized.
 *	- DRAM is configured.
 *	- Loader BSS section is filled with 0.
 *	- Loader stack is configured.
 *	- All interrupts are disabled.
 */
int
main(void)
{
	entry_t entry;

	memset(bootinfo, 0, BOOTINFOSZ);

	/*
	 * Initialize debug port.
	 */
	debug_init();
	DPRINTF(("Prex Boot Loader\n"));

	/*
	 * Do platform dependent initialization.
	 */
	startup();

	/*
	 * Show splash screen.
	 */
	splash();

	/*
	 * Load OS modules to appropriate locations.
	 */
	load_os();

	/*
	 * Dump boot infomation for debug.
	 */
	dump_bootinfo();

	/*
	 * Launch kernel.
	 */
	entry = (entry_t)kvtop(bootinfo->kernel.entry);
	DPRINTF(("Entering kernel (at 0x%lx) ...\n\n", (long)entry));
	(*entry)();

	panic("Oops!");
	/* NOTREACHED */
	return 0;
}

/*
 * panic - show error message and hang up.
 */
void
panic(const char *msg)
{

	DPRINTF(("Panic: %s\n", msg));

	for (;;) ;
	/* NOTREACHED */
}
