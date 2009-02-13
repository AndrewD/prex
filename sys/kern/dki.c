/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * dki.c - Prex Driver Kernel Interface functions
 */

#include <kernel.h>
#include <irq.h>
#include <page.h>
#include <kmem.h>
#include <task.h>
#include <timer.h>
#include <sched.h>
#include <exception.h>
#include <vm.h>
#include <device.h>
#include <system.h>

/*
 * Return boot information
 */
void
machine_bootinfo(struct bootinfo **info)
{
	ASSERT(info != NULL);

	*info = bootinfo;
}

#ifndef DEBUG
extern void printf(const char *fmt, ...)
{
}

extern void panic(const char *fmt, ...)
{
	dev_machine_reset();
}

extern int debug_dump(int item)
{
	return -ENOSYS;
}

extern void debug_attach(void (*fn)(char *))
{
}

extern void assert(const char *file, int line, const char *exp)
{
}
#endif	/* !DEUBG */

/* export the functions used by drivers */
EXPORT_SYMBOL(device_create);
EXPORT_SYMBOL(device_destroy);
EXPORT_SYMBOL(device_broadcast);
EXPORT_SYMBOL(umem_copyin);
EXPORT_SYMBOL(umem_copyout);
EXPORT_SYMBOL(umem_strnlen);
EXPORT_SYMBOL(kmem_alloc);
EXPORT_SYMBOL(kmem_free);
EXPORT_SYMBOL(kmem_map);
EXPORT_SYMBOL(page_alloc);
EXPORT_SYMBOL(page_free);
EXPORT_SYMBOL(page_reserve);
EXPORT_SYMBOL(irq_attach);
EXPORT_SYMBOL(irq_detach);
EXPORT_SYMBOL(irq_lock);
EXPORT_SYMBOL(irq_unlock);
EXPORT_SYMBOL(timer_callout);
EXPORT_SYMBOL(timer_stop);
EXPORT_SYMBOL(timer_delay);
EXPORT_SYMBOL(timer_count);
EXPORT_SYMBOL(timer_hook);
EXPORT_SYMBOL(sched_lock);
EXPORT_SYMBOL(sched_unlock);
EXPORT_SYMBOL(sched_tsleep);
EXPORT_SYMBOL(sched_wakeup);
EXPORT_SYMBOL(sched_wakeone);
EXPORT_SYMBOL(sched_dpc);
EXPORT_SYMBOL(sched_yield);
EXPORT_SYMBOL(task_capable);
EXPORT_SYMBOL(thread_self);
EXPORT_SYMBOL(exception_post);
EXPORT_SYMBOL(machine_bootinfo);
EXPORT_SYMBOL(machine_reset);
EXPORT_SYMBOL(machine_idle);
EXPORT_SYMBOL(machine_setpower);
EXPORT_SYMBOL(vm_translate);
EXPORT_SYMBOL(debug_attach);
EXPORT_SYMBOL(debug_dump);
EXPORT_SYMBOL(printf);
EXPORT_SYMBOL(panic);
EXPORT_SYMBOL(assert);

/* export library functions */
EXPORT_SYMBOL(enqueue);         /* queue.c */
EXPORT_SYMBOL(dequeue);
EXPORT_SYMBOL(queue_insert);
EXPORT_SYMBOL(queue_remove);

#ifdef CONFIG_LITTLE_ENDIAN
EXPORT_SYMBOL(htonl);
EXPORT_SYMBOL(htons);
EXPORT_SYMBOL(ntohl);
EXPORT_SYMBOL(ntohs);
#endif

EXPORT_SYMBOL(strncpy);
EXPORT_SYMBOL(strlcpy);
EXPORT_SYMBOL(strncmp);
EXPORT_SYMBOL(strnlen);
EXPORT_SYMBOL(memcpy);
EXPORT_SYMBOL(memset);
#ifdef CONFIG_DELAY
EXPORT_SYMBOL(delay_usec);
#endif
