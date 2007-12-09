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
 * timer.c - kernel timer routines
 */

/*-
 * A kernel timer provides following features.
 *
 * - Sleep timer:     Put a thread to sleep state for specified time.
 * - Call back timer: Call the routine after specified time passes.
 * - Periodic timer:  Call the routine at the specified interval.
 */

#include <kernel.h>
#include <list.h>
#include <event.h>
#include <timer.h>
#include <irq.h>
#include <sched.h>
#include <thread.h>
#include <kmem.h>
#include <except.h>

/*
 * Periodic timer
 */
struct periodic {
	struct timer	timer;		/* Timer structure */
	u_long		interval;	/* Interval time */
	struct event	event;		/* Event for this timer */
};
typedef struct periodic *periodic_t;

static volatile u_long system_ticks;	/* Ticks since OS boot */

static struct list timer_list = LIST_INIT(timer_list);
static u_long timer_active;	/* True if timer is active */
static u_long next_expire;	/* Tick count of the next timer expiration */

static struct event timer_event = EVENT_INIT(timer_event, "timer");
static struct event delay_event = EVENT_INIT(delay_event, "delay");

/*
 * Setup timer.
 * timer_setup() must be called with irq_lock held.
 */
static void timer_setup(timer_t tmr, u_long ticks)
{
	u_long expire;
	list_t head, n;
	timer_t t;

	ASSERT(tmr);
	ASSERT(ticks);
 
	/* Reset timer if it has already been started. */
	if (tmr->type != TMR_STOP)
		timer_stop(tmr);

	/*
	 * Timer list must be sorted by time. So, we have to find
	 * the appropriate node position in the timer list.
	 */
	expire = system_ticks + ticks;
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		if (time_before(expire, t->expire))
			break;
	}
	tmr->expire = expire;
	list_insert(list_prev(n), &tmr->link);

	/* Update next expire time */
	if (!timer_active || time_before(expire, next_expire))
		next_expire = expire;
	timer_active = 1;
}

/*
 * Stop timer.
 * timer_stop() can be called from ISR at interrupt level.
 */
void timer_stop(timer_t tmr)
{
	list_t n;
	timer_t t;

	irq_lock();
	if (tmr->type == TMR_STOP) {
		irq_unlock();
		return;
	}
	list_remove(&tmr->link);
	tmr->type = TMR_STOP;

	if (list_empty(&timer_list))
		timer_active = 0;
	else {
		n = list_first(&timer_list);
		t = list_entry(n, struct timer, link);
		next_expire = t->expire;
	}
	irq_unlock();
	return;
}

/*
 * Compute the remaining time to expire.
 * If the timer has already expired, it returns 0.
 */
static u_long time_remain(u_long expire)
{
	if (time_before(system_ticks, expire))
		return (u_long)((long)expire - (long)system_ticks);
	else 
		return 0;
}

/*
 * Timer interrupt handler.
 *
 * This is called in each clock tick from machine dependent
 * clock code.  A timer thread will be activated if at least
 * one active timer is expired.
 * All interrupts must be disabled before calling this routine.
 */
void timer_clock(void)
{
	system_ticks++;
	if (timer_active && time_after_eq(system_ticks, next_expire))
		sched_wakeup(&timer_event);
	sched_clock();
}

/*
 * Timer thread.
 *
 * This kernel thread checks all timeout items in the timeout list.
 * If it is found, do the corresponding action for its timer type.
 * Each timer call back routine is called with scheduler locked.
 */
static void timer_thread(u_long arg)
{
	periodic_t ptmr;
	timer_t tmr;

	interrupt_enable();

 next:
	/*
	 * Wait until next timer expiration.
	 */
	sched_sleep(&timer_event);

	while (timer_active &&
	       time_after_eq(system_ticks, next_expire)) {

		sched_lock();
		irq_lock();
		tmr = list_entry(list_first(&timer_list),
				 struct timer, link);
		irq_unlock();

		switch (tmr->type) {
		case TMR_TIMEOUT:
			timer_stop(tmr);
			(*tmr->func)(tmr->arg);
			break;

		case TMR_PERIODIC:
			ptmr = (periodic_t)tmr->arg;
			tmr->expire += ptmr->interval;
			timer_setup(tmr, time_remain(tmr->expire));
			tmr->type = TMR_PERIODIC;
			sched_wakeup(&ptmr->event);
			break;
		default:
			panic("Unknown timer type");
		}
		/*
		 * Unlock scheduler here in order to give chance of
		 * thread switching to higher priority threads.
		 */
		sched_unlock();
	}
	goto next;
}

/*
 * Delay thread execution.
 *
 * The caller thread is blocked for the specified time.
 * Returns 0 on success, or the remaining time (msec) on failure.
 * This can NOT be called from ISR at interrupt level.
 */
int timer_delay(u_long msec)
{
	timer_t tmr;
	int result;

	IRQ_ASSERT();

	tmr = &cur_thread->timeout;
	result = sched_tsleep(&delay_event, msec);
	if (result != SLP_TIMEOUT)
		return (int)(time_remain(tmr->expire) * HZ / 1000);
	return 0;
}

/*
 * Program a kernel timeout timer.
 *
 * @tmr:  timer id.
 * @func: function to be called later.
 * @arg:  argument to pass.
 * @msec: timeout value in msec.
 *
 * The call-out routine will be called from timer thread after
 * specified time passed. The value of arg is passed to the
 * call out routine as argument.
 * timer_timeout() can be called from ISR at interrupt level.
 */
