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
 * thread.c - thread management routines.
 */

#include <kernel.h>
#include <kmem.h>
#include <task.h>
#include <thread.h>
#include <ipc.h>
#include <sched.h>
#include <sync.h>
#include <system.h>

struct thread idle_thread;
thread_t cur_thread = &idle_thread;
static thread_t zombie;

/*
 * Allocate a new thread and attach kernel stack for it.
 * Returns thread pointer on success, or NULL on failure.
 */
static thread_t
thread_alloc(void)
{
	thread_t th;
	void *stack;

	if ((th = kmem_alloc(sizeof(struct thread))) == NULL)
		return NULL;
	memset(th, 0, sizeof(struct thread));

	if ((stack = kmem_alloc(KSTACK_SIZE)) == NULL) {
		kmem_free(th);
		return NULL;
	}
	th->kstack = stack;
	th->magic = THREAD_MAGIC;
	list_init(&th->mutexes);
	return th;
}

static void
thread_free(thread_t th)
{
	th->magic = 0;
	kmem_free(th->kstack);
	kmem_free(th);
}

/*
 * Create a new thread within the specified task.
 *
 * The context of a current thread will be copied to the new thread.
 * The new thread will start from the return address of thread_create()
 * call in user mode code. Since a new thread will share the user
 * mode stack with a current thread, user mode applications are
 * responsible to allocate stack for it. The new thread is initially
 * set to suspend state, and so, thread_resume() must be called to
 * start it.
 *
 * The following scheduling parameters are reset to default values
 * in the created thread.
 *  - Thread State
 *  - Scheduling Policy
 *  - Scheduling Priority
 */
int
thread_create(task_t task, thread_t *thp)
{
	thread_t th;
	int err = 0;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (!task_access(task)) {
		err = EPERM;
		goto out;
	}
	if ((th = thread_alloc()) == NULL) {
		err = ENOMEM;
		goto out;
	}
	/*
	 * At first, we copy a new thread id as return value.
	 * This is done here to simplify all error recoveries
	 * of the subsequent code.
	 */
	if (cur_task() == &kern_task)
		*thp = th;
	else {
		if (umem_copyout(&th, thp, sizeof(thread_t))) {
			thread_free(th);
			err = EFAULT;
			goto out;
		}
	}
	/*
	 * Initialize thread state.
	 */
	th->task = task;
	th->suspend_count = task->suspend_count + 1;
	memcpy(th->kstack, cur_thread->kstack, KSTACK_SIZE);
	context_init(&th->context, (u_long)th->kstack + KSTACK_SIZE);
	list_insert(&task->threads, &th->task_link);
	sched_start(th);
 out:
	sched_unlock();
	return err;
}

/*
 * Permanently stop execution of the specified thread.
 * If given thread is a current thread, this routine never returns.
 */
int
thread_terminate(thread_t th)
{
	int err;

	sched_lock();
	if (!thread_valid(th)) {
		err = ESRCH;
	} else if (!task_access(th->task)) {
		err = EPERM;
	} else {
		err = thread_kill(th);
	}
	sched_unlock();
	return err;
}

/*
 * Kill a thread regardless of the current task state.
 *
 * This may be used to terminate a kernel thread under the non-context
 * condition. For example, a device driver may terminate its interrupt
 * thread even if a current task does not have the capability to
 * terminate it.
 */
int
thread_kill(thread_t th)
{
	/*
	 * Clean up thread state.
	 */
	msg_cleanup(th);
	timer_cleanup(th);
	mutex_cleanup(th);
	list_remove(&th->task_link);
	sched_stop(th);
	th->exc_bitmap = 0;
	th->magic = 0;

	/*
	 * We can not release the context of the "current" thread
	 * because our thread switching always requires the current
	 * context. So, the resource deallocation is deferred until
	 * another thread calls thread_kill().
	 */
	if (zombie != NULL) {
		/*
		 * Deallocate a zombie thread which was killed
		 * in previous request.
		 */
		ASSERT(zombie != cur_thread);
		thread_free(zombie);
		zombie = NULL;
	}
	if (th == cur_thread) {
		/*
		 * If the current thread is being terminated,
		 * enter zombie state and wait for sombody
		 * to be killed us.
		 */
		zombie = th;
	} else
		thread_free(th);
	return 0;
}

/*
 * Load entry/stack address of the user mode context.
 *
 * The entry and stack address can be set to NULL.
 * If it is NULL, old state is just kept.
 */
