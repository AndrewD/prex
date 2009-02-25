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
 * device.c - device I/O support routines
 */

/*
 * The device_* system calls are interfaces for user mode
 * applications to access the specific device object which is
 * handled by the related device driver. A device driver is an
 * execution module different from a kernel on Prex. The routines
 * in this file have the following role to handle the device I/O.
 *
 *  - Manage the name space for device objects.
 *  - Forward user I/O requests to the drivers after checking
 *    parameters.
 *
 * The driver module(s) and kernel are dynamically linked
 * at system boot.
 */

#include <kernel.h>
#include <irq.h>
#include <kmem.h>
#include <task.h>
#include <sched.h>
#include <device.h>
#include <verbose.h>

static struct list device_list;		/* list of the device objects */

/*
 * Increment reference count on an active device.
 * It returns 0 on success, or -1 if the device is invalid.
 */
static int
device_hold(device_t dev)
{
	int err = -1;

	sched_lock();
	if (device_valid(dev)) {
		dev->refcnt++;
		err = 0;
	}
	sched_unlock();
	return err;
}

/*
 * Decrement the reference count on a device. If the
 * reference count becomes zero, we can release the
 * resource for the target device. Assumes the device
 * is already validated by caller.
 */
static void
device_release(device_t dev)
{

	sched_lock();
	if (--dev->refcnt == 0) {
		dev->magic = 0;
		list_remove(&dev->link);
		kmem_free(dev);
	}
	sched_unlock();
}

/*
 * Look up a device object by device name.
 * Return device ID on success, or NULL on failure.
 * This must be called with scheduler locked.
 */
static device_t
device_lookup(const char *name)
{
	list_t head, n;
	device_t dev;

	if (name == NULL)
		return NULL;

	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (!strncmp(dev->name, name, MAXDEVNAME))
			return dev;
	}
	return NULL;
}

/*
 * device_create - create new device object.
 *
 * A device object is created by the device driver to provide
 * I/O services to applications.
 * Returns device ID on success, or 0 on failure.
 */
device_t
device_create(const struct devio *io, const char *name, int flags, void *info)
{
	struct device *dev;
	size_t len;

	ASSERT(irq_level == 0);

	len = strnlen(name, MAXDEVNAME);
	if (len == 0 || len >= MAXDEVNAME)	/* Invalid name? */
		return 0;

	sched_lock();
	if ((dev = device_lookup(name)) != NULL) {
		/*
		 * Error - the device name is already used.
		 */
		sched_unlock();
		return 0;
	}
	if ((dev = kmem_alloc(sizeof(*dev))) == NULL) {
		sched_unlock();
		return 0;
	}
	strlcpy(dev->name, name, len + 1);
	dev->devio = io;
	dev->info = info;
	dev->flags = flags;
	dev->refcnt = 1;
	dev->magic = DEVICE_MAGIC;
	list_insert(&device_list, &dev->link);
	sched_unlock();
	return dev;
}

/*
 * Destroy a device object. If some other threads still
 * refer the target device, the destroy operating will be
 * pending until its reference count becomes 0.
 */
int
device_destroy(device_t dev)
{
	int err = 0;

	ASSERT(irq_level == 0);

	sched_lock();
	if (device_valid(dev))
		device_release(dev);
	else
		err = ENODEV;
	sched_unlock();
	return err;
}

#define FD_MAGIC 0x44455600	/* 'DEV\0' */
static fd_t
fd_alloc(const char *name)
{
	u_int i = 0;
	fd_t fd;
	file_t file = &cur_task()->file[0];
	device_t dev;

	sched_lock();
	while (file->dev) {
		file++;
		i++;
		if (i >= CONFIG_DEV_OPEN_MAX) {
			fd = DERR(-EMFILE);
			goto out;
		}
	}
	fd = i ^ FD_MAGIC;

	if ((dev = device_lookup(name)) == NULL) {
		fd = DERR(-ENXIO);
		goto out;
	}
	device_hold(dev);
	file->dev = dev;
	file->priv = dev->info;
	file->f_flags = 0;

out:
	sched_unlock();
	return fd;
}

static file_t
file_lookup(fd_t fd)
{
	file_t file;
	u_int i = fd ^ FD_MAGIC;

	if (i >= CONFIG_DEV_OPEN_MAX)
		return NULL;

	sched_lock();
	file = &cur_task()->file[i];

	if (file->dev == NULL)
		file = NULL;
	sched_unlock();
	return file;
}

static int
fd_free(fd_t fd)
{
	file_t file;
	int err = 0;

	sched_lock();
	file = file_lookup(fd);

	if (file == NULL) {
		err = DERR(-EBADF);
		goto out;
	}

	device_release(file->dev);
	file->dev = NULL;

out:
	sched_unlock();
	return err;
}

/*
 * Cleanup open driver fd when a task exits
 */
