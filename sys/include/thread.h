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

#ifndef _THREAD_H
#define _THREAD_H

#include <queue.h>
#include <event.h>
#include <timer.h>
#include <arch.h>

struct mutex;

/*
 * Description of a thread.
 */
struct thread {
	int		magic;		/* magic number */
	task_t		task;		/* pointer to owner task */
	struct list 	task_link;	/* link for threads in same task */
	struct queue 	link;		/* linkage on scheduling queue */
	int		state;		/* thread state */
	int		policy;		/* scheduling policy */
	int		prio;		/* current priority */
	int		base_prio;	/* base priority */
	int		ticks_left;	/* remaining ticks to run */
	u_int		total_ticks;	/* total running ticks */
	int		need_resched;	/* true if rescheduling is needed */
	int		lock_count;	/* schedule lock counter */
	int		suspend_count;	/* suspend counter */
	struct event	*sleep_event;	/* sleep event */
	int		sleep_result;	/* sleep result */
	struct timer 	timeout;	/* thread timer */
	struct timer	*periodic;	/* pointer to periodic timer */
	struct queue 	ipc_link;	/* linkage on IPC queue */
	void		*msg_addr;	/* kernel address of IPC message */
	size_t		msg_size;	/* size of IPC message */
	struct thread 	*sender;	/* thread that sends IPC message */
	struct thread 	*receiver;	/* thread that receives IPC message */
	object_t 	send_obj;	/* IPC object sending to */
	object_t 	recv_obj;	/* IPC object receiving from */
	uint32_t 	exc_bitmap;	/* bitmap of pending exceptions */
	struct list 	mutexes;	/* mutexes locked by this thread */
	struct mutex 	*wait_mutex;	/* mutex pointer currently waiting */
	void		*kstack;	/* base address of kernel stack */
	struct context 	context;	/* machine specific context */
};

#define thread_valid(th) (kern_area(th) && ((th)->magic == THREAD_MAGIC))

/*
 * Thread state
 */
#define TH_RUN		0x00	/* running or ready to run */
#define TH_SLEEP	0x01	/* awaiting an event */
#define TH_SUSPEND	0x02	/* suspend count is not 0 */
#define TH_EXIT		0x04	/* terminated */

/*
 * Sleep result
 */
#define SLP_SUCCESS	0	/* success */
#define SLP_BREAK	1	/* break due to some reasons */
#define SLP_TIMEOUT	2	/* timeout */
#define SLP_INVAL	3	/* target event becomes invalid */
#define SLP_INTR	4	/* interrupted by exception */

/*
 * Priorities
 */
#define PRIO_TIMER	15	/* priority for timer thread */
#define PRIO_IST	16	/* top priority for interrupt threads */
#define PRIO_DPC	33	/* priority for Deferred Procedure Call */
#define PRIO_IDLE	255	/* priority for idle thread */
#define PRIO_USER	CONFIG_USER_PRIO

#define MAX_PRIO	0
#define MIN_PRIO	255
#define NR_PRIOS	256	/* number of thread priority */

/*
 * Scheduling operations for thread_schedparam().
 */
#define OP_GETPRIO	0	/* get scheduling priority */
#define OP_SETPRIO	1	/* set scheduling priority */
#define OP_GETPOLICY	2	/* get scheduling policy */
#define OP_SETPOLICY	3	/* set scheduling policy */

extern int	 thread_create(task_t, thread_t *);
extern int	 thread_terminate(thread_t);
extern int	 thread_kill(thread_t);
extern int	 thread_load(thread_t, void (*)(void), void *);
extern thread_t	 thread_self(void);
extern void	 thread_yield(void);
extern int	 thread_suspend(thread_t);
extern int	 thread_resume(thread_t);
extern int	 thread_schedparam(thread_t, int, int *);
extern void	 thread_idle(void);
extern int	 thread_info(struct info_thread *);
extern thread_t	 kernel_thread(int, void (*)(u_long), u_long);
extern void	 thread_dump(void);
extern void	 thread_init(void);

#endif /* !_THREAD_H */