int
thread_load(thread_t th, void (*entry)(void), void *stack)
{
	int err = 0;

	if ((entry != NULL && !user_area(entry)) ||
	    (stack != NULL && !user_area(stack)))
		return EINVAL;

	sched_lock();
	if (!thread_valid(th)) {
		err = ESRCH;
	} else if (!task_access(th->task)) {
		err = EPERM;
	} else {
		if (entry != NULL)
			context_set(&th->context, CTX_UENTRY, (u_long)entry);
		if (stack != NULL)
			context_set(&th->context, CTX_USTACK, (u_long)stack);
	}
	sched_unlock();
	return 0;
}

/*
 * Set thread name.
 *
 * The naming service is separated from thread_create() so
 * the thread name can be changed at any time.
 */
int
thread_name(thread_t th, const char *name)
{
	size_t len;
	int err = 0;

	sched_lock();
	if (!thread_valid(th))
		err = ESRCH;
	else if (!task_access(th->task))
		err = EPERM;
	else {
		if (cur_task() == &kern_task)
			strlcpy(th->name, name, MAXTHNAME);
		else {
			if (umem_strnlen(name, MAXTHNAME, &len))
				err = EFAULT;
			else if (len >= MAXTHNAME) {
				umem_copyin(name, th->name, MAXTHNAME - 1);
				th->name[MAXTHNAME - 1] = '\0';
				err = ENAMETOOLONG;
			} else
				err = umem_copyin(name, th->name, len + 1);
		}
	}
	sched_unlock();
	return err;
}

thread_t
thread_self(void)
{

	return cur_thread;
}

/*
 * Release current thread for other thread.
 */
void
thread_yield(void)
{

	sched_yield();
}

/*
 * Suspend thread.
 *
 * A thread can be suspended any number of times. And, it does
 * not start to run again unless the thread is resumed by the
 * same count of suspend request.
 */
