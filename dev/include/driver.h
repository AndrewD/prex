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
#include <sys/types.h>
#include <sys/errno.h>
#include <sys/list.h>
#include <prex/bootinfo.h>
#include <queue.h>
#include <drvlib.h>
#include <verbose.h>

/*
 * Kernel types
 */
typedef unsigned long	device_t;
typedef unsigned long	task_t;
typedef unsigned long	thread_t;
typedef unsigned long	irq_t;

#define DEVICE_NULL	((device_t)0)
#define TASK_NULL	((task_t)0)
#define IRQ_NULL	((irq_t)0)

/*
 * Driver structure
 *
 * "order" is initialize order which must be between 0 and 15.
 * The driver with order 0 is called first.
 */
struct driver {
	const char	*name;		/* name of device driver */
	const int	order;		/* initialize order */
	int		(*init)(void);	/* initialize routine */
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
	int (*ioctl)	(device_t dev, u_long arg, void *);
	int (*event)	(int event);
};

/*
 * Flags for device_create()
 */
#define DF_CHR		0x00000001	/* character device */
#define DF_BLK		0x00000002	/* block device */
#define DF_RDONLY	0x00000004	/* read only device */
#define DF_REM		0x00000008	/* removable device */

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
#define IPL_NONE	0	/* nothing (lowest) */
#define IPL_COMM	1	/* serial, parallel */
#define IPL_BLOCK	2	/* FDD, IDE */
#define IPL_NET		3	/* network */
#define IPL_DISPLAY	4	/* screen */
#define IPL_INPUT	5	/* keyboard, mouse */
#define IPL_AUDIO	6	/* audio */
#define IPL_BUS		7	/* USB, PCCARD */
#define IPL_RTC		8	/* RTC Alarm */
#define IPL_PROFILE	9	/* profiling timer */
#define IPL_CLOCK	10	/* system Clock Timer */
#define IPL_HIGH	11	/* everything */

#define NIPL		12

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
 */
struct dpc {
	struct queue	link;		/* linkage on DPC queue */
	int		state;
	void		(*func)(void *); /* call back routine */
	void		*arg;		/* argument to pass */
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
	struct list	link;		/* linkage on timer chain */
	int		active;		/* true if active */
	u_long		expire;		/* expire time (ticks) */
	u_long		interval;	/* time interval */
	void		(*func)(void *); /* function to call */
	void		*arg;		/* function argument */
	struct event	event;		/* event for this timer */
};

#define timer_init(tmr)     (tmr)->expire = 0;

/* Items for debug_dump */
#define DUMP_THREAD	1
#define DUMP_TASK	2
#define DUMP_VM		3
#define DUMP_KSYM	4

/* State for machine_setpower */
#define POW_SUSPEND	1
#define POW_OFF		2

__BEGIN_DECLS
device_t device_create(const struct devio *io, const char *name, int flags);
int	 device_destroy(device_t dev);
int	 device_broadcast(int event, int force);
int	 umem_copyin(const void *uaddr, void *kaddr, size_t len);
int	 umem_copyout(const void *kaddr, void *uaddr, size_t len);
int	 umem_strnlen(const char *uaddr, size_t maxlen, size_t *len);
void	*kmem_alloc(size_t size);
void	 kmem_free(void *ptr);
void	*kmem_map(void *addr, size_t size);
void	*page_alloc(size_t size);
void	 page_free(void *paddr, size_t size);
int	 page_reserve(void *paddr, size_t size);
irq_t	 irq_attach(int irqno, int level, int shared, int (*isr)(int), void (*ist)(int));
void	 irq_detach(irq_t irq);
void	 irq_lock(void);
void	 irq_unlock(void);
void	 timer_callout(struct timer *tmr, u_long msec, void (*func)(void *), void *arg);
void	 timer_stop(struct timer *tmr);
u_long	 timer_delay(u_long msec);
u_long	 timer_count(void);
int	 timer_hook(void (*func)(int));
void	 sched_lock(void);
void	 sched_unlock(void);
int	 sched_tsleep(struct event *evt, u_long timeout);
void	 sched_wakeup(struct event *evt);
thread_t sched_wakeone(struct event *evt);

