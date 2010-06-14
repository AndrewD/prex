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

#include <sys/cdefs.h>
#include <sys/prex.h>
#include <sys/elf.h>
#include <sys/param.h>
#include <sys/stat.h>
#include <ipc/exec.h>

#include <unistd.h>
#include <assert.h>

/* #define DEBUG_EXEC 1 */

#ifdef DEBUG_EXEC
#define DPRINTF(a)	dprintf a
#define ASSERT(e)	dassert(e)
#else
#define DPRINTF(a)
#define ASSERT(e)
#endif

#define HEADER_SIZE	512

/*
 * Exec descriptor
 */
struct exec {
	char	*path;			/* path name */
	void	*header;		/* buffer for header */
	char	*xarg1;			/* extra arguments */
	char	*xarg2;			/* extra arguments */
	task_t	task;			/* task id */
	vaddr_t	entry;			/* entry address */
};

/*
 * Definition for exec loader
 */
struct exec_loader {
	const char *el_name;		/* name of loader */
	void	(*el_init)(void);	/* initialize routine */
	int	(*el_probe)(struct exec *);	/* probe routine */
	int	(*el_load)(struct exec *);	/* load routine */
};

/*
 * Probe result
 */
#define PROBE_ERROR		0
#define PROBE_MATCH		1
#define PROBE_INDIRECT		2


/*
 * Capability mapping
 */
struct cap_map {
	char	*c_path;		/* application name */
	cap_t	c_capset;		/* capability set */
};

/*
 * Global variables
 */
extern struct exec_loader loader_table[];
extern const struct cap_map cap_table[];
extern const int nloader;

__BEGIN_DECLS
void	 bind_cap(char *, task_t);
int	 exec_bindcap(struct bind_msg *);
int	 exec_execve(struct exec_msg *);
__END_DECLS

#endif /* !_EXEC_H */