void
device_terminate(task_t task)
{
	u_int i;

	sched_lock();
	for (i = 0; i < CONFIG_DEV_OPEN_MAX; i++) {
		if (task->file[i].dev != NULL)
			device_close(i ^ FD_MAGIC);
	}
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
device_open(const char *name, int flags, fd_t *fdp)
{
	char str[MAXDEVNAME];
	fd_t fd;
	file_t file;
	size_t len;
	int err = 0;

	if (!task_capable(CAP_DEVIO))
		return DERR(EPERM);

	if (umem_strnlen(name, MAXDEVNAME, &len))
		return EFAULT;
	if (len == 0)
		return ENOENT;
	if (len >= MAXDEVNAME)
		return ENAMETOOLONG;

	if (umem_copyin(name, str, len + 1))
		return EFAULT;

	fd = fd_alloc(str);
	if (fd < 0)
		return -fd;

	file = file_lookup(fd);
	file->f_flags = flags;

	if (file->dev->devio->open != NULL)
		err = (*file->dev->devio->open)(file);
	if (!err)
		err = umem_copyout(&fd, fdp, sizeof(fd));
	else
		fd_free(fd);
	return err;
}

/*
 * device_close - close a device.
 *
 * Even if the target driver does not have close routine,
 * this function does not return any errors.
 */
int
device_close(fd_t fd)
{
	file_t file;
	int err = 0;

	if ((file = file_lookup(fd)) == NULL)
		return DERR(EBADF);

	if (file->dev->devio->close != NULL)
		err = (*file->dev->devio->close)(file);

	fd_free(fd);
	return err;
}

/*
 * device_read - read from a device.
 *
 * Actual read count is set in "nbyte" as return.
 * Note: The size of one block is device dependent.
 */
int
device_read(fd_t fd, void *buf, size_t *nbyte, int blkno)
{
	file_t file;
	size_t count;
	int err;

	if ((file = file_lookup(fd)) == NULL)
		return DERR(EBADF);

	if (file->dev->devio->read == NULL)
		return DERR(EIO);

	if (umem_copyin(nbyte, &count, sizeof(count)))
		return EFAULT;

	err = (*file->dev->devio->read)(file, buf, &count, blkno);
	if (err == 0)
		err = umem_copyout(&count, nbyte, sizeof(count));

	return err;
}

/*
 * device_write - write to a device.
 *
 * Actual write count is set in "nbyte" as return.
 */
int
device_write(fd_t fd, void *buf, size_t *nbyte, int blkno)
{
	file_t file;
	size_t count;
	int err;

	if ((file = file_lookup(fd)) == NULL)
		return DERR(EBADF);

	if (file->dev->devio->write == NULL)
		return DERR(EIO);

	if (umem_copyin(nbyte, &count, sizeof(count)))
		return EFAULT;

	err = (*file->dev->devio->write)(file, buf, &count, blkno);
	if (err == 0)
		err = umem_copyout(&count, nbyte, sizeof(count));

	return err;
}

/*
 * device_ioctl - I/O control request.
 *
 * A command and an argument are completely device dependent.
 * The ioctl routine of each driver must validate the user buffer
 * pointed by the arg value.
 */
int
device_ioctl(fd_t fd, u_long cmd, void *arg)
{
	file_t file;

	if ((file = file_lookup(fd)) == NULL)
		return DERR(EBADF);

	if (file->dev->devio->ioctl == NULL)
		return DERR(EIO);

	return (*file->dev->devio->ioctl)(file, cmd, arg);
}

/*
 * device_broadcast - broadcast an event to all device objects.
 *
 * If "force" argument is true, a kernel will continue event
 * notification even if some driver returns error. In this case,
 * this routine returns EIO error if at least one driver returns
 * an error.
 *
 * If force argument is false, a kernel stops the event processing
 * when at least one driver returns an error. In this case,
 * device_broadcast will return the error code which is returned
 * by the driver.
 */
int
device_broadcast(int event, int force)
{
	device_t dev;
	list_t head, n;
	int err, ret = 0;

	sched_lock();
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (dev->devio->event != NULL) {
			/*
			 * Call driver's event routine.
			 */
			err = (*dev->devio->event)(event);
			if (err) {
				if (force)
					ret = EIO;
				else {
					ret = err;
					break;
				}
			}
		}
	}
	sched_unlock();
	return ret;
}

/*
 * Return device information (for devfs).
 */
int
device_info(struct info_device *info)
{
	u_long index, target = info->cookie;
	device_t dev;
	list_t head, n;
	int err = ESRCH;

	sched_lock();
	index = 0;
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (index == target) {
			info->id = dev;
			info->flags = dev->flags;
			strlcpy(info->name, dev->name, MAXDEVNAME);
			err = 0;
			break;
		}
		index++;
	}
	sched_unlock();
	return err;
}

/*
 * Initialize static device drivers.
 */
static void
driver_init(void)
{
	struct driver *drv;
	int order, err;
	extern struct driver __driver_table, __driver_table_end;

	DPRINTF(("Load static drivers\n"));

	/*
	 * Initialize library components.
	 */
#ifdef CONFIG_DELAY
	calibrate_delay();
#endif

	/*
	 * Call init routine for all device drivers with init order.
	 * Smaller value will be run first.
	 */
	for (order = 0; order < 16; order++) {
		for (drv = &__driver_table; drv != &__driver_table_end;
		     drv++) {
			ASSERT(drv->order < 16);
			if (drv->order == order) {
				if (drv->init)
					err = drv->init();
			}
		}
	}
}

/*
 * Initialize device driver module.
 */
void
device_init(void)
{
	void (*drv_entry)(void);

	list_init(&device_list);
	driver_init();

	drv_entry = (void (*)(void))bootinfo->driver.entry;
	if (drv_entry == NULL)
		return;		/* no boot image drivers */

	/*
	 * Call all initialization functions in drivers.
	 */
	(*drv_entry)();
}
