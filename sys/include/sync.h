/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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

/*
 * Semaphore
 */
struct semaphore {
	int		magic;	/* Magic number */
	task_t		task;	/* Owner task */
	struct event	event;	/* Event */
	u_int		value;	/* Current value */
};
typedef struct semaphore *sem_t;

#define SEM_MAX		((u_int)((~0u) >> 1))
#define SEM_MAGIC	0x53656d3f	/* 'Sem?' */

#define sem_valid(s) (kern_area(s) && (s)->magic == SEM_MAGIC)

extern int sem_init(sem_t *sem, u_int value);
extern int sem_destroy(sem_t *sem);
extern int sem_wait(sem_t *sem, u_long timeout);
extern int sem_trywait(sem_t *sem);
extern int sem_post(sem_t *sem);
extern int sem_getvalue(sem_t *sem, u_int *value);


/*
 * Mutex
 */
struct mutex {
	int		magic;	/* Magic number */
	task_t		task;	/* Owner task */
	struct event	event;	/* Event */
	struct list	link;	/* Link to locked mutex list */
	thread_t	owner;	/* Owner thread locking this mutex */
	int		prio;	/* Highest prio in waiting threads */
	int		lock_count; /* Counter for recursive lock */
};
typedef struct mutex *mutex_t;

#define MUTEX_MAGIC		0x4d75783f		/* 'Mux?' */
#define MUTEX_INITIALIZER	(mutex_t)0x4d496e69	/* 'MIni' */

#define mutex_valid(m)  (kern_area(m) && \
			 (m)->magic == MUTEX_MAGIC && \
			 (m)->task == cur_task())

extern int mutex_init(mutex_t *mu);
extern int mutex_destroy(mutex_t *mu);
extern int mutex_lock(mutex_t *mu);
extern int mutex_trylock(mutex_t *mu);
extern int mutex_unlock(mutex_t *mu);
extern void mutex_cleanup(thread_t th);
extern void mutex_setprio(thread_t th, int prio);


/*
 * Condition Variable
 */
struct cond {
	int		magic;	/* Magic number */
	task_t		task;	/* Owner task */
	struct event	event;	/* Event */
};
typedef struct cond *cond_t;

#define COND_MAGIC		0x436f6e3f	/* 'Con?' */
#define COND_INITIALIZER	(cond_t)0x43496e69	/* 'CIni' */

#define cond_valid(c) (kern_area(c) && \
		       (c)->magic == COND_MAGIC && \
		       (c)->task == cur_task())

extern int cond_init(cond_t *cond);
extern int cond_destroy(cond_t *cond);
extern int cond_wait(cond_t *cond, mutex_t *mu);
extern int cond_signal(cond_t *cond);
extern int cond_broadcast(cond_t *cond);

#endif /* !_SYNC_H */
