/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
 * Copyright (c) 2007-2009, Andrew Dennison
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
#include <kmem.h>
#include <thread.h>
#include <sync.h>
#include <verbose.h>

/*
 * Create and initialize a condition variable (CV).
 *
 * If an initialized condition variable is reinitialized,
 * undefined behavior results.
 */
int
cond_init(cond_t *cond)
{
	cond_t c;

	if ((c = kmem_alloc(sizeof(struct cond))) == NULL)
		return DERR(ENOMEM);

	event_init(&c->event, "condition");
	c->task = cur_task();
	c->magic = COND_MAGIC;
	c->wait = c->signal = 0;

	if (umem_copyout(&c, cond, sizeof(c))) {
		kmem_free(c);
		return DERR(EFAULT);
	}
	return 0;
}

/*
 * cond_copyin - copy a condition variable from user space.
 *
 * It also checks if the passed CV is valid.
 */
static int
cond_copyin(cond_t *ucond, cond_t *kcond)
{
	cond_t c;

	if (umem_copyin(ucond, &c, sizeof(ucond)))
		return DERR(EFAULT);
	if (!cond_valid(c))
		return DERR(EINVAL);
	*kcond = c;
	return 0;
}

/*
 * Destroy a condition variable.
 *
 * If there are any blocked thread waiting for the specified
 * CV, it returns EBUSY.
 */
int
cond_destroy(cond_t *cond)
{
	cond_t c;
	int err;

	sched_lock();
	if ((err = cond_copyin(cond, &c))) {
		sched_unlock();
		return err;
	}
	if (event_waiting(&c->event)) {
		sched_unlock();
		return DERR(EBUSY);
	}
	c->magic = 0;
	kmem_free(c);
	sched_unlock();
	return 0;
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
cond_wait(cond_t *cond, mutex_t *mtx, u_long timeout)
{
	cond_t c;
	int err, rc;

	if (umem_copyin(cond, &c, sizeof(cond)))
		return DERR(EFAULT);

	sched_lock();
	if (c == COND_INITIALIZER) {
		if ((err = cond_init(cond))) {
			sched_unlock();
			return err;
		}
		umem_copyin(cond, &c, sizeof(cond));
	} else {
		if (!cond_valid(c)) {
			sched_unlock();
			return DERR(EINVAL);
		}
	}
	ASSERT(c->signal >= 0 && c->signal <= c->wait);
	c->wait++;
	if ((err = mutex_unlock_count(mtx))) {
		if (err < 0) {
			/* mutex was recursively locked - would deadlock */
			mutex_lock(mtx);
			err = DERR(EDEADLK);
		}
		sched_unlock();
		return err;
	}

	rc = sched_tsleep(&c->event, timeout);
	err = mutex_lock(mtx);
	c->wait--;
	if (!err) {
		if (c->signal)
			c->signal--; /* > 1 thread may be waiting */
		else {
			switch (rc) {
			case SLP_TIMEOUT:
				err = ETIMEDOUT;
				break;

			case SLP_INTR:
				err = EINTR;
				break;

			default:		/* unexpected */
				err = DERR(EINVAL);
			}
		}
	}
	sched_unlock();
	return err;
}

/*
 * Unblock one thread that is blocked on the specified CV.
 * The thread which has highest priority will be unblocked.
 */
int
cond_signal(cond_t *cond)
{
	cond_t c;
	int err;

	sched_lock();
	if ((err = cond_copyin(cond, &c)) == 0 && c->signal < c->wait) {
		c->signal++;
		sched_wakeone(&c->event);
	}
	sched_unlock();
	return err;
}

/*
 * Unblock all threads that are blocked on the specified CV.
 */
int
cond_broadcast(cond_t *cond)
{
	cond_t c;
	int err;

	sched_lock();
	if ((err = cond_copyin(cond, &c)) == 0 && c->signal < c->wait) {
		c->signal = c->wait;
		sched_wakeup(&c->event);
	}
	sched_unlock();
	return err;
}
