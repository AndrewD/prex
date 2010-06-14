/*
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
 * dki.S - driver kernel interface
 */

#include <machine/asm.h>
#include <dkient.h>

STUB( 0, copyin)
STUB( 1, copyout)
STUB( 2, copyinstr)
STUB( 3, kmem_alloc)
STUB( 4, kmem_free)
STUB( 5, kmem_map)
STUB( 6, page_alloc)
STUB( 7, page_free)
STUB( 8, page_reserve)
STUB( 9, irq_attach)
STUB(10, irq_detach)
STUB(11, spl0)
STUB(12, splhigh)
STUB(13, splx)
STUB(14, timer_callout)
STUB(15, timer_stop)
STUB(16, timer_delay)
STUB(17, timer_ticks)
STUB(18, sched_lock)
STUB(19, sched_unlock)
STUB(20, sched_tsleep)
STUB(21, sched_wakeup)
STUB(22, sched_dpc)
STUB(23, task_capable)
STUB(24, exception_post)
STUB(25, device_create)
STUB(26, device_destroy)
STUB(27, device_lookup)
STUB(28, device_control)
STUB(29, device_broadcast)
STUB(30, device_private)
STUB(31, machine_bootinfo)
STUB(32, machine_powerdown)
STUB(33, sysinfo)
STUB(34, panic)
STUB(35, printf)
STUB(36, dbgctl)
