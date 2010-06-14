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

#include <sys/param.h>
#include <sys/bootinfo.h>
#include <boot.h>

#define SCREEN_80x25 1
/* #define SCREEN_80x50 1 */

extern paddr_t	lo_mem;
extern paddr_t	hi_mem;

/*
 * Setup boot information.
 *
 * Note: We already got the memory size via BIOS call in head.S.
 */
static void
bootinfo_init(void)
{
	struct bootinfo *bi = bootinfo;

	/*
	 * Screen size
	 */
#ifdef SCREEN_80x25
	bi->video.text_x = 80;
	bi->video.text_y = 25;
#else
	bi->video.text_x = 80;
	bi->video.text_y = 50;
#endif

	/*
	 * Main memory
	 */
	bi->ram[0].base = 0;
	bi->ram[0].size = (size_t)((1024 + hi_mem) * 1024);
	bi->ram[0].type = MT_USABLE;
	bi->nr_rams = 1;

	/*
	 * Add BIOS ROM and VRAM area
	 */
	if (hi_mem != 0) {
		bi->ram[1].base = lo_mem * 1024;
		bi->ram[1].size = (size_t)((1024 - lo_mem) * 1024);
		bi->ram[1].type = MT_MEMHOLE;
		bi->nr_rams++;
	}
}

void
startup(void)
{

	bootinfo_init();
}
