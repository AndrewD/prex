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

/*
 * exec_execve.c - execve support
 */

#include <sys/prex.h>
#include <sys/capability.h>
#include <ipc/fs.h>
#include <ipc/proc.h>
#include <ipc/ipc.h>
#include <sys/list.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <assert.h>
#include <errno.h>
#include <libgen.h>	/* for basename() */

#include "exec.h"

#define	SP_ALIGN(p)	((unsigned)(p) &~ _ALIGNBYTES)

/* forward declarations */
static int	build_args(task_t, void *, char *, struct exec_msg *,
			   char *, char *, void **);
static int	conv_path(char *, char *, char *);
static void	notify_server(task_t, task_t, void *);
static int	read_header(char *);

/*
 * Buffer for file header
 */
static char hdrbuf[HEADER_SIZE];

/*
 * Execute program
 */
int
exec_execve(struct exec_msg *msg)
{
	struct exec_loader *ldr = NULL;
	int error, i;
	task_t old_task, new_task;
	thread_t t;
	void *stack, *sp;
	char path[PATH_MAX];
	struct exec exec;
	int rc;

	DPRINTF(("exec_execve: path=%s task=%x\n", msg->path, msg->hdr.task));

	old_task = msg->hdr.task;

	/*
	 * Make it full path.
	 */
	if ((error = conv_path(msg->cwd, msg->path, path)) != 0) {
		DPRINTF(("exec: invalid path\n"));
		goto err1;
	}

	/*
	 * Check permission.
	 */
	if (access(path, X_OK) == -1) {
		DPRINTF(("exec: no exec access\n"));
		error = errno;
		goto err1;
	}

	exec.path = path;
	exec.header = hdrbuf;
	exec.xarg1 = NULL;
	exec.xarg2 = NULL;

 again:
	/*
	 * Read file header
	 */
	DPRINTF(("exec: read header for %s\n", exec.path));
	if ((error = read_header(exec.path)) != 0)
		goto err1;

	/*
	 * Find file loader
	 */
	rc = PROBE_ERROR;
	for (i = 0; i < nloader; i++) {
		ldr = &loader_table[i];
		if ((rc = ldr->el_probe(&exec)) != PROBE_ERROR) {
			break;
		}
	}
	if (rc == PROBE_ERROR) {
		DPRINTF(("exec: unsupported file format\n"));
		error = ENOEXEC;
		goto err1;
	}

	/*
	 * Check file header again if indirect case.
	 */
	if (rc == PROBE_INDIRECT)
			goto again;

	DPRINTF(("exec: loader=%s\n", ldr->el_name));

	/*
	 * Check file permission.
	 */
	if (access(exec.path, X_OK) == -1) {
		DPRINTF(("exec: no exec access\n"));
		error = errno;
		goto err1;
	}

	/*
	 * Suspend old task
	 */
	if ((error = task_suspend(old_task)) != 0)
		goto err1;

	/*
	 * Create new task
	 */
	if ((error = task_create(old_task, VM_NEW, &new_task)) != 0) {
		DPRINTF(("exec: failed to crete task\n"));
		goto err1;
	}

	if (*exec.path != '\0')
		task_setname(new_task, basename(exec.path));

	/*
	 * Bind capabilities.
	 */
	bind_cap(exec.path, new_task);

	if ((error = thread_create(new_task, &t)) != 0)
		goto err3;

	/*
	 * Allocate stack and build arguments on it.
	 */
	error = vm_allocate(new_task, &stack, DFLSTKSZ, 1);
	if (error) {
		DPRINTF(("exec: failed to allocate stack\n"));
		goto err4;
	}
	if ((error = build_args(new_task, stack, exec.path, msg,
				exec.xarg1, exec.xarg2, &sp)) != 0)
		goto err5;

	/*
	 * Load file image.
	 */
	DPRINTF(("exec: load file image\n"));
	exec.task = new_task;
	if ((error = ldr->el_load(&exec)) != 0)
		goto err5;
	if ((error = thread_load(t, (void (*)(void))exec.entry, sp)) != 0)
		goto err5;

	/*
	 * Notify to servers.
	 */
	notify_server(old_task, new_task, stack);

	/*
	 * Terminate old task.
	 */
	task_terminate(old_task);

	/*
	 * Set him running.
	 */
	thread_setpri(t, PRI_DEFAULT);
	thread_resume(t);

	DPRINTF(("exec done\n"));
	return 0;
 err5:
	vm_free(new_task, stack);
 err4:
	thread_terminate(t);
 err3:
	task_terminate(new_task);
 err1:
	DPRINTF(("exec failed error=%d\n", error));
	return error;
}

/*
 * Convert to full path from the cwd of task and path.
 * @cwd:  current working directory
 * @path: target path
 * @full: full path to be returned
 */
