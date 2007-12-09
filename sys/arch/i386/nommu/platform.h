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

#ifndef _MACHINE_PLATFORM_H
#define _MACHINE_PLATFORM_H

/*--------------------------------------------------------------------------
 * Address/Locations
 */

#define PAGE_OFFSET	0x00000000

/* Memory segments */
#define KERNEL_BASE	0x00000000
#define KERNEL_MAX	0xffffffff
#define USER_BASE	0x00000000
#define USER_MAX	0x80000000

/* Kernel reserved area */
#define RESERVED_START	0x00000000
#define RESERVED_SIZE	0x00004000

/* Predefined pages */
#define INT_STACK	0x00001000
#define BOOT_STACK	0x00002000
#define BOOT_INFO	0x00003000

/* Boot stack */
#define BOOT_STACK_SIZE	0x1000

#define kern_area(addr)		((u_long)(addr) >= (u_long)KERNEL_BASE)
#define user_area(addr)		((u_long)(addr) < (u_long)USER_MAX)


#ifndef __ASSEMBLY__
/*--------------------------------------------------------------------------
 * Interrupt
 */

#define NR_IRQS		16		/* Number of interrupt vectors */

/* Interrupt mode for interrupt_setup() */
#define IMODE_EDGE	0		/* Edge trigger */
#define IMODE_LEVEL	1		/* Level trigger */

extern void interrupt_mask(int vector);
extern void interrupt_unmask(int vector, int level);
extern void interrupt_setup(int vector, int mode);

/*--------------------------------------------------------------------------
 * Clock
 */

extern void clock_init(void);

/*--------------------------------------------------------------------------
 * Misc.
 */

extern void system_reset(void);
extern void diag_print(char *buf);
#define cpu_idle()	__asm__ __volatile__("sti; hlt")
#endif /* !__ASSEMBLY__ */

#endif /* !_MACHINE_PLATFORM_H */
