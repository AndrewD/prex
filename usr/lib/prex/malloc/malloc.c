/*
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

#include <prex/prex.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include "malloc.h"

#ifdef _REENTRANT
static mutex_t malloc_lock = MUTEX_INITIALIZER;
#endif

static struct header *more_core(size_t size);

static struct header free_list;		/* start of free list */
static struct header *scan_head;	/* start point to scan */
#ifdef CONFIG_MCHECK
static struct header malloc_list;		/* start of malloc list */
#endif	/* CONFIG_MCHECK */

/*
 * Simple memory allocator from K&R
 */
void *
malloc(size_t size)
{
	struct header *p, *prev;

	if (size == 0)		/* sanity check */
		return NULL;
	size = ROUNDUP(size + sizeof(struct header));

	MALLOC_LOCK();

	if (scan_head == NULL) {
		/* Initialize */
		free_list.next = &free_list;
		free_list.size = 0;
		free_list.vm_size = 0;
		HDR_MAGIC_SET(&free_list);
		scan_head = &free_list;
#ifdef CONFIG_MCHECK
		malloc_list.next = &malloc_list;
		malloc_list.size = 0;
		malloc_list.vm_size = 0;
		HDR_MAGIC_SET(&malloc_list);
		MALLOC_MAGIC_SET(&malloc_list);
#endif	/* CONFIG_MCHECK */
	}
	prev = scan_head;
	for (p = prev->next;; prev = p, p = p->next) {
		HDR_MAGIC_ASSERT(p, "malloc: corrupt free list");
		if (p->size >= size) {	/* big enough */
			if (p->size == size)	/* exactly */
				prev->next = p->next;
			else {			/* allocate tail end */
				p->size -= size;
				p = (struct header *)((u_long)p + p->size);
				p->size = size;
				p->vm_size = 0;
				HDR_MAGIC_SET(p);
			}
			MALLOC_MAGIC_SET(p);
#ifdef CONFIG_MCHECK
			p->retaddr_p = __builtin_return_address(0);
#endif	/* CONFIG_MCHECK */
			scan_head = prev;
			break;
		}
		if (p == scan_head) {
			if ((p = more_core(size)) == NULL)
				break;
		}
	}
	MALLOC_UNLOCK();

	if (p == NULL) {
#ifdef CONFIG_MCHECK
		sys_panic("malloc: out of memory");
#endif
		errno = ENOMEM;
		return NULL;
	}
#ifdef CONFIG_MCHECK
	p->next = malloc_list.next;
	malloc_list.next = p;
#endif
	return (void *)(p + 1);
}

/*
 * Create new block and insert it to the free list.
 */
static struct header *more_core(size_t size)
{
	struct header *p, *prev;

	size = PAGE_ALIGN(size);
	if (vm_allocate(task_self(), (void *)&p, size, 1))
		return NULL;
	p->size = size;
	p->vm_size = size;
	HDR_MAGIC_SET(p);

	/* Insert to free list */
	for (prev = scan_head; !(p > prev && p < prev->next); prev = prev->next) {
		HDR_MAGIC_ASSERT(prev, "more_core: corrupt free list");
		if (prev >= prev->next && (p > prev || p < prev->next))
			break;
	}
	p->next = prev->next;
	prev->next = p;
	scan_head = prev;
	return prev;
}

void
free(void *addr)
{
	struct header *p, *prev;

	if (addr == NULL)
		return;

	MALLOC_LOCK();
	p = (struct header *)addr - 1;
	HDR_MAGIC_ASSERT(p, "free: corrupt / invalid pointer");
	MALLOC_MAGIC_ASSERT(p, "free: double free");
	MALLOC_MAGIC_CLR(p);
	for (prev = scan_head; !(p > prev && p < prev->next); prev = prev->next) {
		HDR_MAGIC_ASSERT(prev, "free: corrupt free list");
		if (prev >= prev->next && (p > prev || p < prev->next))
			break;
	}
#ifdef CONFIG_MCHECK
	for (struct header *m = &malloc_list; ; m = m->next) {
		HDR_MAGIC_ASSERT(m, "free: malloc_list hdr corrupt");
		MALLOC_MAGIC_ASSERT(m, "free: malloc_list magic corrupt");
		if (m->next == p) { /* drop from malloc list */
			m->next = p->next;
			break;
		} else if (m->next == &malloc_list) {
			VERBOSE(VB_CRIT, "missing %p\n", p);
			sys_panic("free: not in malloc list");
		}
	}
#endif	/* CONFIG_MCHECK */

	if ((prev->next->vm_size == 0) &&	/* join to upper block */
	    ((u_long)p + p->size == (u_long)prev->next)) {
		p->size += prev->next->size;
		p->next = prev->next->next;
		HDR_MAGIC_CLR(prev->next);
	} else {
		p->next = prev->next;
	}
	if ((p->vm_size == 0) &&	/* join to lower block */
	    ((u_long)prev + prev->size == (u_long)p)) {
		prev->size += p->size;
		prev->next = p->next;
		HDR_MAGIC_CLR(p);
		if (prev->size == prev->vm_size) {
			/* need to reset pointers to dealloc this pool */
			p = prev;
			for (prev = scan_head; ; prev = prev->next)
				if (prev->next == p)
					break;
		}
	} else {
		prev->next = p;
	}
	/* Deallocate pool */
	if (p->size == p->vm_size) {
		prev->next = p->next;
		vm_free(task_self(), p);
	}
	scan_head = prev;
	MALLOC_UNLOCK();
}

#ifdef CONFIG_MSTAT
void
mstat(void)
{
	struct header *p;

	syslog(LOG_INFO, "mstat: task=%x\n", task_self());

	if (scan_head == NULL)
		return;

	size_t free_total = 0;
	for (p = free_list.next; p != &free_list; p = p->next) {
		HDR_MAGIC_ASSERT(p, "mstat: free_list corrupt");
		syslog(LOG_INFO, "mstat: free addr=%x size=%d next=%x\n",
		       p, p->size, p->next);
		free_total += p->size;
	}
	syslog(LOG_INFO, "mstat: free total=%d\n", free_total);

#ifdef CONFIG_MCHECK
	size_t malloc_total = 0;
	for (p = malloc_list.next; p != &malloc_list; p = p->next) {
		HDR_MAGIC_ASSERT(p, "mstat: malloc_list corrupt");
		syslog(LOG_INFO, "mstat: malloc addr=%x size=%d retaddr=%p\n",
		       p+1, p->size, p->retaddr_p);
		malloc_total += p->size;
	}
	syslog(LOG_INFO, "mstat: malloc total=%d\n", malloc_total);
#endif	/* CONFIG_MCHECK */
}
#endif	/* CONFIG_MSTAT */

#ifdef CONFIG_MCHECK
void
mchk(void)
{
	struct header *p;

	for (p = malloc_list.next; p != &malloc_list; p = p->next) {
		HDR_MAGIC_ASSERT(p, "mchk: malloc_hdr corrupt");
		MALLOC_MAGIC_ASSERT(p, "mchk: malloc_magic corrupt");
	}
	for (p = free_list.next; p != &free_list; p = p->next)
		HDR_MAGIC_ASSERT(p, "mchk: free_hdr corrupt");
}
#endif	/* CONFIG_MCHECK */
