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
 * except.c - exception handling routines
 */

/*-
 * A user mode task can specify its own exception handler with
 * exception_setup() system call.
 *
 * There are two different types of exception in a system - H/W and
 * S/W exception. The kernel determines to which thread it delivers
 * depending on the exception type.
 *
 *  - H/W exception
 *
 *   This type of exception is caused by H/W trap & fault. The
 *   exception will be sent to the thread which caused the trap.
 *   If no handler is specified by the task, it will be terminated
 *   by the kernel immediately.
 *
 *  - S/W exception
 *
 *   The user mode task can send S/W exception to another task by
 *   exception_raise() system call.
 *   The exception  will be sent to the thread that is sleeping with
 *   exception_wait() call. If no thread is waiting for the exception,
 *   the exception is sent to the first thread in the target task.
 *
 * Kernel supports 32 types of exception. The following pre-defined
 * exceptions are raised by kernel itself.
 *
 *   Exception Type Reason
 *   --------- ---- -----------------------
 *   EXC_ILL   h/w  Illegal instruction
 *   EXC_TRAP  h/w  Break point
 *   EXC_FPE   h/w  Math error
 *   EXC_SEGV  h/w  Invalid memory access
 *   EXC_ALRM  s/w  Alarm event
 *
 * The POSIX emulation library will setup own exception handler to 
 * convert the Prex exceptions into UNIX signals. It will maintain its
 * own signal mask, and transfer control to the POSIX signal handler.
 */

#include <kernel.h>
#include <event.h>
#include <task.h>
#include <thread.h>
#include <sched.h>
#include <task.h>
#include <except.h>

/* Event is used only to identify it. */
static struct event exception_event = EVENT_INIT(exception_event, "exception");

/*
 * Install an exception handler for the current task.
 * NULL can be specified as handler to remove current handler.
 * If handler is removed, all pending exceptions are discarded
 * immediately. In this case, all threads blocked in exception_wait()
 * are unblocked.
 *
 * Only one exception handler can be set per task. If the previous
 * handler exists in task, exception_setup() just override that
 * handler.
 */
__syscall int exception_setup(void (*handler)(int, u_long))
{
	task_t task;
	list_t head, n;
	thread_t th;

	task = cur_task();
	if (handler != NULL && !user_area(handler))
		return EFAULT;

	sched_lock();
	if (task->exc_handler && handler == NULL) {
		/*
		 * Remove exception handler.
		 * Discard all pending exception for all threads in
		 * task. If some thread is waiting exception, cancel it.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, struct thread, task_link);
			th->exc_bitmap = 0;
			if (th->wait_exc)
				sched_unsleep(th, SLP_BREAK);
		}
	}
	task->exc_handler = handler;
	sched_unlock();
	return 0;
}

/*
 * Raise an exception for specified task.
 * @task: task id
 * @exc: exception code
 *
 * The exception pending flag is marked here, and it is processed
 * by exception_deliver() later.
 * If the task want to raise an exception to another task, the
 * caller task must have CAP_KILL capability.
 * It returns error if the exception is sent to the kernel task.
 */
__syscall int exception_raise(task_t task, int exc)
{
	int err = 0;

	sched_lock();

	if (!task_valid(task)) {
		err = ESRCH;
		goto out;
	}
	if (task != cur_task() && !capable(CAP_KILL)) {
		err = EPERM;
		goto out;
	}
	err = __exception_raise(task, exc);
 out:
	sched_unlock();
	return err;
}

int __exception_raise(task_t task, int exc)
{
	list_t head, n;
	thread_t th;

	if (exc < 0 || exc >= NR_EXCEPTIONS)
		return EINVAL;

	if (task == &kern_task ||
	    task->exc_handler == NULL ||
	    list_empty(&task->threads))
		return EPERM;

	/*
	 * Determine which thread should we send an exception.
	 * First, search the thread that is waiting an exception by
	 * calling exception_wait(). Then, if no thread is waiting
	 * exceptions, it is sent to the master thread in task.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		if (th->wait_exc)
			break;
	}
	if (n == head) {
		n = list_first(head);
		th = list_entry(n, struct thread, task_link);
	}
	/* Mark pending bit for this exception */
	th->exc_bitmap |= (1 << exc);

	/* Unsleep if the target thread is sleeping */
	sched_unsleep(th, SLP_INTR);
	return 0;
}

/*
 * Block current thread until some exception is raised to
 * current thread.
 * @exc: exception code returned.
 *
 * The routine returns EINTR on success.
 */
__syscall int exception_wait(int *exc)
{
	int i, err;
	u_int32_t bit;

	if (cur_task()->exc_handler == NULL)
		return EINVAL;
	if (!user_area(exc))
		return EFAULT;

	sched_lock();

	cur_thread->wait_exc = 1;
	err = sched_sleep(&exception_event);
	cur_thread->wait_exc = 0;

	if (err == SLP_BREAK) {
		sched_unlock();
		return EINVAL;
	}
	for (i = 0; i < NR_EXCEPTIONS; i++) {
		bit = (u_int32_t)1 << i;
		if (cur_thread->exc_bitmap & bit)
			break;
	}
	sched_unlock();
	ASSERT(i != NR_EXCEPTIONS);
	if (umem_copyout(&i, exc, sizeof(int)) != 0)
		return EFAULT;
	return EINTR;
}

/*
 * Post specified exception to current thread.
 * This is called from architecture dependent code when H/W trap
 * is occurred.
 * If current task does not have exception handler, then current
 * task will be terminated.
 */
void exception_post(int exc)
{
	thread_t th = cur_thread;
	task_t task;

	ASSERT(exc > 0 && exc < NR_EXCEPTIONS);

	task = cur_task();
	if (task->exc_handler == NULL) {
		printk("Exception #%d is not handled by task.\n", exc);
		printk("Task \"%s\"(id:%x) is terminated.\n",
		       task->name ? task->name : "no name", task);

		/* Teminate current task */
		task_terminate(task);
	} else
		th->exc_bitmap |= (1 << exc);
}

/*
 * Check if pending exception exists for current task, and deliver
 * it to the exception handler if needed.
 * All exception is delivered at the time when the control goes back
 * to the user mode.
 * This routine is called from architecture dependent code.
 *
 * Some application may use longjmp() during its signal handler.
 * So, current context must be saved to user mode stack.
 */
void exception_deliver(void)
{
	thread_t th = cur_thread;
	void (*handler)(int, u_long);
	u_int32_t bit;
	int exc;

	sched_lock();
	handler = cur_task()->exc_handler;
	if (handler != NULL && th->exc_bitmap) {
		for (exc = 0; exc < NR_EXCEPTIONS; exc++) {
			bit = (u_int32_t)1 << exc;
			if (th->exc_bitmap & bit)
				break;
		}
		ASSERT(exc != NR_EXCEPTIONS);
		context_save(&th->context, exc);
		context_set(&th->context, USER_ENTRY, (u_long)handler);
		th->exc_bitmap &= ~bit;
	}
	sched_unlock();
}

/*
 * exception_return() is called from exception handler to restore
 * the original context.
 * @regs: context pointer which is passed to exception handler.
 *
 * TODO: should validate passed data area.
 */
__syscall int exception_return(void *regs)
{
	if ((regs == NULL) || !user_area(regs))
		return EFAULT;
	context_restore(&cur_thread->context, regs);
	return 0;
}
