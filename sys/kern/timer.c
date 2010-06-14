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
 * timer.c - kernel timer services.
 */

#include <kernel.h>
#include <task.h>
#include <event.h>
#include <sched.h>
#include <thread.h>
#include <kmem.h>
#include <exception.h>
#include <timer.h>
#include <sys/signal.h>

static volatile u_long	lbolt;		/* ticks elapsed since bootup */
static volatile u_long	idle_ticks;	/* total ticks for idle */

static struct event	timer_event;	/* event to wakeup a timer thread */
static struct event	delay_event;	/* event for the thread delay */
static struct list	timer_list;	/* list of active timers */
static struct list	expire_list;	/* list of expired timers */

/*
 * Get remaining ticks to the expiration time.
 * Return 0 if timer has been expired.
 */
static u_long
time_remain(u_long expire)
{

	if (time_before(lbolt, expire))
		return expire - lbolt;
	return 0;
}

/*
 * Activate a timer.
 */
static void
timer_add(struct timer *tmr, u_long ticks)
{
	list_t head, n;
	struct timer *t;

	if (ticks == 0)
		ticks++;

	tmr->expire = lbolt + ticks;
	tmr->state = TM_ACTIVE;

	/*
	 * Insert a timer element into the timer list which
	 * is sorted by expiration time.
	 */
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		if (time_before(tmr->expire, t->expire))
			break;
	}
	list_insert(list_prev(n), &tmr->link);
}

/*
 * Stop an active timer.
 */
void
timer_stop(struct timer *tmr)
{
	int s;

	ASSERT(tmr != NULL);

	s = splhigh();
	if (tmr->state == TM_ACTIVE) {
		list_remove(&tmr->link);
		tmr->state = TM_STOP;
	}
	splx(s);
}

/*
 * Schedule a callout function to run after a specified
 * length of time.
 *
 * Note: A device driver can call timer_callout() or
 * timer_stop() from ISR at interrupt level.
 */
void
timer_callout(struct timer *tmr, u_long msec, void (*fn)(void *), void *arg)
{
	int s;

	ASSERT(tmr != NULL);
	ASSERT(fn != NULL);

	s = splhigh();

	if (tmr->state == TM_ACTIVE)
		list_remove(&tmr->link);

	tmr->func = fn;
	tmr->arg = arg;
	tmr->interval = 0;
	timer_add(tmr, mstohz(msec));

	splx(s);
}

/*
 * timer_delay - delay thread execution.
 *
 * The caller thread is blocked for the specified time.
 * Returns 0 on success, or the remaining time (msec) on
 * failure.
 */
u_long
timer_delay(u_long msec)
{
	struct timer *tmr;
	u_long remain = 0;
	int rc;

	rc = sched_tsleep(&delay_event, msec);
	if (rc != SLP_TIMEOUT) {
		tmr = &curthread->timeout;
		remain = hztoms(time_remain(tmr->expire));
	}
	return remain;
}

/*
 * timer_sleep - sleep system call.
 *
 * Stop execution of the current thread for the indicated amount
 * of time.  If the sleep is interrupted, the remaining time is
 * set in "remain".
 */
int
timer_sleep(u_long msec, u_long *remain)
{
	u_long left;

	left = timer_delay(msec);

	if (remain != NULL) {
		if (copyout(&left, remain, sizeof(left)))
			return EFAULT;
	}
	if (left > 0)
		return EINTR;
	return 0;
}

/*
 * Alarm timer expired:
 * Send an alarm exception to the target task.
 */
static void
alarm_expire(void *arg)
{
	task_t task = (task_t)arg;

	exception_post(task, SIGALRM);
}

/*
 * timer_alarm - alarm system call.
 *
 * SIGALRM exception is sent to the caller task when specified
 * delay time is passed. If "msec" argument is 0, stop the
 * current running timer.
 */
int
timer_alarm(u_long msec, u_long *remain)
{
	struct timer *tmr;
	u_long left = 0;
	int s;

	s = splhigh();
	tmr = &curtask->alarm;

	/*
	 * If the timer is active, save the remaining time
	 * before we update the timer setting.
	 */
	if (tmr->state == TM_ACTIVE)
		left = hztoms(time_remain(tmr->expire));

	if (msec == 0)
		timer_stop(tmr);
	else
		timer_callout(tmr, msec, &alarm_expire, curtask);

	splx(s);
	if (remain != NULL) {
		if (copyout(&left, remain, sizeof(left)))
			return EFAULT;
	}
	return 0;
}

/*
 * timer_periodic - set periodic timer for the specified thread.
 *
 * The periodic thread can wait the timer period by calling
 * timer_waitperiod(). The unit of start/period is milli-seconds.
 */
