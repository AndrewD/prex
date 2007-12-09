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

static const struct info_kernel kern_info = KERNEL_INFO(kern_info);

/*
 * Logging system call.
 *
 * Write a message to the logging device.
 * The log function is available only when kernel is built with debug option.
 */
__syscall int sys_log(char *str)
{
#ifdef DEBUG
	char buf[LOGMSG_SIZE];
	size_t len;

	if (umem_strnlen(str, LOGMSG_SIZE, &len))
		return EFAULT;
	if (len >= LOGMSG_SIZE)
		return EINVAL;
	if (umem_copyin(str, buf, len + 1))
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
	irq_lock();
	printk("\nUser mode panic! task=%x thread=%x\n",
	       cur_task(), cur_thread);
	sys_log(str);
	printk("\n");
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
 * Get system information
 */
__syscall int sys_info(int type, void *buf)
{
	int err;
	struct info_memory im;
	struct info_sched is;
	struct info_thread it;
	struct info_device id;
		
	if (buf == NULL || !user_area(buf))
		return EFAULT;

	switch (type) {
	case INFO_KERNEL:
		if (umem_copyout((void *)&kern_info, buf, sizeof(kern_info)))
			return EFAULT;
		break;
	case INFO_MEMORY:
		page_info(&im.total, &im.free);
		kmem_info(&im.kernel);
		if (umem_copyout(&im, buf, sizeof(im)))
			return EFAULT;
		break;
	case INFO_SCHED:
		sched_info(&is);
		if (umem_copyout(&is, buf, sizeof(is)))
			return EFAULT;
		break;
	case INFO_THREAD:
		if (umem_copyin(buf, &it, sizeof(it)))
			return EFAULT;
		if ((err = thread_info(&it)))
			return err;
		it.cookie++;
		if (umem_copyout(&it, buf, sizeof(it)))
			return EFAULT;
		break;
	case INFO_DEVICE:
		if (umem_copyin(buf, &id, sizeof(id)))
			return EFAULT;
		if ((err = device_info(&id)))
			return err;
		id.cookie++;
		if (umem_copyout(&id, buf, sizeof(id)))
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

	t = timer_count();
	return umem_copyout(&t, ticks, sizeof(u_long));
}

/*
 * Kernel debug service.
 */
__syscall int sys_debug(int cmd, int param)
{
#ifdef DEBUG
	int err = EINVAL;;

	if (!task_capable(CAP_DEBUG))
		return EPERM;

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
