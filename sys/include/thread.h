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

#ifndef _THREAD_H
#define _THREAD_H

#include <types.h>
#include <sys/cdefs.h>
#include <sys/queue.h>
#include <sys/list.h>
#include <sys/sysinfo.h>
#include <event.h>
#include <timer.h>
#include <hal.h>

/*
 * Description of a thread.
 */
struct thread {
	struct list	link;		/* linkage on all threads */
	struct list 	task_link;	/* linkage on thread list in task */
	struct queue	sched_link;	/* linkage on scheduling queue */
	task_t		task;		/* task to which I belong */
	int		state;		/* thread state */
	int		policy;		/* scheduling policy */
	int		priority;	/* current priority */
	int		basepri;	/* statical base priority */
	int		timeleft;	/* remaining ticks to run */
	u_int		time;		/* total running time */
	int		resched;	/* true if rescheduling is needed */
	int		locks;		/* schedule lock counter */
	int		suscnt;		/* suspend count */
	struct event	*slpevt;	/* event we are waiting on */
	int		slpret;		/* return value for sched_tleep */
	struct timer 	timeout;	/* thread timer */
	struct timer	*periodic;	/* pointer to periodic timer */
	uint32_t 	excbits;	/* bitmap of pending exceptions */
	struct list 	mutexes;	/* mutexes locked by this thread */
	mutex_t 	mutex_waiting;	/* mutex pointer currently waiting */
	struct queue 	ipc_link;	/* linkage on IPC queue */
	void		*msgaddr;	/* kernel address of IPC message */
	size_t		msgsize;	/* size of IPC message */
	thread_t	sender;		/* thread that sends IPC message */
	thread_t	receiver;	/* thread that receives IPC message */
	object_t 	sendobj;	/* IPC object sending to */
	object_t 	recvobj;	/* IPC object receiving from */
	void		*kstack;	/* base address of kernel stack */
	struct context 	ctx;		/* machine specific context */
};

/*
 * Thread state
 */
#define TS_RUN		0x00	/* running or ready to run */
#define TS_SLEEP	0x01	/* awaiting an event */
#define TS_SUSP		0x02	/* suspend count is not 0 */
#define TS_EXIT		0x04	/* terminated */

/*
 * Sleep result
 */
#define SLP_SUCCESS	0	/* success */
#define SLP_BREAK	1	/* break due to some reasons */
#define SLP_TIMEOUT	2	/* timeout */
#define SLP_INVAL	3	/* target event becomes invalid */
#define SLP_INTR	4	/* interrupted by exception */

/*
 * Scheduling operations for thread_schedparam().
 */
#define SOP_GETPRI	0	/* get scheduling priority */
#define SOP_SETPRI	1	/* set scheduling priority */
#define SOP_GETPOLICY	2	/* get scheduling policy */
#define SOP_SETPOLICY	3	/* set scheduling policy */

__BEGIN_DECLS
int	 thread_create(task_t, thread_t *);
int	 thread_terminate(thread_t);
void	 thread_destroy(thread_t);
int	 thread_load(thread_t, void (*)(void), void *);
thread_t thread_self(void);
int	 thread_valid(thread_t);
void	 thread_yield(void);
int	 thread_suspend(thread_t);
int	 thread_resume(thread_t);
int	 thread_schedparam(thread_t, int, int *);
void	 thread_idle(void);
int	 thread_info(struct threadinfo *);
thread_t kthread_create(void (*)(void *), void *, int);
void	 kthread_terminate(thread_t);
void	 thread_init(void);
__END_DECLS

#endif /* !_THREAD_H */
