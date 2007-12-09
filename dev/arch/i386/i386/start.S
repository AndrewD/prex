/*
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

/*
 * start.S - driver start up routine
 */
#define ENTRY(x) .global x; .align 4,0x90; x##:

.data

drv_serv:
.long	0


.text

/*
 * Entry point which called by kernel
 */
ENTRY(driver_start)
	movl	0x04(%esp), %eax
	movl	%eax, drv_serv
	jmp	driver_main

/*
 * Stub to call kernel device interface
 */
#define STUB(index, func) .global func;\
ENTRY(func) \
	movl	drv_serv, %eax; \
	add	$(index * 4), %eax; \
	jmp	*(%eax);

STUB( 0, device_create)
STUB( 1, device_delete)
STUB( 2, device_broadcast)
STUB( 3, umem_copyin)
STUB( 4, umem_copyout)
STUB( 5, umem_strnlen)
STUB( 6, kmem_alloc)
STUB( 7, kmem_free)
STUB( 8, kmem_map)
STUB( 9, page_alloc)
STUB(10, page_free)
STUB(11, page_reserve)
STUB(12, irq_attach)
STUB(13, irq_detach)
STUB(14, irq_lock)
STUB(15, irq_unlock)
STUB(16, timer_timeout)
STUB(17, timer_stop)
STUB(18, timer_delay)
STUB(19, sched_lock)
STUB(20, sched_unlock)
STUB(21, sched_tsleep)
STUB(22, sched_wakeup)
STUB(23, sched_stat)
STUB(24, printk)
STUB(25, panic)
STUB(26, system_reset)
STUB(27, kernel_dump)
STUB(28, debug_attach)
STUB(29, task_capable)
STUB(30, system_bootinfo)
STUB(31, phys_to_virt)
STUB(32, virt_to_phys)

