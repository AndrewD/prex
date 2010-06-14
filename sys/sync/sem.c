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

/*
 * sem.c - semaphore support
 */

#include <kernel.h>
#include <event.h>
#include <sched.h>
#include <kmem.h>
#include <task.h>
#include <sync.h>

/* forward declarations */
static int	sem_valid(sem_t);
static void	sem_release(sem_t);
static void	sem_reference(sem_t);
static int	sem_copyin(sem_t *, sem_t *);

static struct sem *sem_list = NULL;	/* list head of semaphore list */

/*
 * sem_init - initialize a semaphore; required before use.
 *
 * sem_init() creates a new semaphore if the specified
 * semaphore does not exist yet. If the semaphore already
 * exists, it is re-initialized only if nobody is waiting for
 * it. The initial semaphore value is set to the requested
 * value. A semaphore can be shared among different tasks.
 */
int
sem_init(sem_t *sp, u_int value)
{
	task_t self = curtask;
	sem_t s;

	/*
	 * A couple of quick sanity checks.
	 */
	if (self->nsyncs >= MAXSYNCS)
		return EAGAIN;
	if (value > MAXSEMVAL)
		return EINVAL;
	if (copyin(sp, &s, sizeof(sp)))
		return EFAULT;

	/*
	 * An application can call sem_init() to reset the
	 * value of existing semaphore. So, we have to check
	 * whether the semaphore is already allocated.
	 */
	sched_lock();
	if (sem_valid(s)) {
		/*
		 * Semaphore already exists.
		 */
		if (s->owner != self) {
			sched_unlock();
			return EINVAL;
		}
		if (event_waiting(&s->event)) {
			sched_unlock();
			return EBUSY;
		}
		s->value = value;

	} else {
		/*
		 * Create new semaphore.
		 */
		if ((s = kmem_alloc(sizeof(struct sem))) == NULL) {
			sched_unlock();
			return ENOSPC;
		}
		if (copyout(&s, sp, sizeof(s))) {
			kmem_free(s);
			sched_unlock();
			return EFAULT;
		}
		event_init(&s->event, "semaphore");
		s->owner = self;
		s->refcnt = 1;
		s->value = value;

		list_insert(&self->sems, &s->task_link);
		self->nsyncs++;
		s->next = sem_list;
		sem_list = s;
	}
	sched_unlock();
	return 0;
}

/*
 * Destroy a semaphore.
 * If some thread is waiting for the specified semaphore,
 * this routine fails with EBUSY.
 */
int
sem_destroy(sem_t *sp)
{
	sem_t s;

	sched_lock();
	if (sem_copyin(sp, &s) || s->owner != curtask) {
		sched_unlock();
		return EINVAL;
	}
	if (event_waiting(&s->event) || s->value == 0) {
		sched_unlock();
		return EBUSY;
	}
	sem_release(s);
	sched_unlock();
	return 0;
}

/*
 * sem_wait - lock a semaphore.
 *
 * The value of timeout is msec unit. 0 for no timeout.
 *
 * sem_wait() locks the semaphore referred by sem only if the
 * semaphore value is currently positive. The thread will
 * sleep while the semaphore value is zero. It decrements the
 * semaphore value in return.
 *
 * If waiting thread receives any exception, this routine
 * returns with EINTR in order to invoke exception
 * handler. But, an application assumes this call does NOT
 * return with an error. So, the system call stub routine will
 * automatically call sem_wait again if it gets EINTR.
 */
int
sem_wait(sem_t *sp, u_long timeout)
{
	sem_t s;
	int rc, error = 0;

	sched_lock();
	if (sem_copyin(sp, &s)) {
		sched_unlock();
		return EINVAL;
	}
	sem_reference(s);

	while (s->value == 0) {
		rc = sched_tsleep(&s->event, timeout);
		if (rc == SLP_TIMEOUT) {
			error = ETIMEDOUT;
			break;
		}
		if (rc == SLP_INTR) {
			error = EINTR;
			break;
		}
		/*
		 * We have to check the semaphore value again
		 * because another thread may run and acquire
		 * the semaphore before us.
		 */
	}
	if (!error)
		s->value--;

	sem_release(s);
	sched_unlock();
	return error;
}

/*
 * Try to lock a semaphore.
 * If the semaphore is already locked, it just returns EAGAIN.
 */
int
sem_trywait(sem_t *sp)
{
	sem_t s;

	sched_lock();
	if (sem_copyin(sp, &s)) {
		sched_unlock();
		return EINVAL;
	}
	if (s->value == 0) {
		sched_unlock();
		return EAGAIN;
	}
	s->value--;
	sched_unlock();
	return 0;
}

/*
 * Unlock a semaphore.
 *
 * If the semaphore value becomes non zero, then one of
 * the threads blocked waiting for the semaphore will be
 * unblocked.  This is non-blocking operation.
 */
int
sem_post(sem_t *sp)
{
	sem_t s;

	sched_lock();
	if (sem_copyin(sp, &s)) {
		sched_unlock();
		return EINVAL;
	}
	if (s->value >= MAXSEMVAL) {
		sched_unlock();
		return ERANGE;
	}
	s->value++;
	if (s->value > 0)
		sched_wakeone(&s->event);

	sched_unlock();
	return 0;
}

/*
 * Get the semaphore value.
 */
int
sem_getvalue(sem_t *sp, u_int *value)
{
	sem_t s;

	sched_lock();
	if (sem_copyin(sp, &s)) {
		sched_unlock();
		return EINVAL;
	}
	if (copyout(&s->value, value, sizeof(s->value))) {
		sched_unlock();
		return EFAULT;
	}
	sched_unlock();
	return 0;
}

/*
 * Take out a reference on a semaphore.
 */
static void
sem_reference(sem_t s)
{

	s->refcnt++;
}

/*
 * Release a reference on a semaphore.  If this is the last
 * reference, the semaphore data structure is deallocated.
 */
static void
sem_release(sem_t s)
{
	sem_t *sp;

	if (--s->refcnt > 0)
		return;

	list_remove(&s->task_link);
	s->owner->nsyncs--;

	for (sp = &sem_list; *sp; sp = &(*sp)->next) {
		if (*sp == s) {
			*sp = s->next;
			break;
		}
	}
	kmem_free(s);
}

void
sem_cleanup(task_t task)
{
	list_t head, n;
	sem_t s;

	head = &task->sems;
	for (n = list_first(head); n != head; n = list_next(n)) {
		s = list_entry(n, struct sem, task_link);
		sem_release(s);
	}
}

static int
sem_valid(sem_t s)
{
	sem_t tmp;

	for (tmp = sem_list; tmp; tmp = tmp->next) {
		if (tmp == s)
			return 1;
	}
	return 0;
}

/*
 * sem_copyin - copy a semaphore from user space.
 *
 * It also checks whether the passed semaphore is valid.
 */
static int
sem_copyin(sem_t *usp, sem_t *ksp)
{
	sem_t s;

	if (copyin(usp, &s, sizeof(usp)) || !sem_valid(s))
		return EINVAL;

	*ksp = s;
	return 0;
}
