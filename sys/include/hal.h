/*
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

#ifndef _PREX_HAL_H
#define _PREX_HAL_H

#include <types.h>
#include <sys/bootinfo.h>
#include <context.h>
#include <mmu.h>

#define NO_PGD		((pgd_t)0)	/* non-existent pgd */

/*
 * Types for context_set()
 */
#define CTX_KSTACK	0	/* set kernel mode stack address */
#define CTX_KENTRY	1	/* set kernel mode entry address */
#define CTX_KARG	2	/* set kernel mode argument */
#define CTX_USTACK	3	/* set user mode stack address */
#define CTX_UENTRY	4	/* set user mode entry addres */
#define CTX_UARG	5	/* set user mode argument */

/*
 * page types for mmu_map()
 */
#define PG_UNMAP	0	/* no page */
#define PG_READ		1	/* user - read only */
#define PG_WRITE	2	/* user - read/write */
#define PG_SYSTEM	3	/* system */
#define PG_IOMEM	4	/* system - no cache */

/*
 * Virtual/physical address mapping
 */
struct mmumap
{
	vaddr_t		virt;		/* virtual address */
	paddr_t		phys;		/* physical address */
	psize_t		size;		/* size */
	int		type;		/* mapping type */
};

#define AUTOSIZE	0

/*
 * type of virtual memory mappings
 */
#define VMT_NULL	0
#define VMT_RAM		1
#define VMT_ROM		2
#define VMT_DMA		3
#define VMT_IO		4


/*
 * Return value of ISR
 */
#define INT_DONE	0	/* done */
#define INT_ERROR	1	/* error */
#define INT_CONTINUE	2	/* continue to IST */

/* No IST for irq_attach() */
#define IST_NONE        ((void (*)(void *)) -1)

/*
 * Interrupt mode for interrupt_setup()
 */
#define IMODE_EDGE	0	/* edge trigger */
#define IMODE_LEVEL	1	/* level trigger */


__BEGIN_DECLS
void	  context_set(context_t, int, register_t);
void	  context_switch(context_t, context_t);
void	  context_save(context_t);
void	  context_restore(context_t);
void	  context_dump(context_t);

void	  mmu_init(struct mmumap *);
void	  mmu_premap(paddr_t, vaddr_t);
pgd_t	  mmu_newmap(void);
void	  mmu_terminate(pgd_t);
int	  mmu_map(pgd_t, paddr_t, vaddr_t, size_t, int);
void	  mmu_switch(pgd_t);
paddr_t	  mmu_extract(pgd_t, vaddr_t, size_t);

int	  copyin(const void *, void *, size_t);
int	  copyout(const void *, void *, size_t);
int	  copyinstr(const void *, void *, size_t);

int	  splhigh(void);
int	  spl0(void);
void	  splx(int);

void	  syscall_ret(void);

void	  interrupt_mask(int);
void	  interrupt_unmask(int, int);
void	  interrupt_setup(int, int);
void	  interrupt_init(void);

void	  machine_startup(void);
void	  machine_idle(void);
void	  machine_powerdown(int);
void	  machine_abort(void);
void	  machine_bootinfo(struct bootinfo **);

void	  clock_init(void);

#ifdef DEBUG
void	  diag_init(void);
void	  diag_puts(char *);
#else
#define   diag_init()	((void)0)
#endif
__END_DECLS

#endif /* !_PREX_HAL_H */
