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

/*
 * vm_nommu.c - virtual memory functions for no MMU systems
 */

/*
 * When the platform does not support memory management unit (MMU)
 * all virtual memories are mapped to the physical memory. So, the
 * memory space is shared among all tasks and kernel.
 *
 * Important: The lists of regions are not sorted by address.
 */

#include <kernel.h>
#include <kmem.h>
#include <thread.h>
#include <page.h>
#include <task.h>
#include <sched.h>
#include <vm.h>

#ifdef CONFIG_VMTRACE
static void vm_error(const char *, int);
#define LOG(x)		printk x
#define CHK(fn,x)	do { if (x) vm_error(fn, x); } while (0)
#else
#define LOG(x)
#define CHK(fn,x)
#endif

/* forward declarations */
static struct region *region_create(struct region *, u_long, size_t);
static void region_delete(struct region *, struct region *);
static struct region *region_find(struct region *, u_long, size_t);
static void region_free(struct region *, struct region *);
static void region_init(struct region *);
static int do_allocate(vm_map_t, void **, size_t, int);
static int do_free(vm_map_t, void *);
static int do_attribute(vm_map_t, void *, int);
static int do_map(vm_map_t, void *, size_t, void **);

/* vm mapping for kernel task */
static struct vm_map kern_map;

/**
 * vm_allocate - allocate zero-filled memory for specified address
 * @task:     task id to allocate memory
 * @addr:     required address. set an allocated address in return.
 * @size:     allocation size
 * @anywhere: if it is true, the "addr" argument will be ignored.
 *            In this case, the address of free space will be found
 *            automatically.
 *
 * The allocated area has writable, user-access attribute by default.
 * The "addr" and "size" argument will be adjusted to page boundary.
 */
int
vm_allocate(task_t task, void **addr, size_t size, int anywhere)
{
	int err;
	void *uaddr;

	LOG(("vm_aloc: task=%s addr=%x size=%x anywhere=%d\n",
	     task->name ? task->name : "no name", *addr, size, anywhere));

	sched_lock();

	if (!task_valid(task)) {
		err = ESRCH;
	} else if (task != cur_task() && !task_capable(CAP_MEMORY)) {
		err = EPERM;
	} else if (umem_copyin(addr, &uaddr, sizeof(void *))) {
		err = EFAULT;
	} else if (anywhere == 0 && !user_area(*addr)) {
		err = EACCES;
	} else {
		err = do_allocate(task->map, &uaddr, size, anywhere);
		if (err == 0) {
			if (umem_copyout(&uaddr, addr, sizeof(void *)))
				err = EFAULT;
		}
	}
	sched_unlock();
	CHK("vm_allocate", err);
	return err;
}

static int
do_allocate(vm_map_t map, void **addr, size_t size, int anywhere)
{
	struct region *reg;
	u_long start, end;

	if (size == 0)
		return EINVAL;

	/*
	 * Allocate region, and reserve pages for it.
	 */
	if (anywhere) {
		size = (size_t)PAGE_ALIGN(size);
		if ((start = (u_long)page_alloc(size)) == 0)
			return ENOMEM;
	} else {
		start = PAGE_TRUNC(*addr);
		end = PAGE_ALIGN(start + size);
		size = (size_t)(end - start);

		if (page_reserve((void *)start, size))
			return EINVAL;
	}
	reg = region_create(&map->head, start, size);
	if (reg == NULL) {
     		page_free((void *)start, size);
		return ENOMEM;
	}
	reg->flags = REG_READ | REG_WRITE;

	/* Zero fill */
	memset((void *)start, 0, size);
	*addr = (void *)reg->addr;
	return 0;
}

/*
 * Deallocate memory region for specified address.
 *
 * The "addr" argument points to a memory region previously
 * allocated through a call to vm_allocate() or vm_map(). The number
 * of bytes freed is the number of bytes of the allocated region.
 * If one of the region of previous and next are free, it combines
 * with them, and larger free region is created.
 */
