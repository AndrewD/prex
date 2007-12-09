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
#include <bootinfo.h>
#include <irq.h>
#include <page.h>
#include <kmem.h>
#include <timer.h>
#include <sched.h>
#include <vm.h>
#include <device.h>
#include <system.h>

/* Forward declarations */
static device_t device_create(devio_t io, char *name);
static int device_delete(device_t dev);
static int device_broadcast(int event, int force);
static void system_bootinfo(struct boot_info **info);
static void *_phys_to_virt(void *p_addr);
static void *_virt_to_phys(void *v_addr);

/* Driver-Kernel function */
typedef void (*dki_func_t)(void);
#define DKIENT(func)	(dki_func_t)(func)

#ifndef DEBUG
static void null_func(void);
#undef printk
#define printk null_func

#undef panic
#define panic system_reset
#endif

/* List of device objects */
static struct list device_list = LIST_INIT(device_list);

/*
 * Driver-Kernel Interface (DKI)
 */
static const dki_func_t driver_service[] = {
	DKIENT(device_create),		/* 0 */
	DKIENT(device_delete),
	DKIENT(device_broadcast),
	DKIENT(umem_copyin),
	DKIENT(umem_copyout),
	DKIENT(umem_strnlen),
	DKIENT(kmem_alloc),
	DKIENT(kmem_free),
	DKIENT(kmem_map),
	DKIENT(page_alloc),
	DKIENT(page_free),		/* 10 */
	DKIENT(page_reserve),
	DKIENT(irq_attach),
	DKIENT(irq_detach),
	DKIENT(irq_lock),
	DKIENT(irq_unlock),
	DKIENT(timer_timeout),
	DKIENT(timer_stop),
	DKIENT(timer_delay),
	DKIENT(timer_count),
	DKIENT(timer_hook),		/* 20 */
	DKIENT(timer_unhook),
	DKIENT(sched_lock),
	DKIENT(sched_unlock),
	DKIENT(sched_tsleep),
	DKIENT(sched_wakeup),
	DKIENT(sched_dpc),
	DKIENT(printk),
	DKIENT(panic),
	DKIENT(system_reset),
	DKIENT(kernel_dump),		/* 30 */
	DKIENT(debug_attach),
	DKIENT(__task_capable),
	DKIENT(system_bootinfo),
	DKIENT(_phys_to_virt),
	DKIENT(_virt_to_phys),
};

/*
 * Increment reference count on an active device.
 * This routine checks if the specifid device is valid.
 * It returns 0 on success, or -1 on failure.
 */
static int device_reference(device_t dev)
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
 * Release reference count on a device.
 */
static void device_release(device_t dev)
{
	sched_lock();
	if (--dev->ref_count == 0) {
		list_remove(&dev->link);
		kmem_free(dev);
	}
	sched_unlock();
}

/*
 * Look up a device object for specified name string.
 * Return device ID on success, or NULL on failure.
 * This must be called with scheduler locked.
 */
static device_t device_lookup(char *name)
{
	list_t head, n;
	device_t dev;

	if (name == NULL)
		return NULL;

	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (!strncmp(dev->name, name, MAX_DEVNAME))
			return dev;
	}
	return NULL;
}

/*
 * Create new device object.
 *
 * @io:   pointer to device I/O routines
 * @name: string for device name
 *
 * A device object is created by the device driver to provide
 * I/O services to applications.
 * Returns device ID on success, or 0 on failure.
 */
static device_t device_create(devio_t io, char *name)
{
	device_t dev;
	size_t len;

	IRQ_ASSERT();

	len = strnlen(name, MAX_DEVNAME);
	if (len == 0 || len >= MAX_DEVNAME)
		return 0;	/* Invalid name */

	sched_lock();
	if ((dev = device_lookup(name)) != NULL) {
		/* The device name is already used */
		sched_unlock();
		return 0;
	}
	if ((dev = kmem_alloc(sizeof(*dev))) == NULL) {
		sched_unlock();
		return 0;
	}
	strlcpy(dev->name, name, len + 1);
	dev->devio = io;
	dev->ref_count = 1;
	dev->magic = DEVICE_MAGIC;
	list_insert(&device_list, &dev->link);
	sched_unlock();
	return dev;
}

/*
 * Delete specified device object. If some another thread
 * refers the target device, it will be postponed until
 * its reference count becomes 0.
 */
static int device_delete(device_t dev)
{
	IRQ_ASSERT();

	if (device_reference(dev))
		return ENODEV;
	/*
	 * Release the device twice to delete it.
	 */
	device_release(dev);
	device_release(dev);
	return 0;
}

/*
 * Open specified device.
 *
 * @name: device name (null-terminated)
 * @mode: open mode. (like O_RDONLY etc.)
 * @dev:  device handle of opened device to be returned.
 *
 * Even if the target driver does not have an open routine, this
 * function does not return an error. By using this mechanism, an
 * application can check whether the specific device exists or not.
 */
__syscall int device_open(char *name, int mode, device_t *pdev)
{
	char str[MAX_DEVNAME];
	device_t dev;
	size_t len;
	int err = 0;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (umem_strnlen(name, MAX_DEVNAME, &len))
		return EFAULT;
	if (len == 0)
		return ENOENT;
	if (len >= MAX_DEVNAME)
		return ENAMETOOLONG;

	if (umem_copyin(name, str, len + 1))
		return EFAULT;

	sched_lock();
	if ((dev = device_lookup(str)) == NULL) {
		sched_unlock();
		return ENXIO;
	}
	(void)device_reference(dev);
	sched_unlock();

	if (dev->devio->open)
		err = (dev->devio->open)(dev, mode);

	if (err == 0)
		err = umem_copyout(&dev, pdev, sizeof(device_t));

	device_release(dev);
	return err;
}

