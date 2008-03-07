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
 * The device_* system calls are interfaces for user mode applications
 * to access the specific device object which is handled by the related
 * device driver. A device driver is an execution module different from
 * a kernel on Prex. The routines in this file have the following role
 * to handle the device I/O.
 *
 *  - Manage the name space for device objects.
 *  - Forward user I/O requests to the drivers after checking parameters.
 *
 * The driver module(s) and kernel are dynamically linked at system boot.
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
#include <stdlib.h>
#include <device.h>
#include <system.h>

static struct list device_list;		/* list of the device objects */

/*
 * Increment reference count on an active device.
 * This routine checks whether the specified device is valid.
 * It returns 0 on success, or -1 on failure.
 */
static int
device_hold(device_t dev)
{
	int err = -1;

	sched_lock();
	if (device_valid(dev)) {
		dev->ref_count++;
		err = 0;
	}
	sched_unlock();
	return err;
}

/*
 * Decrement the reference count on a device. If the reference
 * count becomes zero, we can release the resource for the
 * target device. Assumes the device is already validated by caller.
 */
static void
device_release(device_t dev)
{

	sched_lock();
	if (--dev->ref_count == 0) {
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
 * @io:    pointer to device I/O routines
 * @name:  string for device name
 * @flags: flags for device object. (ex. block or character)
 *
 * A device object is created by the device driver to provide
 * I/O services to applications.
 * Returns device ID on success, or 0 on failure.
 */
device_t
device_create(const struct devio *io, const char *name, int flags)
{
	device_t dev;
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
	if ((dev = kmem_alloc(sizeof(struct device))) == NULL) {
		sched_unlock();
		return 0;
	}
	strlcpy(dev->name, name, len + 1);
	dev->devio = io;
	dev->flags = flags;
	dev->ref_count = 1;
	dev->magic = DEVICE_MAGIC;
	list_insert(&device_list, &dev->link);
	sched_unlock();
	return dev;
}

/*
 * Destroy a device object. If some other threads still refer
 * the target device, the destroy operating will be pending
 * until its reference count becomes 0.
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

/*
 * device_open - open the specified device.
 * @name: device name (null-terminated)
 * @mode: open mode. (like O_RDONLY etc.)
 * @devp: device handle of opened device to be returned.
 *
 * Even if the target driver does not have an open routine, this
 * function does not return an error. By using this mechanism, an
 * application can check whether the specific device exists or not.
 * The open mode should be handled by an each device driver if it
 * is needed.
 */
int
device_open(const char *name, int mode, device_t *devp)
{
	char str[MAXDEVNAME];
	device_t dev;
	size_t len;
	int err = 0;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (umem_strnlen(name, MAXDEVNAME, &len))
		return EFAULT;
	if (len == 0)
		return ENOENT;
	if (len >= MAXDEVNAME)
		return ENAMETOOLONG;

	if (umem_copyin((void *)name, str, len + 1))
		return EFAULT;

	sched_lock();
	if ((dev = device_lookup(str)) == NULL) {
		sched_unlock();
		return ENXIO;
	}
	device_hold(dev);
	sched_unlock();

	if (dev->devio->open != NULL)
		err = (dev->devio->open)(dev, mode);

	if (!err)
		err = umem_copyout(&dev, devp, sizeof(device_t));
	device_release(dev);
	return err;
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
	int err = 0;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_hold(dev))
		return ENODEV;

	if (dev->devio->close != NULL)
		err = (dev->devio->close)(dev);

	device_release(dev);
	return err;
}

/*
 * device_read - read from a device.
 * @dev:   device id
 * @buf:   pointer to read buffer
 * @nbyte: number of bytes to read. actual read count is set in return.
 * @blkno: block number (for block device)
 *
 * Note: The size of one block is device dependent.
 */
int
device_read(device_t dev, void *buf, size_t *nbyte, int blkno)
{
	size_t count;
	int err;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_hold(dev))
		return ENODEV;

	if (dev->devio->read == NULL) {
		device_release(dev);
		return EBADF;
	}
	if (umem_copyin(nbyte, &count, sizeof(u_long)) ||
	    vm_access(buf, count, VMA_WRITE)) {
		device_release(dev);
		return EFAULT;
	}
	err = (dev->devio->read)(dev, buf, &count, blkno);
	if (err == 0)
		err = umem_copyout(&count, nbyte, sizeof(u_long));
	device_release(dev);
	return err;
}

/*
 * device_write - write to a device.
 * @dev:   device id
 * @buf:   pointer to write buffer
 * @nbyte: number of bytes to write. actual write count is set in return.
 * @blkno: block number (for block device)
 */
int
device_write(device_t dev, void *buf, size_t *nbyte, int blkno)
{
	size_t count;
	int err;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_hold(dev))
		return ENODEV;

	if (dev->devio->write == NULL) {
		device_release(dev);
		return EBADF;
	}
	if (umem_copyin(nbyte, &count, sizeof(u_long)) ||
	    vm_access(buf, count, VMA_READ)) {
		device_release(dev);
		return EFAULT;
	}
	err = (dev->devio->write)(dev, buf, &count, blkno);
	if (err == 0)
		err = umem_copyout(&count, nbyte, sizeof(u_long));

	device_release(dev);
	return err;
}

