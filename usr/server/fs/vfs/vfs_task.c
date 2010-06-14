/*-
 * Copyright (c) 2007, Kohsuke Ohtani All rights reserved.
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
 * vfs_task.c - Routines to manage the per task data.
 */

#include <sys/prex.h>
#include <sys/list.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>

#include "vfs.h"

#define TASK_MAXBUCKETS	32		/* number of task hash buckets */

#define TASKHASH(x)        (int)((x) & (TASK_MAXBUCKETS - 1))

/*
 * Hash table for task.
 */
static struct list task_table[TASK_MAXBUCKETS];

/*
 * Global lock for task access.
 */
#if CONFIG_FS_THREADS > 1
static mutex_t task_lock = MUTEX_INITIALIZER;
#define TASK_LOCK()	mutex_lock(&task_lock)
#define TASK_UNLOCK()	mutex_unlock(&task_lock)
#else
#define TASK_LOCK()
#define TASK_UNLOCK()
#endif

/*
 * Convert task ID to a task structure.
 * Returns locked task. Caller must unlock it after using it.
 */
struct task *
task_lookup(task_t task)
{
	list_t head, n;
	struct task *t;

	if (task == TASK_NULL)
		return NULL;

	TASK_LOCK();
	head = &task_table[TASKHASH(task)];
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct task, t_link);
		ASSERT(t->t_taskid);
		if (t->t_taskid == task) {
			TASK_UNLOCK();
			mutex_lock(&t->t_lock);
			return t;
		}
	}
	TASK_UNLOCK();

	/* Not found */
	return NULL;
}

/*
 * Allocate new task.
 */
int
task_alloc(task_t task, struct task **pt)
{
	struct task *t;

	/* Check if specified task already exists. */
	if (task_lookup(task) != NULL)
		return EINVAL;

	if (!(t = malloc(sizeof(struct task))))
		return ENOMEM;
	memset(t, 0, sizeof(struct task));
	t->t_taskid = task;
	strlcpy(t->t_cwd, "/", sizeof(t->t_cwd));
	mutex_init(&t->t_lock);

	TASK_LOCK();
	list_insert(&task_table[TASKHASH(task)], &t->t_link);
	TASK_UNLOCK();
	*pt = t;
	return 0;
}

/*
 * Free task and related resource.
 */
void
task_free(struct task *t)
{

	TASK_LOCK();
	list_remove(&t->t_link);
	mutex_unlock(&t->t_lock);
	mutex_destroy(&t->t_lock);
	free(t);
	TASK_UNLOCK();
}

/*
 * Set task id of the specified task.
 */
void
task_setid(struct task *t, task_t task)
{

	TASK_LOCK();
	list_remove(&t->t_link);
	t->t_taskid = task;
	list_insert(&task_table[TASKHASH(task)], &t->t_link);
	TASK_UNLOCK();
}

/*
 * Unlock task.
 */
void
task_unlock(struct task *t)
{

	mutex_unlock(&t->t_lock);
}

/*
 * Convert a file descriptor into a pointer
 * to a file structre.
 */
file_t
task_getfp(struct task *t, int fd)
{

	if (fd < 0 || fd >= OPEN_MAX)
		return NULL;

	return t->t_ofile[fd];
}

/*
 * Set file pointer for task/fd pair.
 */
void
task_setfp(struct task *t, int fd, file_t fp)
{

	t->t_ofile[fd] = fp;
}

/*
 * Get new file descriptor in the task.
 * Returns -1 if there is no empty slot.
 */
int
task_newfd(struct task *t)
{
	int fd;

	/*
	 * Find the smallest empty slot in the fd array.
	 */
	for (fd = 0; fd < OPEN_MAX; fd++) {
		if (t->t_ofile[fd] == NULL)
			break;
	}
	if (fd == OPEN_MAX)
		return -1;	/* slot full */

	return fd;
}

/*
 * Delete a file descriptor.
 */
void
task_delfd(struct task *t, int fd)
{

	t->t_ofile[fd] = NULL;
}

/*
 * Convert to full path from the cwd of task and path.
 * @t:    task structure
 * @path: target path
 * @full: full path to be returned
 * @acc: access mode
 */
int
task_conv(struct task *t, char *path, int acc, char *full)
{
	char *src, *tgt, *p, *end, *cwd;
	size_t len = 0;

	cwd = t->t_cwd;
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

	/* Check if the client task has required permission */
	return sec_file_permission(t->t_taskid, full, acc);
}

#ifdef DEBUG_VFS
void
task_dump(void)
{
	list_t head, n;
	struct task *t;
	int i;

	TASK_LOCK();
	dprintf("Dump file data\n");
	dprintf(" task     opens   cwd\n");
	dprintf(" -------- ------- ------------------------------\n");
	for (i = 0; i < TASK_MAXBUCKETS; i++) {
		head = &task_table[i];
		for (n = list_first(head); n != head; n = list_next(n)) {
			t = list_entry(n, struct task, t_link);
			dprintf(" %08x %7x %s\n", (int)t->t_taskid, t->t_nopens,
			       t->t_cwd);
		}
	}
	dprintf("\n");
	TASK_UNLOCK();
}
#endif

void
task_init(void)
{
	int i;

	for (i = 0; i < TASK_MAXBUCKETS; i++)
		list_init(&task_table[i]);
}
