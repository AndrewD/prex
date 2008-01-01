/*
 * Copyright (c) 2006-2007, Kohsuke Ohtani
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

#ifndef _RAMFS_H
#define _RAMFS_H

#include <prex/prex.h>
#include <sys/types.h>
#include <sys/syslog.h>

#ifdef DEBUG
/* #define DEBUG_RAMFS 1 */
#endif

#ifdef DEBUG_RAMFS
#define dprintf(fmt, args...)	syslog(LOG_DEBUG, "ramfs: " fmt, ## args)
#define ASSERT(e)		assert(e)
#else
#define dprintf(fmt...)		do {} while (0)
#define ASSERT(e)
#endif

#if CONFIG_FS_THREADS > 1
#define malloc(s)		malloc_r(s)
#define free(p)			free_r(p)
#else
#define mutex_init(m)		do {} while (0)
#define mutex_destroy(m)	do {} while (0)
#define mutex_lock(m)		do {} while (0)
#define mutex_unlock(m)		do {} while (0)
#define mutex_trylock(m)	do {} while (0)
#endif

/*
 * File/directory node for RAMFS
 */
struct ramfs_node {
	struct	ramfs_node *next;   /* next node in the same directory */
	struct	ramfs_node *child;  /* first child node */
	int	type;		/* file or directory */
	char	*name;		/* name (null-terminated) */
	size_t	namelen;	/* length of name not including terminator */
	size_t	size;		/* file size */
	char	*buf;		/* buffer to the file data */
	size_t	bufsize;	/* allocated buffer size */
};

extern struct ramfs_node *ramfs_allocate_node(char *name, int type);
extern void ramfs_free_node(struct ramfs_node *node);

#endif /* !_RAMFS_H */
