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

#ifndef CONFIG_MMU

#if defined(DEBUG) && defined(CONFIG_DEBUG_VM)
static void vm_error(const char *func, int err);

#define MEMLOG(x,y...) printk(x, ##y)
#define MEMCHK() do { if (err) vm_error(__FUNCTION__, err); } while (0)
#else
#define MEMLOG(x,y...)
#define MEMCHK()
#endif

/* vm mapping for kernel task */
static struct vm_map kern_map;

/* Forward function */
static int __vm_free(task_t task, void *addr);
static int __vm_attribute(task_t task, void *addr, int attr);
static int __vm_map(task_t target, void *addr, size_t size,
		    void **data);
static int region_create(struct region *prev, u_long addr, size_t size);
static void region_free(vm_map_t map, struct region *reg);
static void region_init(struct region *reg);

/*-
 * Allocate zero-filled memory for specified address
 *
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
__syscall int vm_allocate(task_t task, void **addr, size_t size,
			  int anywhere)
{
	int err;

	MEMLOG("vm_aloc: task=%x addr=%x size=%x name=%s\n",
	       task, *addr, size, task->name ? task->name : "no name");

	sched_lock();

	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (task != cur_task() && !capable(CAP_MEMORY)) {
		err = EPERM;
		goto out;
	}
	err = __vm_allocate(task, addr, size, anywhere, 1);
 out:
	sched_unlock();
	MEMCHK();
	return err;
}

int __vm_allocate(task_t task, void **addr, size_t size,
				int anywhere, int pagemap)
{
	vm_map_t map;
	struct region *reg;
	u_long start, end;

	if (size == 0)
		return EINVAL;

	if (anywhere) {
		ASSERT(pagemap == 1);
		size = (size_t)PAGE_ALIGN(size);
		start = (u_long)page_alloc(size);
		if (start == 0)
			return ENOMEM;
	} else {
		start = PAGE_TRUNC(*addr);
		end = PAGE_ALIGN(start + size);
		size = (size_t)(end - start);
		if (pagemap) {
			if (page_reserve((void *)start, size))
				return EINVAL;
		}
	}
	map = task->map;
	reg = &map->head;
	if (region_create(reg, start, size)) {
		if (pagemap)
			page_free((void *)start, size);
		return ENOMEM;
	}
	reg = reg->next;
	reg->flags = REG_READ | REG_WRITE;
	if (pagemap) {
		/* Zero fill */
		memset((void *)start, 0, size);
	}
	*addr = (void *)reg->addr;
	return 0;
}

/*
 * Deallocate memory region for specified address.
 *
 * The "addr" argument points to a memory region previously
 * allocated through a call to vm_allocate() or vm_map(). The number
 * of bytes freed is the number of bytes of the allocated region.
 * If one of the region of previous and next are free, it
 * combines with them, and larger free region is created.
 */
__syscall int vm_free(task_t task, void *addr)
{
	int err;

	MEMLOG("vm_free: task=%x addr=%x\n", task, addr);

	sched_lock();
	err = __vm_free(task, addr);
	sched_unlock();

	MEMCHK();
	return err;
}

static int __vm_free(task_t task, void *addr)
{
	vm_map_t map;
	struct region *reg;

	if (!task_valid(task))
		return ESRCH;

	if (task != cur_task() && !capable(CAP_MEMORY))
		return EPERM;

	if (!user_area(addr))
		return EFAULT;

	/* Find the target region */
	addr = (void *)PAGE_TRUNC(addr);
	map = task->map;
	reg = &map->head;
	for (;;) {
		if (reg->addr == (u_long)addr)
			break;
		reg = reg->next;
		if (reg == &map->head)
			return EINVAL;	/* Not found */
	}
	if (reg->addr != (u_long)addr || (reg->flags & REG_FREE))
		return EINVAL;	/* not allocated */
	region_free(map, reg);
	return 0;
}

