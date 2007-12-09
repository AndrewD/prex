/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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
 * mutex.c - mutual exclusion service.
 */

/*
 * A mutex is used to protect un-sharable resources.
 * A thread can use mutex_lock() to ensure that global resource is not
 * accessed by other thread. The mutex is effective only the threads
 * belonging to the same task.
 *
 * Prex will change the thread priority to prevent priority inversion.
 *
 * <Priority inheritance>
 *   The priority is changed at the following conditions.
 *
 *   1. When the current thread can not lock the mutex and its mutex
 *      owner has lower priority than current thread, the priority
 *      of mutex owner is boosted to same priority with current thread.
 *      If this mutex owner is waiting for another mutex, such related
 *      mutexes are also processed.
 *
 *   2. When the current thread unlocks the mutex and its priority
 *      has already been inherited, the current priority is reset.
 *      In this time, the current priority is changed to the highest
 *      priority among the threads waiting for the mutexes locked by
 *      current thread.
 *
 *   3. When the thread priority is changed by user request, the inherited
 *      thread's priority is changed.
 *
 * <Limitation>
 *
 *   1. If the priority is changed by user request, the priority
 *      recomputation is done only when the new priority is higher
 *      than old priority. The inherited priority is reset to base
 *      priority when the mutex is unlocked.
 *
 *   2. Even if thread is killed with mutex waiting, the related
 *      priority is not adjusted.
 *
 * <Important>
 *  Since this implementation does not support recursive lock, a thread
 *  can not lock the same mutex twice.
 */

#include <kernel.h>
#include <queue.h>
#include <list.h>
#include <event.h>
#include <sched.h>
#include <kmem.h>
#include <thread.h>
#include <task.h>
#include <sync.h>

/* Forward functions */
static int prio_inherit(thread_t th);
static void prio_uninherit(thread_t th);

/*
 * Initialize a mutex.
 *
 * If an initialized mutex is reinitialized, undefined 
 * behavior results.
 */
__syscall int mutex_init(mutex_t *mu)
{
	mutex_t m;

	if ((m = kmem_alloc(sizeof(struct mutex))) == NULL)
		return ENOMEM;

	event_init(&m->event, "mutex");
	m->task = cur_task();
	m->owner = NULL;
	m->prio = MIN_PRIO;
	m->magic = MUTEX_MAGIC;

	if (umem_copyout(&m, mu, sizeof(mutex_t)) != 0) {
		kmem_free(m);
		return EFAULT;
	}
	return 0;
}

/*
 * Destroy the specified mutex.
 * The mutex must be unlock state, otherwise it fails with EBUSY.
 */
