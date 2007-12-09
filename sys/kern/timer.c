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
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOsODS
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
#include <hook.h>
#include <event.h>
#include <timer.h>
#include <irq.h>
#include <sched.h>
#include <thread.h>
#include <kmem.h>
#include <except.h>

static void timer_callhook(void);

static volatile u_long clock_ticks;	/* Ticks since OS boot */

static struct event timer_event = EVENT_INIT(timer_event, "timer");
static struct event delay_event = EVENT_INIT(delay_event, "delay");

static struct list timer_list = LIST_INIT(timer_list);   /* Active timers */
static struct list expire_list = LIST_INIT(expire_list); /* Expired timers */

static struct list timer_hooks = LIST_INIT(timer_hooks);

/*
 * Macro to get a timer for next expiration
 */
#define timer_next() \
	(list_entry(list_first(&timer_list), struct timer, link))

/*
 * Compute the remaining ticks for timer expire.
 * If the timer has already expired, it returns 0.
 */
static u_long time_remain(u_long expire)
{
	if (time_before(clock_ticks, expire))
		return (u_long)((long)expire - (long)clock_ticks);
	return 0;
}

/*
 * Stop timer.
 * timer_stop() can be called from ISR at interrupt level.
 */
void timer_stop(timer_t tmr)
{
	ASSERT(tmr);

	irq_lock();
	if (tmr->active) {
		list_remove(&tmr->link);
		tmr->active = 0;
	}
	irq_unlock();
	return;
}

/*
 * Add timer to the list.
 * Requires interrupts to be disabled by the caller.
 */
static void timer_add(timer_t tmr, u_long ticks)
{
	u_long expire;
	list_t head, n;
	timer_t t;

	if (ticks == 0)
		ticks++;
	expire = clock_ticks + ticks;
	tmr->expire = expire;

	/*
	 * A timer list is sorted by time. So, we have to insert
	 * the node to the appropriate position in the timer list.
	 */
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		if (time_before(expire, t->expire))
			break;
	}
	list_insert(list_prev(n), &tmr->link);
}

/*
 * Timer tick handler.
 *
 * This is called in each clock tick from a machine dependent
 * clock handler. All interrupts must be disabled before calling
 * this routine.
 */
void timer_tick(void)
{
	timer_t tmr;
	int wakeup = 0;

	clock_ticks++;

	/*
	 * Check all timeout items stored in the timer list.
	 */
	while (!list_empty(&timer_list) &&
	       time_after_eq(clock_ticks, timer_next()->expire)) {
		/*
		 * Remove expired timer from the list and wakup
		 * the appropriate thread. If it is periodic timer,
		 * reprogram the next expiration time. Otherwize,
		 * it is moved to the expired list.
		 */
		tmr = timer_next();
		list_remove(&tmr->link);
		if (tmr->interval) {
			/* Periodic timer */
			timer_add(tmr,
				  time_remain(tmr->expire + tmr->interval));
			sched_wakeup(&tmr->event);
		} else {
			/* One-shot timer */
			list_insert(&expire_list, &tmr->link);
			wakeup = 1;
		}
	}
	if (wakeup)
		sched_wakeup(&timer_event);

	/* Call scheduler */
	sched_tick();

	/* Call hook routines */
	timer_callhook();
}

/*
 * Timer thread.
 *
 * Handle all expired timers. Each call back routine is called
 * with scheduler locked and interrupts enabled.
 *
 * Note: All interrupts have already been disabled at the entry
 * of this routine because this is kernel thread.
 */
static void timer_thread(u_long arg)
{
	timer_t tmr;

	for (;;) {
		/* Wait until next timer expiration. */
		sched_sleep(&timer_event);

		while (!list_empty(&expire_list)) {
			/*
			 * Call a function of the expired timer.
			 */
			tmr = list_entry(list_first(&expire_list),
					 struct timer, link);
			list_remove(&tmr->link);
			tmr->active = 0;
			sched_lock();
			interrupt_enable();
			(*tmr->func)(tmr->arg);

			/*
			 * Unlock scheduler here in order to give
			 * chance to higher priority threads to run.
			 */
			sched_unlock();
			interrupt_disable();
		}
	}
	/* NOTREACHED */
}

/*
 * Delay thread execution.
 *
 * The caller thread is blocked for the specified time.
 * Returns 0 on success, or the remaining time (msec) on failure.
 * This can NOT be called from ISR at interrupt level.
 */
