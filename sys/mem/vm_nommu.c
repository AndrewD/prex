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

/*
 * vm_nommu.c - virtual memory alloctor for no MMU systems
 */

/*
 * When the platform does not support memory management unit (MMU)
 * all virtual memories are mapped to the physical memory. So, the
 * memory space is shared among all tasks and kernel.
 *
 * Important: The lists of segments are not sorted by address.
 */

#include <kernel.h>
#include <kmem.h>
#include <thread.h>
#include <page.h>
#include <task.h>
#include <sched.h>
#include <vm.h>

/* forward declarations */
static void	   seg_init(struct seg *);
static struct seg *seg_create(struct seg *, vaddr_t, size_t);
static void	   seg_delete(struct seg *, struct seg *);
static struct seg *seg_lookup(struct seg *, vaddr_t, size_t);
static struct seg *seg_alloc(struct seg *, size_t);
static void	   seg_free(struct seg *, struct seg *);
static struct seg *seg_reserve(struct seg *, vaddr_t, size_t);
static int	   do_allocate(vm_map_t, void **, size_t, int);
static int	   do_free(vm_map_t, void *);
static int	   do_attribute(vm_map_t, void *, int);
static int	   do_map(vm_map_t, void *, size_t, void **);


static struct vm_map	kernel_map;	/* vm mapping for kernel */

/**
 * vm_allocate - allocate zero-filled memory for specified address
 *
 * If "anywhere" argument is true, the "addr" argument will be
 * ignored.  In this case, the address of free space will be
 * found automatically.
 *
 * The allocated area has writable, user-access attribute by
 * default.  The "addr" and "size" argument will be adjusted
 * to page boundary.
 */
int
vm_allocate(task_t task, void **addr, size_t size, int anywhere)
{
	int error;
	void *uaddr;

	sched_lock();

	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != curtask && !task_capable(CAP_EXTMEM)) {
		sched_unlock();
		return EPERM;
	}
	if (copyin(addr, &uaddr, sizeof(*addr))) {
		sched_unlock();
		return EFAULT;
	}
	if (anywhere == 0 && !user_area(*addr)) {
		sched_unlock();
		return EACCES;
	}

	error = do_allocate(task->map, &uaddr, size, anywhere);
	if (!error) {
		if (copyout(&uaddr, addr, sizeof(uaddr)))
			error = EFAULT;
	}
	sched_unlock();
	return error;
}

static int
do_allocate(vm_map_t map, void **addr, size_t size, int anywhere)
{
	struct seg *seg;
	vaddr_t start, end;

	if (size == 0)
		return EINVAL;
	if (map->total + size >= MAXMEM)
		return ENOMEM;

	/*
	 * Allocate segment, and reserve pages for it.
	 */
	if (anywhere) {
		size = round_page(size);
		if ((seg = seg_alloc(&map->head, size)) == NULL)
			return ENOMEM;
		start = seg->addr;
	} else {
		start = trunc_page((vaddr_t)*addr);
		end = round_page(start + size);
		size = (size_t)(end - start);

		if ((seg = seg_reserve(&map->head, start, size)) == NULL)
			return ENOMEM;
	}
	seg->flags = SEG_READ | SEG_WRITE;

	/* Zero fill */
	memset((void *)start, 0, size);
	*addr = (void *)seg->addr;
	map->total += size;
	return 0;
}

/*
 * Deallocate memory segment for specified address.
 *
 * The "addr" argument points to a memory segment previously
 * allocated through a call to vm_allocate() or vm_map(). The
 * number of bytes freed is the number of bytes of the
 * allocated segment.  If one of the segment of previous and
 * next are free, it combines with them, and larger free
 * segment is created.
 */
int
vm_free(task_t task, void *addr)
{
	int error;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != curtask && !task_capable(CAP_EXTMEM)) {
		sched_unlock();
		return EPERM;
	}
	if (!user_area(addr)) {
		sched_unlock();
		return EFAULT;
	}

	error = do_free(task->map, addr);

	sched_unlock();
	return error;
}

