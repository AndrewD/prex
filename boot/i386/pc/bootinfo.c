/*-
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

#include <boot.h>
#include "platform.h"

#define SCREEN_80x25 1
/* #define SCREEN_80x50 1 */

extern u_long lo_mem;
extern u_long hi_mem;

/*
 * Setup boot information.
 *
 * Note: The memory size are already got via BIOS call in head.S.
 */
void
setup_bootinfo(struct boot_info **bpp)
{
	struct boot_info *bp;

	bp = (struct boot_info *)BOOT_INFO;
	memset(bp, 0, BOOT_INFO_SIZE);

	bp->archive = (u_long)ARCHIVE_START;

#ifdef SCREEN_80x25
	bp->video.text_x = 80;
	bp->video.text_y = 25;
#else
	bp->video.text_x = 80;
	bp->video.text_y = 50;
#endif
#ifdef CONFIG_MIN_MEMORY
	lo_mem = 512;	/* 512KB */
	hi_mem = 0;
#endif
	printk("hi_mem=%x lo_mem=%x\n", hi_mem, lo_mem);

	bp->main_mem.start = 0;
	bp->main_mem.size = (size_t)((1024 + hi_mem) * 1024);
	if (bp->main_mem.size == 0)
		panic("memory size is 0!");

	*bpp = bp;

	if (hi_mem != 0) {
		reserve_memory((u_long)lo_mem * 1024,
			       (size_t)((1024 - lo_mem) * 1024));
	}
}
