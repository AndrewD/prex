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

#ifndef _OFFSETS_H
#define _OFFSETS_H

#define PAGE_OFFSET	0x00000000

#define BOOT_INFO	0x03006000
#define BOOT_INFO_SIZE	0x00000800
#define BOOT_STACK	0x03007000
#define ARCHIVE_START	0x08002000

#ifndef __ASSEMBLY__

/*
 * Page mapping
 */
#define phys_to_virt(p_addr)	(void *)((u_long)(p_addr) + PAGE_OFFSET)
#define virt_to_phys(v_addr)	(void *)((u_long)(v_addr) - PAGE_OFFSET)

#if defined(DEBUG) && defined(CONFIG_DIAG_VBA)
/*
 * Put one character to Visual Boy Advance (VBA) emulator console.
 *
 * Important:
 * This BIOS call is not supported by actual GBA BIOS. So, you must
 * disable this function when you run it on actual GBA H/W.
 * Otherwise, it will hang.
 */
static __inline void
putc(int c)
{
	static char buf[2] = {0,0};

	buf[0] = (char)c;
	__asm__ __volatile__(
		"mov r0, %0;"
		"swi 0xff0000;"
		: /* no ouput */
		: "r" (buf)
		: "r0");
}
#else
#define putc(c)
#endif
extern void setup_bootinfo(struct boot_info **);

#endif /* !__ASSEMBLY__ */
#endif /* _OFFSETS_H */