void	 sched_dpc(struct dpc *dpc, void (*func)(void *), void *arg);
void	 sched_yield(void);
#define	 sched_sleep(event)  sched_tsleep((event), 0)

#ifdef cur_thread
#define thread_self cur_thread
#else
thread_t thread_self(void);
#endif

int	 exception_post(task_t task, int exc);
int	 task_capable(int cap);
void	*phys_to_virt(void *p_addr);
void	*virt_to_phys(void *v_addr);
void	 machine_reset(void);
void	 machine_idle(void);
void	 machine_setpower(int state);
void	 machine_bootinfo(struct bootinfo **);
void	 debug_attach(void (*func)(char *));
int	 debug_dump(int index);
void	 printf(const char *fmt, ...);
void	 panic(const char *msg) __noreturn;
#ifdef DEBUG
void	 assert(const char *file, int line, const char *exp);
#define ASSERT(exp) do { if (!(exp)) \
			     assert(__FILE__, __LINE__, #exp); } while (0)
#else
#define ASSERT(exp) do {} while (0)
#endif
void	driver_main(void);
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

/* useful macros to provide information to optimiser */
#define likely(x) __builtin_expect((!!(x)),1)
#define unlikely(x) __builtin_expect((!!(x)),0)

/*
 * Useful macros
 */
#define ARRAY_SIZE(x) (sizeof(x) / sizeof((x)[0]))

/* REVIST: this has a hard coded timeout and isn't safe (race between
 * condition test and sleep) but it's a start */
#define SLP_TIMEOUT 2
#define wait_event_interruptible(event, condition)			\
	({								\
		int __ret = 0, __ctr = 100;				\
		while (!(condition) && __ctr-- > 0) {			\
			__ret = sched_tsleep(&event, 10);		\
			if (__ret != SLP_TIMEOUT)			\
				break;					\
			if (condition) {				\
				__ret = 0;				\
				break;					\
			}						\
		}							\
		(__ret != 0) ? -ETIMEDOUT : 0;				\
	})

/* spin until condition true or timeout expires */
#define spin_condition(condition, timeout)				\
	({								\
		long __max = timeout;					\
		long __rem = __max;					\
		u_long __until = timer_count() + __max;			\
		while (!(condition)) {					\
			__rem = (long)__until - (long)timer_count();	\
			if (__rem < 0)					\
				break;					\
			sched_yield();					\
		}							\
		(__rem < 0) ? -ETIMEDOUT : __max - __rem;		\
	})

/*
 * simple device driver locking mechanism
 */
#define DEVLOCK_DBG(s) __VERBOSE(VB_TRACE, "(%d %04x %04x)" s, m->free, \
				 (uint16_t)m->owner, (uint16_t)thread_self());

struct devlock {
	struct event	event;
	int		free;
	thread_t	owner;		/* owner thread of this lock */
};

#define devlock_init(m, name) \
	do { event_init(&(m)->event, name); (m)->free = 1; } while (0)

/*
 * leaves the scheduler locked as there is no
 * priority inheritance in these light-weight locks
 */
static inline int devlock_lock(struct devlock *m)
{

	sched_lock();
	if (--m->free < 0) {
		DEVLOCK_DBG("S ");
		ASSERT(m->owner != thread_self()); /* deadlock */

		int rc = sched_sleep(&m->event);
		switch(rc) {
		case 0:
			/* m->owner set by devlock_unlock() */
			DEVLOCK_DBG("L\n");
			break;

		case SLP_INTR:
			m->free++;
			sched_unlock();
			return DERR(EINTR);

		default:
			ASSERT(0); /* only expect SLP_INTR */
		}
	} else			/* was free */
		m->owner = thread_self();

	/* do not unlock scheduler */
	return 0;
}

static inline void devlock_unlock(struct devlock *m)
{

	/* scheduler still locked from devlock_lock() */

	if (++m->free <= 0) {
		m->owner = sched_wakeone(&m->event);
		DEVLOCK_DBG("W ");
	} else {
		ASSERT(m->free == 1); /* must be unlocked */
		/* no need to NULL m->owner when unlocked */
	}

	sched_unlock();
}

#endif /* !_DRIVER_H */
