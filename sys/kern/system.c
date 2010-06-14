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
#include <vm.h>
#include <irq.h>
#include <page.h>
#include <device.h>
#include <system.h>
#include <hal.h>
#include <sys/dbgctl.h>

static char	infobuf[MAXINFOSZ];	/* common information buffer */

static const struct kerninfo kerninfo = {
	"Prex",	HOSTNAME, VERSION, __DATE__, MACHINE
};

/*
 * Get system information.
 */
int
sysinfo(int type, void *buf)
{
	int error = 0;

	sched_lock();

	switch (type) {
	case INFO_KERNEL:
		memcpy(buf, &kerninfo, sizeof(kerninfo));
		break;
	case INFO_MEMORY:
		page_info(buf);
		break;
	case INFO_TIMER:
		timer_info(buf);
		break;
	case INFO_THREAD:
		error = thread_info(buf);
		break;
	case INFO_DEVICE:
		error = device_info(buf);
		break;
	case INFO_TASK:
		error = task_info(buf);
		break;
	case INFO_VM:
		error = vm_info(buf);
		break;
	case INFO_IRQ:
		error = irq_info(buf);
		break;
	default:
		error = EINVAL;
		break;
	}
	sched_unlock();
	return error;
}

/*
 * System call to get system information.
 */
int
sys_info(int type, void *buf)
{
	int error;
	size_t bufsz = 0;

	if (buf == NULL || !user_area(buf))
		return EFAULT;

	sched_lock();

	switch (type) {
	case INFO_KERNEL:
		bufsz = sizeof(struct kerninfo);
		break;
	case INFO_MEMORY:
		bufsz = sizeof(struct meminfo);
		break;
	case INFO_TIMER:
		bufsz = sizeof(struct timerinfo);
		break;
	case INFO_THREAD:
		bufsz = sizeof(struct threadinfo);
		break;
	case INFO_DEVICE:
		bufsz = sizeof(struct devinfo);
		break;
	case INFO_TASK:
		bufsz = sizeof(struct taskinfo);
		break;
	case INFO_VM:
		bufsz = sizeof(struct vminfo);
		break;
	case INFO_IRQ:
		bufsz = sizeof(struct irqinfo);
		break;
	default:
		sched_unlock();
		return EINVAL;
	}

	error = copyin(buf, &infobuf, bufsz);
	if (!error) {
		error = sysinfo(type, &infobuf);
		if (!error) {
			error = copyout(&infobuf, buf, bufsz);
		}
	}
	sched_unlock();
	return error;
}

/*
 * Logging system call.
 *
 * Write a message to the logging device.  The log
 * function is available only when kernel is built
 * with debug option.
 */
int
sys_log(const char *str)
{
#ifdef DEBUG
	char buf[DBGMSGSZ];

	if (copyinstr(str, buf, DBGMSGSZ))
		return EINVAL;

	printf("%s", buf);
	return 0;
#else
	return ENOSYS;
#endif
}

/*
 * Kernel debug service.
 */
int
sys_debug(int cmd, void *data)
{
#ifdef DEBUG
	int error = EINVAL;
	task_t task = 0;

	switch (cmd) {
	case DBGC_LOGSIZE:
	case DBGC_GETLOG:
		error = dbgctl(cmd, data);
		break;
	case DBGC_TRACE:
		task = (task_t)data;
		if (!task_valid(task)) {
			error = ESRCH;
			break;
		}
		dbgctl(cmd, (void *)task);
		error = 0;
		break;
	}
	return error;
#else
	return ENOSYS;
#endif
}

/*
 * Panic system call.
 */
int
sys_panic(const char *str)
{
#ifdef DEBUG
	char buf[DBGMSGSZ];

	sched_lock();
	copyinstr(str, buf, DBGMSGSZ - 20);
	printf("User panic: %s\n", str);
	printf(" task=%s thread=%lx\n", curtask->name, (long)curthread);

	machine_abort();
	/* NOTREACHED */
#else
	task_terminate(curtask);
#endif
	return 0;
}

/*
 * Get system time - return ticks since OS boot.
 */
int
sys_time(u_long *ticks)
{
	u_long t;

	t = timer_ticks();
	return copyout(&t, ticks, sizeof(t));
}

/*
 * nonexistent system call.
 */
int
sys_nosys(void)
{
	return EINVAL;
}
