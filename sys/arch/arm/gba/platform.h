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

#ifndef _GBA_PLATFORM_H
#define _GBA_PLATFORM_H

/*
 * Memory location
 */

#define PAGE_OFFSET	0x00000000

#define KERNEL_BASE	0x02000000
#define KERNEL_MAX	0x02400000
#define USER_BASE	0x02000000
#define USER_MAX	0x02040000

#define BOOT_INFO	0x03006000
#define BOOT_STACK	0x03007000
#define INT_STACK	0x03007900
#define SYS_STACK	0x0203ff00

#ifndef __ASSEMBLY__

/*
 * Page mapping
 */
#define phys_to_virt(p_addr)	(void *)((u_long)(p_addr) + PAGE_OFFSET)
#define virt_to_phys(v_addr)	(void *)((u_long)(v_addr) - PAGE_OFFSET)

/*
 * Kernel/User Locations
 */
#define kern_area(addr)	\
	(((u_long)(addr) >= KERNEL_BASE) && ((u_long)(addr) < KERNEL_MAX))
#define user_area(addr) \
	(((u_long)(addr) >= USER_BASE) && ((u_long)(addr) < USER_MAX))

/*
 * Interrupt
 */
#define NIRQS		14		/* number of interrupt vectors */

static __inline void
interrupt_enable(void)
{
#ifndef __lint__
	u_long val;

	__asm__ __volatile__(
		"mrs %0, cpsr\n\t"
		"bic %0, %0, #0xc0\n\t"		/* Enable IRQ & FIQ */
		"msr cpsr_c, %0\n\t"
		:"=&r" (val)
		:
		: "memory");
#endif
}

static __inline void
interrupt_disable(void)
{
#ifndef __lint__
	u_long val;

	__asm__ __volatile__(
		"mrs %0, cpsr\n\t"
		"orr %0, %0, #0xc0\n\t"		/* Disable IRQ & FIQ */
		"msr cpsr_c, %0\n\t"
		:"=&r" (val)
		:
		: "memory");
#endif
}

static __inline void
interrupt_save(int *sts)
{
	u_long val;

	__asm__ __volatile__(
		"mrs %0, cpsr\n\t"
		:"=&r" (val)
		:
		:"memory");
	*sts = (int)val;
}

static __inline void
interrupt_restore(int sts)
{

	__asm__ __volatile__(
		"msr cpsr_c, %0\n\t"
		:
		:"r" (sts)
		:"memory");
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

#ifdef CONFIG_DIAG_VBA
static __inline void
diag_print(char *buf)
{

	__asm__ __volatile__(
		"mov r0, %0\n\t"
		"swi 0xff0000\n\t"		/* VBA emulator call */
		:
		:"r" (buf)
		:"r0");
}

#else
extern void diag_print(char *);
#endif

static __inline void
machine_idle(void)
{

	__asm__ __volatile__(
		"swi 0x20000\n\t"		/* GBA BIOS call */
		:::"r0", "r1", "r2", "r3");
}

static __inline void
machine_reset(void)
{

	__asm__ __volatile__(
		"swi 0\n\t"			/* GBA BIOS call */
		:::"r0", "r1", "r2", "r3");
}

extern void machine_init(void);

#endif /* !__ASSEMBLY__ */
#endif /* !_GBA_PLATFORM_H */