static int
conv_path(char *cwd, char *path, char *full)
{
	char *src, *tgt, *p, *end;
	size_t len = 0;

	path[PATH_MAX - 1] = '\0';
	len = strlen(path);
	if (len >= PATH_MAX)
		return ENAMETOOLONG;
	if (strlen(cwd) + len >= PATH_MAX)
		return ENAMETOOLONG;
	src = path;
	tgt = full;
	end = src + len;
	if (path[0] == '/') {
		*tgt++ = *src++;
		len++;
	} else {
		strlcpy(full, cwd, PATH_MAX);
		len = strlen(cwd);
		tgt += len;
		if (len > 1 && path[0] != '.') {
			*tgt = '/';
			tgt++;
			len++;
		}
	}
	while (*src) {
		p = src;
		while (*p != '/' && *p != '\0')
			p++;
		*p = '\0';
		if (!strcmp(src, "..")) {
			if (len >= 2) {
				len -= 2;
				tgt -= 2;	/* skip previous '/' */
				while (*tgt != '/') {
					tgt--;
					len--;
				}
				if (len == 0) {
					tgt++;
					len++;
				}
			}
		} else if (!strcmp(src, ".")) {
			/* Ignore "." */
		} else {
			while (*src != '\0') {
				*tgt++ = *src++;
				len++;
			}
		}
		if (p == end)
			break;
		if (len > 0 && *(tgt - 1) != '/') {
			*tgt++ = '/';
			len++;
		}
		src = p + 1;
	}
	*tgt = '\0';
	return 0;
}

/*
 * Build argument on stack.
 *
 * Stack layout:
 *    file name string
 *    env string
 *    arg string
 *    NULL
 *    envp[n]
 *    NULL
 *    argv[n]
 *    argc
 *
 * NOTE: This may depend on processor architecture.
 */
static int
build_args(task_t task, void *stack, char *path, struct exec_msg *msg,
	   char *xarg1, char *xarg2, void **new_sp)
{
	int argc, envc;
	char *file;
	char **argv, **envp;
	int i, error;
	u_long arg_top, mapped, sp;
	int len;

	argc = msg->argc;
	envc = msg->envc;
	DPRINTF(("exec: argc=%d envc=%d\n", argc, envc));
	DPRINTF(("exec: xarg1=%s xarg2=%s\n", xarg1, xarg2));

	/*
	 * Map target stack in current task.
	 */
	error = vm_map(task, stack, DFLSTKSZ, (void *)&mapped);
	if (error)
		return ENOMEM;
	memset((void *)mapped, 0, DFLSTKSZ);
	sp = mapped + DFLSTKSZ - sizeof(int) * 3;

	/*
	 * Copy items
	 */

	/* File name */
	*(char *)sp = '\0';
	sp -= strlen(path);
	sp = SP_ALIGN(sp);
	strlcpy((char *)sp, path, PATH_MAX);
	file = (char *)sp;

	/* arg/env */
	sp -= msg->bufsz;
	sp = SP_ALIGN(sp);
	memcpy((char *)sp, (char *)&msg->buf, msg->bufsz);
	arg_top = sp;

	/*
	 * Insert extra argument for indirect loader.
	 */
	if (xarg2 != NULL) {
		len = strlen(xarg2);
		sp -= (len + 1);
		strlcpy((char *)sp, xarg2, len + 1);
		arg_top = sp;
		argc++;
	}
	if (xarg1 != NULL) {
		len = strlen(xarg1);
		sp -= (len + 1);
		strlcpy((char *)sp, xarg1, len + 1);
		arg_top = sp;
		argc++;
	}

	/* envp[] */
	sp -= ((envc + 1) * sizeof(char *));
	envp = (char **)sp;

	/* argv[] */
	sp -= ((argc + 1) * sizeof(char *));
	argv = (char **)sp;

	/* argc */
	sp -= sizeof(int);
	*(int *)(sp) = argc + 1;

	/*
	 * Build argument list
	 */
	argv[0] = (char *)((u_long)stack + (u_long)file - mapped);

	for (i = 1; i <= argc; i++) {
		argv[i] = (char *)((u_long)stack + (arg_top - mapped));
		while ((*(char *)arg_top++) != '\0');
	}
	argv[argc + 1] = NULL;

	for (i = 0; i < envc; i++) {
		envp[i] = (char *)((u_long)stack + (arg_top - mapped));
		while ((*(char *)arg_top++) != '\0');
	}
	envp[envc] = NULL;

	*new_sp = (void *)((u_long)stack + (sp - mapped));
	vm_free(task_self(), (void *)mapped);

	return 0;
}

/*
 * Notify exec() to servers.
 */
static void
notify_server(task_t org_task, task_t new_task, void *stack)
{
	struct msg m;
	int error;
	object_t fsobj, procobj;

	if (object_lookup("!fs", &fsobj) != 0)
		return;

	if (object_lookup("!proc", &procobj) != 0)
		return;

	/* Notify to file system server */
	do {
		m.hdr.code = FS_EXEC;
		m.data[0] = (int)org_task;
		m.data[1] = (int)new_task;
		error = msg_send(fsobj, &m, sizeof(m));
	} while (error == EINTR);

	/* Notify to process server */
	do {
		m.hdr.code = PS_EXEC;
		m.data[0] = (int)org_task;
		m.data[1] = (int)new_task;
		m.data[2] = (int)stack;
		error = msg_send(procobj, &m, sizeof(m));
	} while (error == EINTR);
}

static int
read_header(char *path)
{
	int fd;
	struct stat st;

	/*
	 * Check target file type.
	 */
	if ((fd = open(path, O_RDONLY)) == -1)
		return ENOENT;

	if (fstat(fd, &st) == -1) {
		close(fd);
		return EIO;
	}
	if (!S_ISREG(st.st_mode)) {
		DPRINTF(("exec: not regular file\n"));
		close(fd);
		return EACCES;	/* must be regular file */
	}
	/*
	 * Read file header.
	 */
	memset(hdrbuf, 0, HEADER_SIZE);
	if (read(fd, hdrbuf, HEADER_SIZE) == -1) {
		close(fd);
		return EIO;
	}
	close(fd);
	return 0;
}
