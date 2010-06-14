/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * device.c - device I/O support routines
 */

/**
 * The device_* system calls are interfaces to access the specific
 * device object which is handled by the related device driver.
 *
 * The routines in this moduile have the following role:
 *  - Manage the name space for device objects.
 *  - Forward user I/O requests to the drivers with minimum check.
 *  - Provide the table for the Driver-Kernel Interface.
 */

#include <kernel.h>
#include <irq.h>
#include <kmem.h>
#include <task.h>
#include <timer.h>
#include <page.h>
#include <sched.h>
#include <exception.h>
#include <vm.h>
#include <device.h>
#include <system.h>
#include <hal.h>

/* forward declarations */
static device_t	device_create(struct driver *, const char *, int);
static int	device_destroy(device_t);
static device_t	device_lookup(const char *);
static int	device_valid(device_t);
static int	device_reference(device_t);
static void	device_release(device_t);
static void	*device_private(device_t);
static int	device_control(device_t, u_long, void *);
static int	device_broadcast(u_long, void *, int);

#define DKIENT(func)	(dkifn_t)(func)

/*
 * Driver-Kernel Interface (DKI)
 */
static const dkifn_t dkient[] = {
	/*  0 */ DKIENT(copyin),
	/*  1 */ DKIENT(copyout),
	/*  2 */ DKIENT(copyinstr),
	/*  3 */ DKIENT(kmem_alloc),
	/*  4 */ DKIENT(kmem_free),
	/*  5 */ DKIENT(kmem_map),
	/*  6 */ DKIENT(page_alloc),
	/*  7 */ DKIENT(page_free),
	/*  8 */ DKIENT(page_reserve),
	/*  9 */ DKIENT(irq_attach),
	/* 10 */ DKIENT(irq_detach),
	/* 11 */ DKIENT(spl0),
	/* 12 */ DKIENT(splhigh),
	/* 13 */ DKIENT(splx),
	/* 14 */ DKIENT(timer_callout),
	/* 15 */ DKIENT(timer_stop),
	/* 16 */ DKIENT(timer_delay),
	/* 17 */ DKIENT(timer_ticks),
	/* 18 */ DKIENT(sched_lock),
	/* 19 */ DKIENT(sched_unlock),
	/* 20 */ DKIENT(sched_tsleep),
	/* 21 */ DKIENT(sched_wakeup),
	/* 22 */ DKIENT(sched_dpc),
	/* 23 */ DKIENT(task_capable),
	/* 24 */ DKIENT(exception_post),
	/* 25 */ DKIENT(device_create),
	/* 26 */ DKIENT(device_destroy),
	/* 27 */ DKIENT(device_lookup),
	/* 28 */ DKIENT(device_control),
	/* 29 */ DKIENT(device_broadcast),
	/* 30 */ DKIENT(device_private),
	/* 31 */ DKIENT(machine_bootinfo),
	/* 32 */ DKIENT(machine_powerdown),
	/* 33 */ DKIENT(sysinfo),
#ifdef DEBUG
	/* 34 */ DKIENT(panic),
	/* 35 */ DKIENT(printf),
	/* 36 */ DKIENT(dbgctl),
#else
	/* 34 */ DKIENT(machine_abort),
	/* 35 */ DKIENT(sys_nosys),
	/* 36 */ DKIENT(sys_nosys),
#endif
};

/* list head of the devices */
static struct device *device_list = NULL;

/*
 * device_create - create new device object.
 *
 * A device object is created by the device driver to provide
 * I/O services to applications.  Returns device ID on
 * success, or 0 on failure.
 */
static device_t
device_create(struct driver *drv, const char *name, int flags)
{
	device_t dev;
	size_t len;
	void *private = NULL;

	ASSERT(drv != NULL);

	/* Check the length of name. */
	len = strnlen(name, MAXDEVNAME);
	if (len == 0 || len >= MAXDEVNAME)
		return NULL;

	sched_lock();

	/* Check if specified name is already used. */
	if (device_lookup(name) != NULL)
		panic("duplicate device");

	/*
	 * Allocate a device structure and device private data.
	 */
	if ((dev = kmem_alloc(sizeof(*dev))) == NULL)
		panic("device_create");

	if (drv->devsz != 0) {
		if ((private = kmem_alloc(drv->devsz)) == NULL)
			panic("devsz");
		memset(private, 0, drv->devsz);
	}
	strlcpy(dev->name, name, len + 1);
	dev->driver = drv;
	dev->flags = flags;
	dev->active = 1;
	dev->refcnt = 1;
	dev->private = private;
	dev->next = device_list;
	device_list = dev;

	sched_unlock();
	return dev;
}

