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
 * driver.h - Kernel Interface for device driver
 */

#ifndef _DRIVER_H
#define _DRIVER_H

#include <conf/config.h>
#include <sys/cdefs.h>
#include <sys/param.h>
#include <sys/null.h>
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/list.h>
#include <prex/bootinfo.h>
#include <queue.h>
#include <drvlib.h>

/*
 * Kernel types
 */
typedef unsigned long	device_t;
typedef unsigned long	task_t;

#define NULL_DEVICE	((device_t)0)
#define NULL_TASK	((task_t)0)

/*
 * Driver structure
 *
 * "order" is initialize order which must be between 0 and 15.
 * The driver with order 0 is called first.
 */
struct driver {
	const char	*name;		/* Name of device driver */
	const int	order;		/* Initialize order */
	int		(*init)(void);	/* Initialize routine */
};

#define __driver_entry __attribute__ ((section(".driver_table")))

/*
 * Device I/O table
 */
struct devio {
	int (*open)	(device_t dev, int mode);
	int (*close)	(device_t dev);
	int (*read)	(device_t dev, char *buf, size_t *nbyte, int blkno);
	int (*write)	(device_t dev, char *buf, size_t *nbyte, int blkno);
	int (*ioctl)	(device_t dev, int cmd, u_long arg);
	int (*event)	(int event);
};

/*
 * Flags for device_create()
 */
#define DF_CHR		0x00000001	/* Character device */
#define DF_BLK		0x00000002	/* Block device */
#define DF_RDONLY	0x00000004	/* Read only device */
#define DF_REM		0x00000008	/* Removable device */

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
#define INT_DONE	0
#define INT_ERROR	1
#define INT_CONTINUE	2

/*
 * Interrupt priority levels
 */
#define IPL_NONE	0	/* Nothing */
#define IPL_COMM	1	/* Serial, Parallel */
#define IPL_BLOCK	2	/* FDD, IDE */
#define IPL_NET		3	/* Network */
#define IPL_DISPLAY	4	/* Screen */
#define IPL_INPUT	5	/* Keyboard, Mouse */
#define IPL_AUDIO	6	/* Audio */
#define IPL_BUS		7	/* USB, PCCARD */
#define IPL_RTC		8	/* RTC Alarm */
#define IPL_PROFILE	9	/* Profiling timer */
#define IPL_CLOCK	10	/* System Clock Timer */
#define IPL_HIGH	11	/* Everything */

#define NIPL		12

/*
 * Event for sleep/wakeup
 */
struct event {
	struct queue	sleepq;		/* queue for waiting thread */
	const char	*name;		/* pointer to event name string */
};

#define EVENT_INIT(event, evt_name) \
    { {&(event).sleepq, &(event).sleepq}, evt_name}

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
 */
struct dpc {
	struct queue	link;		/* Linkage on DPC queue */
	int		state;
	void		(*func)(void *); /* Call back routine */
	void		*arg;		/* Argument to pass */
};

/*
 * Macro to convert milliseconds and tick.
 */
#define msec_to_tick(ms) (((ms) >= 0x20000) ? \
	    ((ms) / 1000UL) * HZ : \
	    ((ms) * HZ) / 1000UL)

#define tick_to_msec(tick) (((tick) * 1000) / HZ)

/*
 * Timer structure
 */
struct timer {
	struct list	link;		/* Linkage on timer chain */
	int		active;		/* True if active */
	u_long		expire;		/* Expire time (ticks) */
	u_long		interval;	/* Time interval */
	void		(*func)(void *); /* Function to call */
	void		*arg;		/* Function argument */
	struct event	event;		/* Event for this timer */
};

#define timer_init(tmr)     (tmr)->expire = 0;

/*
 * Items for debug_dump()
 */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_OBJECT	3
#define DUMP_TIMER	4
#define DUMP_IRQ	5
#define DUMP_DEVICE	6
#define DUMP_VM		7
#define DUMP_MSGLOG	8
#define DUMP_TRACE	9

__BEGIN_DECLS
device_t device_create(const struct devio *io, const char *name, int flags);
int	 device_destroy(device_t dev);
int	 device_broadcast(int event, int force);

int	 umem_copyin(void *uaddr, void *kaddr, size_t len);
int	 umem_copyout(void *kaddr, void *uaddr, size_t len);
int	 umem_strnlen(const char *uaddr, size_t maxlen, size_t *len);

void	*kmem_alloc(size_t size);
void	 kmem_free(void *ptr);
void	*kmem_map(void *addr, size_t size);

void	*page_alloc(size_t size);
void	 page_free(void *addr, size_t size);
int	 page_reserve(void *addr, size_t size);

int	 irq_attach(int irqno, int level, int shared, int (*isr)(int), void (*ist)(int));
void	 irq_detach(int handle);
void	 irq_lock(void);
void	 irq_unlock(void);

void	 timer_callout(struct timer *tmr, void (*func)(u_long), u_long arg, u_long msec);
void	 timer_stop(struct timer *tmr);
u_long	 timer_delay(u_long msec);
u_long	 timer_count(void);
int	 timer_hook(void (*func)(int));

void	 sched_lock(void);
void	 sched_unlock(void);
int	 sched_tsleep(struct event *evt, u_long timeout);
void	 sched_wakeup(struct event *evt);
void	 sched_dpc(struct dpc *dpc, void (*func)(void *), void *arg);
#define	 sched_sleep(event)  sched_tsleep((event), 0)

int	 exception_post(task_t task, int exc);
int	 task_capable(int cap);

void	*phys_to_virt(void *p_addr);
void	*virt_to_phys(void *v_addr);

void	 machine_reset(void);
void	 machine_idle(void);
void	 machine_bootinfo(struct boot_info **);

void	 debug_attach(void (*func)(char *));
int	 debug_dump(int index);

#ifdef DEBUG
void	 printk(const char *fmt, ...);
void	 panic(const char *fmt, ...) __attribute__((noreturn));
void	 assert(const char *file, int line, const char *exp);
#define ASSERT(exp) do { if (!(exp)) \
	assert(__FILE__, __LINE__, #exp); } while (0)
#else
#define printk(fmt...)  do {} while (0)
#define panic(fmt...)  do { for (;;) ; } while (0)
#define ASSERT(exp)
#endif
__END_DECLS

/* Export symbols for drivers. Place the symbol name in .kstrtab and a
 * struct kernel_symbol in the .ksymtab. The elf loader will use this
 * information to resolve these symbols in driver modules */
struct kernel_symbol
{
	u_long value;
	const char *name;
};

#define EXPORT_SYMBOL(sym)						\
	static const char __kstrtab_##sym[]				\
	__attribute__((section(".kstrtab")))				\
		= #sym;							\
	static const struct kernel_symbol __ksymtab_##sym		\
	__attribute__((__used__))					\
		__attribute__((section(".ksymtab"), unused))		\
		= { .value = (u_long)&sym, .name = __kstrtab_##sym }

#endif /* !_DRIVER_H */
