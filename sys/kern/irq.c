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
 * irq.c - interrupt request management routines.
 */

/*-
 * In order to boost the response time of real-time operation,
 * two different types of interrupt service are defined.
 *
 * - Interrupt Service Routine (ISR)
 *
 *  ISR is started by an actual hardware interrupt. The associated
 *  interrupt is disabled in Interrupt Control Unit (ICU), and CPU
 *  interrupt is enabled while ISR runs.
 *  If ISR determines that the corresponding device generated the
 *  interrupt, ISR must program the device to stop that interrupt.
 *  Then, ISR should do minimum I/O operation and return control
 *  as quickly as possible for better overall system performance.
 *  ISR will run within the context of current running thread at
 *  interrupt time. So, only few kernel service is available within
 *  ISR. IRQ_ASSERT() macro can be used to catch the invalid function
 *  call from ISR.
 *
 * - Interrupt Service Thread (IST)
 *
 *  IST is automatically activated if ISR returns INT_CONTINUE. It
 *  will be called when the system enters safer condition than ISR.
 *  Any interrupt driven I/O operation should be done in IST if
 *  possible.
 *  Since ISR for same IRQ may be run during IST, the shared data,
 *  resources, and device registers must be synchronized by using
 *  irq_lock(). IST does not have to be reentrant, since it is not
 *  interrupted by same IST itself.
 */

#include <kernel.h>
#include <event.h>
#include <kmem.h>
#include <sched.h>
#include <thread.h>
#include <irq.h>

/*
 * IRQ descriptor
 */
struct irq {
	int	vector;		/* Vector number */
	int	(*isr)(int);	/* Pointer to ISR */
	void	(*ist)(int);	/* Pointer to IST */
	u_int	count;		/* Interrupt count */
	int	ist_request;	/* Number of IST request */
	thread_t thread;	/* Thread ID of IST */
	struct event ist_event;	/* Event for IST */
};

/* Forward functions */
static void irq_thread(u_long);

/* IRQ descriptor table */
static struct irq *irq_table[NR_IRQS];

/* Lock count for interrupt */
static volatile int nr_irq_lock = 0;

/* Interrupt state saved by irq_lock() */
static volatile int saved_irq_state;

/*
 * Attach ISR and IST to the specified interrupt.
 *
 * @vector: Interrupt vector number
 * @prio:   Interrupt priority level. The smaller level is higher 
 *          priority for interrupt processing.
 * @shared: True if it allows the IRQ sharing.
 * @isr:    Pointer to the interrupt service routine.
 * @ist:    Pointer to the interrupt service thread.
 *
 * The interrupt of attached irq will be unmasked (enabled) in this
 * routine. If the device does not use IST, its pointer must be
 * set as NULL for irq_attach().
 *
 * irq_attach() returns irq handle which is needed for irq_detach().
 * Or, it returns -1 if it failed.
 *
 * TODO: Interrupt sharing is not supported now.
 */
int irq_attach(int vector, int prio, int shared,
	       int (*isr)(int), void (*ist)(int))
{
	struct irq *irq;
	thread_t th;
	int mode;

	IRQ_ASSERT();
	ASSERT(isr != NULL);

	sched_lock();
	if ((irq = kmem_alloc(sizeof(struct irq))) == NULL) {
		sched_unlock();
		return -1;
	}
	memset(irq, 0, sizeof(struct irq));
	irq->vector = vector;
	irq->isr = isr;
	irq->ist = ist;

	if (ist != NULL) {
		/*
		 * Create new thread for IST.
		 */
		if ((th = kernel_thread(irq_thread, (u_long)irq)) == NULL) 
			panic("Failed to create interrupt thread");
		sched_setpolicy(th, SCHED_FIFO);
		sched_setprio(th, PRIO_INTERRUPT + prio, 
			      PRIO_INTERRUPT + prio);
		sched_resume(th);
		irq->thread = th;
		event_init(&irq->ist_event, "interrupt");
	}
	irq_table[vector] = irq;
	mode = shared ? IMODE_LEVEL : IMODE_EDGE;

	irq_lock();
	interrupt_setup(vector, mode);
	interrupt_unmask(vector, prio);
	irq_unlock();

	sched_unlock();
	printk("IRQ%d attached priority=%d\n", vector, prio);
	return (int)irq;
}

