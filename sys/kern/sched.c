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

/**
 * General design:
 *
 * The Prex scheduler is based on the algorithm known as priority based
 * multi level queue. Each thread is assigned the priority between
 * 0 and 255. The lower number means higher priority like BSD unix.
 * The scheduler maintains 256 level run queues mapped to each priority.
 * The lowest priority (=255) is used only by an idle thread.
 *
 * All threads have two different types of priorities:
 *
 *  - Base priority
 *      This is a static priority used for priority computation. A user
 *      mode program can change this value via system call.
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
 *      Thread is blocked for sleep or suspend.
 *      It is put on the tail of the run queue when it becomes
 *      runnable again.
 *
 *  2) Preemption
 *      If higher priority thread becomes runnable, the current
 *      thread is put on the the _head_ of the run queue.
 *
 *  3) Quantum expiration
 *      If the thread consumes its time quantum, it is put on
 *      the tail of the run queue.
 *
 *  4) Yield
 *      If the thread releases CPU by itself, it is put on
 *      the tail of the run queue.
 *
 * There are following three types of scheduling policies.
 *
 *  - SCHED_FIFO   First in-first-out
 *  - SCHED_RR     Round robin (SCHED_FIFO + timeslice)
 *  - SCHED_OTHER  Another scheduling (not supported)
 */

#include <kernel.h>
#include <queue.h>
#include <event.h>
#include <irq.h>
#include <thread.h>
#include <timer.h>
#include <vm.h>
#include <task.h>
#include <system.h>
#include <sched.h>

static struct queue runq[NR_PRIOS];	/* run queues */
static struct queue wakeq;		/* queue for waking threads */
static struct queue dpcq;		/* queue for DPC threads */
static int top_prio;			/* highest priority in runq */

static struct event dpc_event;		/* event for dpc */

/*
 * Search for highest-priority runnable thread.
 */
static int
runq_top(void)
{
	int prio;

	for (prio = 0; prio < MIN_PRIO; prio++)
		if (!queue_empty(&runq[prio]))
			break;
	return prio;
}

/*
 * Put a thread on the tail of the run queue.
 * The rescheduling flag is set if the priority is better than
 * the currently running process.
 */
static void
runq_enqueue(thread_t th)
{

	enqueue(&runq[th->prio], &th->link);
	if (th->prio < top_prio) {
		top_prio = th->prio;
		cur_thread->need_resched = 1;
	}
}

/*
 * Insert a thread to the head of the run queue.
 * We don't change rescheduling flag here because this is called
 * while thread switching.
 */
static void
runq_insert(thread_t th)
{

	queue_insert(&runq[th->prio], &th->link);
	if (th->prio < top_prio)
		top_prio = th->prio;
}

/*
 * Pick up and remove the highest-priority thread from the run
 * queue. At least, an idle thread will be returned because it
 * always residents in the lowest priority queue.
 */
static thread_t
runq_dequeue(void)
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
 * Remove the specified thread from the run queue.
 */
static void
runq_remove(thread_t th)
{

	queue_remove(&th->link);
	top_prio = runq_top();
}

/*
 * Process all pending woken threads.
 * Rescheduling flag may be set.
 * Note: The thread may be still in a suspend state after wakeup.
 */
static void
wakeq_flush(void)
{
	queue_t q;
	thread_t th;

	while (!queue_empty(&wakeq)) {
		/*
		 * Set a thread runnable.
		 */
		q = dequeue(&wakeq);
		th = queue_entry(q, struct thread, link);
		th->sleep_event = 0;
		th->state &= ~TH_SLEEP;

		if (th != cur_thread && th->state == TH_RUN)
			runq_enqueue(th);
	}
}

/*
 * sleep_expire - sleep timer is expired:
 * @arg: thread to unsleep.
 *
 * Wake up the passed thread that is sleeping in sched_tsleep().
 */
static void
sleep_expire(void *arg)
{

	sched_unsleep((thread_t)arg, SLP_TIMEOUT);
}

