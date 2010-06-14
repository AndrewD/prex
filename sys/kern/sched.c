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
 * sched.c - scheduler
 */

/**
 * General Design:
 *
 * The Prex scheduler is based on the algorithm known as priority
 * based multi level queue. Each thread has its own priority assigned
 * between 0 and 255. The lower number means higher priority like BSD
 * UNIX.  The scheduler maintains 256 level run queues mapped to each
 * priority.  The lowest priority (=255) is used only for an idle
 * thread.
 *
 * All threads have two different types of priorities:
 *
 *  Base priority:
 *      This is a static priority used for priority computation.
 *      A user mode program can change this value via system call.
 *
 *  Current priority:
 *      An actual scheduling priority. A kernel may adjust this
 *      priority dynamically if it's needed.
 *
 * Each thread has one of the following state.
 *
 *  - TS_RUN     Running or ready to run
 *  - TS_SLEEP   Sleep for some event
 *  - TS_SUSP    Suspend count is not 0
 *  - TS_EXIT    Terminated
 *
 * The thread is always preemptive even in the kernel mode.
 * There are following 4 reasons to switch thread.
 *
 * (1) Block
 *      Thread is blocked for sleep or suspend.
 *
 * (2) Preemption
 *      Higher priority thread becomes runnable.
 *
 * (3) Quantum expiration
 *      The thread consumes its time quantum.
 *
 * (4) Yield
 *      The thread releases CPU by itself.
 *
 * There are following three types of scheduling policies.
 *
 *  - SCHED_FIFO   First in-first-out
 *  - SCHED_RR     Round robin (SCHED_FIFO + timeslice)
 *  - SCHED_OTHER  Not supported now
 */

#include <kernel.h>
#include <event.h>
#include <thread.h>
#include <timer.h>
#include <vm.h>
#include <task.h>
#include <sched.h>
#include <hal.h>

static struct queue	runq[NPRI];	/* run queues */
static struct queue	wakeq;		/* queue for waking threads */
static struct queue	dpcq;		/* DPC queue */
static struct event	dpc_event;	/* event for DPC */
static int		maxpri;		/* highest priority in runq */

/*
 * Search for highest-priority runnable thread.
 */
static int
runq_getbest(void)
{
	int pri;

	for (pri = 0; pri < MINPRI; pri++)
		if (!queue_empty(&runq[pri]))
			break;
	return pri;
}

/*
 * Put a thread on the tail of the run queue.
 * The rescheduling flag is set if the priority is beter
 * than the currently running thread.
 */
static void
runq_enqueue(thread_t t)
{

	enqueue(&runq[t->priority], &t->sched_link);
	if (t->priority < maxpri) {
		maxpri = t->priority;
		curthread->resched = 1;
	}
}

/*
 * Insert a thread to the head of the run queue.
 * We assume this routine is called while thread switching.
 */
static void
runq_insert(thread_t t)
{

	queue_insert(&runq[t->priority], &t->sched_link);
	if (t->priority < maxpri)
		maxpri = t->priority;
}

/*
 * Pick up and remove the highest-priority thread
 * from the run queue.
 */
static thread_t
runq_dequeue(void)
{
	queue_t q;
	thread_t t;

	q = dequeue(&runq[maxpri]);
	t = queue_entry(q, struct thread, sched_link);
	if (queue_empty(&runq[maxpri]))
		maxpri = runq_getbest();

	return t;
}

/*
 * Remove the specified thread from the run queue.
 */
static void
runq_remove(thread_t t)
{

	queue_remove(&t->sched_link);
	maxpri = runq_getbest();
}

/*
 * Wake up all threads in the wake queue.
 */
static void
wakeq_flush(void)
{
	queue_t q;
	thread_t t;

	while (!queue_empty(&wakeq)) {
		/*
		 * Set a thread runnable.
		 */
		q = dequeue(&wakeq);
		t = queue_entry(q, struct thread, sched_link);
		t->slpevt = NULL;
		t->state &= ~TS_SLEEP;
		if (t != curthread && t->state == TS_RUN)
			runq_enqueue(t);
	}
}

