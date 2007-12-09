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

/*
 * cond.c - condition variable object
 */

#include <kernel.h>
#include <queue.h>
#include <sched.h>
#include <kmem.h>
#include <thread.h>
#include <sync.h>

/*
 * Create and initialize a condition variable.
 *
 * If an initialized condition variable is reinitialized,
 * undefined behavior results.
 */
__syscall int cond_init(cond_t *cond)
{
	cond_t c;

	if ((c = kmem_alloc(sizeof(struct cond))) == NULL)
		return ENOMEM;

	event_init(&c->event, "condition");
	c->task = cur_task();
	c->magic = COND_MAGIC;

	if (umem_copyout(&c, cond, sizeof(cond_t)) != 0) {
		kmem_free(c);
		return EFAULT;
	}
	return 0;
}

/*
 * Destroy a condition variable.
 * If there are any blocked thread waiting for the specified CV, it
 * returns EBUSY.
 */
__syscall int cond_destroy(cond_t *cond)
{
	cond_t c;

	sched_lock();
	if (umem_copyin(cond, &c, sizeof(cond_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	if (!cond_valid(c)) {
		sched_unlock();
		return EINVAL;
	}
	if (event_waiting(&c->event)) {
		sched_unlock();
		return EBUSY;
	}
	c->magic = 0;
	kmem_free(c);

	sched_unlock();
	return 0;
}

/*
 * Wait on a condition.
 *
 * If waiting thread receives any exception, this routine returns
 * with EINTR in order to invoke exception handler. But, an application
 * assumes this call does NOT return with error. So, the stub routine
 * in system call library must call cond_wait() again if it gets EINTR.
 */
__syscall int cond_wait(cond_t *cond, mutex_t *mu)
{
	cond_t c;
	int err;

	sched_lock();
	if (umem_copyin(cond, &c, sizeof(cond_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	if (c == COND_INITIALIZER) {
		if ((err = cond_init(cond)) != 0) {
			sched_unlock();
			return err;
		}
		umem_copyin(cond, &c, sizeof(cond_t));
	} else {
		if (!cond_valid(c)) {
			sched_unlock();
			return EINVAL;
		}
	}
	if ((err = mutex_unlock(mu)) != 0) {
		sched_unlock();
		return err;
	}
	err = sched_sleep(&c->event);
	if (err == SLP_INTR) {
		sched_unlock();
		mutex_lock(mu);
		return EINTR;
	}
	sched_unlock();
	mutex_lock(mu);
	return 0;
}

/*
 * Unblock one thread that is blocked on the specified CV.
 * The thread which has highest priority will be unblocked.
 */
__syscall int cond_signal(cond_t *cond)
{
	cond_t c;

	sched_lock();
	if (umem_copyin(cond, &c, sizeof(cond_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	if (!cond_valid(c)) {
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
__syscall int cond_broadcast(cond_t *cond)
{
	cond_t c;

	sched_lock();
	if (umem_copyin(cond, &c, sizeof(cond_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	if (!cond_valid(c)) {
		sched_unlock();
		return EINVAL;
	}
	sched_wakeup(&c->event);
	sched_unlock();
	return 0;
}