int
timer_periodic(thread_t t, u_long start, u_long period)
{
	struct timer *tmr;
	int s;

	if (start != 0 && period == 0)
		return EINVAL;

	sched_lock();
	if (!thread_valid(t)) {
		sched_unlock();
		return ESRCH;
	}
	if (t->task != curtask) {
		sched_unlock();
		return EPERM;
	}

	tmr = t->periodic;
	if (start == 0) {
		/*
		 * Cancel periodic timer.
		 */
		if (tmr == NULL || tmr->state != TM_ACTIVE) {
			sched_unlock();
			return EINVAL;
		}
		timer_stop(tmr);
	} else {
		if (tmr == NULL) {
			/*
			 * Allocate a timer element at first call.
			 * This is to save the data area in the thread
			 * structure.
			 */
			if ((tmr = kmem_alloc(sizeof(tmr))) == NULL) {
				sched_unlock();
				return ENOMEM;
			}
			memset(tmr, 0, sizeof(*tmr));
			event_init(&tmr->event, "periodic");
			t->periodic = tmr;
		}
		/*
		 * Program an interval timer.
		 */
		s = splhigh();
		tmr->interval = mstohz(period);
		if (tmr->interval == 0)
			tmr->interval = 1;
		timer_add(tmr, mstohz(start));
		splx(s);
	}
	sched_unlock();
	return 0;
}

/*
 * timer_waitperiod - wait next period of the periodic timer.
 *
 * If the caller task receives any exception, this system call
 * will return before target time. So, the caller must retry
 * immediately if the error status is EINTR. This will be
 * automatically done by the library stub routine.
 */
int
timer_waitperiod(void)
{
	struct timer *tmr;
	int rc;

	tmr = curthread->periodic;
	if (tmr == NULL || tmr->state != TM_ACTIVE)
		return EINVAL;

	if (time_before(lbolt, tmr->expire)) {
		/*
		 * Sleep until timer_handler() routine wakes us up.
		 */
		rc = sched_sleep(&tmr->event);
		if (rc != SLP_SUCCESS)
			return EINTR;
	}
	return 0;
}

/*
 * Untimeout the timers for the thread termination.
 */
void
timer_cancel(thread_t t)
{

	if (t->periodic != NULL) {
		timer_stop(t->periodic);
		kmem_free(t->periodic);
		t->periodic = NULL;
	}
}

/*
 * Timer thread.
 *
 * Handle all expired timers. Each callout routine is
 * called with scheduler locked and interrupts enabled.
 */
static void
timer_thread(void *dummy)
{
	struct timer *tmr;

	splhigh();

	for (;;) {
		/*
		 * Wait until next timer expiration.
		 */
		sched_sleep(&timer_event);

		while (!list_empty(&expire_list)) {
			/*
			 * callout
			 */
			tmr = timer_next(&expire_list);
			list_remove(&tmr->link);
			tmr->state = TM_STOP;
			sched_lock();
			spl0();
			(*tmr->func)(tmr->arg);

			/*
			 * Unlock scheduler here in order to give
			 * chance to higher priority threads to run.
			 */
			sched_unlock();
			splhigh();
		}
	}
	/* NOTREACHED */
}

/*
 * Handle clock interrupts.
 *
 * timer_handler() is called directly from the real time clock
 * interrupt.  All interrupts are still disabled at the entry
 * of this routine.
 */
void
timer_handler(void)
{
	struct timer *tmr;
	u_long ticks;
	int wakeup = 0;

	/*
	 * Bump time in ticks.
	 * Note that it is allowed to wrap.
	 */
	lbolt++;
	if (curthread->priority == PRI_IDLE)
		idle_ticks++;

	while (!list_empty(&timer_list)) {
		/*
		 * Check timer expiration.
		 */
		tmr = timer_next(&timer_list);
		if (time_before(lbolt, tmr->expire))
			break;

		list_remove(&tmr->link);
		if (tmr->interval != 0) {
			/*
			 * Periodic timer - reprogram timer again.
			 */
			ticks = time_remain(tmr->expire + tmr->interval);
			timer_add(tmr, ticks);
			sched_wakeup(&tmr->event);
		} else {
			/*
			 * One-shot timer
			 */
			list_insert(&expire_list, &tmr->link);
			wakeup = 1;
		}
	}
	if (wakeup)
		sched_wakeup(&timer_event);

	sched_tick();
}

/*
 * Return ticks since boot.
 */
u_long
timer_ticks(void)
{

	return lbolt;
}

/*
 * Return timer information.
 */
void
timer_info(struct timerinfo *info)
{

	info->hz = HZ;
	info->cputicks = lbolt;
	info->idleticks = idle_ticks;
}

/*
 * Initialize the timer facility, called at system startup time.
 */
void
timer_init(void)
{

	event_init(&timer_event, "timer");
	event_init(&delay_event, "delay");
	list_init(&timer_list);
	list_init(&expire_list);

	if (kthread_create(&timer_thread, NULL, PRI_TIMER) == NULL)
		panic("timer_init");
}
