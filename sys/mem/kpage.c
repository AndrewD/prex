/*-
 * Copyright (c) 2008, Andrew Dennison
 * Adapted from page.c:
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
 * kpage.c - physical page allocator
 */

/*
 * This is a simple list-based page allocator.
 *
 * When the remaining page is exhausted, what should we do ?
 * If the system can stop with panic() here, the error check of many
 * portions in kernel is not necessary, and kernel code can become
 * more simple. But, in general, even if a page is exhausted,
 * a kernel can not be stopped but it should return an error and
 * continue processing.
 * If the memory becomes short during boot time, kernel and drivers
 * can use panic() in that case.
 */

#include <kernel.h>
#include <kpage.h>
#include <sched.h>

/*
 * page_block is put on the head of the first page of
 * each free block.
 */
struct page_block {
	struct	page_block *next;
	struct	page_block *prev;
	size_t	size;		/* number of bytes of this block */
};

static struct page_block page_head;	/* first free block */

static size_t total_bytes;
static size_t used_bytes;

/*
 * page_alloc - allocate continuous pages of the specified size.
 * @size: number of bytes to allocate
 *
 * This routine returns the physical address of a new free page block,
 * or returns NULL on failure. The requested size is automatically
 * round up to the page boundary.
 * The allocated memory is _not_ filled with 0.
 */
void *
kpage_alloc(size_t size)
{
	struct page_block *blk, *tmp;

	ASSERT(size != 0);

	sched_lock();

	/*
	 * Find the free block that has enough size.
	 */
	size = (size_t)PAGE_ALIGN(size);
	blk = &page_head;
	do {
		blk = blk->next;
		if (blk == &page_head) {
			sched_unlock();
			printk("page_alloc: out of memory\n");
			return NULL;	/* Not found. */
		}
	} while (blk->size < size);

	/*
	 * If found block size is exactly same with requested,
	 * just remove it from a free list. Otherwise, the
	 * found block is divided into two and first half is
	 * used for allocation.
	 */
	if (blk->size == size) {
		blk->prev->next = blk->next;
		blk->next->prev = blk->prev;
	} else {
		tmp = (struct page_block *)((u_long)blk + size);
		tmp->size = blk->size - size;
		tmp->prev = blk->prev;
		tmp->next = blk->next;
		blk->prev->next = tmp;
		blk->next->prev = tmp;
	}
	used_bytes += size;
	sched_unlock();

	return virt_to_phys(blk);
}

/*
 * Free page block.
 *
 * This allocator does not maintain the size of allocated page block.
 * The caller must provide the size information of the block.
 */
void
kpage_free(void *addr, size_t size)
{
	struct page_block *blk, *prev;

	ASSERT(addr != NULL);
	ASSERT(size != 0);

	sched_lock();

	size = (size_t)PAGE_ALIGN(size);
	blk = phys_to_virt(addr);

	/*
	 * Find the target position in list.
	 */
	for (prev = &page_head; prev->next < blk; prev = prev->next) {
		if (prev->next == &page_head)
			break;
	}
#ifdef DEBUG
	if (prev != &page_head)
		ASSERT((u_long)prev + prev->size <= (u_long)blk);
	if (prev->next != &page_head)
		ASSERT((u_long)blk + size <= (u_long)prev->next);
#endif /* DEBUG */

	/*
	 * Insert new block into list.
	 */
	blk->size = size;
	blk->prev = prev;
	blk->next = prev->next;
	prev->next->prev = blk;
	prev->next = blk;

	/*
	 * If the adjoining block is free, it combines and
	 * is made on block.
	 */
	if (blk->next != &page_head &&
	    ((u_long)blk + blk->size) == (u_long)blk->next) {
		blk->size += blk->next->size;
		blk->next = blk->next->next;
		blk->next->prev = blk;
	}
	if (blk->prev != &page_head &&
	    (u_long)blk->prev + blk->prev->size == (u_long)blk) {
		blk->prev->size += blk->size;
		blk->prev->next = blk->next;
		blk->next->prev = blk->prev;
	}
	used_bytes -= size;
	sched_unlock();
}

void
kpage_info(size_t *total, size_t *free)
{

	*total = total_bytes;
	*free = total_bytes - used_bytes;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void
kpage_dump(void)
{
	struct page_block *blk;
	void *addr;

	printk("kpage dump:\n");
	printk(" free pages:\n");
	printk(" start      end      size\n");
	printk(" --------   -------- --------\n");

	blk = page_head.next;
	do {
		addr = virt_to_phys(blk);
		printk(" %08x - %08x %8x\n", addr, (u_long)addr + blk->size,
		       blk->size);
		blk = blk->next;
	} while (blk != &page_head);
	printk(" used=%dK free=%dK total=%dK\n\n",
	       used_bytes / 1024, (total_bytes - used_bytes) / 1024,
	       total_bytes / 1024);
}
#endif

/*
 * Initialize page allocator.
 * page_init() must be called prior to other memory manager's
 * initializations.
 */
void
kpage_init(void)
{
	struct page_block *blk;
	struct mem_map kmem;

	kmem.start = boot_info->driver.phys + boot_info->driver.size;
	kmem.size = boot_info->tasks[0].phys - kmem.start;

	printk("kpage mem: base=%x size=%dK\n", kmem.start, kmem.size / 1024);

	/*
	 * First, create one block containing all kmem pages.
	 */
	blk = (struct page_block *)kmem.start;
	blk = phys_to_virt(blk);
	blk->size = kmem.size;
	if (blk->size == 0)
		panic("kpage_init: no pages from loader");
	blk->prev = blk->next = &page_head;
	page_head.next = page_head.prev = blk;

	total_bytes = kmem.size;
	used_bytes = 0;
}
