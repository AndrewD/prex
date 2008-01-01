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

/*
 * kernel information.
 */
static const struct info_kernel kern_info = KERNEL_INFO(kern_info);

/*
 * Logging system call.
 *
 * Write a message to the logging device.
 * The log function is available only when kernel is built with
 * debug option.
 */
int
sys_log(const char *str)
{
#ifdef DEBUG
	char buf[MSGBUFSZ];
	size_t len;

	if (umem_strnlen(str, MSGBUFSZ, &len))
		return EFAULT;
	if (len >= MSGBUFSZ)
		return EINVAL;
	if (umem_copyin((void *)str, buf, len + 1))
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
int
sys_panic(const char *str)
{
#ifdef DEBUG
	irq_lock();
	printk("\nUser mode panic: task:%s thread:%x\n",
	       cur_task()->name ? cur_task()->name : "no name", cur_thread);

	sys_log(str);
	printk("\n");

	sched_lock();
	irq_unlock();
	BREAKPOINT();

	for (;;);
#else
	task_terminate(cur_task());
#endif
	/* NOTREACHED */
	return 0;
}

/*
 * Get system information
 */
int
sys_info(int type, void *buf)
{
	int err = 0;
	struct info_memory imem;
	struct info_timer itmr;
	struct info_thread ithr;
	struct info_device idev;

	if (buf == NULL || !user_area(buf))
		return EFAULT;

	switch (type) {
	case INFO_KERNEL:
		err = umem_copyout((void *)&kern_info, buf,
				   sizeof(kern_info));
		break;

	case INFO_MEMORY:
		page_info(&imem.total, &imem.free);
		kmem_info(&imem.kernel);
		err = umem_copyout(&imem, buf, sizeof(imem));
		break;

	case INFO_THREAD:
		if (umem_copyin(buf, &ithr, sizeof(ithr)))
			return EFAULT;
		if ((err = thread_info(&ithr)))
			return err;
		ithr.cookie++;
		err = umem_copyout(&ithr, buf, sizeof(ithr));
		break;

	case INFO_DEVICE:
		if (umem_copyin(buf, &idev, sizeof(idev)))
			return EFAULT;
		if ((err = device_info(&idev)))
			return err;
		idev.cookie++;
		err = umem_copyout(&idev, buf, sizeof(idev));
		break;

	case INFO_TIMER:
		timer_info(&itmr);
		err = umem_copyout(&itmr, buf, sizeof(itmr));
		break;

	default:
		err = EINVAL;
	}
	return err;
}

/*
 * Get system time - return ticks from OS boot.
 */
int
sys_time(u_long *ticks)
{
	u_long t;

	t = timer_count();
	return umem_copyout(&t, ticks, sizeof(u_long));
}

/*
 * Kernel debug service.
 */
int
sys_debug(int cmd, u_long param)
{
#ifdef DEBUG
	int err = EINVAL;
	size_t size;
	char *buf;

	/*
	 * Check capability for some commands.
	 */
	switch (cmd) {
	case DCMD_DUMP:
		if (!task_capable(CAP_DEBUG))
			return EPERM;
	}

	switch (cmd) {
	case DCMD_DUMP:
		err = debug_dump(param);
		break;
	case DCMD_LOGSIZE:
		if ((err = log_get(&buf, &size)) == 0)
			err = umem_copyout(&size, (void *)param, sizeof(size));
		break;
	case DCMD_GETLOG:
		if ((err = log_get(&buf, &size)) == 0)
			err = umem_copyout(buf, (void *)param, size);
		break;
	default:
		break;
	}
	return err;
#else
	return ENOSYS;
#endif
}