/*
 * Detach an interrupt handler from the interrupt chain.
 * The detached interrupt will be masked off if nobody attaches
 * to it, anymore.
 */
void irq_detach(int handle)
{
	struct irq *irq = (struct irq *)handle;

	IRQ_ASSERT();
	ASSERT(irq);
	ASSERT(irq->vector < NR_IRQS);

	irq_lock();
	interrupt_mask(irq->vector);
	irq_unlock();
	irq_table[irq->vector] = NULL;
	__thread_terminate(irq->thread);
	kmem_free(irq);
	return;
}

/*
 * Lock IRQ.
 *
 * All H/W interrupts are masked off.
 * Caller is no need to save the interrupt state before irq_lock()
 * because it is automatically restored in irq_unlock() when no one
 * is locking the IRQ anymore.
 */
void irq_lock(void)
{
	int stat;

	interrupt_save(&stat);
	interrupt_disable();
	if (++nr_irq_lock == 1)
		saved_irq_state = stat;
}

/*
 * Unlock IRQ.
 *
 * If lock count becomes 0, the interrupt is restored to original
 * state at first irq_lock() call.
 */
void irq_unlock(void)
{
	ASSERT(nr_irq_lock > 0);

	if (--nr_irq_lock == 0)
		interrupt_restore(saved_irq_state);
}

/*
 * Interrupt service thread.
 * This is a common dispatcher to all interrupt threads.
 */
static void irq_thread(u_long arg)
{
	int vector;
	void (*ist)(int);
	struct irq *irq;

	interrupt_enable();

	irq = (struct irq *)arg;
	ist = irq->ist;
	vector = irq->vector;

 next:
	interrupt_disable();
	if (irq->ist_request <= 0) {
		/*
		 * Since the interrupt is disabled above, an interrupt
		 * for this vector keeps pending until completion
		 * of the thread switch in sched_sleep below.
		 * This is important not to lose any IST request
		 * from ISR even if the interrupt is fired here.
		 * Hint: ISR can not wake the current _running_ IST.
		 */
		sched_sleep(&irq->ist_event);
	}
	irq->ist_request--;
	ASSERT(irq->ist_request >= 0);
	interrupt_enable();
	
	/* Call IST */
	(ist)(vector);
	goto next;
}

/*
 * Interrupt handler.
 *
 * This routine will call the corresponding ISR for the requested
 * interrupt vector. This routine is called from the code in the 
 * architecture dependent layer. It assumes the scheduler is already
 * locked by caller.
 */
void irq_handler(int vector)
{
	struct irq *irq;
	int result;

	irq = irq_table[vector];
	if (irq == NULL) {
		printk("Stray interrupt! irq%d\n", vector);
		return;
	}
	ASSERT(irq->isr);

	/* Call ISR */
	result = (irq->isr)(vector);
	if (result == INT_CONTINUE) {
		ASSERT(irq->ist);
		irq->ist_request++;

		/* Run IST */
		sched_wakeup(&irq->ist_event);
	}
	irq->count++;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void irq_dump(void)
{
	int vec;
	struct irq *irq;

	printk("IRQ dump:\n");
	printk(" vector isr      ist      ist-thr  ist-prio count\n");
	printk(" ------ -------- -------- -------- -------- --------\n");

	for (vec = 0; vec < NR_IRQS; vec++) {
		irq = irq_table[vec];
		if (irq == NULL)
			continue;
		printk("   %4d %08x %08x %08x      %3d %8d\n",
		       vec, irq->isr, irq->ist, irq->thread,
		       (irq->thread ? irq->thread->prio : 0), irq->count);
	}
}
#endif

void irq_init(void)
{
	int i;

	for (i = 0; i < NR_IRQS; i++)
		irq_table[i] = NULL;
	interrupt_enable();
}
