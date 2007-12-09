/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

/*
 * VM Region
 */
struct region {
	struct region *prev;	/* Region list sorted by address */
	struct region *next;
	struct region *sh_prev;	/* Link for all shared region */
	struct region *sh_next;
	u_long	addr;		/* Base address */
	size_t 	size;		/* Size */
	int 	flags;		/* Flag */
#ifdef CONFIG_MMU
	u_long 	phys;		/* Physical address */
#endif
};

/*
 * VM Map
 */
struct vm_map {
	struct region head;	/* List head of regions */
	int	ref_count;	/* Reference count */
	pgd_t	pgd;		/* Page directory */
};
typedef struct vm_map *vm_map_t;

/*
 * Flags for region
 */
#define REG_READ	0x00000001
#define REG_WRITE	0x00000002
#define REG_EXEC	0x00000004
#define REG_SHARED	0x00000008
#define REG_MAPPED	0x00000010
#define REG_FREE	0x80000000

/*
 * VM attribute
 */
#define ATTR_READ	0x01
#define ATTR_WRITE	0x02
#define ATTR_EXEC	0x04

extern void vm_init(void);
extern int vm_allocate(task_t task, void **addr, size_t size, int anywhere);
extern int __vm_allocate(task_t task, void **addr, size_t size,
			 int anywhere, int pagemap);
extern int vm_free(task_t task, void *addr);
extern int vm_attribute(task_t task, void *addr, int attr);
extern int vm_map(task_t target, void *addr, size_t size, void **alloc);
extern vm_map_t vm_fork(vm_map_t map);
extern vm_map_t vm_create(void);
extern int vm_reference(vm_map_t map);
extern void vm_terminate(vm_map_t map);
extern void *vm_translate(void *addr, size_t size);
extern int vm_access(void *addr, size_t size, int type);

#endif /* !_VM_H */