/*
 * Destroy a device object. If some other threads still
 * refer the target device, the destroy operation will be
 * pending until its reference count becomes 0.
 */
static int
device_destroy(device_t dev)
{

	sched_lock();
	if (!device_valid(dev)) {
		sched_unlock();
		return ENODEV;
	}
	dev->active = 0;
	device_release(dev);
	sched_unlock();
	return 0;
}

/*
 * Look up a device object by device name.
 */
static device_t
device_lookup(const char *name)
{
	device_t dev;

	for (dev = device_list; dev != NULL; dev = dev->next) {
		if (!strncmp(dev->name, name, MAXDEVNAME))
			return dev;
	}
	return NULL;
}

/*
 * Return device's private data.
 */
static void *
device_private(device_t dev)
{
	ASSERT(dev != NULL);
	ASSERT(dev->private != NULL);

	return dev->private;
}

/*
 * Return true if specified device is valid.
 */
static int
device_valid(device_t dev)
{
	device_t tmp;
	int found = 0;

	for (tmp = device_list; tmp != NULL; tmp = tmp->next) {
		if (tmp == dev) {
			found = 1;
			break;
		}
	}
	if (found && dev->active)
		return 1;
	return 0;
}

/*
 * Increment the reference count on an active device.
 */
static int
device_reference(device_t dev)
{

	sched_lock();
	if (!device_valid(dev)) {
		sched_unlock();
		return ENODEV;
	}
	if (!task_capable(CAP_RAWIO)) {
		sched_unlock();
		return EPERM;
	}
	dev->refcnt++;
	sched_unlock();
	return 0;
}

/*
 * Decrement the reference count on a device. If the reference
 * count becomes zero, we can release the resource for the device.
 */
static void
device_release(device_t dev)
{
	device_t *tmp;

	sched_lock();
	if (--dev->refcnt > 0) {
		sched_unlock();
		return;
	}
	/*
	 * No more references - we can remove the device.
	 */
	for (tmp = &device_list; *tmp; tmp = &(*tmp)->next) {
		if (*tmp == dev) {
			*tmp = dev->next;
			break;
		}
	}
	kmem_free(dev);
	sched_unlock();
}

/*
 * device_open - open the specified device.
 *
 * Even if the target driver does not have an open
 * routine, this function does not return an error. By
 * using this mechanism, an application can check whether
 * the specific device exists or not. The open mode
 * should be handled by an each device driver if it is
 * needed.
 */
int
device_open(const char *name, int mode, device_t *devp)
{
	struct devops *ops;
	device_t dev;
	char str[MAXDEVNAME];
	int error;

	error = copyinstr(name, str, MAXDEVNAME);
	if (error)
		return error;

	sched_lock();
	if ((dev = device_lookup(str)) == NULL) {
		sched_unlock();
		return ENXIO;
	}
	error = device_reference(dev);
	if (error) {
		sched_unlock();
		return error;
	}
	sched_unlock();

	ops = dev->driver->devops;
	ASSERT(ops->open != NULL);
	error = (*ops->open)(dev, mode);
	if (!error)
		error = copyout(&dev, devp, sizeof(dev));

	device_release(dev);
	return error;
}

/*
 * device_close - close a device.
 *
 * Even if the target driver does not have close routine,
 * this function does not return any errors.
 */
int
device_close(device_t dev)
{
	struct devops *ops;
	int error;

	if ((error = device_reference(dev)) != 0)
		return error;

	ops = dev->driver->devops;
	ASSERT(ops->close != NULL);
	error = (*ops->close)(dev);

	device_release(dev);
	return error;
}

/*
 * device_read - read from a device.
 *
 * Actual read count is set in "nbyte" as return.
 * Note: The size of one block is device dependent.
 */