/*
 * sched_switch - This routine is called to reschedule the CPU.
 *
 * If the scheduling reason is preemption, the current thread
 * will remain at the head of the run queue. So, the thread
 * still has right to run first again among the same priority
 * threads. For other scheduling reason, the current thread is
 * inserted into the tail of the run queue.
 */
static void
sched_switch(void)
{
	thread_t prev, next;

	ASSERT(irq_level == 0);

	prev = cur_thread;
	if (prev->state == TH_RUN) {
		if (prev->prio > top_prio)
			runq_insert(prev);	/* preemption */
		else
			runq_enqueue(prev);
	}
	prev->need_resched = 0;

	/*
	 * This is the scheduler proper.
	 */
	next = runq_dequeue();
	if (next == prev)
		return;
	cur_thread = next;

	if (prev->task != next->task)
		vm_switch(next->task->map);

	context_switch(&prev->context, &next->context);
}

/*
 * sched_tsleep - sleep the current thread until a wakeup is
 * performed on the specified event.
 * @timeout: time out value in msec. (0 means no timeout)
 *
 * This routine returns a sleep result. If the thread is woken
 * by sched_wakeup()/sched_wakeone(), it returns 0. Otherwise,
 * it will return the result value which is passed by sched_unsleep().
 * We allow calling sched_sleep() with interrupt disabled.
 *
 * sched_sleep() is also defined as a wrapper macro for sched_tsleep()
 * without timeout.
 * Note that all sleep requests are interruptible with this kernel.
 */
int
sched_tsleep(struct event *evt, u_long timeout)
{
	int s;

	ASSERT(irq_level == 0);
	ASSERT(evt);

	sched_lock();
	interrupt_save(&s);
	interrupt_disable();

	cur_thread->sleep_event = evt;
	cur_thread->state |= TH_SLEEP;
	enqueue(&evt->sleepq, &cur_thread->link);

	if (timeout != 0) {
		timer_callout(&cur_thread->timeout, sleep_expire,
			      cur_thread, timeout);
	}
	wakeq_flush();
	sched_switch();	/* Sleep here. Zzzz.. */

	interrupt_restore(s);
	sched_unlock();
	return cur_thread->sleep_result;
}

/*
 * sched_wakeup - wake up all threads sleeping on event.
 *
 * A thread can have sleep and suspend state simultaneously.
 * So, the thread does not always run even if it woke up.
 *
 * Since this routine can be called from ISR at interrupt level, it
 * should not touch any data of runq. Otherwise, we must frequently
 * disable interrupts while accessing runq. Thus, this routine will
 * temporary move the waking thread into wakeq, and the thread is
 * moved to runq at more safer time in wakeq_flush().
 *
 * The woken thread will be put on the tail of runq regardless
 * of its policy. If woken threads have same priority, next running
 * thread is selected by FIFO order.
 */
void
sched_wakeup(struct event *evt)
{
	queue_t q;
	thread_t th;

	irq_lock();
	while (!queue_empty(&evt->sleepq)) {
		/*
		 * Move a sleeping thread to the wake queue.
		 */
		q = dequeue(&evt->sleepq);
		th = queue_entry(q, struct thread, link);
		th->sleep_result = 0;
		enqueue(&wakeq, q);
		timer_stop(&th->timeout);
	}
	irq_unlock();
}

/*
 * sched_wakeone - wake up one thread sleeping for the event.
 *
 * The highest priority thread is woken among sleeping threads.
 * sched_wakeone() returns the thread ID of the woken thread, or
 * NULL if no threads are sleeping.
 */
thread_t
sched_wakeone(struct event *evt)
{
	queue_t head, q;
	thread_t top = NULL, th;

	irq_lock();
	head = &evt->sleepq;
	if (!queue_empty(head)) {
		/*
		 * Select the highet priority thread in
		 * the sleeping threads, and move it to
		 * the wake queue.
		 */
		q = queue_first(head);
		top = queue_entry(q, struct thread, link);
		while (!queue_end(head, q)) {
			th = queue_entry(q, struct thread, link);
			if (th->prio < top->prio)
				top = th;
			q = queue_next(q);
		}
		top->sleep_result = 0;
		queue_remove(&top->link);
		enqueue(&wakeq, &top->link);
		timer_stop(&top->timeout);
	}
	irq_unlock();
	return top;
}

