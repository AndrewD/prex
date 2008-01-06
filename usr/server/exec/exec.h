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

#ifndef _EXEC_H
#define _EXEC_H

#include <prex/prex.h>
#include <prex/elf.h>
#include <sys/param.h>
#include <sys/syslog.h>
#include <sys/stat.h>
#include <server/exec.h>

#include <unistd.h>

#ifdef DEBUG
/* #define DEBUG_EXEC 1 */
#endif

#ifdef DEBUG_EXEC
#define dprintf(fmt, ...)	syslog(LOG_DEBUG, "exec: " fmt, ## __VA_ARGS__)
#else
#define dprintf(fmt, ...)	do {} while (0)
#endif

#define PRIO_EXEC       127

#define HEADER_SIZE	512

/*
 * Definition for exec loader
 */
struct exec_loader {
	const char *name;		/* name of loader */
	void	(*init)(void);		/* initialize routine */
	int	(*probe)(void *);	/* probe routine */
	int	(*load)(void *, task_t, int, void **);	/* load routine */
};

extern object_t proc_obj;
extern object_t fs_obj;
extern struct exec_loader loader_table[];

extern int build_args(task_t, void *, struct exec_msg *, void **);

extern int file_open(char *path, int flags);
extern int file_close(int fd);
extern int file_read(int fd, void *buf, size_t len);
extern int file_lseek(int fd, off_t offset, int whence);
extern int file_fstat(int fd, struct stat *st);

#endif /* !_EXEC_H */
