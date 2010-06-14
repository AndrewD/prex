/*
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

#include <sys/prex.h>
#include <sys/posix.h>
#include <sys/signal.h>
#include <ipc/proc.h>
#include <ipc/fs.h>
#include <ipc/ipc.h>

#include <stddef.h>
#include <setjmp.h>
#include <errno.h>
#include <unistd.h>

static void __child_entry(void);

static jmp_buf __fork_env;

/*
 * fork() - fork for MMU system.
 *
 * RETURN VALUE:
 *
 *  fork() returns 0 to the child process and return the process
 *  ID of the child process to the parent process. Or, -1 will be
 *  returned to the parent process if error.
 *
 * ERRORS:
 *
 *  EAGAIN
 *  ENOMEM
 *
 * NOTE:
 *
 *  Since no thread is created by task_create(), thread_create()
 *  must be called follwing task_crate(). But, when new thread is
 *  created by thread_create(), the stack pointer of new thread is
 *  used at thread_create() although the stack image is copied at
 *  task_create(). So, the stack pointer must be reset to same
 *  address of thread_create() before calling task_create();
 *  This is a little tricky...
 *
 * The new process is an exact copy of the calling process
 * except as detailed below:
 * - Process IDs are different.
 * - tms_* is set to 0.
 * - Alarm clock is set to 0.
 * - Opend semaphore is inherited.
 * - Pending signals are cleared.
 *
 * - File lock is not inherited.
 * - File descriptor is shared.
 * - Directory stream is shared.
 */
static pid_t
fork(void)
{
	struct msg m;
	task_t tsk;
	thread_t t;
	int error;
	pid_t pid;

	/* Save current stack pointer */
	if (setjmp(__fork_env) == 0) {
		/*
		 * Create new task
		 */
		error = task_create(task_self(), VM_COPY, &tsk);
		if (error) {
			errno = error;
			return -1;
		}
		if ((error = thread_create(tsk, &t)) != 0) {
			task_terminate(tsk);
			errno = error;
			return -1;
		}
		/*
		 * Notify to process server
		 */
		m.hdr.code = PS_FORK;
		m.data[0] = tsk;		/* child task */
		m.data[1] = 0;			/* fork type */
		if (__posix_call(__proc_obj, &m, sizeof(m), 1) != 0)
			return -1;
		pid = m.data[0];

		/*
		 * Notify to file system server
		 */
		m.hdr.code = FS_FORK;
		m.data[0] = tsk;		/* child task */
		if (__posix_call(__fs_obj, &m, sizeof(m), 1) != 0)
			return -1;

		/*
		 * Start child task
		 */
		thread_load(t, __child_entry, NULL);
		thread_resume(t);
	} else {
		/*
		 * Child task
		 */
#ifdef _REENTRANT
		error = mutex_init(&__sig_lock);
#endif
		__sig_pending = 0;
		return 0;
	}
	return pid;
}

static void
__child_entry(void)
{

	longjmp(__fork_env, 1);
	/* NOTREACHED */
}

pid_t
vfork(void)
{

	return fork();
}

