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
 * sched.c - scheduler
 */

/*-
 * The Prex scheduler is based on the algorithm known as priority based
 * multi level queue. Each thread is assigned the priority between
 * 0 and 255. The lower number means higher priority like BSD unix.
 * The scheduler maintains 256 level run queues mapped to each priority.
 * The lowest priority (=255) is used only by an idle thread.
 *
 * All threads have two different types of priority:
 *
 *  - Base priority
 *      This is a static priority used for priority computation. A user
 *      mode program can change this value by system call.
 *
 *  - Current priority
 *      An actual scheduling priority. A kernel may adjust this priority
 *      dynamically if it's needed.
 *
 * Each thread has one of the following state.
 *
 *  - TH_RUN     Running or ready to run
 *  - TH_SLEEP   Sleep for some event
 *  - TH_SUSPEND Suspend count is not 0
 *  - TH_EXIT    Terminated
 *
 * The thread is always preemptive even in the kernel mode.
 * There are following 4 reasons to switch thread.
 *
 *  1) Block
 *     Thread is blocked for sleep or suspend.
 *     It is put on the tail of the run queue when it becomes
 *     runnable again.
 *
 *  2) Preemption
 *     If higher priority thread becomes runnable, the current
 *     thread is put on the the _head_ of run queue.
 *
 *  3) Quantum expiration
 *     If the thread consumes its time quantum, it is put on
 *     the tail of run queue.
 *
 *  4) Yield
 *     If the thread releases CPU by itself, it is put on the
 *     tail of run queue.
 *
 * There are following three types of scheduling policy.
 *  - SCHED_FIFO   First-in First-out
 *  - SCHED_RR     Round Robin (SCHED_FIFO + timeslice)
 *  - SCHED_OTHER  N/A
 */

#include <kernel.h>
#include <queue.h>
#include <list.h>
#include <event.h>
#include <irq.h>
#include <thread.h>
#include <timer.h>
#include <vm.h>
#include <task.h>
#include <system.h>
#include <sched.h>

thread_t cur_thread = &idle_thread;	/* Current thread */

static struct queue runq[NR_PRIO];	/* Queue for running threads */
static int top_prio;			/* Highest priority in runq */

static struct queue wakeq;		/* Queue for waking threads */

/*
 * Get highest priority in the active run queue.
 */
static int runq_top(void)
{
	int prio;

	for (prio = 0; prio < MIN_PRIO; prio++) {
		if (!queue_empty(&runq[prio]))
			break;
	}
	return prio;
}

/*
 * Enqueue a thread to the end of the run queue corresponding
 * to its priority.
 */
static void runq_enqueue(thread_t th)
{
	enqueue(&runq[th->prio], &th->link);
	if (th->prio < top_prio) {
		top_prio = th->prio;
		cur_thread->need_resched = 1;
	}
}

/*
 * Insert a thread to the front of the run queue corresponding
 * to its priority.
 */
static void runq_insert(thread_t th)
{
	queue_insert(&runq[th->prio], &th->link);
	if (th->prio < top_prio)
		top_prio = th->prio;
}

/*
 * Pick up and remove the highest priority thread from the run queue.
 * At least, an idle thread will be returned because it always exists
 * in the lowest priority queue.
 */
static thread_t runq_dequeue(void)
{
	queue_t q;
	thread_t th;

	q = dequeue(&runq[top_prio]);
	th = queue_entry(q, struct thread, link);
	if (queue_empty(&runq[top_prio]))
		top_prio = runq_top();
	return th;
}

/*
 * The specified thread is removed from its run queue.
 */
static void runq_remove(thread_t th)
{
	queue_remove(&th->link);
	top_prio = runq_top();
}

/*
 * sched_switch() - Switch running thread.
 *
 * If the scheduling reason is the preemption, the current thread
 * is put on the head of the run queue. So, the thread still has
 * right to run first again among the same priority threads.
 * For other scheduling reason, the current thread is inserted into
 * the tail of the run queue.
 *
 * For performance reason, the page directory is not switched if ...
 *  1) The owner task of next thread is same with the current task.
 *  2) The next thread is a kernel thread.
 * Since a kernel task does not have user mode memory image, we don't
 * have to set the page directory for it. Thus, an idle thread and
 * interrupt threads can be switched faster.
 */
static void sched_switch(void)
{
	thread_t prev, next;

	IRQ_ASSERT();

	prev = cur_thread;
	if (prev->state == TH_RUN) {
		if (prev->prio > top_prio)
			runq_insert(prev);
		else
			runq_enqueue(prev);
	}
	if (prev->ticks_left <= 0)
		prev->ticks_left = prev->quantum;

	next = runq_dequeue();
	if (next == prev)
		return;
	cur_thread = next;

	if (prev->task != next->task && next->task != &kern_task)
		mmu_switch(next->task->map->pgd);

	context_switch(&prev->context, &next->context);
}

/*
 * sched_clock() is called from a clock interrupt handler.
 */
void sched_clock(void)
{
	cur_thread->total_ticks++;

	if (cur_thread->policy == SCHED_RR) {
		if (--cur_thread->ticks_left <= 0)
			cur_thread->need_resched = 1;
	}
}