/*
 * Set the thread running:
 * Put a thread on the wake queue. This thread will be moved
 * to the run queue later in wakeq_flush().
 */
static void
sched_setrun(thread_t t)
{

	enqueue(&wakeq, &t->sched_link);
	timer_stop(&t->timeout);
}

/*
 * sched_swtch - this is the scheduler proper:
 *
 * If the scheduling reason is preemption, the current thread
 * will remain at the head of the run queue.  So, the thread
 * still has right to run next again among the same priority
 * threads. For other scheduling reason, the current thread is
 * inserted into the tail of the run queue.
 */
static void
sched_swtch(void)
{
	thread_t prev, next;

	/*
	 * Put the current thread on the run queue.
	 */
	prev = curthread;
	if (prev->state == TS_RUN) {
		if (prev->priority > maxpri)
			runq_insert(prev);	/* preemption */
		else
			runq_enqueue(prev);
	}
	prev->resched = 0;

	/*
	 * Select the thread to run the CPU next.
	 * If it's same with previous one, return.
	 */
	next = runq_dequeue();
	if (next == prev)
		return;
	curthread = next;

	/*
	 * Switch to the new thread.
	 * You are expected to understand this..
	 */
	if (prev->task != next->task)
		vm_switch(next->task->map);
	context_switch(&prev->ctx, &next->ctx);
}

/*
 * sleep_timeout - sleep timer is expired:
 *
 * Wake up the thread which is sleeping in sched_tsleep().
 */
static void
sleep_timeout(void *arg)
{
	thread_t t = (thread_t)arg;

	sched_unsleep(t, SLP_TIMEOUT);
}

/*
 * sched_tsleep - sleep the current thread until a wakeup
 * is performed on the specified event.
 * This routine returns a sleep result.
 */
int
sched_tsleep(struct event *evt, u_long msec)
{
	int s;

	ASSERT(evt != NULL);

	sched_lock();
	s = splhigh();

	/*
	 * Put the current thread on the sleep queue.
	 */
	curthread->slpevt = evt;
	curthread->state |= TS_SLEEP;
	enqueue(&evt->sleepq, &curthread->sched_link);

	/*
	 * Program timer to wake us up at timeout.
	 */
	if (msec != 0) {
		timer_callout(&curthread->timeout, msec,
			      &sleep_timeout, curthread);
	}

	wakeq_flush();
	sched_swtch();		/* Sleep here. Zzzz.. */

	splx(s);
	sched_unlock();
	return curthread->slpret;
}

/*
 * sched_wakeup - wake up all threads sleeping on event.
 *
 * A thread can have sleep and suspend state simultaneously.
 * So, the thread may keep suspending even if it woke up.
 */
void
sched_wakeup(struct event *evt)
{
	queue_t q;
	thread_t t;
	int s;

	ASSERT(evt != NULL);

	sched_lock();
	s = splhigh();
	while (!queue_empty(&evt->sleepq)) {
		q = dequeue(&evt->sleepq);
		t = queue_entry(q, struct thread, sched_link);
		t->slpret = 0;
		sched_setrun(t);
	}
	splx(s);
	sched_unlock();
}

/*
 * sched_wakeone - wake up one thread sleeping on event.
 *
 * The highest priority thread is woken among sleeping
 * threads. This routine returns the thread ID of the
 * woken thread, or NULL if no threads are sleeping.
 */
thread_t
sched_wakeone(struct event *evt)
{
	queue_t head, q;
	thread_t top, t = NULL;
	int s;

	sched_lock();
	s = splhigh();
	head = &evt->sleepq;
	if (!queue_empty(head)) {
		/*
		 * Select the highet priority thread in
		 * the sleep queue, and wake it up.
		 */
		q = queue_first(head);
		top = queue_entry(q, struct thread, sched_link);
		while (!queue_end(head, q)) {
			t = queue_entry(q, struct thread, sched_link);
			if (t->priority < top->priority)
				top = t;
			q = queue_next(q);
		}
		queue_remove(&top->sched_link);
		top->slpret = 0;
		sched_setrun(top);
	}
	splx(s);
	sched_unlock();
	return t;
}