int
thread_suspend(thread_t th)
{
	int err = 0;

	sched_lock();
	if (!thread_valid(th)) {
		err = ESRCH;
	} else if (!task_access(th->task)) {
		err = EPERM;
	} else {
		if (++th->suspend_count == 1)
			sched_suspend(th);
	}
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
thread_resume(thread_t th)
{
	int err = 0;

	ASSERT(th != cur_thread);

	sched_lock();
	if (!thread_valid(th)) {
		err = ESRCH;
	} else if (!task_access(th->task)) {
		err= EPERM;
	} else if (th->suspend_count == 0) {
		err = EINVAL;
	} else {
		th->suspend_count--;
		if (th->suspend_count == 0 && th->task->suspend_count == 0)
			sched_resume(th);
	}
	sched_unlock();
	return err;
}

/*
 * thread_schedparam - get/set scheduling parameter.
 * @th:    target thread
 * @op:    operation ID
 * @param: pointer to parameter
 *
 * If the caller has CAP_NICE capability, all operations are allowed.
 * Otherwise, the caller can change the parameter for the threads in
 * the same task, and it can not set the priority to higher value.
 */
int
thread_schedparam(thread_t th, int op, int *param)
{
	int prio, policy, err = 0;
	int capable = 0;

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (task_capable(CAP_NICE))
		capable = 1;

	if (th->task != cur_task() && !capable) {
		sched_unlock();
		return EPERM;
	}
	if ((th->task == &kern_task) &&
	    (op == OP_SETPRIO || op == OP_SETPOLICY)) {
		sched_unlock();
		return EPERM;
	}
	switch (op) {
	case OP_GETPRIO:
		prio = sched_getprio(th);
		err = umem_copyout(&prio, param, sizeof(int));
		break;
	case OP_SETPRIO:
		if ((err = umem_copyin(param, &prio, sizeof(int))))
			break;
		if (prio < 0)
			prio = 0;
		else if (prio >= PRIO_IDLE)
			prio = PRIO_IDLE - 1;

		if (prio < th->prio && !capable) {
			err = EPERM;
			break;
		}
		/*
		 * If a current priority is inherited for mutex,
		 * we can not change the priority to lower value.
		 * In this case, only the base priority is changed,
		 * and a current priority will be adjusted to correct
		 * value, later.
		 */
		if (th->prio != th->base_prio && prio > th->prio)
			prio = th->prio;

		mutex_setprio(th, prio);
		sched_setprio(th, prio, prio);
		break;
	case OP_GETPOLICY:
		policy = sched_getpolicy(th);
		err = umem_copyout(&policy, param, sizeof(int));
		break;
	case OP_SETPOLICY:
		if ((err = umem_copyin(param, &policy, sizeof(int))))
			break;
		if (sched_setpolicy(th, policy))
			err = EINVAL;
		break;
	default:
		err = EINVAL;
		break;
	}
	sched_unlock();
	return err;
}

/*
 * Idle thread.
 *
 * This routine is called only once after kernel initialization
 * is completed. An idle thread has the role of cutting down the power
 * consumption of a system. An idle thread has FIFO scheduling policy
 * because it does not have time quantum.
 */
void
thread_idle(void)
{
	thread_name(cur_thread, "idle");

	for (;;) {
		machine_idle();
		sched_yield();
	}
	/* NOTREACHED */
}

/*
 * Create a thread running in the kernel address space.
 *
 * A kernel thread does not have user mode context, and its
 * scheduling policy is set to SCHED_FIFO. kernel_thread() returns
 * thread ID on success, or NULL on failure. We assume scheduler
 * is already locked.
 *
 * Important: Since sched_switch() will disable interrupts in CPU,
 * the interrupt is always disabled at the entry point of the kernel
 * thread. So, the kernel thread must enable the interrupt first when
 * it gets control.
 */
thread_t
kernel_thread(int prio, void (*entry)(u_long), u_long arg)
{
	thread_t th;

	if ((th = thread_alloc()) == NULL)
		return NULL;

	th->task = &kern_task;
	memset(th->kstack, 0, KSTACK_SIZE);
	context_init(&th->context, (u_long)th->kstack + KSTACK_SIZE);
	context_set(&th->context, CTX_KENTRY, (u_long)entry);
	context_set(&th->context, CTX_KARG, arg);
	list_insert(&kern_task.threads, &th->task_link);

	sched_start(th);
	sched_setpolicy(th, SCHED_FIFO);
	sched_setprio(th, prio, prio);
	sched_resume(th);
	return th;
}

/*
 * Return thread information for ps command.
 */
int
thread_info(struct info_thread *info)
{
	u_long index, target = info->cookie;
	list_t i, j;
	thread_t th;
	task_t task;

	sched_lock();
	index = 0;
	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		j = &task->threads;
		j = list_first(j);
		do {
			th = list_entry(j, struct thread, task_link);
			if (index++ == target)
				goto found;
			j = list_next(j);
		} while (j != &task->threads);
		i = list_next(i);
	} while (i != &kern_task.link);

	sched_unlock();
	return ESRCH;
 found:
	info->state = th->state;
	info->policy = th->policy;
	info->prio = th->prio;
	info->base_prio = th->base_prio;
	info->suspend_count = th->suspend_count;
	info->total_ticks = th->total_ticks;
	info->id = th;
	info->task = th->task;
	strlcpy(info->task_name, task->name, MAXTASKNAME);
	strlcpy(info->th_name, th->name, MAXTHNAME);
	strlcpy(info->sleep_event,
		th->sleep_event ? th->sleep_event->name : "-", 12);

	sched_unlock();
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void
thread_dump(void)
{
	static const char state[][4] = \
		{ "RUN", "SLP", "SUS", "S&S", "EXT" };
	static const char pol[][5] = { "FIFO", "RR  " };
	list_t i, j;
	thread_t th;
	task_t task;

	printk("Thread dump:\n");
	printk(" mod thread   task     stat pol  prio base ticks    "
	       "susp sleep event\n");
	printk(" --- -------- -------- ---- ---- ---- ---- -------- "
	       "---- ------------\n");

	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		j = &task->threads;
		j = list_first(j);
		do {
			th = list_entry(j, struct thread, task_link);
			printk(" %s %08x %8s %s%c %s  %3d  %3d %8d %4d %s\n",
			       (task == &kern_task) ? "Knl" : "Usr", th,
			       task->name, state[th->state],
			       (th == cur_thread) ? '*' : ' ',
			       pol[th->policy], th->prio, th->base_prio,
			       th->total_ticks, th->suspend_count,
			       th->sleep_event ? th->sleep_event->name : "-");
			j = list_next(j);
		} while (j != &task->threads);
		i = list_next(i);
	} while (i != &kern_task.link);
}
#endif

/*
 * The first thread in system is created here by hand. This thread
 * will become an idle thread when thread_idle() is called later.
 */
void
thread_init(void)
{
	void *stack;

	if ((stack = kmem_alloc(KSTACK_SIZE)) == NULL)
		panic("thread_init");

	memset(stack, 0, KSTACK_SIZE);
	idle_thread.kstack = stack;
	idle_thread.magic = THREAD_MAGIC;
	idle_thread.task = &kern_task;
	idle_thread.state = TH_RUN;
	idle_thread.policy = SCHED_FIFO;
	idle_thread.prio = PRIO_IDLE;
	idle_thread.base_prio = PRIO_IDLE;
	idle_thread.lock_count = 1;

	context_init(&idle_thread.context, (u_long)stack + KSTACK_SIZE);
	list_insert(&kern_task.threads, &idle_thread.task_link);
}
