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
 * system.c - system services
 */

#include <kernel.h>
#include <thread.h>
#include <sched.h>
#include <task.h>
#include <irq.h>
#include <page.h>
#include <kmem.h>
#include <vm.h>
#include <device.h>
#include <system.h>

static const struct stat_kernel kstat = KERNEL_STAT(kstat);

/*
 * Logging system call.
 *
 * Write a message to the logging device.
 * The log function is available only when kernel is built with debug option.
 */
__syscall int sys_log(char *str)
{
#ifdef DEBUG
	char buf[LOGBUF_SIZE];
	size_t len;

	if (umem_strnlen(str, LOGBUF_SIZE, &len))
		return EFAULT;
	if (len >= LOGBUF_SIZE)
		return EINVAL;

	if (umem_copyin(str, buf, len + 1) != 0)
		return EFAULT;
	printk(buf);
	return 0;
#else
	return ENOSYS;
#endif
}

/*
 * Panic system call.
 *
 * Kernel behavior for sys_panic() is different for its debug option.
 *  - Debug build
 *     Show a panic message and stop the entire system.
 *  - Release build
 *     Terminate the task which called sys_panic().
 */
__syscall int sys_panic(char *str)
{
#ifdef DEBUG
	static const char line[] =
	    "\n=====================================================\n";

	irq_lock();
	printk(line);
	printk("User mode panic! task=%x thread=%x\n",
	       cur_task(), cur_thread);
	sys_log(str);
	printk(line);
	sched_lock();
	irq_unlock();
	BREAKPOINT();

	for (;;);
#else
	task_terminate(cur_task());
#endif
	return 0;
}

/*
 * Get kernel statistics.
 */
__syscall int sys_stat(int type, void *buf)
{
	struct stat_memory sm;
	struct stat_sched ss;
		
	if (buf == NULL || !user_area(buf))
		return EFAULT;

	switch (type) {
	case STAT_KERNEL:
		if (umem_copyout((void *)&kstat, buf, sizeof(struct stat_kernel)) != 0)
			return EFAULT;
		break;

	case STAT_MEMORY:
		page_stat(&sm.total, &sm.free);
		kmem_stat(&sm.kernel);
		if (umem_copyout(&sm, buf, sizeof(struct stat_memory)) != 0)
			return EFAULT;
		break;

	case STAT_SCHED:
		sched_stat(&ss);
		if (umem_copyout(&ss, buf, sizeof(struct stat_sched)) != 0)
			return EFAULT;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

/*
 * Get system time.
 *
 * Returns ticks from OS boot.
 */
__syscall int sys_time(u_long *ticks)
{
	u_long t;

	t = timer_ticks();

	return umem_copyout(&t, ticks, sizeof(u_long));
}

/*
 * Kernel debug service.
 */
__syscall int sys_debug(int cmd, int param)
{
#ifdef DEBUG
	int err = EINVAL;;

	switch (cmd) {
	case DBGCMD_DUMP:
		err = kernel_dump(param);
		break;
	default:
		break;
	}
	return err;
#else
	return ENOSYS;
#endif
}
