/*-
 * Copyright (c) 2009, Kohsuke Ohtani
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

#ifndef _PPC_CONTEXT_H
#define _PPC_CONTEXT_H

#ifndef __ASSEMBLY__

#include <sys/types.h>

/*
 * Common register frame for trap/interrupt.
 * These cpu state are saved into top of the kernel stack in
 * trap/interrupt entries. Since the arguments of system calls are
 * passed via registers, the system call library is completely
 * dependent on this register format.
 */
struct cpu_regs {
	uint32_t	gr[32];		/* R0-R31 */
	uint32_t	lr;
	uint32_t	cr;
	uint32_t	xer;
	uint32_t	ctr;
	uint32_t	srr0;
	uint32_t	srr1;
	uint32_t	trap_no;	/* trap number */
};

/*
 * Kernel mode context for context switching.
 */
struct kern_regs {
	uint32_t	gr[19];		/* R13-R31 */
	uint32_t	r2;
	uint32_t	sp;
	uint32_t	lr;
	uint32_t	cr;
	uint32_t	kstack;		/* kernel stack */
};

/*
 * Processor context
 */
struct context {
	struct kern_regs kregs;		/* kernel mode registers */
	struct cpu_regs	*uregs;		/* user mode registers */
	struct cpu_regs	*saved_regs;	/* saved user mode registers */
};

typedef struct context *context_t;	/* context id */

#endif /* !__ASSEMBLY__ */

#define REG_R0		0x00
#define REG_R1		0x04
#define REG_R2		0x08
#define REG_R3		0x0c
#define REG_R4		0x10
#define REG_R5		0x14
#define REG_R6		0x18
#define REG_R7		0x1c
#define REG_R8		0x20
#define REG_R9		0x24
#define REG_R10		0x28
#define REG_R11		0x2c
#define REG_R12		0x30
#define REG_R13		0x34
#define REG_R14		0x38
#define REG_R15		0x3c
#define REG_R16		0x40
#define REG_R17		0x44
#define REG_R18		0x48
#define REG_R19		0x4c
#define REG_R20		0x50
#define REG_R21		0x54
#define REG_R22		0x58
#define REG_R23		0x5c
#define REG_R24		0x60
#define REG_R25		0x64
#define REG_R26		0x68
#define REG_R27		0x6c
#define REG_R28		0x70
#define REG_R29		0x74
#define REG_R30		0x78
#define REG_R31		0x7c
#define REG_LR		0x80
#define REG_CR		0x84
#define REG_XER		0x88
#define REG_CTR		0x8c
#define REG_SRR0	0x90
#define REG_SRR1	0x94
#define CTX_TRAPNO	0x98

#define CTXREGS		(4*39)

#endif /* !_PPC_CONTEXT_H */