/*
 * Close a device.
 *
 * Even if the target driver does not have close routine,
 * this function does not return any error.
 */
__syscall int device_close(device_t dev)
{
	int err = 0;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_reference(dev))
		return ENODEV;

	if (dev->devio->close)
		err = (dev->devio->close)(dev);

	device_release(dev);
	return err;
}

/*
 * Read from a device.
 *
 * @dev:   device id 
 * @buf:   pointer to read buffer
 * @nbyte: number of bytes to read. actual read count is set in return.
 * @blkno: block number (for block device)
 *
 * Note: The size of one block is device dependent.
 */
__syscall int device_read(device_t dev, void *buf, size_t *nbyte, int blkno)
{
	size_t count;
	int err;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_reference(dev))
		return ENODEV;

	if (dev->devio->read == NULL) {
		device_release(dev);
		return EBADF;
	}
	if (umem_copyin(nbyte, &count, sizeof(u_long)) ||
	    vm_access(buf, count, ATTR_WRITE)) {
		device_release(dev);
		return EFAULT;
	}
	err = (dev->devio->read)(dev, buf, &count, blkno);
	if (!err)
		err = umem_copyout(&count, nbyte, sizeof(u_long));

	device_release(dev);
	return err;
}

/*
 * Write to a device.
 *
 * @dev:   device id 
 * @buf:   pointer to write buffer
 * @nbyte: number of bytes to write. actual write count is set in return.
 * @blkno: block number (for block device)
 */
__syscall int device_write(device_t dev, void *buf, size_t *nbyte, int blkno)
{
	size_t count;
	int err;

	if (!task_capable(CAP_DEVIO))
		return EPERM;

	if (device_reference(dev))
		return ENODEV;

	if (dev->devio->write == NULL) {
		device_release(dev);
		return EBADF;
	}
	if (umem_copyin(nbyte, &count, sizeof(u_long)) ||
	    vm_access(buf, count, ATTR_READ)) {
		device_release(dev);
		return EFAULT;
	}
	err = (dev->devio->write)(dev, buf, &count, blkno);
	if (!err)
		err = umem_copyout(&count, nbyte, sizeof(u_long));

	device_release(dev);
	return err;
}

/*
 * I/O control request.
 *
 * @dev: device id 
 * @cmd: command
 * @arg: argument
 *
 * A command and an argument are completely device dependent.
 * If argument type is pointer, the driver routine must validate
 * the pointer address.
 */
__syscall int device_ioctl(device_t dev, int cmd, u_long arg)
{
	int err;

	if (!task_capable(CAP_DEVIO))
		return EPERM;
	if (device_reference(dev))
		return ENODEV;

	err = EBADF;
	if (dev->devio->ioctl)
		err = (dev->devio->ioctl)(dev, cmd, arg);

	device_release(dev);
	return err;
}

/*
 * Broadcast the event message to all device objects.
 * This function can be called from the kernel mode drivers.
 *
 * @event: event code
 * @force: true to ignore the return value from driver.
 *
 * If force argument is true, a kernel will continue event
 * notification even if some driver returns error. If at least
 * one driver returns an error, this routine returns EIO error.
 *
 * If force argument is false, a kernel stops the event processing
 * when at least one driver returns an error. In this case,
 * device_broadcast will return the error code which is returne
 * by the driver.
 */
static int device_broadcast(int event, int force)
{
	device_t dev;
	list_t head, n;
	int ret, err = 0;

	sched_lock();
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (dev->devio->event) {
			if ((ret = (dev->devio->event)(event)) != 0) {
				if (force) {
					err = EIO;
				} else {
					err = ret;
					break;
				}
			}
		}
	}
	sched_unlock();
	return err;
}

/*
 * Return device information.
 */
int device_info(struct info_device *info)
{
	u_long index, target = info->cookie;
	device_t dev;
	devio_t io;
	list_t head, n;

	sched_lock();
	index = 0;
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n), index++) {
		dev = list_entry(n, struct device, link);
		io = dev->devio;
		if (index == target)
			break;
	}
	if (n == head) {
		sched_unlock();
		return ESRCH;
	}
	strlcpy(info->name, dev->name, MAX_DEVNAME);
	sched_unlock();
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void device_dump(void)
{
	device_t dev;
	devio_t io;
	list_t head, n;

	printk("Device dump:\n");
	printk(" device   open     close    read     write    ioctl    event    name\n");
	printk(" -------- -------- -------- -------- -------- -------- -------- ------------\n");

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
static void null_func(void)
{
	return;
}
#endif

static void system_bootinfo(struct boot_info **info)
{
	ASSERT(info != NULL);

	*info = boot_info;
	return;
}

static void *_phys_to_virt(void *p_addr)
{
	return phys_to_virt(p_addr);
}

static void *_virt_to_phys(void *v_addr)
{
	return virt_to_phys(v_addr);
}

/*
 * Initialize device driver module.
 */
void device_init(void)
{
	struct img_info *img;
	void (*drv_entry)(const dki_func_t *);

	img = &boot_info->driver;
	if (img == NULL)
		return;

	drv_entry = (void (*)())img->entry;
	if (drv_entry == NULL)
		return;
	/*
	 * Call driver module and initialize all device drivers.
	 */
	drv_entry(driver_service);
}
