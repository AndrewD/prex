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
 * mutex.c - mutual exclusion service.
 */

/*
 * A mutex is used to protect un-sharable resources. A thread can use
 * mutex_lock() to ensure that global resource is not accessed by
 * other thread. The mutex is effective only the threads belonging to
 * the same task.
 *
 * Prex will change the thread priority to prevent priority inversion.
 *
 * <Priority inheritance>
 *   The priority is changed at the following conditions.
 *
 *   1. When the current thread can not lock the mutex and its
 *      mutex holder has lower priority than current thread, the
 *      priority of mutex holder is boosted to same priority with
 *      current thread.  If this mutex holder is waiting for another
 *      mutex, such related mutexes are also processed.
 *
 *   2. When the current thread unlocks the mutex and its priority
 *      has already been inherited, the current priority is reset.
 *      In this time, the current priority is changed to the highest
 *      priority among the threads waiting for the mutexes locked by
 *      current thread.
 *
 *   3. When the thread priority is changed by user request, the
 *      inherited thread's priority is changed.
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
 */

#include <kernel.h>
#include <event.h>
#include <sched.h>
#include <kmem.h>
#include <thread.h>
#include <task.h>
#include <sync.h>

/* forward declarations */
static int	mutex_valid(mutex_t);
static int	mutex_copyin(mutex_t *, mutex_t *);
static int	prio_inherit(thread_t);
static void	prio_uninherit(thread_t);

/*
 * Initialize a mutex.
 *
 * If an initialized mutex is reinitialized, undefined
 * behavior results. Technically, we can not detect such
 * error condition here because we can not touch the passed
 * object in kernel.
 */
int
mutex_init(mutex_t *mp)
{
	task_t self = curtask;
	mutex_t m;

	if (self->nsyncs >= MAXSYNCS)
		return EAGAIN;

	if ((m = kmem_alloc(sizeof(struct mutex))) == NULL)
		return ENOMEM;

	event_init(&m->event, "mutex");
	m->owner = self;
	m->holder = NULL;
	m->priority = MINPRI;

	if (copyout(&m, mp, sizeof(m))) {
		kmem_free(m);
		return EFAULT;
	}

	sched_lock();
	list_insert(&self->mutexes, &m->task_link);
	self->nsyncs++;
	sched_unlock();
	return 0;
}

/*
 * Internal version of mutex_destroy.
 */
static void
mutex_deallocate(mutex_t m)
{

	m->owner->nsyncs--;
	list_remove(&m->task_link);
	kmem_free(m);
}

/*
 * Destroy the specified mutex.
 * The mutex must be unlock state, otherwise it fails with EBUSY.
 */
int
mutex_destroy(mutex_t *mp)
{
	mutex_t m;

	sched_lock();
	if (copyin(mp, &m, sizeof(mp))) {
		sched_unlock();
		return EFAULT;
	}
	if (!mutex_valid(m)) {
		sched_unlock();
		return EINVAL;
	}
	if (m->holder || event_waiting(&m->event)) {
		sched_unlock();
		return EBUSY;
	}
	mutex_deallocate(m);
	sched_unlock();
	return 0;
}

/*
 * Clean up for task termination.
 */
void
mutex_cleanup(task_t task)
{
	mutex_t m;

	while (!list_empty(&task->mutexes)) {
		m = list_entry(list_first(&task->mutexes),
			       struct mutex, task_link);
		mutex_deallocate(m);
	}
}

/*
 * Lock a mutex.
 *
 * A current thread is blocked if the mutex has already been
 * locked. If current thread receives any exception while
 * waiting mutex, this routine returns with EINTR in order to
 * invoke exception handler. But, POSIX thread assumes this
 * function does NOT return with EINTR.  So, system call stub
 * routine in library must call this again if it gets EINTR.
 */
int
mutex_lock(mutex_t *mp)
{
	mutex_t m;
	int error, rc;

	sched_lock();
	if ((error = mutex_copyin(mp, &m)) != 0) {
		sched_unlock();
		return error;
	}

	if (m->holder == curthread) {
		/*
		 * Recursive lock
		 */
		m->locks++;
		ASSERT(m->locks != 0);
	} else {
		/*
		 * Check whether a target mutex is locked.
		 * If the mutex is not locked, this routine
		 * returns immediately.
		 */
		if (m->holder == NULL)
			m->priority = curthread->priority;
		else {
			/*
			 * Wait for a mutex.
			 */
			curthread->mutex_waiting = m;
			if ((error = prio_inherit(curthread)) != 0) {
				curthread->mutex_waiting = NULL;
				sched_unlock();
				return error;
			}
			rc = sched_sleep(&m->event);
			curthread->mutex_waiting = NULL;
			if (rc == SLP_INTR) {
				sched_unlock();
				return EINTR;
			}
		}
		m->locks = 1;
		m->holder = curthread;
		list_insert(&curthread->mutexes, &m->link);
	}
	sched_unlock();
	return 0;
}

/*
 * Try to lock a mutex without blocking.
 */
int
mutex_trylock(mutex_t *mp)
{
	mutex_t m;
	int error;

	sched_lock();
	if ((error = mutex_copyin(mp, &m)) != 0) {
		sched_unlock();
		return error;
	}

	if (m->holder == curthread) {
		m->locks++;
		ASSERT(m->locks != 0);
	} else {
		if (m->holder != NULL)
			error = EBUSY;
		else {
			m->locks = 1;
			m->holder = curthread;
			list_insert(&curthread->mutexes, &m->link);
		}
	}
	sched_unlock();
	return error;
}

