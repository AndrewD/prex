/*-
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

#ifndef _VM_H
#define _VM_H

#include <arch.h>

/*
 * One structure per allocated region.
 */
struct region {
	struct region	*prev;		/* region list sorted by address */
	struct region	*next;
	struct region	*sh_prev;	/* link for all shared region */
	struct region	*sh_next;
	u_long		addr;		/* base address */
	size_t		size;		/* size */
	u_int		flags;		/* flag */
#ifdef CONFIG_MMU
	u_long		phys;		/* physical address */
#endif
};

/* Flags for region */
#define REG_READ	0x00000001
#define REG_WRITE	0x00000002
#define REG_EXEC	0x00000004
#define REG_SHARED	0x00000008
#define REG_MAPPED	0x00000010
#define REG_FREE	0x00000080

/*
 * VM mapping per one task.
 */
struct vm_map {
	struct region	head;		/* list head of regions */
	int		ref_count;	/* reference count */
	pgd_t		pgd;		/* page directory */
};

/* VM attributes */
#define VMA_READ	0x01
#define VMA_WRITE	0x02
#define VMA_EXEC	0x04

extern int	 vm_allocate(task_t, void **, size_t, int);
extern int	 vm_free(task_t, void *);
extern int	 vm_attribute(task_t, void *, int);
extern int	 vm_map(task_t, void *, size_t, void **);
extern vm_map_t	 vm_fork(vm_map_t);
extern vm_map_t	 vm_create(void);
extern int	 vm_reference(vm_map_t);
extern void	 vm_terminate(vm_map_t);
extern void	 vm_switch(vm_map_t);
extern int	 vm_load(vm_map_t, struct module *, void **);
extern void	*vm_translate(void *, size_t);
extern int	 vm_access(void *, size_t, int);
extern void	 vm_dump(void);
extern void	 vm_init(void);

#endif /* !_VM_H */
