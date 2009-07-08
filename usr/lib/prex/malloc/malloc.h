/*
 * Copyright (c) 2007, Kohsuke Ohtani
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
#include <verbose.h>

/* #define CONFIG_MSTAT		1 */
/* #define CONFIG_MCHECK	1 */

#define MALLOC_MAGIC	(int)0xBAADF00D	/* "bad food" from LocalAlloc :) */
#define HDR_MAGIC	(int)0xCAFEBEEF

#ifdef _REENTRANT
#define MALLOC_LOCK()	mutex_lock(&malloc_lock);
#define MALLOC_UNLOCK()	mutex_unlock(&malloc_lock);
#else
#define MALLOC_LOCK()	do {} while (0)
#define MALLOC_UNLOCK()	do {} while (0)
#endif

#ifdef CONFIG_MCHECK
#define HDR_MAGIC_SET(p) (p)->hdr_magic = HDR_MAGIC
#define HDR_MAGIC_CLR(p) (p)->hdr_magic = 0
#define HDR_MAGIC_ASSERT(p, str) do {					\
		if ((p)->hdr_magic != HDR_MAGIC) {			\
			VERBOSE(VB_CRIT, "HDR %p", (p)+1);		\
			sys_panic(str);					\
		}							\
	} while (0)

#define MALLOC_MAGIC_SET(p) (p)->malloc_magic = MALLOC_MAGIC
#define MALLOC_MAGIC_CLR(p) (p)->malloc_magic = 0
#define MALLOC_MAGIC_ASSERT(p, str) do {				\
		if ((p)->malloc_magic != MALLOC_MAGIC) {		\
			VERBOSE(VB_CRIT, "MALLOC %p", (p)+1);		\
			sys_panic(str);					\
		}							\
	} while (0)
#else
#define HDR_MAGIC_SET(p)
#define HDR_MAGIC_CLR(p)
#define HDR_MAGIC_ASSERT(p, str)

#define MALLOC_MAGIC_SET(p)
#define MALLOC_MAGIC_CLR(p)
#define MALLOC_MAGIC_ASSERT(p, str)
#endif

#define ALIGN_SIZE      16
#define ALIGN_MASK      (ALIGN_SIZE - 1)
#define ROUNDUP(size)   (((u_long)(size) + ALIGN_MASK) & ~ALIGN_MASK)

struct header {
#ifdef CONFIG_MCHECK
	int hdr_magic;		/* set for every header */
#endif
	struct header *next;
	size_t size;
	size_t vm_size;
#ifdef CONFIG_MCHECK
	int malloc_magic;	/* only set when allocated */
#endif
};
