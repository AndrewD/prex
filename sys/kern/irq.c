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
 * irq.c - interrupt request management routines.
 */

/**
 * We define the following two different types of interrupt
 * services in order to improve real-time performance.
 *
 * - Interrupt Service Routine (ISR)
 *
 *  ISR is started by an actual hardware interrupt. The associated
 *  interrupt is disabled in Interrupt Control Unit (ICU), and CPU
 *  interrupt is enabled while ISR runs.
 *  If ISR determines that the corresponding device generated the
 *  interrupt, ISR must program the device to stop that interrupt.
 *  Then, ISR should do minimum I/O operation and return control
 *  as quickly as possible. ISR will run within a context of the
 *  thread running when interrupt occurs. So, only few kernel
 *  services are available within ISR. We can use irq_level value
 *  to detect the illegal call from ISR code.
 *
 * - Interrupt Service Thread (IST)
 *
 *  IST is automatically activated if ISR returns INT_CONTINUE. It
 *  will be called when the system enters safer condition than ISR.
 *  A device driver should use IST to do heavy I/O operation as much
 *  as possible. Since ISR for the same IRQ line may be invoked
 *  during IST, the shared data, resources, and device registers
 *  must be synchronized by using irq_lock(). IST does not have to
 *  be reentrant because it is not interrupted by same IST itself.
 */

#include <kernel.h>
#include <event.h>
#include <kmem.h>
#include <sched.h>
#include <thread.h>
#include <irq.h>

/* forward declarations */
static void irq_thread(u_long);

static struct irq *irq_table[NIRQS];	/* IRQ descriptor table */
static volatile int nr_irq_locks;	/* lock count for interrupt */
static volatile int saved_irq_state;	/* IRQ state saved by irq_lock() */

/*
 * irq_attach - attach ISR and IST to the specified interrupt.
 * @vector: interrupt vector number
 * @prio:   interrupt priority level.
 * @shared: true if it allows the IRQ sharing.
 * @isr:    pointer to the interrupt service routine.
 * @ist:    pointer to the interrupt service thread. NULL for no ist.
 *
 * irq_attach() returns irq handle which is needed for irq_detach().
 * Or, it returns -1 if it failed.
 * The interrupt of attached irq will be unmasked (enabled) in this
 * routine.
 *
 * TODO: Interrupt sharing is not supported, for now.
 */
int
irq_attach(int vector, int prio, int shared, int (*isr)(int),
	   void (*ist)(int))
{
	struct irq *irq;
	thread_t th;
	int mode;

	ASSERT(irq_level == 0);
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
		 * Create a new thread for IST.
		 */
		th = kernel_thread(ISTPRIO(prio), irq_thread, (u_long)irq);
		if (th == NULL)
			panic("irq_attach");
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
void
irq_detach(int handle)
{
	struct irq *irq = (struct irq *)handle;

	ASSERT(irq_level == 0);
	ASSERT(irq);
	ASSERT(irq->vector < NIRQS);

	irq_lock();
	interrupt_mask(irq->vector);
	irq_unlock();

	irq_table[irq->vector] = NULL;
	if (irq->thread != NULL)
		thread_kill(irq->thread);
	kmem_free(irq);
}

/*
 * Lock IRQ.
 *
 * All H/W interrupts are masked off.
 * Caller is no need to save the interrupt state before irq_lock()
 * because it is automatically restored in irq_unlock() when no one
 * is locking the IRQ anymore.
 */
void
irq_lock(void)
{
	int s;

	interrupt_save(&s);
	interrupt_disable();
	if (++nr_irq_locks == 1)
		saved_irq_state = s;
}

/*
 * Unlock IRQ.
 *
 * If lock count becomes 0, the interrupt is restored to original
 * state at first irq_lock() call.
 */
void
irq_unlock(void)
{
	ASSERT(nr_irq_locks > 0);

	if (--nr_irq_locks == 0)
		interrupt_restore(saved_irq_state);
}

/*
 * Interrupt service thread.
 * This is a common dispatcher to all interrupt threads.
 */
static void
irq_thread(u_long arg)
{
	int vec;
	void (*func)(int);
	struct irq *irq;

	interrupt_enable();

	irq = (struct irq *)arg;
	func = irq->ist;
	vec = irq->vector;

	for (;;) {
		interrupt_disable();
		if (irq->ist_request <= 0) {
			/*
			 * Since the interrupt is disabled above, an
			 * interrupt for this vector keeps pending until
			 * this thread enters sleep state. Thus, we don't
			 * lose any IST requests even if the interrupt
			 * is fired here.
			 */
			sched_sleep(&irq->ist_event);
		}
		irq->ist_request--;
		ASSERT(irq->ist_request >= 0);
		interrupt_enable();

		/* Call IST */
		(func)(vec);
	}
	/* NOTREACHED */
}

/*
 * Interrupt handler.
 *
 * This routine will call the corresponding ISR for the requested
 * interrupt vector. This routine is called from the code in the
 * architecture dependent layer. We assumes the scheduler is already
 * locked by caller.
 */
void
irq_handler(int vector)
{
	struct irq *irq;
	int rc;

	irq = irq_table[vector];
	if (irq == NULL)
		return;		/* Ignore stray interrupt */
	ASSERT(irq->isr);

	/* Call ISR */
	rc = (irq->isr)(vector);

	if (rc == INT_CONTINUE) {
		/* Kick IST */
		ASSERT(irq->ist);
		irq->ist_request++;
		sched_wakeup(&irq->ist_event);
	}
	irq->count++;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void
irq_dump(void)
{
	int vector;
	struct irq *irq;

	printk("IRQ dump:\n");
	printk(" vector isr      ist      prio     count\n");
	printk(" ------ -------- -------- -------- --------\n");

	for (vector = 0; vector < NIRQS; vector++) {
		irq = irq_table[vector];
		if (irq) {
			printk("   %4d %08x %08x      %3d %8d\n",
			       vector, irq->isr, irq->ist,
			       (irq->thread ? irq->thread->prio : 0),
			       irq->count);
		}
	}
}
#endif

void
irq_init(void)
{
	/*
	 * Start interrupt processing.
	 */
	interrupt_enable();
}
