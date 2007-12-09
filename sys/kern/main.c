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
 * main.c - kernel main routine
 */

#include <kernel.h>
#include <thread.h>
#include <timer.h>
#include <page.h>
#include <kmem.h>
#include <vm.h>
#include <sched.h>
#include <task.h>
#include <except.h>
#include <irq.h>
#include <ipc.h>
#include <device.h>
#include <sync.h>
#include <system.h>

/*
 * Kernel main routine.
 *
 * This is called from kernel_start() routine that is included
 * in architecture dependent layer. We assumes that the following
 * machine state are already set before calling this routine.
 *
 *  - All interrupts are disabled.
 *  - Minimum page table is set.
 *  - Kernel BSS is cleared.
 *  - Kernel stack is configured.
 */
void kernel_main(void)
{
	/*
	 * Initialize debug core
	 */
	debug_init();
	printk(BANNAR);

	/*
	 * Initialize memory managers
	 */
	page_init();
	mmu_init();
	kmem_init();
	vm_init();

	/*
	 * Initialize kernel core
	 */
	object_init();
	thread_init();
	sched_init();
	task_init();

	/*
	 * Initialize device drivers
	 */
	irq_init();
	clock_init();
	timer_init();
	device_init();

	/*
	 * Start boot tasks
	 */
	task_boot();

	/*
	 * Do idle loop
	 */
	thread_idle();

	/* NOTREACHED */
}
