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
#include <string.h>
#include <errno.h>

extern int __isthreaded;

#define MALLOC_MAGIC	0xBAADF00D	/* "bad food" from LocalAlloc :) */

#if DEBUG
#define LOG(x,y...)	syslog(LOG_DEBUG, x, ##y)
#else
#define LOG(x,y...)
#endif

#define MALLOC_LOCK()	if (__isthreaded) mutex_lock(&malloc_lock);
#define MALLOC_UNLOCK()	if (__isthreaded) mutex_unlock(&malloc_lock);

#define ALIGN_SIZE      16
#define ALIGN_MASK      (ALIGN_SIZE - 1)
#define ROUNDUP(size)   (((u_long)(size) + ALIGN_MASK) & ~ALIGN_MASK)

struct header {
	struct header *next;
	size_t size;
	size_t vm_size;
#ifdef DEBUG
	int magic;
#endif
};

static mutex_t malloc_lock = MUTEX_INITIALIZER;

static struct header *more_core(size_t size);

static struct header free_list;		/* start of free list */
static struct header *scan_head;	/* start point to scan */

/*
 * Simple memory allocator from K&R
 */
void *malloc(size_t size)
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
		scan_head = &free_list;
	}
	prev = scan_head;
	for (p = prev->next;; prev = p, p = p->next) {
		if (p->size >= size) {	/* big enough */
			if (p->size == size)	/* exactly */
				prev->next = p->next;
			else {			/* allocate tail end */
				p->size -= size;
				p = (struct header *)((void *)p + p->size);
				p->size = size;
				p->vm_size = 0;
			}
#ifdef DEBUG
			p->magic = MALLOC_MAGIC;
#endif
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
#ifdef DEBUG
		sys_panic("malloc: no memory!");
#endif
		return NULL;
	}
	return (void *)(p + 1);
}

/*
 * Create new block and insert it to the free list.
 */
static struct header *more_core(size_t size)
{
	struct header *p, *prev;

	size = round_page(size);
	if (vm_allocate(task_self(), (void *)&p, size, 1))
		return NULL;
	p->size = size;
	p->vm_size = size;

	/* Insert to free list */
	for (prev = scan_head; !(p > prev && p < prev->next); prev = prev->next) {
		if (prev >= prev->next && (p > prev || p < prev->next))
			break;
	}
	p->next = prev->next;
	prev->next = p;
	scan_head = prev;
	return prev;
}

void free(void *addr)
{
	struct header *p, *prev;

	if (addr == NULL)
		return;

	MALLOC_LOCK();
	p = (struct header *)addr - 1;
#ifdef DEBUG
	if (p->magic != MALLOC_MAGIC)
		sys_panic("free: invalid pointer");
	p->magic = 0;
#endif
	for (prev = scan_head; !(p > prev && p < prev->next); prev = prev->next) {
		if (prev >= prev->next && (p > prev || p < prev->next))
			break;
	}
	if ((prev->next->vm_size == 0) &&	/* join to upper block */
	    ((void *)p + p->size == prev->next)) {
		p->size += prev->next->size;
		p->next = prev->next->next;
	} else {
		p->next = prev->next;
	}
	if ((p->vm_size == 0) &&	/* join to lower block */
	    ((void *)prev + prev->size == p)) {
		prev->size += p->size;
		prev->next = p->next;
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

void *realloc(void *addr, size_t size)
{

	/* XXX: todo */

	return NULL;
}

void mstat(void)
{
#if DEBUG
	struct header *p;

	syslog(LOG_INFO, "mstat: task=%x\n", task_self());
	for (p = free_list.next; p != &free_list; p = p->next) {
		syslog(LOG_INFO, "mstat: addr=%x size=%d next=%x\n", p, p->size, p->next);
	}
#endif
}
