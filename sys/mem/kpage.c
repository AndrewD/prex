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
#include <syspage.h>
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
static size_t total_size;

/*
 * kpage_alloc - allocate continuous pages of the specified size.
 *
 * This routine returns the physical address of a new free page
 * block, or returns NULL on failure. The requested size is
 * automatically round up to the page boundary.  The allocated
 * memory is _not_ filled with 0.
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
			DPRINTF(("kpage_alloc: out of memory\n"));
			kpage_dump();
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
		tmp = (struct page_block *)((char *)blk + size);
		tmp->size = blk->size - size;
		tmp->prev = blk->prev;
		tmp->next = blk->next;
		blk->prev->next = tmp;
		blk->next->prev = tmp;
	}
	sched_unlock();
	return virt_to_phys(blk);
}

/*
 * Free page block.
 *
 * This allocator does not maintain the size of allocated page
 * block. The caller must provide the size information of the
 * block.
 */
void
kpage_free(void *addr, size_t size)
{
	struct page_block *blk, *prev;

	ASSERT(size != 0);

	sched_lock();

	size = (size_t)PAGE_ALIGN(size);
	blk = (struct page_block *)phys_to_virt(addr);

	/*
	 * Find the target position in list.
	 */
	for (prev = &page_head; prev->next < blk; prev = prev->next) {
		if (prev->next == &page_head)
			break;
	}

#ifdef DEBUG
	if (prev != &page_head)
		ASSERT((char *)prev + prev->size <= (char *)blk);
	if (prev->next != &page_head)
		ASSERT((char *)blk + size <= (char *)prev->next);
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
	    ((char *)blk + blk->size) == (char *)blk->next) {
		blk->size += blk->next->size;
		blk->next = blk->next->next;
		blk->next->prev = blk;
	}
	if (blk->prev != &page_head &&
	    (char *)blk->prev + blk->prev->size == (char *)blk) {
		blk->prev->size += blk->size;
		blk->prev->next = blk->next;
		blk->next->prev = blk->prev;
	}
	sched_unlock();
}

void
kpage_info(struct info_memory *info)
{
	struct page_block *blk;
	size_t free = 0;

	blk = page_head.next;
	while (blk != &page_head) {
		free += blk->size;
		blk = blk->next;
	}

	info->kpage_total = total_size;
	info->kpage_free = free;
}

void
kpage_dump(void)
{
#ifdef DEBUG
	struct page_block *blk;
	void *addr;
	size_t free = 0;

	printf("kpage Free pages:\n");
	printf(" start      end      size\n");
	printf(" --------   -------- --------\n");

	blk = page_head.next;
	while (blk != &page_head) {
		free += blk->size;
		addr = virt_to_phys(blk);
		printf(" %08x - %08x %7dK\n", addr, (char *)addr + blk->size,
		       blk->size / 1024);
		blk = blk->next;
	}

	printf(" used=%dK free=%dK total=%dK\n",
	       (total_size - free) / 1024, free / 1024, total_size / 1024);
#endif
}

/*
 * Initialize page allocator.
 * page_init() must be called prior to other memory manager's
 * initializations.
 */
void
kpage_init(void)
{
	struct physmem *ram;
	int i;

	total_size = 0;
	page_head.next = page_head.prev = &page_head;

	/*
	 * Create a free list from the boot information.
	 */
	for (i = 0; i < bootinfo->nr_rams; i++) {
		ram = &bootinfo->ram[i];
		if (ram->type == MT_KPAGE) {
			kpage_free((void *)ram->base, ram->size);
			total_size += ram->size;
		}
	}

	kpage_dump();
}