u_long timer_delay(u_long msec)
{
	timer_t tmr;
	int result;

	IRQ_ASSERT();

	tmr = &cur_thread->timeout;
	result = sched_tsleep(&delay_event, msec);
	if (result != SLP_TIMEOUT)
		return (tick_to_msec(time_remain(tmr->expire)));
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
 * call out routine as argument. timer_timeout() can be called
 * from ISR at interrupt level.
 */
void timer_timeout(timer_t tmr, void (*func)(void *),
		   void *arg, u_long msec)
{
	u_long ticks;

	ASSERT(tmr);

	ticks = msec_to_tick(msec);
	if (ticks == 0)
		ticks = 1;

	irq_lock();
	if (tmr->active)
		list_remove(&tmr->link);
	tmr->func = func;
	tmr->arg = arg;
	tmr->active = 1;
	timer_add(tmr, ticks);
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
	u_long msec;

	msec = timer_delay(delay);
	if (remain && umem_copyout(&msec, remain, sizeof(u_long)))
		return EFAULT;
	if (msec > 0)
		return EINTR;
	return 0;
}

/*
 * Alarm call back handler.
 */
static void alarm_callback(void *task)
{
	__exception_raise((task_t)task, EXC_ALRM);
}

/*
 * Alarm system call.
 * It schedules an alarm exception.
 *
 * @delay:  delay time in milli-second. If delay is 0, stop the timer
 * @remain: remaining time of the previous alarm request.
 *
 * EXC_ALRM is sent to the caller task when specified delay time
 * is passed.
 */
__syscall int timer_alarm(u_long delay, u_long *remain)
{
	timer_t tmr;
	u_long msec = 0;

	sched_lock();
	tmr = &cur_task()->alarm;

	/*
	 * Save current expiration time here to calculate
	 * the remaining time later.
	 */
	irq_lock();
	if (tmr->active)
		msec = tick_to_msec(time_remain(tmr->expire));

	if (delay) {
		timer_timeout(tmr, alarm_callback, (void *)cur_task(),
			      delay);
	} else {
		timer_stop(tmr);
	}
	irq_unlock();

	/* Store remaining time if it's needed */
	if (remain && umem_copyout(&msec, remain, sizeof(u_long))) {
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
 * @th:     thread to set timer.
 * @start:  first start time to wakeup. if start is 0, stop the timer.
 * @period: time interval to wakeup. This must be non-zero.
 *
 * The unit of start/period is milli-seconds.
 *
 * The memory for the periodic timer structure is allocated in
 * the first call of timer_periodic(). This is because only few
 * threads will use the periodic timer function.
 */
__syscall int timer_periodic(thread_t th, u_long start, u_long period)
{
	timer_t tmr;
	int err = 0;

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
	if (start != 0 && period == 0) {
		sched_unlock();
		return EINVAL;
	}
	tmr = th->periodic;
	if (start == 0) {
		/* Stop timer */
		if (tmr)
			timer_stop(tmr);
		else
			err = EINVAL;
	} else {
		/* Program timer */
		if (tmr == NULL) {
			/* Allocate timer resource at first call. */
			tmr = kmem_alloc(sizeof(struct timer));
			if (tmr == NULL) {
				sched_unlock();
				return ENOMEM;
			}
			event_init(&tmr->event, "periodic");
			tmr->active = 1;
			th->periodic = tmr;
		}
		irq_lock();
		tmr->interval = msec_to_tick(period);
		if (tmr->interval == 0)
			tmr->interval++;
		timer_add(tmr, msec_to_tick(start));
		irq_unlock();
	}
	sched_unlock();
	return err;
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
	timer_t tmr;
	int result;

	IRQ_ASSERT();

	if ((tmr = cur_thread->periodic) == NULL)
		return EINVAL;
	/*
	 * Check current timer state. The following sched_lock()
	 * is needed to prevent to run the timer handler after we
	 * checks the timer state.
	 */
	sched_lock();
	if (time_after_eq(clock_ticks, tmr->expire)) {
		/* The timer has already expired. */
		sched_unlock();
		return 0;
	}
	result = sched_sleep(&tmr->event);
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
	timer_t tmr;

	tmr = th->periodic;
	if (tmr != NULL) {
		timer_stop(tmr);
		kmem_free(tmr->arg);
	}
}

/*
 * Call all registered timer hook routines.
 * Some device drivers require the tick event for power
 * management or profiling jobs.
 */
static void timer_callhook(void)
{
	int idle;
	list_t head, n;
	hook_t hook;
	void (*func)(void *);

	idle = (cur_thread == &idle_thread) ? 1 : 0;
	head = &timer_hooks;
	for (n = list_first(head); n != head; n = list_next(n)) {
		hook = list_entry(n, struct hook, link);
		func = hook->func;
		(func)((void *)idle);
	}
}

/*
 * Hook timer tick routine
 */
void timer_hook(hook_t hook, void (*func)(void *))
{
	ASSERT(hook);
	ASSERT(func);

	irq_lock();
	hook->func = func;
	list_insert(&timer_hooks, &hook->link);
	irq_unlock();
}

/*
 * Unhook timer tick routine
 */
void timer_unhook(hook_t hook)
{
	irq_lock();
	list_remove(&hook->link);
	irq_unlock();
}

/*
 * Return current ticks since system boot.
 */
u_long timer_count(void)
{
	return clock_ticks;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void timer_dump(void)
{
	timer_t t;
	list_t head, n;

	printk("Timer dump:\n");
	printk("clock_ticks=%d\n", clock_ticks);
	head = &timer_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct timer, link);
		printk("timer=%x func=%x arg=%x expire=%d\n",
		       (int)t, (int)t->func, (int)t->arg,
		       (int)t->expire);
	}
}
#endif

void timer_init(void)
{
	thread_t th;

	clock_ticks = 0;

	/* Start timer thread */
	if ((th = kernel_thread(timer_thread, 0)) == NULL)
		panic("Failed to create timer thread");
	sched_setprio(th, PRIO_TIMER, PRIO_TIMER);
	sched_resume(th);
}
