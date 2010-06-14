/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * thread.c - thread management routines.
 */

#include <kernel.h>
#include <kmem.h>
#include <task.h>
#include <thread.h>
#include <ipc.h>
#include <sched.h>
#include <sync.h>
#include <hal.h>

/* forward declarations */
static thread_t thread_allocate(task_t);
static void	thread_deallocate(thread_t);

static struct thread	idle_thread;	/* idle thread */
static thread_t		zombie;		/* zombie thread */
static struct list	thread_list;	/* list of all threads */

/* global variable */
thread_t curthread = &idle_thread;	/* current thread */

/*
 * Create a new thread.
 *
 * The new thread will start from the return address of
 * thread_create() in user mode.  Since a new thread shares
 * the user mode stack of the caller thread, the caller is
 * responsible to allocate and set new stack for it. The new
 * thread is initially set to suspend state, and so,
 * thread_resume() must be called to start it.
 */
int
thread_create(task_t task, thread_t *tp)
{
	thread_t t;
	vaddr_t sp;

	sched_lock();

	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}
	if (task->nthreads >= MAXTHREADS) {
		sched_unlock();
		return EAGAIN;
	}
	/*
	 * We check the pointer to the return value here.
	 * This will simplify the error recoveries of the
	 * subsequent code.
	 */
	if ((curtask->flags & TF_SYSTEM) == 0) {
		t = NULL;
		if (copyout(&t, tp, sizeof(t))) {
			sched_unlock();
			return EFAULT;
		}
	}
	/*
	 * Make thread entry for new thread.
	 */
	if ((t = thread_allocate(task)) == NULL) {
		DPRINTF(("Out of text\n"));
		sched_unlock();
		return ENOMEM;
	}
	memcpy(t->kstack, curthread->kstack, KSTACKSZ);
	sp = (vaddr_t)t->kstack + KSTACKSZ;
	context_set(&t->ctx, CTX_KSTACK, (register_t)sp);
	context_set(&t->ctx, CTX_KENTRY, (register_t)&syscall_ret);
	sched_start(t, curthread->basepri, SCHED_RR);
	t->suscnt = task->suscnt + 1;

	/*
	 * No page fault here:
	 */
	if (curtask->flags & TF_SYSTEM)
		*tp = t;
	else
		copyout(&t, tp, sizeof(t));

	sched_unlock();
	return 0;
}

/*
 * Permanently stop execution of the specified thread.
 * If given thread is a current thread, this routine
 * never returns.
 */
int
thread_terminate(thread_t t)
{

	sched_lock();
	if (!thread_valid(t)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(t->task)) {
		sched_unlock();
		return EPERM;
	}
	thread_destroy(t);
	sched_unlock();
	return 0;
}

/*
 * thread_destroy-- the internal version of thread_terminate.
 */
void
thread_destroy(thread_t th)
{

	msg_cancel(th);
	mutex_cancel(th);
	timer_cancel(th);
	sched_stop(th);
	thread_deallocate(th);
}

/*
 * Load entry/stack address of the user mode context.
 *
 * If the entry or stack address is NULL, we keep the
 * old value for it.
 */
int
thread_load(thread_t t, void (*entry)(void), void *stack)
{
	int s;

	if (entry != NULL && !user_area(entry))
		return EINVAL;
	if (stack != NULL && !user_area(stack))
		return EINVAL;

	sched_lock();

	if (!thread_valid(t)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(t->task)) {
		sched_unlock();
		return EPERM;
	}
	s = splhigh();
	if (entry != NULL)
		context_set(&t->ctx, CTX_UENTRY, (register_t)entry);
	if (stack != NULL)
		context_set(&t->ctx, CTX_USTACK, (register_t)stack);
	splx(s);

	sched_unlock();
	return 0;
}

/*
 * Return the current thread.
 */
thread_t
thread_self(void)
{

	return curthread;
}

/*
 * Return true if specified thread is valid.
 */
int
thread_valid(thread_t t)
{
	list_t head, n;
	thread_t tmp;

	head = &thread_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		tmp = list_entry(n, struct thread, link);
		if (tmp == t)
			return 1;
	}
	return 0;
}

/*
 * Release a current thread for other thread.
 */
void
thread_yield(void)
{

	sched_yield();
}

/*
 * Suspend thread.
 *
 * A thread can be suspended any number of times.
 * And, it does not start to run again unless the thread
 * is resumed by the same count of suspend request.
 */
