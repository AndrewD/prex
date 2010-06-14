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
 * cond.c - condition variable object
 */

#include <kernel.h>
#include <sched.h>
#include <event.h>
#include <kmem.h>
#include <task.h>
#include <sync.h>

/* forward declarations */
static int	cond_valid(cond_t);
static int	cond_copyin(cond_t *, cond_t *);

/*
 * Create and initialize a condition variable (CV).
 *
 * If an initialized condition variable is reinitialized,
 * undefined behavior results.
 */
int
cond_init(cond_t *cp)
{
	task_t self = curtask;
	cond_t c;

	if (self->nsyncs >= MAXSYNCS)
		return EAGAIN;

	if ((c = kmem_alloc(sizeof(struct cond))) == NULL)
		return ENOMEM;

	event_init(&c->event, "condvar");
	c->owner = self;

	if (copyout(&c, cp, sizeof(c))) {
		kmem_free(c);
		return EFAULT;
	}
	sched_lock();
	list_insert(&self->conds, &c->task_link);
	self->nsyncs++;
	sched_unlock();
	return 0;
}

static void
cond_deallocate(cond_t c)
{

	c->owner->nsyncs--;
	list_remove(&c->task_link);
	kmem_free(c);
}

/*
 * Tear down a condition variable.
 *
 * If there are any blocked thread waiting for the specified
 * CV, it returns EBUSY.
 */
int
cond_destroy(cond_t *cp)
{
	cond_t c;

	sched_lock();
	if (cond_copyin(cp, &c)) {
		sched_unlock();
		return EINVAL;
	}
	if (event_waiting(&c->event)) {
		sched_unlock();
		return EBUSY;
	}
	cond_deallocate(c);
	sched_unlock();
	return 0;
}

/*
 * Clean up for task termination.
 */
void
cond_cleanup(task_t task)
{
	cond_t c;

	while (!list_empty(&task->conds)) {
		c = list_entry(list_first(&task->conds),
			       struct cond, task_link);
		cond_deallocate(c);
	}
}

/*
 * Wait on a condition.
 *
 * If the thread receives any exception while waiting CV, this
 * routine returns immediately with EINTR in order to invoke
 * exception handler. However, an application assumes this call
 * does NOT return with an error. So, the stub routine in a
 * system call library must call cond_wait() again if it gets
 * EINTR as error.
 */
int
cond_wait(cond_t *cp, mutex_t *mp)
{
	cond_t c;
	int error, rc;

	if (copyin(cp, &c, sizeof(cp)))
		return EINVAL;

	sched_lock();
	if (c == COND_INITIALIZER) {
		if ((error = cond_init(cp)) != 0) {
			sched_unlock();
			return error;
		}
		copyin(cp, &c, sizeof(cp));
	} else {
		if (!cond_valid(c)) {
			sched_unlock();
			return EINVAL;
		}
	}
        /* unlock mutex */
	if ((error = mutex_unlock(mp)) != 0) {
		sched_unlock();
		return error;
	}

	/* and block */
	rc = sched_sleep(&c->event);
	if (rc == SLP_INTR)
		error = EINTR;
	sched_unlock();

        /* grab mutex before returning */
	if (error == 0)
		error = mutex_lock(mp);

	return error;
}

/*
 * Unblock one thread that is blocked on the specified CV.
 * The thread which has highest priority will be unblocked.
 */
int
cond_signal(cond_t *cp)
{
	cond_t c;

	sched_lock();
	if (cond_copyin(cp, &c)) {
		sched_unlock();
		return EINVAL;
	}
	sched_wakeone(&c->event);
	sched_unlock();
	return 0;
}

/*
 * Unblock all threads that are blocked on the specified CV.
 */
int
cond_broadcast(cond_t *cp)
{
	cond_t c;

	sched_lock();
	if (cond_copyin(cp, &c)) {
		sched_unlock();
		return EINVAL;
	}
	sched_wakeup(&c->event);
	sched_unlock();
	return 0;
}

/*
 * Check if the specified cv is valid.
 */
static int
cond_valid(cond_t c)
{
	cond_t tmp;
	list_t head, n;

	head = &curtask->conds;
	for (n = list_first(head); n != head; n = list_next(n)) {
		tmp = list_entry(n, struct cond, task_link);
		if (tmp == c)
			return 1;
	}
	return 0;
}

/*
 * cond_copyin - copy a condition variable from user space.
 * It also checks if the passed CV is valid.
 */
static int
cond_copyin(cond_t *ucp, cond_t *kcp)
{
	cond_t c;

	if (copyin(ucp, &c, sizeof(ucp)) || !cond_valid(c))
		return EINVAL;
	*kcp = c;
	return 0;
}
