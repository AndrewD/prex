/*
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

#ifndef _ARCH_H
#define _ARCH_H

/*--------------------------------------------------------------------------
 * Context
 */

/*
 * Common register frame for trap/interrupt.
 * These cpu state are saved into top of the kernel stack in
 * trap/interrupt entries. Since the arguments of system calls are
 * passed via registers, the system call library is completely 
 * dependent on this register format.
 *
 * The value of ss & esp are not valid for kernel mode trap
 * because these are set only when privilege level is changed.
 */
struct cpu_regs {
	u_long	ebx;		/*  +0 (00) --- s/w trap frame --- */
	u_long	ecx;		/*  +4 (04) */
	u_long	edx;		/*  +8 (08) */
	u_long	esi;		/* +12 (0C) */
	u_long	edi;		/* +16 (10) */
	u_long	ebp;		/* +20 (14) */
	u_long	eax;		/* +24 (18) */
	u_long	ds;		/* +28 (1C) */
	u_long	es;		/* +32 (20) */
	u_long	trap_no;	/* +36 (24) --- h/w trap frame --- */
	u_long	err_code;	/* +40 (28) */
	u_long	eip;		/* +44 (2C) */
	u_long	cs;		/* +48 (30) */
	u_long	eflags;		/* +52 (34) */
	u_long	esp;		/* +56 (38) */
	u_long	ss;		/* +60 (3C) */
};

/*
 * Kernel mode context for context switching.
 */
struct kern_regs {
	u_long	eip;		/*  +0 (00) */
	u_long	edi;		/*  +4 (04) */
	u_long	esi;		/*  +8 (08) */
	u_long	ebp;		/* +12 (0C) */
	u_long	esp;		/* +16 (10) */
};

#ifdef CONFIG_FPU
/*
 * FPU register for fsave/frstor
 */
struct fpu_regs {
	u_long	ctrl_word;
	u_long	stat_word;
	u_long	tag_word;
	u_long	ip_offset;
	u_long	cs_sel;
	u_long	op_offset;
	u_long	op_sel;
	u_long	st[20];
};
#endif

/*
 * Processor context
 */
struct context {
	struct kern_regs kregs;		/* Kernel mode registers */
	struct cpu_regs	*uregs;		/* User mode registers */
#ifdef CONFIG_FPU
	struct fpu_regs	*fregs; */	/* Co-processor registers */
#endif
	char   *esp0;			/* Top of kernel stack */
};

typedef struct context *context_t;	/* context id */

/* Types for context_set */
#define USER_ENTRY	0		/* Set user mode entry addres */
#define USER_STACK	1		/* Set user mode stack address */
#define KERN_ENTRY	2		/* Set kernel mode entry address */
#define KERN_ARG	3		/* Set kernel mode argument */

extern void context_init(context_t ctx, void *kstack);
extern void context_set(context_t ctx, int type, u_long val);
extern void context_switch(context_t prev, context_t next);
extern void context_save(context_t ctx, int exc);
extern void context_restore(context_t ctx, void *regs);

/*--------------------------------------------------------------------------
 * Interrupt
 */

#define interrupt_enable()	__asm__ __volatile__("sti"::)
#define interrupt_disable()	__asm__ __volatile__("cli"::)

static inline void interrupt_save(int *sts)
{
	register u_long eflags;
	__asm__ __volatile__(
		"pushfl\n\t"
		"popl %0\n\t"
		:"=r" (eflags));
	*sts = (int)(eflags & 0x200);
}

static inline void interrupt_restore(int sts)
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

/*--------------------------------------------------------------------------
 * Memory Management Unit
 */

typedef long *pgd_t;				/* Page directory */

/* Memory page type */
#define PG_UNMAP	0		/* no page */
#define PG_READ		1		/* read only */
#define PG_WRITE	2		/* read/write */

#ifdef CONFIG_MMU
extern void mmu_init(void);
extern int  mmu_map(pgd_t pgd, void *phys, void *virt, size_t size, int type);
extern pgd_t mmu_newmap(void);
extern void mmu_delmap(pgd_t pgd);
extern void mmu_switch(pgd_t pgd);
extern void *mmu_extract(pgd_t pgd, void *virt, size_t size);
#else /* CONFIG_MMU */
#define mmu_init()		do {} while (0)
#define mmu_switch(pgd)		do {} while (0)
#endif /* CONFIG_MMU */

/*--------------------------------------------------------------------------
 * User Memory
 */

extern int umem_copyin(void *uaddr, void *kaddr, size_t len);
extern int umem_copyout(void *kaddr, void *uaddr, size_t len);
extern int umem_strnlen(void *uaddr, size_t maxlen, size_t *len);

/*--------------------------------------------------------------------------
 * Misc.
 */

#define breakpoint()	__asm__ __volatile__("int $3"::)

/*
 * All the arguments of system calls are passed via a stack in i386 system.
 * The following macro directs the compiler not to use any register for
 * the arguments of the system call function.
 */
#define __syscall __attribute__ ((regparm(0)))

#endif /* !_ARCH_H */
