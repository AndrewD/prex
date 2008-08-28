/*-
 * Copyright (c) 2008-2009, Andrew Dennison
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

#include <sys/types.h>

#include <sys/list.h>
#include <prex/prex.h>
#include <errno.h>

#include <pthread.h>
#include <verbose.h>

struct pthread_info {
	u_long stack[1];	/* thread arg passed on stack */
	int detached;
	int cancel;
	int zombie;
	thread_t th;
	void *vm_addr;
	u_long vm_size;
	void * (*start_routine)(void *);
	void *arg;
	const void *key;	/* REVISIT: do keys better! */
	void *value_ptr;
	struct list link;
};

#define PTHREAD_ATTR_MAGIC 0xCAFEBEEF

const pthread_attr_t pthread_attr_default = {
	.sched_priority = 0,
	.sched_policy = SCHED_RR,
	.stacksize = CONFIG_USTACK_SIZE,
	.detached = PTHREAD_CREATE_JOINABLE,
	.magic = PTHREAD_ATTR_MAGIC,
};

static struct list head = LIST_INIT(head);

/* use to lock head and signal thread termination */
static mutex_t mutex = MUTEX_INITIALIZER;
static cond_t cond = COND_INITIALIZER;

static int pthread_wrapper(void *arg)
{
	pthread_t thread = arg;

	pthread_exit(thread->start_routine(thread->arg));
}

/******************************************************************************
 * pthread_create()
 */
int pthread_create(pthread_t *thread, const pthread_attr_t *attr,
			 void * (*start_routine)(void *), void *arg)
{
	void *vm_addr = NULL;
	u_long vm_size;
	task_t self = task_self();
	pthread_t pth, tmp;

	if (attr == NULL)
		attr = &pthread_attr_default;
	else if (attr->magic != PTHREAD_ATTR_MAGIC)
		return DERR(EINVAL);

	vm_size = PAGE_ALIGN(attr->stacksize);

	/* reap zombies and reuse vm_addr iff correct size */
	mutex_lock(&mutex);
	list_for_each_entry_safe(pth, tmp, &head, link) {
		CVERBOSE(VB_PTHREAD|VB_TRACE, pth->zombie, "zombie %p", pth);
		if (pth->zombie && pth->detached == PTHREAD_CREATE_DETACHED) {
			VERBOSE(VB_PTHREAD|VB_TRACE, "reap %p", pth);
			list_remove(&pth->link);

			if (vm_addr == NULL && vm_size == pth->vm_size)
				vm_addr = pth->vm_addr;
			else
				vm_free(self, pth->vm_addr);
		}
	}
	mutex_unlock(&mutex);

	if (vm_addr == NULL && vm_allocate(self, &vm_addr, vm_size, 1) != 0)
		return DERR(EAGAIN);	/* posix mandated */

	pth = (void *)((u_long)vm_addr + vm_size);
	pth--;			/* allocate pth at top of stack */

	pth->vm_addr = vm_addr;
	pth->vm_size = vm_size;
	pth->detached = attr->detached;
	pth->start_routine = start_routine;
	pth->arg = arg;
	pth->cancel = 0;
	pth->zombie = 0;
	pth->stack[0] = (u_long)pth; /* arg for start_routine() wrapper */

	int rc = EAGAIN;
	if (thread_create(self, &pth->th) != 0)
		goto out1;

	if (thread_load(pth->th, (void *)pthread_wrapper, pth->stack) != 0)
		goto out2;

	/* prex specific extension */
	pth->key = attr->key;
	thread_name(pth->th, attr->name);

	if (attr != &pthread_attr_default) {
		rc = EPERM;
		if (thread_setprio(pth->th, CONFIG_USER_PRIO + attr->sched_priority) != 0)
			goto out2;
		if (thread_setpolicy(pth->th, attr->sched_policy) != 0)
			goto out2;
	}

	mutex_lock(&mutex);
	list_insert(&head, &pth->link);

	if (thread_resume(pth->th) != 0)
		goto out3;

	mutex_unlock(&mutex);
	VERBOSE(VB_PTHREAD|VB_TRACE, "create %p", pth);

	*thread = pth;
	return 0;

out3:
	list_remove(&pth->link);
	mutex_unlock(&mutex);
out2:
	thread_terminate(pth->th);
out1:
	vm_free(self, vm_addr);
	VERBOSE(VB_PTHREAD|VB_WARN, "error %d", rc);
	return rc;
}

/******************************************************************************
 * pthread_valid() - internal test
 */
static inline int pthread_valid(pthread_t thread)
{
	pthread_t pth, tmp;

	list_for_each_entry_safe(pth, tmp, &head, link) {
		if (pth == thread)
			return !0;
	}

	VERBOSE(VB_PTHREAD|VB_DEBUG, "%p not a pthread", thread);
	return 0;
}

/******************************************************************************
 * pthread_self()
 */
