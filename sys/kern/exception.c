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
 * exception.c - exception handling routines
 */

/**
 * An user mode task can set its own exception handler with
 * exception_setup() system call.
 *
 * There are two different types of exceptions in a system - H/W and
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
 * Kernel supports 32 types of exceptions. The following pre-defined
 * exceptions are raised by kernel itself.
 *
 *   Exception Type Reason
 *   --------- ---- -----------------------
 *   SIGILL    h/w  illegal instruction
 *   SIGTRAP   h/w  break point
 *   SIGFPE    h/w  math error
 *   SIGSEGV   h/w  invalid memory access
 *   SIGALRM   s/w  alarm event
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
#include <hal.h>
#include <exception.h>

static struct event	exception_event;

/*
 * Install an exception handler for the current task.
 *
 * EXC_DFL can be specified as handler to remove the current handler.
 * If handler is removed, all pending exceptions are discarded
 * immediately. At this time, all threads blocked in exception_wait()
 * are automatically unblocked.
 *
 * We allow only one exception handler per task. If the handler
 * has already been set in task, exception_setup() just override
 * the previous handler.
 */
int
exception_setup(void (*handler)(int))
{
	task_t self = curtask;
	list_t head, n;
	thread_t t;
	int s;

	if (handler != EXC_DFL && !user_area(handler))
		return EFAULT;
	if (handler == NULL)
		return EINVAL;

	sched_lock();
	if (self->handler != EXC_DFL && handler == EXC_DFL) {
		/*
		 * Remove existing exception handler. Do clean up
		 * job for all threads in the target task.
		 */
		head = &self->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {

			/*
			 * Clear pending exceptions.
			 */
			s = splhigh();
			t = list_entry(n, struct thread, task_link);
			t->excbits = 0;
			splx(s);

			/*
			 * If the thread is waiting for an exception,
			 * cancel it.
			 */
			if (t->slpevt == &exception_event) {
				DPRINTF(("Exception cancelled task=%s\n",
					 self->name));
				sched_unsleep(t, SLP_BREAK);
			}
		}
	}
	self->handler = handler;
	sched_unlock();
	return 0;
}

/*
 * exception_raise - system call to raise an exception.
 *
 * The exception pending flag is marked here, and it is
 * processed by exception_deliver() later. The task must have
 * CAP_KILL capability to raise an exception to another task.
 */
int
exception_raise(task_t task, int excno)
{
	int error;

	sched_lock();
	if (!task_valid(task)) {
		DPRINTF(("Bad exception task=%lx\n", (long)task));
		sched_unlock();
		return ESRCH;
	}
	if (task != curtask && !task_capable(CAP_KILL)) {
		sched_unlock();
		return EPERM;
	}
	error = exception_post(task, excno);
	sched_unlock();
	return error;
}

/*
 * exception_post-- the internal version of exception_raise().
 */
int
exception_post(task_t task, int excno)
{
	list_t head, n;
	thread_t t = NULL;
	int s, found = 0;

	sched_lock();
	if (task->flags & TF_SYSTEM) {
		sched_unlock();
		return EPERM;
	}

	if ((task->handler == EXC_DFL) ||
	    (task->nthreads == 0) ||
	    (excno < 0) || (excno >= NEXC)) {
		sched_unlock();
		return EINVAL;
	}

	/*
	 * Determine which thread should we send an exception.
	 * First, search the thread that is currently waiting
	 * an exception by calling exception_wait().
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct thread, task_link);
		if (t->slpevt == &exception_event) {
			found = 1;
			break;
		}
	}

	/*
	 * If no thread is waiting exceptions, we send it to
	 * the master thread in the task.
	 */
	if (!found) {
		if (!list_empty(&task->threads)) {
			n = list_first(&task->threads);
			t = list_entry(n, struct thread, task_link);
		}
	}

	/*
	 * Mark pending bit for this exception.
	 */
	s = splhigh();
	t->excbits |= (1 << excno);
	splx(s);

	/*
	 * Wakeup the target thread regardless of its
	 * waiting event.
	 */
	sched_unsleep(t, SLP_INTR);

	sched_unlock();
	return 0;
}

/*
 * exception_wait - block a current thread until some
 * exceptions are raised to the current thread.
 *
 * The routine returns EINTR on success.
 */
int
exception_wait(int *excno)
{
	int i, rc, s;

	if (curtask->handler == EXC_DFL)
		return EINVAL;

	/* Check fault before sleeping. */
	i = 0;
	if (copyout(&i, excno, sizeof(i)))
		return EFAULT;

	sched_lock();

	/*
	 * Sleep until some exceptions occur.
	 */
	rc = sched_sleep(&exception_event);
	if (rc == SLP_BREAK) {
		sched_unlock();
		return EINVAL;
	}
	s = splhigh();
	for (i = 0; i < NEXC; i++) {
		if (curthread->excbits & (1 << i))
			break;
	}
	splx(s);
	ASSERT(i != NEXC);
	sched_unlock();

	if (copyout(&i, excno, sizeof(i)))
		return EFAULT;
	return EINTR;
}

/*
 * Mark an exception flag for the current thread.
 *
 * This is called by HAL code when H/W trap is occurred. If a
 * current task does not have exception handler, then the
 * current task will be terminated. This routine can be called
 * at interrupt level.
 */
void
exception_mark(int excno)
{
	int s;

	ASSERT(excno > 0 && excno < NEXC);

	/* Mark pending bit */
	s = splhigh();
	curthread->excbits |= (1 << excno);
	splx(s);
}

/*
 * exception_deliver - deliver pending exception to the task.
 *
 * Check if pending exception exists for the current task, and
 * deliver it to the exception handler if needed. All
 * exceptions are delivered at the time when the control goes
 * back to the user mode.  Some application may use longjmp()
 * during its signal handler. So, current context must be
 * saved to the user mode stack.
 */
void
exception_deliver(void)
{
	task_t self = curtask;
	void (*handler)(int);
	uint32_t bitmap;
	int s, excno;

	ASSERT(curthread->state != TS_EXIT);
	sched_lock();

	s = splhigh();
	bitmap = curthread->excbits;
	splx(s);

	if (bitmap != 0) {
		/*
		 * Find a pending exception.
		 */
		for (excno = 0; excno < NEXC; excno++) {
			if (bitmap & (1 << excno))
				break;
		}
		handler = self->handler;
		if (handler == EXC_DFL) {
			DPRINTF(("Exception #%d is not handled by task.\n",
				excno));
			DPRINTF(("Terminate task:%s (id:%lx)\n",
				 self->name, (long)self));

			task_terminate(self);
			/* NOTREACHED */
		}

		/*
		 * Transfer control to an exception handler.
		 */
		s = splhigh();
		context_save(&curthread->ctx);
		context_set(&curthread->ctx, CTX_UENTRY, (register_t)handler);
		context_set(&curthread->ctx, CTX_UARG, (register_t)excno);
		curthread->excbits &= ~(1 << excno);
		splx(s);
	}
	sched_unlock();
}

/*
 * exception_return() is called from exception handler to
 * restore the original context.
 */
void
exception_return(void)
{
	int s;

	s = splhigh();
	context_restore(&curthread->ctx);
	splx(s);
}

void
exception_init(void)
{

	event_init(&exception_event, "exception");
}
