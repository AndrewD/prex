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

/*-
 * Creating a thread and loading its register state are defined as
 * separate routine. These two routines are used by fork(), exec(),
 * and pthread_create() in the POSIX emulation library.
 *
 *                     thread_create()  thread_load()
 *                     ---------------  -------------
 *  fork()           :       O                X
 *  exec()           :       X                O
 *  pthread_create() :       O                O
 */

#include <kernel.h>
#include <list.h>
#include <kmem.h>
#include <task.h>
#include <thread.h>
#include <ipc.h>
#include <sched.h>
#include <sync.h>
#include <system.h>

/*
 * An idle thread is the first thread in the system, and it will
 * be set running when no other thread is active.
 */
struct thread idle_thread = IDLE_THREAD(idle_thread);

/* Thread waiting to be killed */
static thread_t zombie_thread;

/*
 * Allocate a new thread and attach a new kernel stack.
 * Returns thread pointer on success, or NULL on failure.
 */
static thread_t thread_alloc(void)
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

/*
 * Deallocate all thread data.
 */
static void thread_free(thread_t th)
{
	kmem_free(th->kstack);
	kmem_free(th);
}

/*
 * Create a new thread within the specified task.
 *
 * The context of a current thread will be copied to the new thread.
 * The new thread will start from the return address of thread_create()
 * call in the user mode. Since a new thread will share the user mode
 * stack with a current thread, user mode applications are responsible
 * for allocating new user mode stack. The new thread is initially set
 * to suspend state, and so, thread_resume() must be called to start it.
 *
 * The following scheduling parameters are reset to default values.
 *  - Thread State
 *  - Scheduling Policy
 *  - Scheduling Priority
 */
__syscall int thread_create(task_t task, thread_t *pth)
{
	int err;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != cur_task() && !task_capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	err = __thread_create(task, pth);
	sched_unlock();
	return err;
}

int __thread_create(task_t task, thread_t *pth)
{
	thread_t th;

	if ((th = thread_alloc()) == NULL)
		return ENOMEM;
	/*
	 * We copy a new thread id as return value, first. This
	 * is done here to simplify all error recovery for the
	 * subsequent code.
	 */
	if (cur_task() == &kern_task) {
		/* We are called inside kernel */
		*pth = th;
	} else {
		if (umem_copyout(&th, pth, sizeof(thread_t))) {
			thread_free(th);
			return EFAULT;
		}
	}
	/*
	 * We can not return any error from here.
	 */
	memcpy(th->kstack, cur_thread->kstack, KSTACK_SIZE);
	th->task = task;
	th->sus_count = task->sus_count + 1;
	context_init(&th->ctx, th->kstack + KSTACK_SIZE);
	list_insert(&task->threads, &th->task_link);
	sched_start(th);
	return 0;
}

/*
 * Terminate a thread.
 *
 * Release all resources of the specified thread. However, we can
 * not release the context of the current thread because our
 * thread switching always requires current context. So, the thread
 * termination is deferred until next thread_terminate() called by
 * another thread.
 * If specified thread is current thread, this routine never returns.
 */
__syscall int thread_terminate(thread_t th)
{
	int err;

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task == &kern_task ||
	    (th->task != cur_task() && !task_capable(CAP_TASK))) {
		sched_unlock();
		return EPERM;
	}
	err = __thread_terminate(th);
	sched_unlock();
	return err;
}

int __thread_terminate(thread_t th)
{
	/* Clear pending exception */
	th->exc_bitmap = 0;

	/* Clean up all resources */
	msg_cleanup(th);
	timer_cleanup(th);
	mutex_cleanup(th);

	list_remove(&th->task_link);
	sched_stop(th);
	th->magic = 0;

	/* If previous pending thread exists, kill it now. */
	if (zombie_thread && zombie_thread != cur_thread) {
		thread_free(zombie_thread);
		zombie_thread = NULL;
	}
	if (th == cur_thread) {
		/*
		 * The thread context can not be deallocated for
		 * current thread. So, wait for somebody to kill it.
		 */
		zombie_thread = th;
	} else {
		thread_free(th);
	}
	return 0;
}

/*
 * Load entry/stack address of user mode.
 *
 * The entry and stack address can be set to NULL. If it is
 * NULL, old state is just kept.
 */
__syscall int thread_load(thread_t th, void *entry, void *stack)
{
	if (cur_task() != &kern_task) {
		if (((entry && !user_area(entry)) ||
		     (stack && !user_area(stack))))
			return EINVAL;
	}
	sched_lock();

	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task != cur_task() && !task_capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	if (entry != NULL)
		context_set(&th->ctx, USER_ENTRY, (u_long)entry);
	if (stack != NULL)
		context_set(&th->ctx, USER_STACK, (u_long)stack);

	sched_unlock();
	return 0;
}

/*
 * Return id of a current thread.
 */
__syscall thread_t thread_self(void)
{
	return cur_thread;
}

/*
 * Release current thread for other thread.
 */
__syscall void thread_yield(void)
{
	sched_yield();
}

/*
 * Suspend thread.
 *
 * A thread can be suspended any number of times. And, it does not
 * start to run again unless the thread is resumed by the same count
 * of suspend request.
 */