/*
 * Suspend the specified thread.
 * The scheduler must be locked before calling this routine.
 * Note that the suspend count is handled in thread_suspend().
 */
void sched_suspend(thread_t th)
{
	ASSERT(cur_thread->lock_count > 0);

	if (th->state == TH_RUN) {
		if (th == cur_thread)
			cur_thread->need_resched = 1;
		else
			runq_remove(th);
	}
	th->state |= TH_SUSPEND;
}

/*
 * Resume the specified thread.
 * The scheduler must be locked before calling this routine.
 */
void sched_resume(thread_t th)
{
	ASSERT(cur_thread->lock_count > 0);

	if (th->state & TH_SUSPEND) {
		th->state &= ~TH_SUSPEND;
		if (th->state == TH_RUN)
			runq_enqueue(th);
	}
}

/*
 * sched_yiled() - Yield the current processor to another thread.
 *
 * If a thread switching occurs, the current thread will be moved on
 * the tail of the run queue regardless of its policy.
 * Note that the current thread may run immediately again, if no
 * other thread exists in the same priority queue.
 */
void sched_yield(void)
{
	IRQ_ASSERT();

	sched_lock();
	if (!queue_empty(&runq[cur_thread->prio]))
		cur_thread->need_resched = 1;
	sched_unlock();
}

/*
 * A timeout callback routine for sched_tsleep().
 * Wake up the specified thread that is sleeping in sched_tsleep().
 * @arg: thread to unsleep.
 */
static void sleep_timeout(void *arg)
{
	sched_unsleep((thread_t)arg, SLP_TIMEOUT);
}

/*
 * Process all pending woken threads.
 *
 * If pending thread exists in wakeq, move it to the runq.
 * Rescheduling flag may be set.
 * Interrupt must be disabled by caller.
 */
static void wakeq_flush(void)
{
	queue_t q;
	thread_t th;

	while (!queue_empty(&wakeq)) {
		q = dequeue(&wakeq);
		th = queue_entry(q, struct thread, link);
		th->sleep_result = 0;
		th->sleep_event = 0;
		th->state &= ~TH_SLEEP;
		if (th != cur_thread && th->state == TH_RUN)
			runq_enqueue(th);
	}
}

/*
 * sched_tsleep() - Sleep the current thread for the specified event.
 * @timeout: time out value in msec.
 *
 * This routine returns a sleep result. If the thread is woken by
 * sched_wakeup() or sched_wakeone(), it returns 0. Otherwise, it
 * will return the value which is passed by sched_unsleep().
 *
 * sched_sleep() is also defined as a wrapper macro for sched_tsleep()
 * without timeout.
 * Note that all sleep requests are interruptible with this kernel.
 */
int sched_tsleep(event_t event, u_long timeout)
{
	register thread_t th;
	int stat;

	IRQ_ASSERT();
	ASSERT(event);

	interrupt_save(&stat);
	interrupt_disable();

	th = cur_thread;
	th->sleep_event = event;
	th->state |= TH_SLEEP;
	enqueue(&event->sleepq, &th->link);

	/*
	 * Program the timeout timer to call sleep_timeout() later.
	 * It will make this thread runnable again if timeout occurs.
	 */
	if (timeout)
		timer_timeout(&th->timeout, sleep_timeout, th, timeout);

	/* Flush all pending wakeup request here. */
	wakeq_flush();

	/* Sleep here. Zzzz.. */
	sched_switch();

	interrupt_restore(stat);
	return th->sleep_result;
}

/*
 * sched_wakeup() - Wake up all threads sleeping for the specified event.
 *
 * A thread can have sleep and suspend state simultaneously.
 * So, the thread does not always run immediately even if
 * it woke up.
 *
 * Since this routine can be called from ISR at interrupt level, it
 * can not touch any data in the thread structure. Otherwise, we
 * must always disable interrupts before accessing thread's data.
 * Thus, this routine will move the waking thread into wakeq, and
 * all data access will be done at more safer time in wakeq_flush() later.
 *
 * The woken thread will be put on the tail of runq regardless
 * of its policy. If woken threads have same priority, next running
 * thread is selected by FIFO order.
 */
void sched_wakeup(event_t event)
{
	queue_t q;
	struct thread *th;

	irq_lock();
	while (!queue_empty(&event->sleepq)) {
		q = dequeue(&event->sleepq);
		th = queue_entry(q, struct thread, link);
		enqueue(&wakeq, q);
		timer_stop(&th->timeout);
	}
	irq_unlock();
}

/*
 * sched_wakeone() - Wake up one thread sleeping for the specified event.
 *
 * The highest priority thread is woken among sleeping threads.
 * sched_wakeone() returns the thread ID of the woken thread, or
 * NULL if no threads are sleeping.
 * Unlike sched_wakeup(), this routine can not be called from ISR.
 */