/*
 * sched_unsleep - cancel sleep.
 *
 * sched_unsleep() removes the specified thread from its
 * sleep queue. The specified sleep result will be passed
 * to the sleeping thread as a return value of sched_tsleep().
 */
void
sched_unsleep(thread_t t, int result)
{
	int s;

	sched_lock();
	if (t->state & TS_SLEEP) {
		s = splhigh();
		queue_remove(&t->sched_link);
		t->slpret = result;
		sched_setrun(t);
		splx(s);
	}
	sched_unlock();
}

/*
 * Yield the current processor to another thread.
 *
 * Note that the current thread may run immediately again,
 * if no other thread exists in the same priority queue.
 */
void
sched_yield(void)
{

	sched_lock();

	if (!queue_empty(&runq[curthread->priority]))
		curthread->resched = 1;

	sched_unlock();		/* Switch a current thread here */
}

/*
 * Suspend the specified thread.
 * Called with scheduler locked.
 */
void
sched_suspend(thread_t t)
{

	if (t->state == TS_RUN) {
		if (t == curthread)
			curthread->resched = 1;
		else
			runq_remove(t);
	}
	t->state |= TS_SUSP;
}

/*
 * Resume the specified thread.
 * Called with scheduler locked.
 */
void
sched_resume(thread_t t)
{

	if (t->state & TS_SUSP) {
		t->state &= ~TS_SUSP;
		if (t->state == TS_RUN)
			runq_enqueue(t);
	}
}

/*
 * sched_tick() is called from timer_clock() once every tick.
 * Check quantum expiration, and mark a rescheduling flag.
 * We don't need locking in here.
 */
void
sched_tick(void)
{

	if (curthread->state != TS_EXIT) {
		/*
		 * Bill time to current thread.
		 */
		curthread->time++;

		if (curthread->policy == SCHED_RR) {
			if (--curthread->timeleft <= 0) {
				/*
				 * The quantum is up.
				 * Give the thread another.
				 */
				curthread->timeleft += QUANTUM;
				curthread->resched = 1;
			}
		}
	}
}

/*
 * Setup the thread structure to start scheduling.
 */
void
sched_start(thread_t t, int pri, int policy)
{

	t->state = TS_RUN | TS_SUSP;
	t->policy = policy;
	t->priority = pri;
	t->basepri = pri;
	if (t->policy == SCHED_RR)
		t->timeleft = QUANTUM;
}

/*
 * Stop scheduling of the specified thread.
 */
void
sched_stop(thread_t t)
{

	if (t == curthread) {
		/*
		 * If specified thread is a current thread,
		 * the scheduling lock count is force set
		 * to 1 to ensure the thread switching in
		 * the next sched_unlock().
		 */
		curthread->locks = 1;
		curthread->resched = 1;
	} else {
		if (t->state == TS_RUN)
			runq_remove(t);
		else if (t->state & TS_SLEEP)
			queue_remove(&t->sched_link);
	}
	timer_stop(&t->timeout);
	t->state = TS_EXIT;
}

/*
 * sched_lock - lock the scheduler.
 *
 * The thread switch is disabled during scheduler locked.
 * Since the scheduling lock can be nested any number of
 * times, the caller has the responsible to unlock the same
 * number of locks.
 */
void
sched_lock(void)
{

	curthread->locks++;
}

/*
 * sched_unlock - unlock scheduler.
 *
 * If nobody locks the scheduler anymore, it checks the
 * rescheduling flag and kick the scheduler if it's required.
 * This routine will be always called at the end of each
 * interrupt handler.
 */