/*
 * deivce_ioctl - I/O control request.
 * @dev: device id
 * @cmd: command
 * @arg: argument
 *
 * A command and an argument are completely device dependent.
 * If argument type is pointer, the driver routine must validate
 * the pointer address.
 */
int
device_ioctl(device_t dev, int cmd, u_long arg)
{
	int err;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_hold(dev))
		return ENODEV;

	err = EBADF;
	if (dev->devio->ioctl != NULL)
		err = (dev->devio->ioctl)(dev, cmd, arg);

	device_release(dev);
	return err;
}

/*
 * device_broadcast - broadcast an event to all device objects.
 * @event: event code
 * @force: true to ignore the return value from driver.
 *
 * If force argument is true, a kernel will continue event
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

#ifdef DEBUG
	printk("Broadcasting device event:%d\n", event);
#endif
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (dev->devio->event == NULL)
			continue;

		err = (dev->devio->event)(event);
		if (err) {
			if (force)
				ret = EIO;
			else {
				ret = err;
				break;
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

	sched_lock();

	index = 0;
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n), index++) {
		dev = list_entry(n, struct device, link);
		if (index == target)
			break;
	}
	if (n == head) {
		sched_unlock();
		return ESRCH;
	}
	info->id = dev;
	info->flags = dev->flags;
	strlcpy(info->name, dev->name, MAXDEVNAME);

	sched_unlock();
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void
device_dump(void)
{
	device_t dev;
	const struct devio *io;
	list_t head, n;

	printk("Device dump:\n");
	printk(" device   open     close    read     write    ioctl    "
	       "event    name\n");
	printk(" -------- -------- -------- -------- -------- -------- "
	       "-------- ------------\n");

	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		io = dev->devio;
		printk(" %08x %08x %08x %08x %08x %08x %08x %s\n",
		       dev, io->open, io->close, io->read, io->write,
		       io->ioctl, io->event, dev->name);
	}
}
#endif

#ifndef DEBUG
static void
nosys(void)
{
}
#endif

/*
 * Check the capability of the current task.
 */
static int
task__capable(cap_t cap)
{

	return task_capable(cap);
}

/*
 * Return boot information
 */
void
machine_bootinfo(struct boot_info **info)
{
	ASSERT(info != NULL);

	*info = boot_info;
}

static void
machine__reset(void)
{

	machine_reset();
}

static void
machine__idle(void)
{

	machine_idle();
}

/*
 *  Address transtion (physical -> virtual)
 */
static void *
phys__to_virt(void *phys)
{

	return phys_to_virt(phys);
}

/*
 *  Address transtion (virtual -> physical)
 */
static void *
virt__to_phys(void *virt)
{

	return virt_to_phys(virt);
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

	printk("Load static drivers\n");

	/*
	 * Call init routine for all device drivers with init order.
	 * Smaller value will be run first.
	 */
	for (order = 0; order < 16; order++) {
		for (drv = &__driver_table; drv != &__driver_table_end;
		     drv++) {
			ASSERT(drv->order < 16);
			if (drv->order == order) {
				if (drv->init) {
					printk("Initializing %s\n", drv->name);
					err = drv->init();
				}
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
	struct module *m;
	void (*drv_entry)(void);

	list_init(&device_list);
	driver_init();

	m = &boot_info->driver;
	if (m == NULL)
		return;

	drv_entry = (void (*)(void))m->entry;
	if (drv_entry == NULL)
		return;
	/*
	 * Call all driver initialization functions.
	 */
	drv_entry();
}

#undef machine_idle
#undef machine_reset
#undef phys_to_virt
#undef virt_to_phys
#undef task_capable

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
EXPORT_SYMBOL(sched_dpc);
EXPORT_SYMBOL(sched_yield);
__EXPORT_SYMBOL(task_capable, task__capable);
EXPORT_SYMBOL(exception_post);
EXPORT_SYMBOL(machine_bootinfo);
__EXPORT_SYMBOL(machine_reset, machine__reset);
__EXPORT_SYMBOL(machine_idle, machine__idle);
__EXPORT_SYMBOL(phys_to_virt, phys__to_virt);
__EXPORT_SYMBOL(virt_to_phys, virt__to_phys);
EXPORT_SYMBOL(debug_attach);
EXPORT_SYMBOL(debug_dump);
#ifdef DEBUG
EXPORT_SYMBOL(printk);
EXPORT_SYMBOL(panic);
EXPORT_SYMBOL(assert);
#else
__EXPORT_SYMBOL(printk, nosys);
__EXPORT_SYMBOL(panic, machine__reset);
__EXPORT_SYMBOL(assert, nosys);
#endif

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
