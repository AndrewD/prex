/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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

#ifndef _BOOT_H
#define _BOOT_H

#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/types.h>
#include <sys/bootinfo.h>

/* #define DEBUG_ELF      1 */
/* #define DEBUG_BOOTINFO 1 */

#ifdef DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#ifdef DEBUG_ELF
#define ELFDBG(a) printf a
#else
#define ELFDBG(a)
#endif

extern struct bootinfo *const bootinfo;

__BEGIN_DECLS
void	 panic(const char *);
void	 dump_bootinfo(void);
void	 splash(void);
#ifdef DEBUG
void	 printf(const char *, ...);
#endif
/* string.c */
long	 atol(const char *);
size_t	 strlcpy(char *, const char *, size_t);
int	 strncmp(const char *, const char *, size_t);
size_t	 strlcpy(char *, const char *, size_t);
size_t	 strnlen(const char *, size_t);
void	 *memcpy(void *, const void *, size_t);
void	 *memset(void *, int, size_t);
__END_DECLS

#endif /* !_BOOT_H */