thread_t sched_wakeone(event_t event)
{
	queue_t head, q;
	thread_t th, top;

	IRQ_ASSERT();

	irq_lock();
	head = &event->sleepq;
	if (queue_empty(head)) {
		irq_unlock();
		return NULL;
	}
	q = queue_first(head);
	top = queue_entry(q, struct thread, link);
	while (!queue_end(head, q)) {
		th = queue_entry(q, struct thread, link);
		if (th->prio < top->prio)
			top = th;
		q = queue_next(q);
	}
	queue_remove(&top->link);
	th = top;

	enqueue(&wakeq, &th->link);
	timer_stop(&th->timeout);
	irq_unlock();
	return th;
}

/*
 * Cancel sleep.
 *
 * sched_unsleep() removes the specified thread from its sleep queue.
 * The specified sleep result will be passed to the sleeping thread
 * as a return value of sched_tsleep().
 * Unlike sched_wakeup(), this routine can not be called from ISR.
 */
void sched_unsleep(thread_t th, int result)
{
	IRQ_ASSERT();

	if (!(th->state & TH_SLEEP))
		return;
	ASSERT(th->sleep_event);

	sched_lock();

	irq_lock();
	queue_remove(&th->link);
	irq_unlock();

	th->sleep_event = 0;
	th->sleep_result = result;
	th->state &= ~TH_SLEEP;
	if (th->state == TH_RUN)
		runq_enqueue(th);

	timer_stop(&th->timeout);
	sched_unlock();
}

/*
 * Setup thread structure to start.
 */
void sched_start(thread_t th)
{
	th->state = TH_RUN | TH_SUSPEND;
	th->policy = SCHED_RR;
	th->prio = PRIO_DEFAULT;
	th->base_prio = PRIO_DEFAULT;
	th->quantum = DEFAULT_QUANTUM;
	th->ticks_left = DEFAULT_QUANTUM;
}

/*
 * Stop thread.
 *
 * If specified thread is current thread, the scheduling lock
 * count is set to 1 to ensure this thread is switched in next
 * sched_unlock().
 */
void sched_stop(thread_t th)
{
	IRQ_ASSERT();
	ASSERT(cur_thread->lock_count > 0);

	if (th == cur_thread) {
		cur_thread->lock_count = 1;
		cur_thread->need_resched = 1;
	} else {
		if (th->state == TH_RUN)
			runq_remove(th);
		else if (th->state & TH_SLEEP)
			queue_remove(&th->link);
	}
	timer_stop(&th->timeout);
	th->state = TH_EXIT;
}

/*
 * Get the current priority of the specified thread.
 */
int sched_getprio(thread_t th)
{
	return th->prio;
}

/*
 * Set thread priority
 *
 * The scheduler must be locked before calling this routine.
 * Thread switch may be invoked here by priority change.
 */
void sched_setprio(thread_t th, int base, int prio)
{
	th->base_prio = base;

	if (th == cur_thread) {
		th->prio = prio;
		top_prio = runq_top();
		if (prio < top_prio)
			cur_thread->need_resched = 1;
	} else if (th->state == TH_RUN) {
		/* Reset runq position */
		runq_remove(th);
		th->prio = prio;
		runq_enqueue(th);
	} else {
		th->prio = prio;
	}
}

/*
 * Get scheduling policy
 */
int sched_getpolicy(thread_t th)
{
	return th->policy;
}

/*
 * Set scheduling policy
 */
int sched_setpolicy(thread_t th, int policy)
{
	switch (policy) {
	case SCHED_RR:
	case SCHED_FIFO:
		th->ticks_left = DEFAULT_QUANTUM;
		th->policy = policy;
		return 0;
	}
	return -1;
}

/*
 * sched_lock() - Lock the scheduler.
 *
 * The thread switch is disabled during scheduler locked. This is mainly
 * used to synchronize the thread execution to protect global resources.
 * Any interrupt handler can run even when scheduler is locked.
 *
 * Since the scheduling lock can be nested any number of time, the caller
 * has the responsible to unlock the same number of locks.
 * sched_lock() may be called before sched_init() is called at boot time.
 */
void sched_lock(void)
{
	cur_thread->lock_count++;
}

/*
 * sched_unlock() - Unlock scheduler.
 *
 * If nobody locks the scheduler anymore, it runs pending wake
 * threads and check the reschedule flag. The thread switch is
 * invoked if the rescheduling request exists.
 * Note that this routine is also called from the end of the interrupt
 * handler.
 */
void sched_unlock(void)
{
	int stat;

	ASSERT(cur_thread->lock_count > 0);

	interrupt_save(&stat);
	interrupt_disable();

	if (cur_thread->lock_count == 1) {
		wakeq_flush();
		if (cur_thread->need_resched) {
			sched_switch();
			cur_thread->need_resched = 0;
		}
	}
	cur_thread->lock_count--;

	interrupt_restore(stat);
}

/*
 * Fill the scheduling statistics into the specified buffer.
 */
void sched_stat(struct stat_sched *ss)
{
	ss->system_ticks = timer_ticks();
	ss->idle_ticks = idle_thread.total_ticks;
}

void sched_init(void)
{
	int i;

	for (i = 0; i < NR_PRIO; i++)
		queue_init(&runq[i]);

	queue_init(&wakeq);
	top_prio = PRIO_IDLE;
	cur_thread->need_resched = 1;
}
