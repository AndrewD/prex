/*
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
 * dki.h - Driver-Kernel Interface
 */

#ifndef _DKI_H
#define _DKI_H

#include <types.h>
#include <sys/queue.h>
#include <sys/dbgctl.h>
#include <sys/bootinfo.h>
#include <sys/ipl.h>
#include <sys/power.h>
#include <sys/capability.h>
#include <sys/device.h>

extern dkifn_t	*dki_table;		/* pointer to DKI function table */

/*
 * Device open mode
 */
#define DO_RDONLY	0x0
#define DO_WRONLY	0x1
#define DO_RDWR		0x2
#define DO_RWMASK	0x3

/*
 * Return value of ISR
 */
#define INT_DONE	0	/* done */
#define INT_ERROR	1	/* error */
#define INT_CONTINUE	2	/* continue to IST */

/* No IST for irq_attach() */
#define IST_NONE        ((void (*)(void *)) -1)

/*
 * Event for sleep/wakeup
 */
struct event {
	struct queue	sleepq;		/* queue for waiting thread */
	const char	*name;		/* pointer to event name string */
};

#define event_init(event, evt_name) \
    do { list_init(&(event)->sleepq); (event)->name = evt_name; } while (0)

/*
 * Sleep result
 */
#define SLP_SUCCESS	0
#define SLP_BREAK	1
#define SLP_TIMEOUT	2
#define SLP_INVAL	3
#define SLP_INTR	4

/*
 * DPC (Deferred Procedure Call) object
 * The data member is private to kernel.
 */
struct dpc {
	void	*_data[5];
};

typedef struct dpc	dpc_t;

/*
 * Timer structure
 * The data member is private to kernel.
 */
struct timer {
	void	*_data[10];
};

typedef struct timer	timer_t;

__BEGIN_DECLS
device_t device_create(struct driver *, const char *, int);
int	 device_destroy(device_t);
device_t device_lookup(const char *);
int	 device_control(device_t, u_long, void *);
int	 device_broadcast(u_long, void *, int);
void	*device_private(device_t);

int	 copyin(const void *, void *, size_t);
int	 copyout(const void *, void *, size_t);
int	 copyinstr(const void *, void *, size_t);

void	*kmem_alloc(size_t);
void	 kmem_free(void *);
void	*kmem_map(void *, size_t);

paddr_t	 page_alloc(psize_t);
void	 page_free(paddr_t, psize_t);
void	 page_reserve(paddr_t, psize_t);

irq_t	 irq_attach(int, int, int, int (*)(void *), void (*)(void *), void *);
void	 irq_detach(irq_t);

int	 spl0(void);
int	 splhigh(void);
void	 splx(int);

void	 timer_callout(timer_t *, u_long, void (*)(void *), void *);
void	 timer_stop(timer_t *);
u_long	 timer_delay(u_long);
u_long	 timer_ticks(void);

void	 sched_lock(void);
void	 sched_unlock(void);
int	 sched_tsleep(struct event *, u_long);
void	 sched_wakeup(struct event *);
void	 sched_dpc(struct dpc *, void (*)(void *), void *);
#define	 sched_sleep(event)  sched_tsleep((event), 0)

int	 task_capable(cap_t);
int	 exception_post(task_t, int);
void	 machine_bootinfo(struct bootinfo **);
void	 machine_powerdown(int);
int	 sysinfo(int, void *);
void	 panic(const char *);
void	 printf(const char *, ...);
void	 dbgctl(int, void *);
__END_DECLS

#endif /* !_DKI_H */