/*
 * Change attribute of specified virtual address.
 *
 * The "addr" argument points to a memory region previously
 * allocated through a call to vm_allocate().
 * The attribute type can be chosen a combination of
 * ATTR_READ, ATTR_WRITE.
 * Note: ATTR_EXEC is not supported, yet.
 */
__syscall int vm_attribute(task_t task, void *addr, int attr)
{
	int err;

	MEMLOG("vm_attr: task=%x addr=%x attr=%x\n", task, addr, attr);

	sched_lock();
	err = __vm_attribute(task, addr, attr);
	sched_unlock();

	MEMCHK();
	return err;
}

static int __vm_attribute(task_t task, void *addr, int attr)
{
	vm_map_t map;
	struct region *reg;
	int new_flag = 0;

	if (attr == 0 || attr & ~(ATTR_READ | ATTR_WRITE))
		return EINVAL;

	if (!task_valid(task))
		return ESRCH;

	if (task != cur_task() && !capable(CAP_MEMORY))
		return EPERM;

	addr = (void *)PAGE_TRUNC(addr);

	map = task->map;

	/* Find the target region */
	reg = &map->head;
	for (;;) {
		if (reg->addr == (u_long)addr)
			break;
		reg = reg->next;
		if (reg == &map->head)
			return EINVAL;	/* Not found */
	}
	if (reg->addr != (u_long)addr || (reg->flags & REG_FREE))
		return EINVAL;	/* not allocated */

	/*
	 * The attribute of the mapped or shared region
	 * can not be changed.
	 */
	if ((reg->flags & REG_MAPPED) || (reg->flags & REG_SHARED))
		return EINVAL;

	/* Check new and old flag */
	if (reg->flags & REG_WRITE) {
		if (!(attr & ATTR_WRITE))
			new_flag = REG_READ;
	} else {
		if (attr & ATTR_WRITE)
			new_flag = REG_READ | REG_WRITE;
	}
	if (new_flag == 0)
		return 0;	/* same attribute */
	reg->flags = new_flag;
	return 0;
}

/*-
 * Map another task's memory to current task.
 *
 * @target: memory owner
 * @addr:   target address
 * @size:   map size
 * @alloc:  map address returned
 *
 * Note: This routine does not support mapping to the specific address.
 */
__syscall int vm_map(task_t target, void *addr, size_t size, void **alloc)
{
	int err;

	MEMLOG("vm_map : task=%x addr=%x size=%x\n", target, addr, size);

	sched_lock();
	err = __vm_map(target, addr, size, alloc);
	sched_unlock();

	MEMCHK();
	return err;
}

static int __vm_map(task_t target, void *addr, size_t size, void **alloc)
{
	vm_map_t map;
	u_long start, end;
	struct region *reg, *tgt;

	if (size == 0)
		return EINVAL;

	if (!task_valid(target))
		return ESRCH;

	if (target == cur_task())
		return EINVAL;

	if (!capable(CAP_MEMORY))
		return EPERM;

	if (!user_area(addr))
		return EFAULT;

	start = PAGE_TRUNC(addr);
	end = PAGE_ALIGN(addr + size);
	size = (size_t)(end - start);

	/*
	 * Find the region that includes target address
	 */
	map = target->map;
	reg = &map->head;
	for (;;) {
		if (reg->addr <= start && reg->addr + reg->size >= end)
			break;
		reg = reg->next;
		if (reg == &map->head)
			return EINVAL;	/* not found */
	}
	if (reg->flags & REG_FREE)
		return EINVAL;	/* not allocated */
	tgt = reg;

	/*
	 * Create new region to map
	 */
	map = cur_task()->map;
	reg = &map->head;
	if (region_create(reg, start, size))
		return ENOMEM;
	reg = reg->next;
	reg->flags = tgt->flags | REG_MAPPED;

	*alloc = addr;
	return 0;
}

/*
 * Create new virtual memory space.
 * No memory is inherited.
 *
 * Must be called with scheduler locked.
 */
