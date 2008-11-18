/*-
 * Copyright (c) 2008, Andrew Dennison
 * Adapted from page.h:
 * Copyright (c) 2005, Kohsuke Ohtani
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

#ifndef _KPAGE_H
#define _KPAGE_H

#ifdef CONFIG_KMEM_PROTECT

#ifndef __ppc__
#warning only implemented on ppc
#endif	/* __ppc__ */

extern void	*kpage_alloc(size_t);
extern void	 kpage_free(void *, size_t);
extern void	 kpage_info(size_t *, size_t *);
extern void	 kpage_dump(void);
extern void	 kpage_init(void);

#else  /* !CONFIG_KMEM_PROTECT */

#include <page.h>
#define kpage_alloc(sz) page_alloc(sz)
#define kpage_free(p, sz) page_free(p, sz)
#define kpage_info(p_total, p_free) do {	\
		*(p_total) = 0;			\
		*(p_free) = 0;			\
	} while (0)
#define kpage_dump()
#define kpage_init()

#endif	/* !CONFIG_KMEM_PROTECT */

#endif /* !_KPAGE_H */
