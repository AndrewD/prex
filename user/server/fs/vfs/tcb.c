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

/*
 * tcb.c - Routines to manage task control block.
 */

#include <prex/prex.h>
#include <sys/list.h>

#include <limits.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>

#include "vfs.h"

#define TASK_MAXBUCKETS	32		/* Number of task hash buckets */

#define TASKHASH(x)        (int)((x) & (TASK_MAXBUCKETS - 1))

/*
 * Hash table for tcb.
 */
static struct list tcb_table[TASK_MAXBUCKETS];

/*
 * Global lock for tcb access.
 */
#if NR_FS_THREADS > 1
static mutex_t tcb_lock = MUTEX_INITIALIZER;
#define TCB_LOCK()	mutex_lock(&tcb_lock)
#define TCB_UNLOCK()	mutex_unlock(&tcb_lock)
#else
#define TCB_LOCK()
#define TCB_UNLOCK()
#endif

/*
 * Lookup tcb by task id.
 * Returns locked tcb. Caller must unlock it after using it.
 */
tcb_t tcb_lookup(task_t task)
{
	list_t head, n;
	tcb_t tcb;

	if (task == NULL)
		return NULL;

	TCB_LOCK();
	head = &tcb_table[TASKHASH(task)];
	for (n = list_first(head); n != head; n = list_next(n)) {
		tcb = list_entry(n, struct tcb, link);
		ASSERT(tcb->task);
		if (tcb->task == task) {
			TCB_UNLOCK();
			mutex_lock(&tcb->lock);
			return tcb;
		}
	}
	TCB_UNLOCK();
	return NULL;
}

int tcb_alloc(task_t task, tcb_t *ptcb)
{
	tcb_t tcb;

	/* Check if specified task already exists. */
	if (tcb_lookup(task) != NULL)
		return EINVAL;

	if (!(tcb = malloc(sizeof(struct tcb))))
		return ENOMEM;
	memset(tcb, 0, sizeof(struct tcb));
	tcb->task = task;
	strcpy(tcb->cwd, "/");
	mutex_init(&tcb->lock);

	TCB_LOCK();
	list_insert(&tcb_table[TASKHASH(task)], &tcb->link);
	TCB_UNLOCK();
	*ptcb = tcb;
	return 0;
}

void tcb_free(tcb_t tcb)
{
	TCB_LOCK();
	list_remove(&tcb->link);
	TCB_UNLOCK();
	mutex_destroy(&tcb->lock);
	free(tcb);
}

/*
 * Update task id of specified tcb.
 */
void tcb_update(tcb_t tcb, task_t task)
{
	TCB_LOCK();
	list_remove(&tcb->link);
	tcb->task = task;
	list_insert(&tcb_table[TASKHASH(task)], &tcb->link);
	TCB_UNLOCK();
}

void tcb_unlock(tcb_t tcb)
{
	mutex_unlock(&tcb->lock);
}

/*
 * Get file pointer from tcb/fd pair.
 */
file_t tcb_getfp(tcb_t tcb, int fd)
{
	if (fd >= OPEN_MAX)
		return NULL;
	return tcb->file[fd];
}

/*
 * Convert to full path from specified cwd and path.
 *
 * @tcb:  task control block
 * @path: target path
 * @full: full path to be returned
 */
int tcb_conv(tcb_t tcb, char *path, char *full)
{
	char *src, *tgt, *p, *end, *cwd;
	size_t len = 0;

	cwd = tcb->cwd;
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
		strcpy(full, cwd);
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

#ifdef DEBUG
void tcb_dump(void)
{
	list_t head, n;
	tcb_t tcb;
	int i;

	TCB_LOCK();
	printf("Dump file data\n");
	printf(" task     nr_open cwd\n");
	printf(" -------- ------- ------------------------------\n");
	for (i = 0; i < TASK_MAXBUCKETS; i++) {
		head = &tcb_table[i];
		for (n = list_first(head); n != head; n = list_next(n)) {
			tcb = list_entry(n, struct tcb, link);
			printf(" %08x %7x %s\n",
			       tcb->task, tcb->nr_open, tcb->cwd);
		}
	}
	printf("\n");
	TCB_UNLOCK();
}
#endif

void tcb_init(void)
{
	int i;

	for (i = 0; i < TASK_MAXBUCKETS; i++)
		list_init(&tcb_table[i]);
}

void tcb_debug(void)
{
	int i;
	list_t head;

	for (i = 0; i < TASK_MAXBUCKETS; i++) {
		head = &tcb_table[i];
		syslog(LOG_DEBUG, "head=%x head->next=%x head->prev=%x\n",
		       head, head->next, head->prev);
		ASSERT(head->next);
		ASSERT(head->prev);
	}
}
