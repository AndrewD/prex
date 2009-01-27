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

/*
 * args.c - routine to build arguments
 */

#include <prex/prex.h>
#include <server/fs.h>
#include <server/proc.h>
#include <sys/list.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>

#include "exec.h"

/*
 * Build argument on stack.
 *
 * Stack layout:
 *    file name string
 *    env string
 *    arg string
 *    padding for alignment
 *    NULL
 *    envp[n]
 *    NULL
 *    argv[n]
 *    argc
 *
 * NOTE: This stack layout is the minimum required and is not
 * suficient to pass to main directly. Architecture dependent code in
 * crt0 or context.c processes this stack before calling main()
 */
int
build_args(task_t task, void *stack, struct exec_msg *msg, void **new_sp)
{
	int argc, envc;
	char *path, *file;
	char **argv, **envp;
	int i, err;
	u_long arg, arg_top, mapped, sp;
	size_t path_len;

	argc = msg->argc + 1;	/* allow for filename */
	envc = msg->envc;
	path = (char *)&msg->path;

	/*
	 * Validate args
	 */
	if (msg->bufsz > ARG_MAX)
		return E2BIG;

	path_len = strnlen(path, PATH_MAX);
	if (path_len >= PATH_MAX)
		return ENAMETOOLONG;

	/*
	 * Map target stack in current task.
	 */
	err = vm_map(task, stack, USTACK_SIZE, (void *)&mapped);
	if (err)
		return ENOMEM;
	memset((void *)mapped, 0, USTACK_SIZE);

	sp = mapped + USTACK_SIZE;

	/*
	 * Copy items
	 */

	/* File name */
	sp -= path_len + 1;		/* space for '\0' */
	file = (char *)sp;
	strcpy(file, path);
	DPRINTF(("exec: path %s len %d file %s\n", path, path_len, file));

	/* arg/env */
	arg_top = sp;
	sp -= msg->bufsz;
	memcpy((char *)sp, (char *)&msg->buf, msg->bufsz);
	arg = sp;

	/* envp[] */
	sp = TRUNC(sp);		/* round down to valid pointer alignment */
	sp -= ((envc + 1) * sizeof(char *));
	envp = (char **)sp;

	/* argv[] */
	sp -= ((argc + 1) * sizeof(char *));
	argv = (char **)sp;

	/* argc */
	sp -= sizeof(int);
	*(int *)(sp) = argc;

	/*
	 * Build argument list. argv[] and envp[] translated to target
	 * task addresses
	 */
	argv[0] = (char *)((u_long)stack + (u_long)file - mapped);
	DPRINTF(("exec: argv[0] %p = %s\n", argv[0], file));

	for (i = 1; i < argc; i++) { /* start after filename */
		argv[i] = (char *)((u_long)stack + (arg - mapped));
		DPRINTF(("exec: argv[%d] %p = %s\n", i, argv[i], arg));
		while (arg < arg_top && (*(char *)arg++) != '\0');
	}
	argv[argc] = NULL;

	for (i = 0; i < envc; i++) {
		envp[i] = (char *)((u_long)stack + (arg - mapped));
		DPRINTF(("exec: envp[%d] %p = %s\n", i, envp[i], arg));
		while (arg < arg_top && (*(char *)arg++) != '\0');
	}
	envp[envc] = NULL;

	*new_sp = (void *)((u_long)stack + (sp - mapped));
	vm_free(task_self(), (void *)mapped);

	return 0;
}
