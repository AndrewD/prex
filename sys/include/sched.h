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

#ifndef _SCHED_H
#define _SCHED_H

#include <event.h>
#include <thread.h>
#include <system.h>

/*
 * Scheduling policy
 */
#define SCHED_FIFO	0	/* First In First Out */
#define SCHED_RR	1	/* Round Robin */
#define SCHED_OTHER	2	/* Other */

/*
 * Scheduling quantum (Ticks for context switch)
 */
#define QUANTUM		(TIME_SLICE * HZ / 1000)

/*
 * Scheduling priority (Smaller value means higher priority.)
 */
#define MAX_PRIO	0
#define MIN_PRIO	(NR_PRIO - 1)
#define PRIO_IDLE	MIN_PRIO /* Priority for idle thread */

/*
 * DPC (Deferred Procedure Call) object
 */
struct dpc {
	struct queue	link;		/* Linkage on DPC queue */
	int		state;
	void		(*func)(void *); /* Call back routine */
	void		*arg;		/* Argument to pass */
};
typedef struct dpc *dpc_t;

/*
 * State for DPC
 */
#define DPC_FREE	0x4470463f	/* 'DpF?' */
#define DPC_PENDING	0x4470503f	/* 'DpP?' */

#define sched_sleep(evt) sched_tsleep((evt), 0)

extern void sched_init(void);
extern void sched_lock(void);
extern void sched_unlock(void);
extern void sched_yield(void);
extern int sched_tsleep(event_t event, u_long timeout);
extern void sched_wakeup(event_t event);
extern thread_t sched_wakeone(event_t event);
extern void sched_unsleep(thread_t th, int result);
extern void sched_suspend(thread_t th);
extern void sched_resume(thread_t th);
extern void sched_start(thread_t th);
extern void sched_stop(thread_t th);
extern void sched_tick(void);
extern int sched_getprio(thread_t th);
extern void sched_setprio(thread_t th, int base, int prio);
extern int sched_getpolicy(thread_t th);
extern int sched_setpolicy(thread_t th, int policy);
extern void sched_info(struct info_sched *info);
extern void sched_dpc(dpc_t dpc, void (*func)(void *), void *arg);

#endif /* !_SCHED_H */
