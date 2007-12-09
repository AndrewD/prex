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

#include <config.h>
#include <types.h>
#include <errno.h>
#include <list.h>
#include <queue.h>
#include <bootinfo.h>

/*
 * Clock Rate
 */
#define HZ		CONFIG_HZ	/* Ticks per second */

/*
 * Page
 */
#define PAGE_SIZE	CONFIG_PAGE_SIZE	/* Bytes per page */

/*
 * Driver structure
 *
 * "order" is initialize order which must be between 0 and 15.
 * The driver with order 0 is called first.
 *
 * The maximum length of device name is 12.
 */
struct driver {
	const char	*name;		/* Name of device driver */
	const int	order;		/* Initialize order */
	int		(*init)();	/* Initialize routine */
};
typedef struct driver *driver_t;

#define __driver_entry __attribute__ ((section(".driver_table")))

extern struct driver __driver_table, __driver_table_end;

/*
 * Device I/O table
 */
struct devio {
	int (*open)();
	int (*close)();
	int (*read)();
	int (*write)();
	int (*ioctl)();
	int (*event)();
};
typedef struct devio *devio_t;

typedef int *device_t;

/*
 * Device Object
 */
extern device_t device_create(devio_t io, const char *name);
extern int device_delete(device_t dev);
extern int device_broadcast(int event, int force);

#define MAX_DEVNAME	12

/*
 * Device open mode
 */
#define DO_RDONLY	0x0
#define DO_WRONLY	0x1
#define DO_RDWR		0x2
#define DO_RWMASK	0x3

/*
 * Device event
 */
#define EVT_SHUTDOWN	0
#define EVT_SUSPEND	1
#define EVT_RESUME	2
#define EVT_REMOVE	3
#define EVT_INSERT	4
	
/*
 * Kernel Memory
 */
extern void *kmem_alloc(size_t size);
extern void  kmem_free(void *ptr);
extern void *kmem_map(void *addr, size_t size);

/*
 * User memory access
 */
extern int umem_copyin(void *uaddr, void *kaddr, size_t len);
extern int umem_copyout(void *kaddr, void *uaddr, size_t len);
extern int umem_strnlen(void *uaddr, size_t maxlen, size_t *len);

/*
 * Physical page services
 */
#define PAGE_SIZE	CONFIG_PAGE_SIZE
#define PAGE_MASK	(PAGE_SIZE-1)
#define PAGE_ALIGN(n)	((((u_long)(n)) + PAGE_MASK) & ~PAGE_MASK)
#define PAGE_TRUNC(n)	(((u_long)(n)) & ~PAGE_MASK)

extern void *page_alloc(size_t size);
extern void page_free(void *addr, size_t size);
extern int page_reserve(void *addr, size_t size);

extern void *phys_to_virt(void *p_addr);
extern void *virt_to_phys(void *v_addr);

/*
 * Return value of ISR
 */
#define INT_DONE	0
#define INT_ERROR	1
#define INT_CONTINUE	2

/*
 * Interrupt services
 */
extern int irq_attach(int irqno, int level, int shared, int (*isr)(int), void (*ist)(int));
extern void irq_detach(int handle);
extern void irq_lock(void);
extern void irq_unlock(void);

/*
 * Interrupt priority level
 */
#define IPL_CLOCK	0	/* System Clock Timer */
#define IPL_RTC		1	/* RTC Alarm */
#define IPL_BUS		2	/* USB, PCCARD */
#define IPL_AUDIO	3	/* Audio */
#define IPL_INPUT	4	/* Keyboard, Mouse */
#define IPL_DISPLAY	4	/* Screen */
#define IPL_NET		5	/* Network */
#define IPL_BLOCK	6	/* FDD, IDE */
#define IPL_COMM	7	/* Serial, Parallel */

/*
 * Event for sleep/wakeup
 */
struct event {
	struct queue sleepq;	/* Queue for waiting thread */
	char *name;		/* Event name */
};
typedef struct event *event_t;

#define EVENT_INIT(event, evt_name) \
    { {&(event).sleepq, &(event).sleepq}, evt_name}

#define event_init(event, evt_name) \
    do { list_init(&(event)->sleepq); (event)->name = evt_name; } while (0)

/*
 * System hook descriptor
 */
struct hook {
	struct list	link;		/* Linkage on hook chain */
	void		(*func)(void *); /* Hook function */
};
typedef struct hook *hook_t;

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
typedef struct dpc *dpc_t;

/*
 * Scheduler services
 */
extern void sched_lock(void);
extern void sched_unlock(void);
extern int sched_tsleep(event_t event, u_long timeout);
extern void sched_wakeup(event_t event);
extern void sched_dpc(dpc_t dpc, void (*func)(void *), void *arg);

#define sched_sleep(event)  sched_tsleep((event), 0)

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
typedef struct timer *timer_t;

#define timer_init(tmr)     (tmr)->expire = 0;

/*
 * Timer services
 */
extern void timer_timeout(timer_t tmr, void (*func)(u_long), u_long arg, u_long msec);
extern void timer_stop(timer_t tmr);
extern u_long timer_delay(u_long msec);
extern u_long timer_count(void);
extern void timer_hook(hook_t hook, void (*func)(void *));
extern void timer_unhook(hook_t hook);

/*
 * Security services
 */
extern int task_capable(int cap);

/*
 * printk()
 *
 * Print the debug message to the output device.
 * The message is enabled only when DEBUG build.
 */
#ifdef DEBUG
extern void printk(const char *fmt, ...);
#else
#define printk(fmt...)
#endif

/*
 * panic()
 *
 * Reset CPU for fatal error.
 * If debugger is attached, break into it.
 */
#ifdef DEBUG
extern void panic(const char *fmt, ...);
#else
#undef panic
#define panic(fmt...)  do { for (;;) ; } while (0)
#endif

/*
 * ASSERT(exp)
 *
 *  If exp is false(zero), stop with source info.
 *  This is enabled only when DEBUG build.
 */
#ifdef DEBUG
extern void assert(const char *file, int line, const char *exp);
#define ASSERT(exp) do { if (!(exp)) \
    assert(__FILE__, __LINE__, #exp); } while (0)
#else
#define ASSERT(exp)
#endif

/*
 * Kenrel dump
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

extern int kernel_dump(int index);

extern void system_reset(void);
extern void debug_attach(void (*func)(char *));
extern void system_bootinfo(struct boot_info **);

#endif /* !_DRIVER_H */