int
vm_free(task_t task, void *addr)
{
	int err;

	LOG(("vm_free: task=%s addr=%x\n",
	     task->name ? task->name : "no name", addr));

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
	} else if (task != cur_task() && !task_capable(CAP_MEMORY)) {
		err = EPERM;
	} else if (!user_area(addr)) {
		err = EFAULT;
	} else {
		err = do_free(task->map, addr);
	}
	sched_unlock();
	CHK("vm_free", err);
	return err;
}

static int
do_free(vm_map_t map, void *addr)
{
	struct region *reg;

	addr = (void *)PAGE_TRUNC(addr);

	/*
	 * Find the target region.
	 */
	reg = region_find(&map->head, (u_long)addr, 1);
	if (reg == NULL || reg->addr != (u_long)addr ||
	    (reg->flags & REG_FREE))
		return EINVAL;	/* not allocated */

	/*
	 * Free pages if it is not shared and mapped.
	 */
	if (!(reg->flags & REG_SHARED) && !(reg->flags & REG_MAPPED))
		page_free((void *)reg->addr, reg->size);

	region_free(&map->head, reg);
	return 0;
}

/*
 * Change attribute of specified virtual address.
 *
 * The "addr" argument points to a memory region previously allocated
 * through a call to vm_allocate(). The attribute type can be chosen
 * a combination of VMA_READ, VMA_WRITE.
 * Note: VMA_EXEC is not supported, yet.
 */
int
vm_attribute(task_t task, void *addr, int attr)
{
	int err;

	LOG(("vm_attr: task=%s addr=%x attr=%x\n",
	     task->name ? task->name : "no name", addr, attr));

	sched_lock();
	if (attr == 0 || attr & ~(VMA_READ | VMA_WRITE)) {
		err = EINVAL;
	} else if (!task_valid(task)) {
		err = ESRCH;
	} else if (task != cur_task() && !task_capable(CAP_MEMORY)) {
		err = EPERM;
	} else if (!user_area(addr)) {
		err = EFAULT;
	} else {
		err = do_attribute(task->map, addr, attr);
	}
	sched_unlock();
	CHK("vm_attribute", err);
	return err;
}

static int
do_attribute(vm_map_t map, void *addr, int attr)
{
	struct region *reg;
	int new_flags = 0;

	addr = (void *)PAGE_TRUNC(addr);

	/*
	 * Find the target region.
	 */
	reg = region_find(&map->head, (u_long)addr, 1);
	if (reg == NULL || reg->addr != (u_long)addr ||
	    (reg->flags & REG_FREE)) {
		return EINVAL;	/* not allocated */
	}
	/*
	 * The attribute of the mapped or shared region can not be changed.
	 */
	if ((reg->flags & REG_MAPPED) || (reg->flags & REG_SHARED))
		return EINVAL;

	/*
	 * Check new and old flag.
	 */
	if (reg->flags & REG_WRITE) {
		if (!(attr & VMA_WRITE))
			new_flags = REG_READ;
	} else {
		if (attr & VMA_WRITE)
			new_flags = REG_READ | REG_WRITE;
	}
	if (new_flags == 0)
		return 0;	/* same attribute */
	reg->flags = new_flags;
	return 0;
}

/**
 * vm_map - map another task's memory to current task.
 * @target: memory owner
 * @addr:   target address
 * @size:   map size
 * @alloc:  map address returned
 *
 * Note: This routine does not support mapping to the specific address.
 */
int
vm_map(task_t target, void *addr, size_t size, void **alloc)
{
	int err;

	LOG(("vm_map : task=%s addr=%x size=%x\n",
	     target->name ? target->name : "no name", addr, size));

	sched_lock();
	if (!task_valid(target)) {
		err = ESRCH;
	} else if (target == cur_task()) {
		err = EINVAL;
	} else if (!task_capable(CAP_MEMORY)) {
		err = EPERM;
	} else if (!user_area(addr)) {
		err = EFAULT;
	} else {
		err = do_map(target->map, addr, size, alloc);
	}
	sched_unlock();
	CHK("vm_map", err);
	return err;
}

