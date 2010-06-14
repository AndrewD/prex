/*-
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

#ifndef _VM_H
#define _VM_H

#include <types.h>
#include <sys/cdefs.h>
#include <sys/sysinfo.h>
#include <sys/bootinfo.h>

/*
 * One structure per allocated segment.
 */
struct seg {
	struct seg	*prev;		/* segment list sorted by address */
	struct seg	*next;
	struct seg	*sh_prev;	/* link for all shared segments */
	struct seg	*sh_next;
	vaddr_t		addr;		/* base address */
	size_t		size;		/* size */
	int		flags;		/* SEG_* flag */
	paddr_t		phys;		/* physical address */
};

/* Flags for segment */
#define SEG_READ	0x00000001
#define SEG_WRITE	0x00000002
#define SEG_EXEC	0x00000004
#define SEG_SHARED	0x00000008
#define SEG_MAPPED	0x00000010
#define SEG_FREE	0x00000080

/* Attribute for vm_attribute() */
#define	PROT_NONE	0x0		/* pages cannot be accessed */
#define	PROT_READ	0x1		/* pages can be read */
#define	PROT_WRITE	0x2		/* pages can be written */
#define	PROT_EXEC	0x4		/* pages can be executed */

/*
 * VM mapping per one task.
 */
struct vm_map {
	struct seg	head;		/* list head of segements */
	int		refcnt;		/* reference count */
	pgd_t		pgd;		/* page directory */
	size_t		total;		/* total used size */
};

__BEGIN_DECLS
int	 vm_allocate(task_t, void **, size_t, int);
int	 vm_free(task_t, void *);
int	 vm_attribute(task_t, void *, int);
int	 vm_map(task_t, void *, size_t, void **);
vm_map_t vm_dup(vm_map_t);
vm_map_t vm_create(void);
int	 vm_reference(vm_map_t);
void	 vm_terminate(vm_map_t);
void	 vm_switch(vm_map_t);
int	 vm_load(vm_map_t, struct module *, void **);
paddr_t	 vm_translate(vaddr_t, size_t);
int	 vm_info(struct vminfo *);
void	 vm_init(void);
__END_DECLS

#endif /* !_VM_H */