int
thread_suspend(thread_t t)
{

	sched_lock();
	if (!thread_valid(t)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(t->task)) {
		sched_unlock();
		return EPERM;
	}
	if (++t->suscnt == 1)
		sched_suspend(t);

	sched_unlock();
	return 0;
}

/*
 * Resume thread.
 *
 * A thread does not begin to run, unless both thread
 * suspend count and task suspend count are set to 0.
 */
int
thread_resume(thread_t t)
{

	ASSERT(t != curthread);

	sched_lock();
	if (!thread_valid(t)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(t->task)) {
		sched_unlock();
		return EPERM;
	}
	if (t->suscnt == 0) {
		sched_unlock();
		return EINVAL;
	}
	t->suscnt--;
	if (t->suscnt == 0 && t->task->suscnt == 0)
		sched_resume(t);

	sched_unlock();
	return 0;
}

/*
 * thread_schedparam - get/set scheduling parameter.
 */
int
thread_schedparam(thread_t t, int op, int *param)
{
	int pri, policy;
	int error = 0;

	sched_lock();
	if (!thread_valid(t)) {
		sched_unlock();
		return ESRCH;
	}
	if (t->task->flags & TF_SYSTEM) {
		sched_unlock();
		return EINVAL;
	}
	/*
	 * A thread can change the scheduling parameters of the
	 * threads in the same task or threads in the child task.
	 */
	if (!(t->task == curtask || t->task->parent == curtask) &&
	    !task_capable(CAP_NICE)) {
		sched_unlock();
		return EPERM;
	}

	switch (op) {
	case SOP_GETPRI:
		pri = sched_getpri(t);
		if (copyout(&pri, param, sizeof(pri)))
			error = EINVAL;
		break;

	case SOP_SETPRI:
		if (copyin(param, &pri, sizeof(pri))) {
			error = EINVAL;
			break;
		}
		/*
		 * Validate the priority range.
		 */
		if (pri < 0)
			pri = 0;
		else if (pri >= PRI_IDLE)
			pri = PRI_IDLE - 1;

		/*
		 * If the caller has CAP_NICE capability, it can
		 * change the thread priority to any level.
		 * Otherwise, the caller can not set the priority
		 * to higher above realtime priority.
		 */
		if (pri <= PRI_REALTIME && !task_capable(CAP_NICE)) {
			error = EPERM;
			break;
		}
		/*
		 * If a current priority is inherited for mutex,
		 * we can not change the priority to lower value.
		 * In this case, only the base priority is changed,
		 * and a current priority will be adjusted to
		 * correct value, later.
		 */
		if (t->priority != t->basepri && pri > t->priority)
			pri = t->priority;

		mutex_setpri(t, pri);
		sched_setpri(t, pri, pri);
		break;

	case SOP_GETPOLICY:
		policy = sched_getpolicy(t);
		if (copyout(&policy, param, sizeof(policy)))
			error = EINVAL;
		break;

	case SOP_SETPOLICY:
		if (copyin(param, &policy, sizeof(policy))) {
			error = EINVAL;
			break;
		}
		error = sched_setpolicy(t, policy);
		break;

	default:
		error = EINVAL;
		break;
	}
	sched_unlock();
	return error;
}

/*
 * Idle thread.
 *
 * Put the system into low power mode until we get an
 * interrupt.  Then, we try to release the current thread to
 * run the thread who was woken by ISR.  This routine is
 * called only once after kernel initialization is completed.
 */
void
thread_idle(void)
{

	for (;;) {
		machine_idle();
		sched_yield();
	}
}

/*
 * Allocate a thread.
 */
static thread_t
thread_allocate(task_t task)
{
	struct thread *t;
	void *stack;

	if ((t = kmem_alloc(sizeof(*t))) == NULL)
		return NULL;

	if ((stack = kmem_alloc(KSTACKSZ)) == NULL) {
		kmem_free(t);
		return NULL;
	}
	memset(t, 0, sizeof(*t));

	t->kstack = stack;
	t->task = task;
	list_init(&t->mutexes);
	list_insert(&thread_list, &t->link);
	list_insert(&task->threads, &t->task_link);
	task->nthreads++;

	return t;
}

