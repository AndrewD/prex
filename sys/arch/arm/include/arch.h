/*
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

#ifndef _ARCH_H
#define _ARCH_H

/**
 * ARM register reference:
 *
 *  Name    Number	ARM Procedure Calling Standard Role
 *
 *  a1	    r0		argument 1 / integer result / scratch register / argc
 *  a2	    r1		argument 2 / scratch register / argv
 *  a3	    r2		argument 3 / scratch register / envp
 *  a4	    r3		argument 4 / scratch register
 *  v1	    r4		register variable
 *  v2	    r5		register variable
 *  v3	    r6		register variable
 *  v4	    r7		register variable
 *  v5	    r8		register variable
 *  sb/v6   r9		static base / register variable
 *  sl/v7   r10		stack limit / stack chunk handle / reg. variable
 *  fp	    r11		frame pointer
 *  ip	    r12		scratch register / new-sb in inter-link-unit calls
 *  sp	    r13		lower end of current stack frame
 *  lr	    r14		link address / scratch register
 *  pc	    r15		program counter
 */

/*
 * Common register frame for trap/interrupt.
 * These cpu state are saved into top of the kernel stack in
 * trap/interrupt entries. Since the arguments of system calls are
 * passed via registers, the system call library is completely
 * dependent on this register format.
 */
struct cpu_regs {
	u_long	r0;	/*  +0 (00) */
	u_long	r1;	/*  +4 (04) */
	u_long	r2;	/*  +8 (08) */
	u_long	r3;	/* +12 (0C) */
	u_long	r4;	/* +16 (10) */
	u_long	r5;	/* +20 (14) */
	u_long	r6;	/* +24 (18) */
	u_long	r7;	/* +28 (1C) */
	u_long	r8;	/* +32 (20) */
	u_long	r9;	/* +36 (24) */
	u_long	r10;	/* +40 (28) */
	u_long	r11; 	/* +44 (2C) */
	u_long	r12;	/* +48 (30) */
	u_long	sp;	/* +52 (34) */
	u_long	lr;	/* +56 (38) */
	u_long	svc_sp;	/* +60 (3C) */
	u_long	svc_lr;	/* +64 (40) */
	u_long	pc;	/* +68 (44) */
	u_long	cpsr;	/* +72 (48) */
};

/*
 * Kernel mode context for context switching.
 */
struct kern_regs {
	u_long	r4;
	u_long	r5;
	u_long	r6;
	u_long	r7;
	u_long	r8;
	u_long	r9;
	u_long	r10;
	u_long	r11;
	u_long	sp;
	u_long	lr;
};

/*
 * Processor context
 */
struct context {
	struct kern_regs kregs;		/* Kernel mode registers */
	struct cpu_regs	*uregs;		/* User mode registers */
};

typedef struct context *context_t;	/* context id */

/* Types for context_set */
#define CTX_UENTRY	0		/* Set user mode entry addres */
#define CTX_USTACK	1		/* Set user mode stack address */
#define CTX_KENTRY	2		/* Set kernel mode entry address */
#define CTX_KARG	3		/* Set kernel mode argument */

extern void context_init(context_t, u_long);
extern void context_set(context_t, int type, u_long);
extern void context_switch(context_t, context_t);
extern void context_save(context_t, int);
extern void context_restore(context_t, void *);

/*
 * Memory Management Unit
 */

typedef long *pgd_t;				/* Page directory */

/* Memory page type */
#define PG_UNMAP	0		/* no page */
#define PG_READ		1		/* read only */
#define PG_WRITE	2		/* read/write */

#ifdef CONFIG_MMU
extern void mmu_init(void);
extern int  mmu_map(pgd_t, void *, void *, size_t, int);
extern pgd_t mmu_newmap(void);
extern void mmu_delmap(pgd_t);
extern void mmu_switch(pgd_t);
extern void *mmu_extract(pgd_t, void *, size_t);
#else /* CONFIG_MMU */
#define mmu_init()		do {} while (0)
#endif /* !CONFIG_MMU */

/*
 * User Memory access
 */
extern int umem_copyin(const void *, void *, size_t);
extern int umem_copyout(const void *, void *, size_t);
extern int umem_strnlen(const char *, size_t, size_t *);

/* #define breakpoint()	__asm__ __volatile__("bkpt"::) */
#define breakpoint()		do {} while (0);

#endif /* !_ARCH_H */
