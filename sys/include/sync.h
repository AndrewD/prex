/*-
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

#ifndef _SYNC_H
#define _SYNC_H

#include <event.h>
#include <task.h>

struct sem {
	int		magic;		/* magic number */
	task_t		task;		/* owner task */
	struct event	event;		/* event */
	u_int		value;		/* current value */
};

struct mutex {
	int		magic;		/* magic number */
	task_t		task;		/* owner task */
	struct event	event;		/* event */
	struct list	link;		/* linkage on locked mutex list */
	thread_t	owner;		/* owner thread locking this mutex */
	int		prio;		/* highest prio in waiting threads */
	int		lock_count;	/* counter for recursive lock */
};

struct cond {
	int		magic;		/* magic number */
	task_t		task;		/* owner task */
	struct event	event;		/* event */
};

#define sem_valid(s)	(kern_area(s) && ((s)->magic == SEM_MAGIC))

#define mutex_valid(m)	(kern_area(m) && \
			 ((m)->magic == MUTEX_MAGIC) && \
			 ((m)->task == cur_task()))

#define cond_valid(c)	(kern_area(c) && \
			 ((c)->magic == COND_MAGIC) && \
			 ((c)->task == cur_task()))

/* maximum value for semaphore. */
#define MAXSEMVAL		((u_int)((~0u) >> 1))

#define MUTEX_INITIALIZER	(mutex_t)0x4d496e69	/* 'MIni' */
#define COND_INITIALIZER	(cond_t)0x43496e69	/* 'CIni' */

extern int	 sem_init(sem_t *, u_int);
extern int	 sem_destroy(sem_t *);
extern int	 sem_wait(sem_t *, u_long);
extern int	 sem_trywait(sem_t *);
extern int	 sem_post(sem_t *);
extern int	 sem_getvalue(sem_t *, u_int *);

extern int	 mutex_init(mutex_t *);
extern int	 mutex_destroy(mutex_t *);
extern int	 mutex_lock(mutex_t *);
extern int	 mutex_trylock(mutex_t *);
extern int	 mutex_unlock(mutex_t *);
extern int	 mutex_unlock_count(mutex_t *);
extern void	 mutex_cleanup(thread_t);
extern void	 mutex_setprio(thread_t, int);

extern int	 cond_init(cond_t *);
extern int	 cond_destroy(cond_t *);
extern int	 cond_wait(cond_t *, mutex_t *, u_long timeout);
extern int	 cond_signal(cond_t *);
extern int	 cond_broadcast(cond_t *);

#endif /* !_SYNC_H */
