/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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

#ifndef _IRQ_H
#define _IRQ_H

#include <thread.h>

/*
 * IRQ descriptor
 */
struct irq {
	int		vector;		/* Vector number */
	int		(*isr)(int);	/* Pointer to ISR */
	void		(*ist)(int);	/* Pointer to IST */
	u_int		count;		/* Interrupt count */
	int		ist_request;	/* Number of IST request */
	thread_t	thread;		/* Thread ID of IST */
	struct event	ist_event;	/* Event for IST */
};

/*
 * Return values from ISR
 */
#define INT_DONE	0	/* Success */
#define INT_ERROR	1	/* Interrupt not handled */
#define INT_CONTINUE	2	/* Continue processing (Request IST) */

extern void irq_init(void);
extern int irq_attach(int irqno, int prio, int shared,
		      int (*isr)(int), void (*ist)(int));
extern void irq_detach(int handle);
extern void irq_lock(void);
extern void irq_unlock(void);
extern void irq_handler(int irqno);

#endif /* !_IRQ_H */