__syscall int mutex_destroy(mutex_t *mu)
{
	mutex_t m;

	sched_lock();
	if (umem_copyin(mu, &m, sizeof(mutex_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	if (!mutex_valid(m)) {
		sched_unlock();
		return EINVAL;
	}
	if (m->owner || event_waiting(&m->event)) {
		sched_unlock();
		return EBUSY;
	}
	m->magic = 0;
	kmem_free(m);

	sched_unlock();
	return 0;
}

/*
 * Get mutex from user space.
 * If it is not initialized, create new mutex.
 *
 * @um: Pointer to mutex in user space.
 * @km: Pointer to mutex in kernel space.
 */
static int mutex_get(mutex_t *um, mutex_t *km)
{
	mutex_t m;
	int err;

	if (umem_copyin(um, &m, sizeof(mutex_t)) != 0)
		return EFAULT;

	if (m == MUTEX_INITIALIZER) {
		if ((err = mutex_init(um)) != 0)
			return err;
		umem_copyin(um, &m, sizeof(mutex_t));
	} else {
		if (!mutex_valid(m))
			return EINVAL;
	}
	*km = m;
	return 0;
}

/*
 * Lock a mutex.
 *
 * A current thread is blocked if the mutex has already been locked.
 * If current thread receives any exception while waiting mutex, this
 * routine returns with EINTR in order to invoke exception handler.
 * But, POSIX thread assumes this function does NOT return with EINTR.
 * So, system call stub routine in library must call this again
 * if it gets EINTR.
 */
__syscall int mutex_lock(mutex_t *mu)
{
	mutex_t m;
	int err;

	sched_lock();
	if ((err = mutex_get(mu, &m)) != 0) {
		sched_unlock();
		return err;
	}
	if (m->owner == cur_thread) {
		m->lock_count++;
		sched_unlock();
		return 0;
	}
	/*
	 * Check whether a target mutex is locked. If the mutex
	 * is not locked, this routine returns immediatedly.
	 */
	if (m->owner == NULL)
		m->prio = cur_thread->prio;
	else {
		/*
		 * Wait for a mutex.
		 */
		cur_thread->wait_mutex = m;
		if (prio_inherit(cur_thread)) {
			sched_unlock();
			return EDEADLK;
		}
		err = sched_sleep(&m->event);
		cur_thread->wait_mutex = NULL;
		if (err == SLP_INTR) {
			sched_unlock();
			return EINTR;
		}
	}
	m->owner = cur_thread;
	m->lock_count = 1;
	list_insert(&cur_thread->mutexes, &m->link);

	sched_unlock();
	return 0;
}

/*
 * Try to lock a mutex without blocking.
 */
__syscall int mutex_trylock(mutex_t *mu)
{
	mutex_t m;
	int err;

	sched_lock();
	if ((err = mutex_get(mu, &m)) != 0) {
		sched_unlock();
		return err;
	}
	if (m->owner == cur_thread) {
		m->lock_count++;
		sched_unlock();
		return 0;
	}
	if (m->owner != NULL) {
		sched_unlock();
		return EBUSY;
	}
	m->owner = cur_thread;
	m->lock_count = 1;
	list_insert(&cur_thread->mutexes, &m->link);

	sched_unlock();
	return 0;
}

/*
 * Unlock a mutex.
 * Caller must be a current mutex owner.
 */
__syscall int mutex_unlock(mutex_t *mu)
{
	mutex_t m;

	sched_lock();
	if (umem_copyin(mu, &m, sizeof(mutex_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	if (!mutex_valid(m)) {
		sched_unlock();
		return EINVAL;
	}
	if (m->owner != cur_thread) {
		sched_unlock();
		return EPERM;
	}
	if (--m->lock_count > 0) {
		sched_unlock();
		return 0;
	}
	list_remove(&m->link);
	prio_uninherit(cur_thread);

	/*
	 * Change the mutex owner, and make the thread
	 * runnable if it exists.
	 */
	m->owner = sched_wakeone(&m->event);
	if (m->owner)
		m->owner->wait_mutex = NULL;

	m->prio = m->owner ? m->owner->prio : MIN_PRIO;

	sched_unlock();
	return 0;
}


/*
 * Clean up mutex.
 * This is called with scheduling locked when thread is terminated.
 *
 * If a thread is terminated with mutex hold, all waiting threads
 * keeps waiting forever. So, all mutex locked by terminated thread
 * must be unlocked.
 *
 * Even if the terminated thread is waiting some mutex, the inherited
 * priority of other mutex owner is not adjusted.
 */
void mutex_cleanup(thread_t th)
{
	list_t head, n;
	mutex_t mu;
	thread_t owner;

	/* Process all mutexes locked by the thread. */
	head = &th->mutexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		/*
		 * Release locked mutex.
		 */
		mu = list_entry(n, struct mutex, link);
		mu->lock_count = 0;
		list_remove(&mu->link);
		/* 
		 * Change the mutex owner if other thread is
		 * waiting for it.
		 */
		owner = sched_wakeone(&mu->event);
		if (owner) {
			owner->wait_mutex = NULL;
			mu->lock_count = 1;
			list_insert(&owner->mutexes, &mu->link);

		}
		mu->owner = owner;
	}
}

/*
 * This is called with scheduling locked before thread priority is changed.
 */
void mutex_setprio(thread_t th, int prio)
{
	if (th->wait_mutex && prio < th->prio)
		prio_inherit(th);
}

/*
 * Inherit priority.
 *
 * @waiter: Thread that is about to wait a mutex.
 *
 * The higher priority thread should not wait lower priority thread.
 * So, raise the priority of mutex owner which blocks the specified thread.
 * If mutex owner is also waiting for other mutex, that mutex is also
 * processed.
 */
static int prio_inherit(thread_t waiter)
{
	mutex_t mu = waiter->wait_mutex;
	thread_t owner;

	do {
		owner = mu->owner;

		/*
		 * If the owner of relative mutex has already been waiting
		 * for the "waiter" thread, it causes a deadlock.
		 */
		if (owner == waiter) {
			printk("Detect deadlock! mutex=%x owner=%x waiter=%x\n",
			       mu, owner, waiter);
			return EDEADLK;
		}
		/*
		 * If the priority of the mutex owner is lower than "waiter"
		 * thread's, it is automatically adjusted.
		 */
		if (owner->prio > waiter->prio) {
			sched_setprio(owner, owner->base_prio, waiter->prio);
			mu->prio = waiter->prio;
		}
		/*
		 * If the mutex owner is waiting for another mutex, that
		 * mutex is also processed.
		 */
		mu = (mutex_t)owner->wait_mutex;

	} while (mu != NULL);
	return 0;
}

/*
 * Un-inherit priority
 *
 * The priority of specified thread is reset to the base priority.
 * If specified thread locks other mutex and higher priority thread
 * is waiting for it, the priority is kept to that level.
 */
static void prio_uninherit(thread_t th)
{
	int top_prio;
	list_t head, n;
	mutex_t mu;

	/* Check if the priority is inherited. */
	if (th->prio == th->base_prio)
		return;

	top_prio = th->base_prio;
	/*
	 * Find the highest priority thread that is waiting for the thread.
	 * This is done by checking all mutexes that the thread locks.
	 */
	head = &th->mutexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		mu = list_entry(n, struct mutex, link);
		if (mu->prio < top_prio)
			top_prio = mu->prio;
	}
	sched_setprio(th, th->base_prio, top_prio);
}