void timer_timeout(timer_t tmr, void (*func)(void *), void *arg,
		   u_long msec)
{
	u_long ticks;

	ticks = msec * HZ / 1000;
	if (ticks == 0)
		ticks = 1;

	irq_lock();
	timer_setup(tmr, ticks);
	tmr->func = func;
	tmr->arg = arg;
	tmr->type = TMR_TIMEOUT;
	irq_unlock();
}

/*
 * Sleep system call.
 *
 * @delay:  delay time in milli-second
 * @remain: remaining time returned when the request is failed.
 *
 * Stop execution of current thread until specified time passed.
 * Returns EINTR if sleep is canceled by some reason.
 */
__syscall int timer_sleep(u_long delay, u_long *remain)
{
	int msec;

	msec = timer_delay(delay);
	if (remain && umem_copyout(&msec, remain, sizeof(int)) != 0)
		return EFAULT;
	if (msec > 0)
		return EINTR;
	return 0;
}

/*
 * Alarm call back handler.
 */
static void timer_ring(void *task)
{
	__exception_raise((task_t)task, EXC_ALRM);
}

/*
 * Alarm system call.
 * It schedules an alarm exception.
 *
 * @delay:  delay time in milli-second. If delay is 0, stop the timer
 * @remain: remaining time of the precious alarm request.
 *
 * EXC_ALRM is sent to the caller task when specified delay
 * time is passed.
 */
__syscall int timer_alarm(u_long delay, u_long *remain)
{
	timer_t tmr;
	int msec = 0;

	sched_lock();
	tmr = &cur_task()->alarm;
	irq_lock();

	/* Calculate remaining time if the alarm has been set. */
	if (tmr->type == TMR_TIMEOUT)
		msec = (int)(time_remain(tmr->expire) * HZ / 1000);

	if (delay == 0)
		timer_stop(tmr);
	else
		timer_timeout(tmr, timer_ring, (void *)cur_task(), delay);

	irq_unlock();

	if (remain && umem_copyout(&msec, remain, sizeof(int)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	sched_unlock();
	return 0;
}

/*
 * Set periodic timer for the specified thread.
 * The thread will be woken up in specified time interval.
 *
 * @start:  first start time to wakeup. if start is 0, stop the timer.
 * @period: time interval to wakeup.
 *
 * The unit of start/period is milli-seconds.
 *
 * The memory for the periodic timer structure is allocated in
 * the first call of timer_periodic(). This is because only few
 * threads will use the periodic timer.
 */
__syscall int timer_periodic(thread_t th, u_long start, u_long period)
{
	timer_t tmr;
	periodic_t ptmr;

	IRQ_ASSERT();

	sched_lock();
	if (!thread_valid(th)) {
		sched_unlock();
		return ESRCH;
	}
	if (th->task != cur_task()) {
		sched_unlock();
		return EPERM;
	}
	tmr = th->periodic;
	if (start == 0) {
		/* Stop timer */
		if (tmr == NULL)
			return EINVAL;
		else {
			timer_stop(tmr);
			sched_unlock();
			return 0;
		}
	}
	if (tmr)
		ptmr = tmr->arg;
	else {
		/* Create timer for first call. */
		ptmr = kmem_alloc(sizeof(struct periodic));
		if (ptmr == NULL) {
			sched_unlock();
			return ENOMEM;
		}
		event_init(&ptmr->event, "periodic");
		tmr = &ptmr->timer;
		tmr->type = TMR_STOP;
		tmr->arg = ptmr;
		th->periodic = tmr;
	}
	ptmr->interval = period * HZ / 1000;
	if (ptmr->interval == 0)
		ptmr->interval = 1;

	irq_lock();
	timer_setup(tmr, start * HZ / 1000);
	tmr->type = TMR_PERIODIC;
	irq_unlock();

	sched_unlock();
	return 0;
}

/*
 * Wait next period of the running periodic timer.
 *
 * Since this routine can exit by any exception, the control
 * may return at non-period time. So, the caller must retry
 * immediately if the error status is EINTR. This is done by
 * the library stub routine.
 */
__syscall int timer_waitperiod(void)
{
	periodic_t ptmr;
	timer_t tmr;
	int result;

	IRQ_ASSERT();

	tmr = cur_thread->periodic;
	if (tmr == NULL)
		return EINVAL;
	/*
	 * Check current timer state. The following sched_lock()
	 * is needed to prevent to run the timer handler after we
	 * checks the timer state.
	 */
	sched_lock();
	if (time_after_eq(system_ticks, tmr->expire)) {
		/* The timer has already expired. */
		sched_unlock();
		return 0;
	}
	ptmr = (periodic_t)tmr->arg;
	result = sched_sleep(&ptmr->event);
	sched_unlock();
	if (result != SLP_SUCCESS)
		return EINTR;
	return 0;
}

/*
 * Clean up for thread termination.
 */
void timer_cleanup(thread_t th)
{
	if (th->periodic) {
		timer_stop(th->periodic);
		kmem_free(th->periodic);
	}
}

/*
 * Return current ticks since system boot.
 */
u_long timer_ticks(void)
{
	return system_ticks;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void timer_dump(void)
{
	timer_t t;
	list_t head, n;

	printk("Timer dump:\n");
	printk("system_ticks=%d\n", system_ticks);

	irq_lock();
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		printk("timer=%x type=%d func=%x arg=%x expire=%d\n",
		       (int)t, t->type, (int)t->func, (int)t->arg,
		       (int)t->expire);
	}
	irq_unlock();
}
#endif

void timer_init(void)
{
	thread_t th;

	timer_active = 0;
	list_init(&timer_list);

	/* Start timer thread */
	if ((th = kernel_thread(timer_thread, 0)) == NULL)
		panic("Failed to create timer thread");
	sched_setprio(th, PRIO_TIMER, PRIO_TIMER);
	sched_resume(th);
}