static int
do_map(vm_map_t map, void *addr, size_t size, void **alloc)
{
	vm_map_t curmap;
	u_long start, end;
	struct region *reg, *tgt;
	void *tmp;

	if (size == 0)
		return EINVAL;

	/* check fault */
	tmp = NULL;
	if (umem_copyout(&tmp, alloc, sizeof(void *)))
		return EFAULT;

	start = PAGE_TRUNC(addr);
	end = PAGE_ALIGN((u_long)addr + size);
	size = (size_t)(end - start);

	/*
	 * Find the region that includes target address
	 */
	reg = region_find(&map->head, start, size);
	if (reg == NULL || (reg->flags & REG_FREE))
		return EINVAL;	/* not allocated */
	tgt = reg;

	/*
	 * Create new region to map
	 */
	curmap = cur_task()->map;
	reg = region_create(&curmap->head, start, size);
	if (reg == NULL)
		return ENOMEM;
	reg->flags = tgt->flags | REG_MAPPED;

	umem_copyout(&addr, alloc, sizeof(void *));
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
	vm_map_t map;

	/* Allocate new map structure */
	if ((map = kmem_alloc(sizeof(struct vm_map))) == NULL)
		return NULL;

	map->ref_count = 1;
	region_init(&map->head);
	return map;
}

/*
 * Terminate specified virtual memory space.
 * This is called when task is terminated.
 */
void
vm_terminate(vm_map_t map)
{
	struct region *reg, *tmp;

	if (--map->ref_count >= 1)
		return;

	sched_lock();
	reg = &map->head;
	do {
		if (reg->flags != REG_FREE) {
			/* Free region if it is not shared and mapped */
			if (!(reg->flags & REG_SHARED) &&
			    !(reg->flags & REG_MAPPED)) {
				page_free((void *)reg->addr, reg->size);
			}
		}
		tmp = reg;
		reg = reg->next;
		region_delete(&map->head, tmp);
	} while (reg != &map->head);

	kmem_free(map);
	sched_unlock();
}

/*
 * Duplicate specified virtual memory space.
 */
vm_map_t
vm_fork(vm_map_t org_map)
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

	map->ref_count++;
	return 0;
}

/*
 * Translate virtual address of current task to physical address.
 * Returns physical address on success, or NULL if no mapped memory.
 */
void *
vm_translate(void *addr, size_t size)
{
	return addr;
}

/*
 * Check if specified access can be allowed.
 * return 0 on success, or EFAULT on failure.
 */
int
vm_access(void *addr, size_t size, int type)
{
	u_long start, end;
	int err;
	char tmp;

	ASSERT(size);
	start = (u_long)addr;
	end = (u_long)addr + size - 1;
	if ((err = umem_copyin((void *)start, &tmp, 1)))
		return EFAULT;
	if (type == VMA_WRITE) {
		if ((err = umem_copyout(&tmp, (void *)start, 1)))
			return EFAULT;
	}
	if ((err = umem_copyin((void *)end, &tmp, 1)))
		return EFAULT;
	if (type == VMA_WRITE) {
		if ((err = umem_copyout(&tmp, (void *)end, 1)))
			return EFAULT;
	}
	return 0;
}

/*
 * Reserve specific area for boot tasks.
 */
static int
do_reserve(vm_map_t map, void **addr, size_t size)
{
	struct region *reg;
	u_long start, end;

	if (size == 0)
		return EINVAL;

	start = PAGE_TRUNC(*addr);
	end = PAGE_ALIGN(start + size);
	size = (size_t)(end - start);

	reg = region_create(&map->head, start, size);
	if (reg == NULL)
		return ENOMEM;
	reg->flags = REG_READ | REG_WRITE;
	*addr = (void *)reg->addr;
	return 0;
}

/*
 * Setup task image for boot task. (NOMMU version)
 * Return 0 on success, -1 on failure.
 *
 * Note: We assume that the task images are already copied to
 * the proper address by a boot loader.
 */
