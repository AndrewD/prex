/*
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

#include <prex/prex.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <string.h>
#include <errno.h>

#define ALIGN_SIZE      8
#define ALIGN_MASK      (ALIGN_SIZE - 1)
#define ROUNDUP(size)   (((u_long)(size) + ALIGN_MASK) & ~ALIGN_MASK)

struct header {
	struct header *next;
	size_t size;
};

static struct header free_list;
static struct header *scan_head;

#ifdef _REENTRANT
static mutex_t malloc_lock = MUTEX_INITIALIZER;
#define MALLOC_LOCK()	mutex_lock(&malloc_lock)
#define MALLOC_UNLOCK()	mutex_unlock(&malloc_lock)
#else
#define MALLOC_LOCK()
#define MALLOC_UNLOCK()
#endif

static struct header *more_core(size_t size);
static void add_free(struct header *hdr);


/*
 * Simple memory allocator from K&R
 */
void *malloc(size_t size)
{
	struct header *hdr, *prev;

	if (size == 0)		/* sanity check */
		return NULL;
	size = ROUNDUP(size + sizeof(struct header));

	MALLOC_LOCK();

	if (scan_head == NULL) {
		/* Initialize */
		free_list.next = &free_list;
		free_list.size = 0;
		scan_head = &free_list;
	}

	prev = scan_head;
	for (hdr = prev->next;; prev = hdr, hdr = hdr->next) {
		if (hdr->size >= size) {	/* big enough */
			if (hdr->size == size)	/* exactly */
				prev->next = hdr->next;
			else {		/* allocate tail end */
				hdr->size -= size;
				hdr = (struct header *)
					((u_long)hdr + hdr->size);
				hdr->size = size;
			}
			scan_head = prev;
			break;
		}
		if (hdr == scan_head) {
			if ((hdr = more_core(size)) == NULL)
				break;
		}
	}

	MALLOC_UNLOCK();

	if (hdr == NULL)
		return NULL;
	return (void *)((u_long)hdr + sizeof(struct header));
}

/*
 * Create new block and insert it to the free list.
 */
static struct header *more_core(size_t size)
{
	struct header *hdr;

	size = round_page(size);

	if (vm_allocate(task_self(), (void *)&hdr, size, 1))
		return NULL;
	hdr->size = size;
	add_free(hdr);
	return scan_head;
}

static void add_free(struct header *hdr)
{
	struct header *prev;

	for (prev = scan_head; !(hdr > prev && hdr < prev->next); prev = prev->next)
		if (prev >= prev->next && (hdr > prev || hdr < prev->next))
			break;

	if (hdr + hdr->size == prev->next) {
		hdr->size += prev->next->size;
		hdr->next = prev->next->next;
	} else
		hdr->next = prev->next;

	if (prev + prev->size == hdr) {
		prev->size += hdr->size;
		prev->next = hdr->next;
	} else
		prev->next = hdr;

	scan_head = prev;
}

void free(void *addr)
{
	struct header *hdr;

	MALLOC_LOCK();

	hdr = (struct header *)((u_long)addr - sizeof(struct header));
	add_free(hdr);

	MALLOC_UNLOCK();
}

void *realloc(void *addr, size_t size)
{

	/* XXX: todo */

	return NULL;
}

void mstat(void)
{
#if 0
	struct header *hdr;

	syslog(LOG_INFO, "malloc usage task=%x\n", task_self());
	for (hdr = free_list.next; hdr != &free_list; hdr = hdr->next) {
		syslog(LOG_INFO, "free: addr=%x size=%d byte\n", hdr, hdr->size);
	}
#endif
}