/*
 * sched_unsleep - cancel sleep.
 *
 * sched_unsleep() removes the specified thread from its sleep
 * queue. The specified sleep result will be passed to the sleeping
 * thread as a return value of sched_tsleep().
 */
void
sched_unsleep(thread_t th, int result)
{
	sched_lock();
	if (th->state & TH_SLEEP) {
		irq_lock();
		queue_remove(&th->link);
		th->sleep_result = result;
		enqueue(&wakeq, &th->link);
		timer_stop(&th->timeout);
		irq_unlock();

	}
	sched_unlock();
}

/*
 * Yield the current processor to another thread.
 *
 * If a thread switching occurs, the current thread will be moved
 * on the tail of the run queue regardless of its policy.
 * Note that the current thread may run immediately again, if no
 * other thread exists in the same priority queue.
 */
void
sched_yield(void)
{
	ASSERT(irq_level == 0);

	sched_lock();

	if (!queue_empty(&runq[cur_thread->prio]))
		cur_thread->need_resched = 1;

	sched_unlock();	/* Switch current thread here */
}

/*
 * Suspend the specified thread.
 * The scheduler must be locked before calling this routine.
 * Note that the suspend count is handled in thread_suspend().
 */
void
sched_suspend(thread_t th)
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
void
sched_resume(thread_t th)
{
	ASSERT(cur_thread->lock_count > 0);

	if (th->state & TH_SUSPEND) {
		th->state &= ~TH_SUSPEND;
		if (th->state == TH_RUN)
			runq_enqueue(th);
	}
}

/*
 * sched_tick() is called from timer_tick() once every tick.
 * Check quantum expiration, and mark a rescheduling flag.
 * We don't need locking in here.
 */
void
sched_tick(void)
{

	cur_thread->total_ticks++;

	if (cur_thread->policy == SCHED_RR) {
		if (--cur_thread->ticks_left <= 0) {
			cur_thread->ticks_left = QUANTUM;
			cur_thread->need_resched = 1;
		}
	}
}

/*
 * Set up stuff for thread scheduling.
 */
void
sched_start(thread_t th)
{

	th->state = TH_RUN | TH_SUSPEND;
	th->policy = SCHED_RR;
	th->prio = PRIO_USER;
	th->base_prio = PRIO_USER;
	th->ticks_left = QUANTUM;
}

/*
 * Stop thread scheduling.
 */