int
device_read(device_t dev, void *buf, size_t *nbyte, int blkno)
{
	struct devops *ops;
	size_t count;
	int error;

	if (!user_area(buf))
		return EFAULT;

	if ((error = device_reference(dev)) != 0)
		return error;

	if (copyin(nbyte, &count, sizeof(count))) {
		device_release(dev);
		return EFAULT;
	}

	ops = dev->driver->devops;
	ASSERT(ops->read != NULL);
	error = (*ops->read)(dev, buf, &count, blkno);
	if (!error)
		error = copyout(&count, nbyte, sizeof(count));

	device_release(dev);
	return error;
}

/*
 * device_write - write to a device.
 *
 * Actual write count is set in "nbyte" as return.
 */
int
device_write(device_t dev, void *buf, size_t *nbyte, int blkno)
{
	struct devops *ops;
	size_t count;
	int error;

	if (!user_area(buf))
		return EFAULT;

	if ((error = device_reference(dev)) != 0)
		return error;

	if (copyin(nbyte, &count, sizeof(count))) {
		device_release(dev);
		return EFAULT;
	}

	ops = dev->driver->devops;
	ASSERT(ops->write != NULL);
	error = (*ops->write)(dev, buf, &count, blkno);
	if (!error)
		error = copyout(&count, nbyte, sizeof(count));

	device_release(dev);
	return error;
}

/*
 * device_ioctl - I/O control request.
 *
 * A command and its argument are completely device dependent.
 * The ioctl routine of each driver must validate the user buffer
 * pointed by the arg value.
 */
int
device_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct devops *ops;
	int error;

	if ((error = device_reference(dev)) != 0)
		return error;

	ops = dev->driver->devops;
	ASSERT(ops->ioctl != NULL);
	error = (*ops->ioctl)(dev, cmd, arg);

	device_release(dev);
	return error;
}

/*
 * Device control - devctl is similar to ioctl, but is invoked from
 * other device driver rather than from user application.
 */
static int
device_control(device_t dev, u_long cmd, void *arg)
{
	struct devops *ops;
	int error;

	ASSERT(dev != NULL);

	sched_lock();
	ops = dev->driver->devops;
	ASSERT(ops->devctl != NULL);
	error = (*ops->devctl)(dev, cmd, arg);
	sched_unlock();
	return error;
}

/*
 * device_broadcast - broadcast devctl command to all device objects.
 *
 * If "force" argument is true, we will continue command
 * notification even if some driver returns an error. In this
 * case, this routine returns EIO error if at least one driver
 * returns an error.
 *
 * If force argument is false, a kernel stops the command processing
 * when at least one driver returns an error. In this case,
 * device_broadcast will return the error code which is returned
 * by the driver.
 */
static int
device_broadcast(u_long cmd, void *arg, int force)
{
	device_t dev;
	struct devops *ops;
	int error, retval = 0;

	sched_lock();

	for (dev = device_list; dev != NULL; dev = dev->next) {
		/*
		 * Call driver's devctl() routine.
		 */
		ops = dev->driver->devops;
		if (ops == NULL)
			continue;

		ASSERT(ops->devctl != NULL);
		error = (*ops->devctl)(dev, cmd, arg);
		if (error) {
			DPRINTF(("%s returns error=%d for cmd=%ld\n",
				 dev->name, error, cmd));
			if (force)
				retval = EIO;
			else {
				retval = error;
				break;
			}
		}
	}
	sched_unlock();
	return retval;
}

/*
 * Return device information.
 */
int
device_info(struct devinfo *info)
{
	u_long target = info->cookie;
	u_long i = 0;
	device_t dev;
	int error = ESRCH;

	sched_lock();
	for (dev = device_list; dev != NULL; dev = dev->next) {
		if (i++ == target) {
			info->cookie = i;
			info->id = dev;
			info->flags = dev->flags;
			strlcpy(info->name, dev->name, MAXDEVNAME);
			error = 0;
			break;
		}
	}
	sched_unlock();
	return error;
}

/*
 * Initialize device driver module.
 */
void
device_init(void)
{
	struct module *mod;
	struct bootinfo *bi;
	void (*entry)(const dkifn_t *);

	machine_bootinfo(&bi);
	mod = &bi->driver;
	if (mod == NULL) {
		DPRINTF(("Warning: No driver found\n"));
		return;
	}
	entry = (void (*)(const dkifn_t *))mod->entry;
	ASSERT(entry != NULL);

	/* Show module location to add the driver symbols for gdb. */
	DPRINTF(("Entering driver module (at 0x%x)\n", (int)entry));

	(*entry)(dkient);
}