/*
 * Unlock a mutex.
 * Caller must be a current mutex holder.
 */
int
mutex_unlock(mutex_t *mp)
{
	mutex_t m;
	int error;

	sched_lock();
	if ((error = mutex_copyin(mp, &m)) != 0) {
		sched_unlock();
		return error;
	}

	if (m->holder != curthread || m->locks <= 0) {
		sched_unlock();
		return EPERM;
	}
	if (--m->locks == 0) {
		list_remove(&m->link);
		prio_uninherit(curthread);
		/*
		 * Change the mutex holder, and make the next
		 * holder runnable if it exists.
		 */
		m->holder = sched_wakeone(&m->event);
		if (m->holder)
			m->holder->mutex_waiting = NULL;

		m->priority = m->holder ? m->holder->priority : MINPRI;
	}
	sched_unlock();
	return 0;
}

/*
 * Cancel mutex operations.
 *
 * This is called with scheduling locked when thread is
 * terminated. If a thread is terminated with mutex hold, all
 * waiting threads keeps waiting forever. So, all mutex locked by
 * terminated thread must be unlocked. Even if the terminated
 * thread is waiting some mutex, the inherited priority of other
 * mutex holder is not adjusted.
 */
void
mutex_cancel(thread_t t)
{
	list_t head;
	mutex_t m;
	thread_t holder;

	/*
	 * Purge all mutexes held by the thread.
	 */
	head = &t->mutexes;
	while (!list_empty(head)) {
		/*
		 * Release locked mutex.
		 */
		m = list_entry(list_first(head), struct mutex, link);
		m->locks = 0;
		list_remove(&m->link);

		/*
		 * Change the mutex holder if other thread
		 * is waiting for it.
		 */
		holder = sched_wakeone(&m->event);
		if (holder) {
			holder->mutex_waiting = NULL;
			m->locks = 1;
			list_insert(&holder->mutexes, &m->link);
		}
		m->holder = holder;
	}
}

/*
 * This is called with scheduling locked before thread priority
 * is changed.
 */
void
mutex_setpri(thread_t t, int pri)
{

	if (t->mutex_waiting && pri < t->priority)
		prio_inherit(t);
}

/*
 * Check if the specified mutex is valid.
 */
static int
mutex_valid(mutex_t m)
{
	mutex_t tmp;
	list_t head, n;

	head = &curtask->mutexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		tmp = list_entry(n, struct mutex, task_link);
		if (tmp == m)
			return 1;
	}
	return 0;
}

/*
 * Copy mutex from user space.
 * If it is not initialized, create new mutex.
 */
static int
mutex_copyin(mutex_t *ump, mutex_t *kmp)
{
	mutex_t m;
	int error;

	if (copyin(ump, &m, sizeof(ump)))
		return EFAULT;

	if (m == MUTEX_INITIALIZER) {
		/*
		 * Allocate new mutex, and retreive its id
		 * from the user space.
		 */
		if ((error = mutex_init(ump)) != 0)
			return error;
		copyin(ump, &m, sizeof(ump));
	} else {
		if (!mutex_valid(m))
			return EINVAL;
	}
	*kmp = m;
	return 0;
}

/*
 * Inherit priority.
 *
 * To prevent priority inversion, we must ensure the higher
 * priority thread does not wait other lower priority thread. So,
 * raise the priority of mutex holder which blocks the "waiter"
 * thread. If such mutex holder is also waiting for other mutex,
 * that mutex is also processed. Returns EDEALK if it finds
 * deadlock condition.
 */
static int
prio_inherit(thread_t waiter)
{
	mutex_t m = waiter->mutex_waiting;
	thread_t holder;
	int count = 0;

	do {
		holder = m->holder;
		/*
		 * If the holder of relative mutex has already
		 * been waiting for the "waiter" thread, it
		 * causes a deadlock.
		 */
		if (holder == waiter) {
			DPRINTF(("Deadlock! mutex=%lx holder=%lx waiter=%lx\n",
				 (long)m, (long)holder, (long)waiter));
			return EDEADLK;
		}
		/*
		 * If the priority of the mutex holder is lower
		 * than "waiter" thread's, we rise the mutex
		 * holder's priority.
		 */
		if (holder->priority > waiter->priority) {
			sched_setpri(holder, holder->basepri, waiter->priority);
			m->priority = waiter->priority;
		}
		/*
		 * If the mutex holder is waiting for another
		 * mutex, that mutex is also processed.
		 */
		m = (mutex_t)holder->mutex_waiting;

		/* Fail safe... */
		ASSERT(count < MAXINHERIT);
		if (count >= MAXINHERIT)
			break;

	} while (m != NULL);
	return 0;
}

/*
 * Un-inherit priority
 *
 * The priority of specified thread is reset to the base
 * priority.  If specified thread locks other mutex and higher
 * priority thread is waiting for it, the priority is kept to
 * that level.
 */
static void
prio_uninherit(thread_t t)
{
	int maxpri;
	list_t head, n;
	mutex_t m;

	/* Check if the priority is inherited. */
	if (t->priority == t->basepri)
		return;

	maxpri = t->basepri;

	/*
	 * Find the highest priority thread that is waiting
	 * for the thread. This is done by checking all mutexes
	 * that the thread locks.
	 */
	head = &t->mutexes;
	for (n = list_first(head); n != head; n = list_next(n)) {
		m = list_entry(n, struct mutex, link);
		if (m->priority < maxpri)
			maxpri = m->priority;
	}

	sched_setpri(t, t->basepri, maxpri);
}
