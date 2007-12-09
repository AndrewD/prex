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

#include <list.h>
#include <queue.h>
#include <event.h>
#include <task.h>
#include <timer.h>
#include <ipc.h>
#include <arch.h>
#include <system.h>

#define THREAD_MAGIC	0x5468723f	/* 'Thr?' */

struct mutex;

/*
 * Description of a thread.
 */
struct thread {
	int		magic;		/* Magic number */
	task_t		task;		/* Pointer to owner task */
	struct list 	task_link;	/* Link for all threads in same task */
	struct queue 	link;		/* Link for scheduling queue */
	int		state;		/* Thread state */
	int		policy;		/* Scheduling policy */
	int		prio;		/* Current priority */
	int		base_prio;	/* Base priority */
	int		ticks_left;	/* Remaining ticks to run */
	u_int		total_ticks;	/* Total running ticks */
	int		need_resched;	/* True if rescheduling is needed */
	int		lock_count;	/* Schedule lock counter */
	int		sus_count;	/* Suspend counter */
	event_t		sleep_event;	/* Sleep event */
	int		sleep_result;	/* Sleep result */
	struct timer 	timeout;	/* Thread timer */
	struct timer	*periodic;	/* Pointer to periodic timer */
	struct queue 	ipc_link;	/* Link for IPC queue */
	void		*msg_addr;	/* Kernel address of IPC message */
	size_t		msg_size;	/* Size of IPC message */
	struct thread 	*sender;	/* Thread that sends IPC message */
	struct thread 	*receiver;	/* Thread that receives IPC message */
	object_t 	send_obj;	/* IPC object sending to */
	object_t 	recv_obj;	/* IPC object receiving from */
	uint32_t 	exc_bitmap;	/* Bitmap of pending exceptions */
	int 		wait_exc;	/* True if waiting exception */
	struct list 	mutexes;	/* Mutexes locked by this thread */
	struct mutex 	*wait_mutex;	/* Mutex pointer currently waiting */
	void		*kstack;	/* Base address of kernel stack */
	struct context 	ctx;		/* Machine specific context */
};

typedef struct thread *thread_t;

#define thread_valid(th) (kern_area(th) && ((th)->magic == THREAD_MAGIC))

/*
 * Thread state
 */
#define TH_RUN		0x00	/* Running or ready to run */
#define TH_SLEEP	0x01	/* Sleep for events */
#define TH_SUSPEND	0x02	/* Suspend count is not 0 */
#define TH_EXIT		0x04	/* Terminated */

/*
 * Sleep result
 */
#define SLP_SUCCESS	0	/* Success */
#define SLP_BREAK	1	/* Break due to some reason */
#define SLP_TIMEOUT	2	/* Timeout */
#define SLP_INVAL	3	/* Target event becomes invalid */
#define SLP_INTR	4	/* Interrupted by exception */

extern struct task kern_task;
extern struct thread idle_thread;


#define IDLE_THREAD(th) \
{ \
	.magic = THREAD_MAGIC, \
	.task = &kern_task, \
	.state = TH_RUN, \
	.policy	= SCHED_FIFO, \
	.prio = PRIO_IDLE, \
	.base_prio = PRIO_IDLE, \
	.lock_count = 1, \
}

/*
 * Scheduling operations for thread_schedparam().
 */
#define OP_GETPRIO	0	/* Get scheduling priority */
#define OP_SETPRIO	1	/* Set scheduling priority */
#define OP_GETPOLICY	2	/* Get scheduling policy */
#define OP_SETPOLICY	3	/* Set scheduling policy */


extern struct thread *cur_thread;

extern void thread_init(void);
extern int __thread_create(task_t task, thread_t * th);
extern int thread_create(task_t task, thread_t * th);
extern int __thread_terminate(thread_t th);
extern int thread_terminate(thread_t th);
extern int thread_load(thread_t th, void *entry, void *stack);
extern thread_t thread_self(void);
extern void thread_yield(void);
extern int thread_suspend(thread_t th);
extern int thread_resume(thread_t th);
extern int thread_schedparam(thread_t th, int op, int *param);
extern void thread_idle(void);
extern thread_t kernel_thread(void (*entry)(u_long), u_long arg);
extern int thread_info(struct info_thread *info);

#endif /* !_THREAD_H */
