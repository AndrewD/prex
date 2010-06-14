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

#ifndef _DEBUG_H
#define _DEBUG_H

#include <sys/cdefs.h>
#include <hal.h>

#ifdef CONFIG_TINY
#define LOGBUFSZ	512	/* size of log buffer */
#else
#define LOGBUFSZ	2048	/* size of log buffer */
#endif

#define DBGMSGSZ	128	/* Size of one message */

#ifdef DEBUG
#define DPRINTF(a)	printf a
#define ASSERT(exp)	do { if (!(exp)) \
			     assert(__FILE__, __LINE__, #exp); } while (0)
#else
#define DPRINTF(a)	((void)0)
#define ASSERT(exp)	((void)0)
#define panic(x)	machine_abort()
#endif

__BEGIN_DECLS
#ifdef DEBUG
void	printf(const char *, ...);
void	assert(const char *, int, const char *);
void	panic(const char *);
int	dbgctl(int, void *);
#endif
__END_DECLS

#endif /* !_DEBUG_H */