/*
 * Deallocate a thread.
 *
 * We can not release the context of the "current" thread
 * because our thread switching always requires the current
 * context. So, the resource deallocation is deferred until
 * another thread calls thread_deallocate() later.
 */
static void
thread_deallocate(thread_t t)
{

	list_remove(&t->task_link);
	list_remove(&t->link);
	t->excbits = 0;
	t->task->nthreads--;

	if (zombie != NULL) {
		/*
		 * Deallocate a zombie thread which
		 * was killed in previous request.
		 */
		ASSERT(zombie != curthread);
		kmem_free(zombie->kstack);
		zombie->kstack = NULL;
		kmem_free(zombie);
		zombie = NULL;
	}
	if (t == curthread) {
		/*
		 * Enter zombie state and wait for
		 * somebody to be killed us.
		 */
		zombie = t;
		return;
	}

	kmem_free(t->kstack);
	t->kstack = NULL;
	kmem_free(t);
}

/*
 * Return thread information.
 */
int
thread_info(struct threadinfo *info)
{
	u_long target = info->cookie;
	u_long i = 0;
	thread_t t;
	list_t n;

	sched_lock();
	n = list_last(&thread_list);
	do {
		if (i++ == target) {
			t = list_entry(n, struct thread, link);
			info->cookie = i;
			info->id = t;
			info->state = t->state;
			info->policy = t->policy;
			info->priority = t->priority;
			info->basepri = t->basepri;
			info->time = t->time;
			info->suscnt = t->suscnt;
			info->task = t->task;
			info->active = (t == curthread) ? 1 : 0;
			strlcpy(info->taskname, t->task->name, MAXTASKNAME);
			strlcpy(info->slpevt, t->slpevt ?
				t->slpevt->name : "-", MAXEVTNAME);
			sched_unlock();
			return 0;
		}
		n = list_prev(n);
	} while (n != &thread_list);
	sched_unlock();
	return ESRCH;
}

/*
 * Create a thread running in the kernel address space.
 *
 * Since we disable an interrupt during thread switching, the
 * interrupt is still disabled at the entry of the kernel
 * thread.  So, the kernel thread must enable interrupts
 * immediately when it gets control.
 * This routine assumes the scheduler is already locked.
 */
thread_t
kthread_create(void (*entry)(void *), void *arg, int pri)
{
	thread_t t;
	vaddr_t sp;

	ASSERT(curthread->locks > 0);

	/*
	 * If there is not enough core for the new thread,
	 * the caller should just drop to panic().
	 */
	if ((t = thread_allocate(&kernel_task)) == NULL)
		return NULL;

	memset(t->kstack, 0, KSTACKSZ);
	sp = (vaddr_t)t->kstack + KSTACKSZ;
	context_set(&t->ctx, CTX_KSTACK, (register_t)sp);
	context_set(&t->ctx, CTX_KENTRY, (register_t)entry);
	context_set(&t->ctx, CTX_KARG, (register_t)arg);
	sched_start(t, pri, SCHED_FIFO);
	t->suscnt = 1;
	sched_resume(t);

	return t;
}

/*
 * Terminate a kernel thread.
 */
void
kthread_terminate(thread_t t)
{
	ASSERT(t != NULL);
	ASSERT(t->task->flags & TF_SYSTEM);

	sched_lock();

	mutex_cancel(t);
	timer_cancel(t);
	sched_stop(t);
	thread_deallocate(t);

	sched_unlock();
}

/*
 * The first thread in the system is created here by hand.
 * This thread will become an idle thread when thread_idle()
 * is called later in main().
 */
void
thread_init(void)
{
	void *stack;
	vaddr_t sp;

	list_init(&thread_list);

	if ((stack = kmem_alloc(KSTACKSZ)) == NULL)
		panic("thread_init");

	memset(stack, 0, KSTACKSZ);
	sp = (vaddr_t)stack + KSTACKSZ;
	context_set(&idle_thread.ctx, CTX_KSTACK, (register_t)sp);
	sched_start(&idle_thread, PRI_IDLE, SCHED_FIFO);
	idle_thread.kstack = stack;
	idle_thread.task = &kernel_task;
	idle_thread.state = TS_RUN;
	idle_thread.locks = 1;
	list_init(&idle_thread.mutexes);

	list_insert(&thread_list, &idle_thread.link);
	list_insert(&kernel_task.threads, &idle_thread.task_link);
	kernel_task.nthreads = 1;
}