static int
do_free(vm_map_t map, void *addr)
{
	struct seg *seg;
	vaddr_t va;

	va = trunc_page((vaddr_t)addr);

	/*
	 * Find the target segment.
	 */
	seg = seg_lookup(&map->head, va, 1);
	if (seg == NULL || seg->addr != va || (seg->flags & SEG_FREE))
		return EINVAL;	/* not allocated */

	/*
	 * Relinquish use of the page if it is not shared and mapped.
	 */
	if (!(seg->flags & SEG_SHARED) && !(seg->flags & SEG_MAPPED))
		page_free(seg->phys, seg->size);

	map->total -= seg->size;
	seg_free(&map->head, seg);

	return 0;
}

/*
 * Change attribute of specified virtual address.
 *
 * The "addr" argument points to a memory segment previously
 * allocated through a call to vm_allocate(). The attribute
 * type can be chosen a combination of PROT_READ, PROT_WRITE.
 * Note: PROT_EXEC is not supported, yet.
 */
int
vm_attribute(task_t task, void *addr, int attr)
{
	int error;

	sched_lock();
	if (attr == 0 || attr & ~(PROT_READ | PROT_WRITE)) {
		sched_unlock();
		return EINVAL;
	}
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != curtask && !task_capable(CAP_EXTMEM)) {
		sched_unlock();
		return EPERM;
	}
	if (!user_area(addr)) {
		sched_unlock();
		return EFAULT;
	}

	error = do_attribute(task->map, addr, attr);

	sched_unlock();
	return error;
}

static int
do_attribute(vm_map_t map, void *addr, int attr)
{
	struct seg *seg;
	int new_flags = 0;
	vaddr_t va;

	va = trunc_page((vaddr_t)addr);

	/*
	 * Find the target segment.
	 */
	seg = seg_lookup(&map->head, va, 1);
	if (seg == NULL || seg->addr != va || (seg->flags & SEG_FREE)) {
		return EINVAL;	/* not allocated */
	}
	/*
	 * The attribute of the mapped or shared segment can not be changed.
	 */
	if ((seg->flags & SEG_MAPPED) || (seg->flags & SEG_SHARED))
		return EINVAL;

	/*
	 * Check new and old flag.
	 */
	if (seg->flags & SEG_WRITE) {
		if (!(attr & PROT_WRITE))
			new_flags = SEG_READ;
	} else {
		if (attr & PROT_WRITE)
			new_flags = SEG_READ | SEG_WRITE;
	}
	if (new_flags == 0)
		return 0;	/* same attribute */
	seg->flags = new_flags;
	return 0;
}

/**
 * vm_map - map another task's memory to current task.
 *
 * Note: This routine does not support mapping to the specific
 * address.
 */
int
vm_map(task_t target, void *addr, size_t size, void **alloc)
{
	int error;

	sched_lock();
	if (!task_valid(target)) {
		sched_unlock();
		return ESRCH;
	}
	if (target == curtask) {
		sched_unlock();
		return EINVAL;
	}
	if (!task_capable(CAP_EXTMEM)) {
		sched_unlock();
		return EPERM;
	}
	if (!user_area(addr)) {
		sched_unlock();
		return EFAULT;
	}

	error = do_map(target->map, addr, size, alloc);

	sched_unlock();
	return error;
}