__syscall int thread_suspend(thread_t th)
{
	sched_lock();

	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task != cur_task() && !task_capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	if (++th->sus_count == 1)
		sched_suspend(th);

	sched_unlock();
	return 0;
}

/*
 * Resume thread.
 *
 * A thread does not begin to run, unless both a thread suspend
 * count and a task suspend count are set to 0.
 */
__syscall int thread_resume(thread_t th)
{
	ASSERT(th != cur_thread);

	sched_lock();

	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task != cur_task() && !task_capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	if (th->sus_count == 0) {
		sched_unlock();
		return EINVAL;
	}
	th->sus_count--;
	if (th->sus_count == 0 && th->task->sus_count == 0)
		sched_resume(th);

	sched_unlock();
	return 0;
}

/*
 * Get/set scheduling parameter.
 *
 * @th:    target thread
 * @op:    operation ID
 * @param: pointer to parameter
 */
__syscall int thread_schedparam(thread_t th, int op, int *param)
{
	int prio, policy, err = 0;

	sched_lock();

	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task != cur_task() && !task_capable(CAP_NICE)) {
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
		/*
		 * If a current priority is inherited for mutex, we can
		 * not change the priority to lower value. In this case,
		 * only the base priority is changed, and a current
		 * priority will be adjusted to correct value, later.
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
 * Do idle loop.
 *
 * This routine is called only once after kernel initialization
 * is completed. An idle thread runs when no other thread is active.
 * It has the role of cutting down the power consumption of a system.
 * An idle thread has FIFO scheduling policy because it does not
 * have time quantum.
 */
void thread_idle(void)
{
	ASSERT(cur_thread->lock_count == 1);

	/* Unlock scheduler to start scheduling */
	sched_unlock();

	for (;;) {
		cpu_idle();
		sched_yield();
	}
	/* NOTREACHED */
}

/*
 * Create kernel thread.
 *
 * Kernel thread does not have user mode context, and its scheduling
 * policy is set to SCHED_FIFO. Currently, the kernel thread is used
 * for interrupt threads, timer thread, and an idle thread.
 * kernel_thread() returns thread ID on success, or NULL on failure.
 * The scheduler must be locked before calling this routine.
 *
 * Important: Since sched_switch() will disable interrupts in CPU, the
 * interrupt is disabled when the kernel thread is started first time.
 * So, the kernel thread must enable the interrupt by itself when it
 * runs first.
 */
thread_t kernel_thread(void (*entry)(u_long), u_long arg)
{
	thread_t th;

	sched_lock();

	if ((th = thread_alloc()) == NULL) {
		sched_unlock();
		return NULL;
	}
	memset(th->kstack, 0, KSTACK_SIZE);

	context_init(&th->ctx, th->kstack + KSTACK_SIZE);
	context_set(&th->ctx, KERN_ENTRY, (u_long)entry);
	context_set(&th->ctx, KERN_ARG, arg);
	th->task = &kern_task;
	th->policy = SCHED_FIFO;
	list_insert(&kern_task.threads, &th->task_link);

	sched_unlock();
	sched_start(th);
	return th;
}

/*
 * Return thread information.
 */
int thread_info(struct info_thread *info)
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
	info->sus_count = th->sus_count;
	info->total_ticks = th->total_ticks;
	info->task = th->task;
	strlcpy(info->task_name, task->name, MAX_TASKNAME);

	sched_unlock();
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void thread_dump(void)
{
	list_t i, j;
	thread_t th;
	task_t task;
	char state[][4] = { "RUN", "SLP", "SUS", "S&S", "EXT" };
	char pol[][5] = { "FIFO", "RR  " };

	printk("Thread dump:\n");
	printk(" mod thread   task     stat pol  prio base ticks total    susp sleep event\n");
	printk(" --- -------- -------- ---- ---- ---- ---- ----- -------- ---- ------------\n");

	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);
		j = &task->threads;
		j = list_first(j);
		do {
			th = list_entry(j, struct thread, task_link);
			printk(" %s %08x %08x %s%c %s  %3d  %3d   %3d %8d %4d %s\n",
			     (task == &kern_task) ? "Knl" : "Usr", th,
			     task, state[th->state],
			     (th == cur_thread) ? '*' : ' ',
			     pol[th->policy], th->prio, th->base_prio,
			     th->ticks_left, th->total_ticks, th->sus_count,
			     th->sleep_event ? th->sleep_event->name : "-");
			j = list_next(j);
		} while (j != &task->threads);
		i = list_next(i);
	} while (i != &kern_task.link);
}
#endif

/*
 * The first thread in system is created here, and this thread
 * becomes an idle thread when thread_idle() is called later.
 * The scheduler is locked until thread_idle() is called, in order
 * to prevent thread switch during kernel initialization.
 */
void thread_init(void)
{
	void *stack;

	/*
	 * Initialize idle thread
	 */
	if ((stack = kmem_alloc(KSTACK_SIZE)) == NULL)
		panic("Failed to allocate idle stack");
	memset(stack, 0, KSTACK_SIZE);
	idle_thread.kstack = stack;
	context_init(&idle_thread.ctx, stack + KSTACK_SIZE);
	list_insert(&kern_task.threads, &idle_thread.task_link);
}
