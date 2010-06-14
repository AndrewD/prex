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
 * machdep.c - machine-dependent functions for HAL
 */

#include <kernel.h>
#include <page.h>
#include <machine/syspage.h>
#include <sys/power.h>
#include <sys/bootinfo.h>
#include <mmu.h>
#include <cpu.h>
#include <cpufunc.h>
#include <locore.h>


#ifdef CONFIG_MMU
/*
 * Virtual and physical address mapping
 *
 *      { virtual, physical, size, type }
 */
static struct mmumap mmumap_table[] =
{
	/*
	 * RAM
	 */
	{ 0x80000000, 0x00000000, AUTOSIZE, VMT_RAM },

	{ 0,0,0,0 }
};
#endif

/*
 * Idle
 */
void
machine_idle(void)
{

	cpu_idle();
}

/*
 * Cause an i386 machine reset.
 */
static void
machine_reset(void)
{
	int i;

	/*
	 * Try to do keyboard reset.
	 */
	outb(0x64, 0xfe);
	for (i = 0; i < 10000; i++)
		outb(0x80, 0);

	/*
	 * Do cpu reset.
	 */
	cpu_reset();

	/* NOTREACHED */
}

/*
 * Power down system.
 */
void
machine_powerdown(int state)
{

	splhigh();

	DPRINTF(("Power down machine\n\n"));

	switch (state) {
	case PWR_SUSPEND:
	case PWR_OFF:
		for (;;)
			cpu_idle();
		/* NOTREACHED */
		break;
	case PWR_REBOOT:
		machine_reset();
		/* NOTREACHED */
		break;
	}
}

/*
 * Get pointer to the boot information.
 */
void
machine_bootinfo(struct bootinfo **bip)
{
	ASSERT(bip != NULL);

	*bip = (struct bootinfo *)BOOTINFO;
}

void
machine_abort(void)
{

	for (;;) ;
}

/*
 * Machine-dependent startup code
 */
void
machine_startup(void)
{
#ifdef CONFIG_MMU
	struct bootinfo *bi = (struct bootinfo *)BOOTINFO;
#endif

	/*
	 * Initialize CPU and basic hardware.
	 */
	cpu_init();
	cache_init();

	/*
	 * Reserve system pages.
	 */
	page_reserve(kvtop(SYSPAGE), SYSPAGESZ);

#ifdef CONFIG_MMU
	/*
	 * Modify page mapping
	 * We assume the first block in ram[] for x86 is main memory.
	 */
	mmumap_table[0].size = bi->ram[0].size;

	/*
	 * Initialize MMU
	 */
	mmu_init(mmumap_table);
#endif
}