static int
do_map(vm_map_t map, void *addr, size_t size, void **alloc)
{
	struct seg *seg, *tgt;
	vm_map_t curmap;
	vaddr_t start, end;
	void *tmp;

	if (size == 0)
		return EINVAL;
	if (map->total + size >= MAXMEM)
		return ENOMEM;

	/* check fault */
	tmp = NULL;
	if (copyout(&tmp, alloc, sizeof(tmp)))
		return EFAULT;

	start = trunc_page((vaddr_t)addr);
	end = round_page((vaddr_t)addr + size);
	size = (size_t)(end - start);

	/*
	 * Find the segment that includes target address
	 */
	seg = seg_lookup(&map->head, start, size);
	if (seg == NULL || (seg->flags & SEG_FREE))
		return EINVAL;	/* not allocated */
	tgt = seg;

	/*
	 * Create new segment to map
	 */
	curmap = curtask->map;
	if ((seg = seg_create(&curmap->head, start, size)) == NULL)
		return ENOMEM;
	seg->flags = tgt->flags | SEG_MAPPED;

	copyout(&addr, alloc, sizeof(addr));

	curmap->total += size;
	return 0;
}

/*
 * Create new virtual memory space.
 * No memory is inherited.
 * Must be called with scheduler locked.
 */
vm_map_t
vm_create(void)
{
	struct vm_map *map;

	/* Allocate new map structure */
	if ((map = kmem_alloc(sizeof(*map))) == NULL)
		return NULL;

	map->refcnt = 1;
	map->total = 0;

	seg_init(&map->head);
	return map;
}

/*
 * Terminate specified virtual memory space.
 * This is called when task is terminated.
 */
void
vm_terminate(vm_map_t map)
{
	struct seg *seg, *tmp;

	if (--map->refcnt > 0)
		return;

	sched_lock();
	seg = &map->head;
	do {
		if (seg->flags != SEG_FREE) {
			/* Free segment if it is not shared and mapped */
			if (!(seg->flags & SEG_SHARED) &&
			    !(seg->flags & SEG_MAPPED)) {
				page_free(seg->phys, seg->size);
			}
		}
		tmp = seg;
		seg = seg->next;
		seg_delete(&map->head, tmp);
	} while (seg != &map->head);

	kmem_free(map);
	sched_unlock();
}

/*
 * Duplicate specified virtual memory space.
 */
vm_map_t
vm_dup(vm_map_t org_map)
{
	/*
	 * This function is not supported with no MMU system.
	 */
	return NULL;
}

/*
 * Switch VM mapping.
 */
void
vm_switch(vm_map_t map)
{
}

/*
 * Increment reference count of VM mapping.
 */
int
vm_reference(vm_map_t map)
{

	map->refcnt++;
	return 0;
}

/*
 * Setup task image for boot task. (NOMMU version)
 * Return 0 on success, errno on failure.
 *
 * Note: We assume that the task images are already copied to
 * the proper address by a boot loader.
 */
int
vm_load(vm_map_t map, struct module *mod, void **stack)
{
	struct seg *seg;
	vaddr_t base, start, end;
	size_t size;

	DPRINTF(("Loading task:\'%s\'\n", mod->name));

	/*
	 * Reserve text & data area
	 */
	base = mod->text;
	size = mod->textsz + mod->datasz + mod->bsssz;
	if (size == 0)
		return EINVAL;

	start = trunc_page(base);
	end = round_page(start + size);
	size = (size_t)(end - start);

	if ((seg = seg_create(&map->head, start, size)) == NULL)
		return ENOMEM;

	seg->flags = SEG_READ | SEG_WRITE;

	if (mod->bsssz != 0)
		memset((void *)(mod->data + mod->datasz), 0, mod->bsssz);

	/*
	 * Create stack
	 */
	return do_allocate(map, stack, DFLSTKSZ, 1);
}

/*
 * Translate virtual address of current task to physical address.
 * Returns physical address on success, or NULL if no mapped memory.
 */
paddr_t
vm_translate(vaddr_t addr, size_t size)
{

	return (paddr_t)addr;
}