void
sched_unlock(void)
{
	int s;

	ASSERT(curthread->locks > 0);

	s = splhigh();
	if (curthread->locks == 1) {
		wakeq_flush();
		while (curthread->resched) {
			/*
			 * Kick scheduler.
			 */
			sched_swtch();

			/*
			 * Now we run pending interrupts which fired
			 * during the thread switch. So, we can catch
			 * the rescheduling request from such ISRs.
			 * Otherwise, the reschedule may be deferred
			 * until _next_ sched_unlock() call.
			 */
			splx(s);
			s = splhigh();
			wakeq_flush();
		}
	}
	curthread->locks--;
	splx(s);
}

/*
 * Return the priority of the specified thread.
 */
int
sched_getpri(thread_t t)
{

	return t->priority;
}

/*
 * sched_setpri - set priority of thread.
 *
 * Arrange to reschedule if the resulting priority is
 * better than that of the current thread.
 * Called with scheduler locked.
 */
void
sched_setpri(thread_t t, int basepri, int pri)
{

	t->basepri = basepri;

	if (t == curthread) {
		/*
		 * If we change the current thread's priority,
		 * rescheduling may be happened.
		 */
		t->priority = pri;
		maxpri = runq_getbest();
		if (pri != maxpri)
			curthread->resched = 1;
	} else {
		if (t->state == TS_RUN) {
			/*
			 * Update the thread priority and adjust
			 * the run queue position for new priority.
			 * The rescheduling flag may be set.
			 */
			runq_remove(t);
			t->priority = pri;
			runq_enqueue(t);
		} else
			t->priority = pri;
	}
}

/*
 * Get the scheduling policy.
 */
int
sched_getpolicy(thread_t t)
{

	return t->policy;
}

/*
 * Set the scheduling policy.
 */
int
sched_setpolicy(thread_t t, int policy)
{
	int error = 0;

	switch (policy) {
	case SCHED_RR:
	case SCHED_FIFO:
		t->timeleft = QUANTUM;
		t->policy = policy;
		break;
	default:
		error = EINVAL;
		break;
	}
	return error;
}

/*
 * Schedule DPC callback.
 *
 * DPC (Deferred Procedure Call) is used to call the specific
 * function at some later time with a DPC priority.
 * This routine can be called from ISR.
 */
void
sched_dpc(struct dpc *dpc, void (*fn)(void *), void *arg)
{
	int s;

	ASSERT(dpc != NULL);
	ASSERT(fn != NULL);

	sched_lock();

	s = splhigh();
	dpc->func = fn;
	dpc->arg = arg;
	if (dpc->state != DPC_PENDING)
		enqueue(&dpcq, &dpc->link);
	dpc->state = DPC_PENDING;
	splx(s);

	sched_wakeup(&dpc_event);
	sched_unlock();
}

/*
 * DPC thread.
 *
 * This is a kernel thread to process the pending call back
 * request in DPC queue. Each DPC routine is called with
 * the following conditions.
 *  - Interrupt is enabled.
 *  - Scheduler is unlocked.
 *  - The scheduling priority is PRI_DPC.
 */
static void
dpc_thread(void *dummy)
{
	queue_t q;
	struct dpc *dpc;

	splhigh();

	for (;;) {
		/*
		 * Wait until next DPC request.
		 */
		sched_sleep(&dpc_event);

		while (!queue_empty(&dpcq)) {
			q = dequeue(&dpcq);
			dpc = queue_entry(q, struct dpc, link);
			dpc->state = DPC_FREE;

			/*
			 * Call DPC routine.
			 */
			spl0();
			(*dpc->func)(dpc->arg);
			splhigh();
		}
	}
	/* NOTREACHED */
}

/*
 * Initialize the global scheduler state.
 */
void
sched_init(void)
{
	thread_t t;
	int i;

	for (i = 0; i < NPRI; i++)
		queue_init(&runq[i]);

	queue_init(&wakeq);
	queue_init(&dpcq);
	event_init(&dpc_event, "dpc");
	maxpri = PRI_IDLE;
	curthread->resched = 1;

	t = kthread_create(dpc_thread, NULL, PRI_DPC);
	if (t == NULL)
		panic("sched_init");

	DPRINTF(("Time slice is %d msec\n", CONFIG_TIME_SLICE));
}
