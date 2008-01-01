/*
 * Copyright (c) 2007, Kohsuke Ohtani
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

#ifndef _PC_PLATFORM_H
#define _PC_PLATFORM_H

/*
 * Memory location
 */

#ifdef CONFIG_MMU
#define PAGE_OFFSET	0x80000000
#else
#define PAGE_OFFSET	0x00000000
#endif

#define KERNEL_BASE	PAGE_OFFSET
#define KERNEL_MAX	0xffffffff
#define USER_BASE	0x00000000
#define USER_MAX	0x80000000

#define BOOT_PTE0	0x00001000
#define INT_STACK	0x00001000
#define BOOT_INFO	0x00002000
#define BOOT_STACK	0x00002800
#define KERNEL_PGD	0x00003000

#ifdef CONFIG_MMU
#define RESERVED_BASE	0x00000000
#define RESERVED_MAX	0x00004000
#else
#define RESERVED_BASE	0x00000000
#define RESERVED_MAX	0x00003000
#endif


#ifndef __ASSEMBLY__

/*
 * Page mapping
 */
#define phys_to_virt(p_addr)	(void *)((u_long)(p_addr) + PAGE_OFFSET)
#define virt_to_phys(v_addr)	(void *)((u_long)(v_addr) - PAGE_OFFSET)

/*
 * Kernel/User Locations
 */
#if KERNEL_BASE == 0
#define kern_area(addr)		1
#else
#define kern_area(addr)		((u_long)(addr) >= (u_long)KERNEL_BASE)
#endif
#define user_area(addr)		((u_long)(addr) < (u_long)USER_MAX)

/*
 * Interrupt
 */
#define NIRQS		16		/* number of interrupt vectors */

#define interrupt_enable()	__asm__ __volatile__("sti"::)
#define interrupt_disable()	__asm__ __volatile__("cli"::)

static __inline void
interrupt_save(int *sts)
{
	register u_long eflags;
	__asm__ __volatile__(
		"pushfl\n\t"
		"popl %0\n\t"
		:"=r" (eflags));
	*sts = (int)(eflags & 0x200);
}

static __inline void
interrupt_restore(int sts)
{
	__asm__ __volatile__(
		"pushfl\n\t"
		"popl %%eax\n\t"
		"andl $0xfffffdff, %%eax\n\t"
		"orl %0, %%eax\n\t"
		"pushl %%eax\n\t"
		"popfl\n\t"
		:
		:"r" (sts)
		:"eax");
}

extern void interrupt_mask(int);
extern void interrupt_unmask(int, int);
extern void interrupt_setup(int, int);
extern void interrupt_init(void);

/* Interrupt mode for interrupt_setup() */
#define IMODE_EDGE	0		/* edge trigger */
#define IMODE_LEVEL	1		/* level trigger */

extern void clock_init(void);

extern void diag_init(void);
extern void diag_print(char *);

#define machine_idle()	__asm__ __volatile__("sti; hlt")
extern void machine_reset(void);
extern void machine_init(void);

#endif /* !__ASSEMBLY__ */
#endif /* !_PC_PLATFORM_H */