void
sched_stop(thread_t th)
{
	ASSERT(irq_level == 0);
	ASSERT(cur_thread->lock_count > 0);

	if (th == cur_thread) {
		/*
		 * If specified thread is current thread, the
		 * scheduling lock count is force set to 1 to
		 * ensure the thread switching in the next
		 * sched_unlock().
		 */
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
 * sched_lock - lock the scheduler.
 *
 * The thread switch is disabled during scheduler locked. This
 * is mainly used to synchronize the thread execution to protect
 * global resources. Even when scheduler is locked, any interrupt
 * handler can run. So, we have to use irq_lock() to synchronize
 * a global data with ISR.
 *
 * Since the scheduling lock can be nested any number of times,
 * the caller has the responsible to unlock the same number of
 * locks.
 */
void
sched_lock(void)
{

	cur_thread->lock_count++;
}

/*
 * sched_unlock - unlock scheduler.
 *
 * If nobody locks the scheduler anymore, it runs pending wake
 * threads and check the reschedule flag. The thread switch is
 * invoked if the rescheduling request exists.
 *
 * Note that this routine will be called at the end of the
 * interrupt handler.
 */
void
sched_unlock(void)
{
	int s;

	ASSERT(cur_thread->lock_count > 0);

	interrupt_save(&s);
	interrupt_disable();

	if (cur_thread->lock_count == 1) {
		wakeq_flush();
		while (cur_thread->need_resched) {

			/* Kick scheduler */
			sched_switch();

			/*
			 * Now we run pending interrupts which fired
			 * during the thread switch. So, we can catch
			 * the rescheduling request from such ISRs.
			 * Otherwise, the reschedule may be deferred
			 * until _next_ sched_unlock() call.
			 */
			interrupt_restore(s);
			interrupt_disable();
			wakeq_flush();
		}
	}
	cur_thread->lock_count--;

	interrupt_restore(s);
}

int
sched_getprio(thread_t th)
{

	return th->prio;
}

/*
 * sched_setprio - set priority of thread.
 * @base: Base priority
 * @prio: Current priority
 *
 * Thread switch may be invoked here by priority change.
 * Called with scheduler locked.
 */
void
sched_setprio(thread_t th, int base, int prio)
{

	th->base_prio = base;

	if (th == cur_thread) {
		/*
		 * If we change the current thread's priority,
		 * rescheduling may be happened.
		 */
		th->prio = prio;
		top_prio = runq_top();
		if (prio != top_prio)
			cur_thread->need_resched = 1;
	} else {
		if (th->state == TH_RUN) {
			/*
			 * Update the thread priority and adjust the
			 * run queue position for new priority.
			 */
			runq_remove(th);
			th->prio = prio;
			runq_enqueue(th);
		} else
			th->prio = prio;
	}
}

int
sched_getpolicy(thread_t th)
{

	return th->policy;
}

int
sched_setpolicy(thread_t th, int policy)
{
	int err = 0;

	switch (policy) {
	case SCHED_RR:
	case SCHED_FIFO:
		th->ticks_left = QUANTUM;
		th->policy = policy;
		break;
	default:
		err = -1;
		break;
	}
	return err;
}

/*
 * DPC thread
 *
 * This is a kernel thread to process the pending call back request
 * within DPC queue. Each DPC routine is called with the following
 * conditions.
 *  - Interrupt is enabled.
 *  - Scheduler is unlocked.
 */
static void
dpc_thread(u_long unused)
{
	queue_t q;
	struct dpc *dpc;

	for (;;) {

		/* Wait until next DPC request. */
		sched_sleep(&dpc_event);

		while (!queue_empty(&dpcq)) {
			q = dequeue(&dpcq);
			dpc = queue_entry(q, struct dpc, link);
			dpc->state = DPC_FREE;

			interrupt_enable();
			(*dpc->func)(dpc->arg);
			interrupt_disable();
		}
	}
	/* NOTREACHED */
}

/*
 * Qeueue DPC (Deferred Procedure Call) request
 *
 * Call function at some later time in a DPC priority. This is
 * typically used by device drivers to do the low-priority jobs.
 * This routine can be called from ISR.
 */
void
sched_dpc(struct dpc *dpc, void (*func)(void *), void *arg)
{
	ASSERT(dpc);
	ASSERT(func);

	irq_lock();
	dpc->func = func;
	dpc->arg = arg;
	if (dpc->state != DPC_PENDING)
		enqueue(&dpcq, &dpc->link);
	dpc->state = DPC_PENDING;
	sched_wakeup(&dpc_event);
	irq_unlock();
}

/*
 * Initialize the global scheduler state.
 */
void
sched_init(void)
{
	int i;

	for (i = 0; i < NR_PRIOS; i++)
		queue_init(&runq[i]);

	queue_init(&wakeq);
	queue_init(&dpcq);
	event_init(&dpc_event, "dpc");
	top_prio = PRIO_IDLE;
	cur_thread->need_resched = 1;

	/*
	 * Create a DPC thread.
	 */
	if (kernel_thread(PRIO_DPC, dpc_thread, 0) == NULL)
		panic("sched_init");

	printk("Time slice is %d msec\n", CONFIG_TIME_SLICE);
}