int
vm_load(vm_map_t map, struct module *m, void **stack)
{
	void *base;
	size_t size;

	printk("Loading task:\'%s\'\n", m->name);

	/*
	 * Reserve text & data area
	 */
	base = (void *)m->text;
	size = m->textsz + m->datasz + m->bsssz;
	if (do_reserve(map, &base, size))
		return -1;
	if (m->bsssz != 0)
		memset((void *)(m->data + m->datasz), 0, m->bsssz);

	/*
	 * Create stack
	 */
	if (do_allocate(map, stack, USTACK_SIZE, 1))
		return -1;
	return 0;
}

/*
 * Create new free region after the specified region.
 * Returns region on success, or NULL on failure.
 */
static struct region *
region_create(struct region *prev, u_long addr, size_t size)
{
	struct region *reg;

	if ((reg = kmem_alloc(sizeof(struct region))) == NULL)
		return NULL;

	reg->addr = addr;
	reg->size = size;
	reg->flags = REG_FREE;
	reg->sh_next = reg->sh_prev = reg;

	reg->next = prev->next;
	reg->prev = prev;
	prev->next->prev = reg;
	prev->next = reg;
	return reg;
}

/*
 * Delete specified region
 */
static void
region_delete(struct region *head, struct region *reg)
{

	/*
	 * If it is shared region, unlink from shared list.
	 */
	if (reg->flags & REG_SHARED) {
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
	}
	if (head != reg)
		kmem_free(reg);
}

/*
 * Find the region at the specified area.
 */
static struct region *
region_find(struct region *head, u_long addr, size_t size)
{
	struct region *reg;

	reg = head;
	do {
		if (reg->addr <= addr &&
		    reg->addr + reg->size >= addr + size) {
			return reg;
		}
		reg = reg->next;
	} while (reg != head);
	return NULL;
}

/*
 * Free specified region
 */
static void
region_free(struct region *head, struct region *reg)
{
	ASSERT(reg->flags != REG_FREE);

	/*
	 * If it is shared region, unlink from shared list.
	 */
	if (reg->flags & REG_SHARED) {
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
	}
	reg->prev->next = reg->next;
	reg->next->prev = reg->prev;
	kmem_free(reg);
}

/*
 * Initialize region
 */
static void
region_init(struct region *reg)
{

	reg->next = reg->prev = reg;
	reg->sh_next = reg->sh_prev = reg;
	reg->addr = 0;
	reg->size = 0;
	reg->flags = REG_FREE;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void
vm_dump_one(task_t task)
{
	vm_map_t map;
	struct region *reg;
	char flags[6];
	u_long total = 0;

	printk("task=%x map=%x name=%s\n", task, task->map,
	       task->name ? task->name : "no name");
	printk(" region   virtual  size     flags\n");
	printk(" -------- -------- -------- -----\n");

	map = task->map;
	reg = &map->head;
	do {
		if (reg->flags != REG_FREE) {
			strlcpy(flags, "-----", 6);
			if (reg->flags & REG_READ)
				flags[0] = 'R';
			if (reg->flags & REG_WRITE)
				flags[1] = 'W';
			if (reg->flags & REG_EXEC)
				flags[2] = 'E';
			if (reg->flags & REG_SHARED)
				flags[3] = 'S';
			if (reg->flags & REG_MAPPED)
				flags[4] = 'M';

			printk(" %08x %08x %08x %s\n", reg,
			       reg->addr, reg->size, flags);
			if ((reg->flags & REG_MAPPED) == 0)
				total += reg->size;
		}
		reg = reg->next;
	} while (reg != &map->head);	/* Process all regions */
	printk(" *total=%dK bytes\n\n", total / 1024);
}

void
vm_dump(void)
{
	list_t n;
	task_t task;

	printk("\nVM dump:\n");
	n = &kern_task.link;
	do {
		task = list_entry(n, struct task, link);
		ASSERT(task_valid(task));
		vm_dump_one(task);
		n = list_next(n);
	} while (n != &kern_task.link);
}
#endif


#ifdef CONFIG_VMTRACE
static void
vm_error(const char *func, int err)
{
	printk("Error!!: %s returns err=%x\n", func, err);
}
#endif

void
vm_init(void)
{

	region_init(&kern_map.head);
	kern_task.map = &kern_map;
}