pthread_t pthread_self(void)
{
	pthread_t thread, tmp;
	thread_t th = thread_self();

	list_for_each_entry_safe(thread, tmp, &head, link) {
		if (thread->th == th)
			return thread;
	}

	VERBOSE(VB_PTHREAD|VB_DEBUG, "%p not a pthread", th);
	return NULL;
}

/******************************************************************************
 * pthread_equal()
 */
int pthread_equal(pthread_t t1, pthread_t t2)
{
	return !(t1 - t2);	/* non-zero if equal */
}

/******************************************************************************
 * pthread_exit() - terminate current thread and reap any detached zombies
 */
void pthread_exit(void *value_ptr)
{
	pthread_t thread, tmp;
	thread_t th = thread_self();

	mutex_lock(&mutex);
	list_for_each_entry_safe(thread, tmp, &head, link) {
		CVERBOSE(VB_PTHREAD|VB_TRACE, thread->zombie, "zombie %p", thread);
		if (thread->th == th) {
			VERBOSE(VB_PTHREAD|VB_TRACE, "exit %p", thread);
			thread->value_ptr = value_ptr;
			thread->zombie = 1;
			if (cond != COND_INITIALIZER)
				cond_signal(&cond);
			/* continue looping to reap threads */
		} else if (thread->zombie &&
			   thread->detached == PTHREAD_CREATE_DETACHED)
		{
			/* can only reap other threads */
			VERBOSE(VB_PTHREAD|VB_TRACE, "reap %p", thread);
			list_remove(&thread->link);
			vm_free(task_self(), thread->vm_addr);
		}
	}

	/* mutex unlocked by thread_terminate() */
	thread_terminate(th);
	sys_panic("bad th");
}

/******************************************************************************
 * pthread_join() - wait for thread to exit
 */
int pthread_join(pthread_t thread, void **value_ptr)
{
	int rc;

	if (!pthread_valid(thread))
		return DERR(ESRCH);

	mutex_lock(&mutex);
	if (thread->detached == PTHREAD_CREATE_DETACHED)
		rc = DERR(EINVAL);
	else if (thread->th == thread_self())
		rc = DERR(EDEADLK);
	else {
		VERBOSE(VB_PTHREAD|VB_TRACE, "wait %p", thread);
		while (thread->zombie == 0)
			cond_wait(&cond, &mutex, 0);

		VERBOSE(VB_PTHREAD|VB_TRACE, "join %p", thread);
		list_remove(&thread->link);
		if (value_ptr)
			*value_ptr = thread->value_ptr;
		vm_free(task_self(), thread->vm_addr);
		rc = 0;
	}
	mutex_unlock(&mutex);
	return rc;
}

/******************************************************************************
 * pthread_cancel() - cancel a thread
 */
int pthread_cancel(pthread_t thread)
{
	if (!pthread_valid(thread))
		return DERR(ESRCH);

	mutex_lock(&mutex);
	thread->cancel = 1;
	mutex_unlock(&mutex);

	VERBOSE(VB_PTHREAD|VB_TRACE, "cancel %p", thread);
	return 0;
}

/******************************************************************************
 * pthread_testcancel() - check if thread has been canceled. For speed
 * only lock when cancelled and touching thread
 */
void pthread_testcancel(void)
{
	pthread_t thread = pthread_self();

	if (thread == NULL)
		return;

	if (thread->cancel == 0)
		return;

	VERBOSE(VB_PTHREAD|VB_TRACE, "cancelled %p", thread);

	mutex_lock(&mutex);
	thread->value_ptr = NULL;
	thread->zombie = 1;
	cond_signal(&cond);
	thread_terminate(thread->th); /* mutex unlocked by thread_terminate() */
	/* not reached */
}

/******************************************************************************
 * pthread_detach()
 */
int pthread_detach(pthread_t thread)
{
	int rc = 0;

	if (!pthread_valid(thread))
		return DERR(ESRCH);

	mutex_lock(&mutex);
	if (thread->detached == PTHREAD_CREATE_DETACHED)
		rc = DERR(EINVAL);
	else
		thread->detached = PTHREAD_CREATE_DETACHED;
	mutex_unlock(&mutex);

	CVERBOSE(VB_PTHREAD|VB_TRACE, (rc == 0), "detached %p", thread);
	return rc;
}

/******************************************************************************
 * pthread_setspecific()
 */
int pthread_setspecific(pthread_key_t key, const void *value)
{
	pthread_t thread = pthread_self();

	if (thread == NULL)
		return DERR(ESRCH);

	if (key)
		return DERR(EINVAL);

	mutex_lock(&mutex);
	thread->key = value;
	mutex_unlock(&mutex);

	VERBOSE(VB_PTHREAD|VB_TRACE, "setspecific %p", thread);
	return 0;
}

/******************************************************************************
 * pthread_setspecific() - no locking for speed
 */
void *pthread_getspecific(pthread_key_t key)
{
	pthread_t thread = pthread_self();

	if (thread != NULL)
		return (void *)thread->key;

	return NULL;
}