vm_map_t vm_create(void)
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
void vm_terminate(vm_map_t map)
{
	struct region *reg, *next;

	if (--map->ref_count >= 1)
		return;

	sched_lock();
	reg = &map->head;
	do {
		next = reg->next;
		if (reg->flags != REG_FREE)
			region_free(map, reg);
		reg = next;
	} while (reg != &map->head);

	kmem_free(map);
	sched_unlock();
}

/*
 * Duplicate specified virtual memory space.
 *
 * This function is not supported with no MMU system.
 */
vm_map_t vm_fork(vm_map_t org_map)
{
	ASSERT(1);
	return NULL;
}

/*
 * Increment reference count of VM mapping.
 */
int vm_reference(vm_map_t map)
{
	map->ref_count++;
	return 0;
}

/*
 * Translate virtual address of current task to physical address.
 * Returns physical address on success, or NULL if no mapped memory.
 */
void *vm_translate(void *addr, size_t size)
{
	return addr;
}

/*
 * Check if specified access can be allowed.
 * return 0 on success, or EFAULT on failure.
 */
int vm_access(void *addr, size_t size, int type)
{
	u_long start, end;
	int err;
	char tmp;

	ASSERT(size);
	start = (u_long)addr;
	end = (u_long)addr + size - 1;
	if ((err = umem_copyin((void *)start, &tmp, 1)) != 0)
		return EFAULT;
	if (type == ATTR_WRITE) {
		if ((err = umem_copyout(&tmp, (void *)start, 1)) != 0)
			return EFAULT;
	}
	if ((err = umem_copyin((void *)end, &tmp, 1)) != 0)
		return EFAULT;
	if (type == ATTR_WRITE) {
		if ((err = umem_copyout(&tmp, (void *)end, 1)) != 0)
			return EFAULT;
	}
	return 0;
}

/*
 * Create new free region _after_ specified region
 * Returns 0 on success, or -1 on failure.
 */
static int region_create(struct region *prev, u_long addr, size_t size)
{
	struct region *reg;

	if ((reg = kmem_alloc(sizeof(struct region))) == NULL)
		return -1;

	reg->addr = addr;
	reg->size = size;
	reg->flags = REG_FREE;
	reg->sh_next = reg->sh_prev = reg;

	reg->next = prev->next;
	reg->prev = prev;
	prev->next->prev = reg;
	prev->next = reg;
	return 0;
}

/*
 * Free specified region
 */
static void region_free(vm_map_t map, struct region *reg)
{
	ASSERT(reg->flags != REG_FREE);

	/* If it is shared region, unlink from shared list */
	if (reg->flags & REG_SHARED) {
		reg->sh_prev->sh_next = reg->sh_next;
		reg->sh_next->sh_prev = reg->sh_prev;
		if (reg->sh_prev == reg->sh_next)
			reg->sh_prev->flags &= ~REG_SHARED;
	}
	/* Free region if it is not shared and mapped */
	if (!(reg->flags & REG_SHARED) && !(reg->flags & REG_MAPPED))
		page_free((void *)reg->addr, reg->size);

	reg->prev->next = reg->next;
	reg->next->prev = reg->prev;

	kmem_free(reg);
}

/*
 * Initialize region
 */
static void region_init(struct region *reg)
{
	reg->next = reg->prev = reg;
	reg->sh_next = reg->sh_prev = reg;
	reg->addr = 0;
	reg->size = 0;
	reg->flags = REG_FREE;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void vm_dump_one(task_t task)
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

void vm_dump(void)
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


#if defined(DEBUG) && defined(CONFIG_DEBUG_VM)
static void vm_error(const char *func, int err)
{
	printk("Error!!: %s returns err=%x\n", func, err);
}
#endif

void vm_init(void)
{
	region_init(&kern_map.head);
	kern_task.map = &kern_map;
}

#endif /* !CONFIG_MMU */
