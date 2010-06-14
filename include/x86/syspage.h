/*-
 * Copyright (c) 2008-2009, Kohsuke Ohtani
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

#ifndef _X86_SYSPAGE_H
#define _X86_SYSPAGE_H

#include <conf/config.h>

/**
 * syspage layout:
 *
 * +------------------+ CONFIG_SYSPAGE_BASE
 * | NULL pointer     |
 * | detection page   |
 * |                  |
 * +------------------+ +0x1000
 * | Interrupt stack  |
 * |                  |
 * |                  |
 * +------------------+ +0x2000
 * | Boot information |
 * +------------------+ +0x2400
 * | Boot stack       |
 * +------------------+ +0x3000
 * | PGD for boot     |
 * | (MMU only)       |
 * |                  |
 * +------------------+ +0x4000
 * | PTE0 for boot    |
 * | (MMU only)       |
 * |                  |
 * +------------------+ +0x5000
 *
 * Note: Interrupt stack should be placed after NULL page
 * to detect the stack overflow.
 */

#define SYSPAGE		CONFIG_SYSPAGE_BASE
#define INTSTK		(SYSPAGE + 0x1000)
#define BOOTINFO	(SYSPAGE + 0x2000)
#define BOOTSTK		(SYSPAGE + 0x2400)
#define BOOT_PGD	(SYSPAGE + 0x3000)
#define BOOT_PTE0	(SYSPAGE + 0x4000)

#define BOOT_PGD_PHYS	0x3000
#define BOOT_PTE0_PHYS	0x4000

#define INTSTKSZ	0x1000		/* size of interrupt stack */
#define BOOTSTKSZ	0x0c00		/* size of boot stack */

#define INTSTKTOP	(INTSTK + INTSTKSZ)
#define BOOTSTKTOP	(BOOTSTK + BOOTSTKSZ)

#ifdef CONFIG_MMU
#define SYSPAGESZ	0x5000
#else
#define SYSPAGESZ	0x3000
#endif

#endif /* !_X86_SYSPAGE_H */