int
vm_info(struct vminfo *info)
{
	u_long target = info->cookie;
	task_t task = info->task;
	u_long i;
	vm_map_t map;
	struct seg *seg;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	map = task->map;
	seg = &map->head;
	i = 0;
	do {
		if (i++ == target) {
			info->cookie = i;
			info->virt = seg->addr;
			info->size = seg->size;
			info->flags = seg->flags;
			info->phys = seg->phys;
			sched_unlock();
			return 0;
		}
		seg = seg->next;
	} while (seg != &map->head);
	sched_unlock();
	return ESRCH;
}

void
vm_init(void)
{

	seg_init(&kernel_map.head);
	kernel_task.map = &kernel_map;
}

/*
 * Initialize segment.
 */
static void
seg_init(struct seg *seg)
{

	seg->next = seg->prev = seg;
	seg->sh_next = seg->sh_prev = seg;
	seg->addr = 0;
	seg->phys = 0;
	seg->size = 0;
	seg->flags = SEG_FREE;
}

/*
 * Create new free segment after the specified segment.
 * Returns segment on success, or NULL on failure.
 */
static struct seg *
seg_create(struct seg *prev, vaddr_t addr, size_t size)
{
	struct seg *seg;

	if ((seg = kmem_alloc(sizeof(*seg))) == NULL)
		return NULL;

	seg->addr = addr;
	seg->size = size;
	seg->phys = (paddr_t)addr;
	seg->flags = SEG_FREE;
	seg->sh_next = seg->sh_prev = seg;

	seg->next = prev->next;
	seg->prev = prev;
	prev->next->prev = seg;
	prev->next = seg;

	return seg;
}

/*
 * Delete specified segment.
 */
static void
seg_delete(struct seg *head, struct seg *seg)
{

	/*
	 * If it is shared segment, unlink from shared list.
	 */
	if (seg->flags & SEG_SHARED) {
		seg->sh_prev->sh_next = seg->sh_next;
		seg->sh_next->sh_prev = seg->sh_prev;
		if (seg->sh_prev == seg->sh_next)
			seg->sh_prev->flags &= ~SEG_SHARED;
	}
	if (head != seg)
		kmem_free(seg);
}

/*
 * Find the segment at the specified address.
 */
static struct seg *
seg_lookup(struct seg *head, vaddr_t addr, size_t size)
{
	struct seg *seg;

	seg = head;
	do {
		if (seg->addr <= addr &&
		    seg->addr + seg->size >= addr + size) {
			return seg;
		}
		seg = seg->next;
	} while (seg != head);
	return NULL;
}

/*
 * Allocate free segment for specified size.
 */
static struct seg *
seg_alloc(struct seg *head, size_t size)
{
	struct seg *seg;
	paddr_t pa;

	if ((pa = page_alloc(size)) == 0)
		return NULL;

	if ((seg = seg_create(head, (vaddr_t)pa, size)) == NULL) {
     		page_free(pa, size);
		return NULL;
	}
	return seg;
}

/*
 * Delete specified free segment.
 */
static void
seg_free(struct seg *head, struct seg *seg)
{
	ASSERT(seg->flags != SEG_FREE);

	/*
	 * If it is shared segment, unlink from shared list.
	 */
	if (seg->flags & SEG_SHARED) {
		seg->sh_prev->sh_next = seg->sh_next;
		seg->sh_next->sh_prev = seg->sh_prev;
		if (seg->sh_prev == seg->sh_next)
			seg->sh_prev->flags &= ~SEG_SHARED;
	}
	seg->prev->next = seg->next;
	seg->next->prev = seg->prev;

	kmem_free(seg);
}

/*
 * Reserve the segment at the specified address/size.
 */
static struct seg *
seg_reserve(struct seg *head, vaddr_t addr, size_t size)
{
	struct seg *seg;
	paddr_t pa;

	pa = (paddr_t)addr;

	if (page_reserve(pa, size) != 0)
		return NULL;

	if ((seg = seg_create(head, (vaddr_t)pa, size)) == NULL) {
     		page_free(pa, size);
		return NULL;
	}
	return seg;
}
