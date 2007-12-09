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
 *  - Forward user I/O requests to the drivers after checking paremters.
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

/* Forward functions */
static device_t device_create(devio_t io, char *name);
static int device_delete(device_t dev);
static int device_broadcast(int event);
static void system_bootinfo(struct boot_info **info);
static void *_phys_to_virt(void *p_addr);
static void *_virt_to_phys(void *v_addr);

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
 * Driver service table
 *
 * Device drivers can call kernel functions listed in this table.
 */
static void *const driver_service[] = {
	device_create,
	device_delete,
	device_broadcast,
	umem_copyin,
	umem_copyout,
	umem_strnlen,
	kmem_alloc,
	kmem_free,
	kmem_map,
	page_alloc,
	page_free,		/* 10 */
	page_reserve,
	irq_attach,
	irq_detach,
	irq_lock,
	irq_unlock,
	timer_timeout,
	timer_stop,
	timer_delay,
	sched_lock,
	sched_unlock,		/* 20 */
	sched_tsleep,
	sched_wakeup,
	sched_stat,
	printk,
	panic,
	system_reset,
	kernel_dump,
	debug_attach,
	task_capable,
	system_bootinfo,	/* 30 */
	_phys_to_virt,
	_virt_to_phys,
};

/*
 * Look up a device object for specified name string.
 * Return device ID on success, or NULL on failure.
 */
static device_t device_lookup(char *name)
{
	list_t head, n;
	device_t dev;

	if (name == NULL)
		return NULL;

	sched_lock();
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (!strncmp(dev->name, name, MAX_DEVNAME)) {
			sched_unlock();
			return dev;
		}
	}
	sched_unlock();
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
	if (device_lookup(name))
		return 0;	/* Same device name is already used */

	if ((dev = kmem_alloc(sizeof(struct device))) == NULL)
		return 0;
	strlcpy(dev->name, name, len + 1);
	dev->devio = io;
	dev->magic = DEVICE_MAGIC;

	sched_lock();
	list_insert(&device_list, &dev->link);
	sched_unlock();
	return dev;
}

/*
 * Delete specified device object.
 */
static int device_delete(device_t dev)
{
	IRQ_ASSERT();

	if (!device_valid(dev))
		return ENODEV;

	sched_lock();
	list_remove(&dev->link);
	kmem_free(dev);
	sched_unlock();
	return 0;
}

/*
 * Open specified device.
 *
 * @name: device name (null-terminated)
 * @mode: open mode. 
 * @dev:  device handle of opened device to be returned.
 *
 * Open mode is generic open flag like O_RDONLY etc.
 *
 * Even if the target driver does not have an open routine, this
 * function does not return an error. By using this mechanism, an
 * application can check whether the specific device exists or not.
 */
__syscall int device_open(char *name, int mode, device_t *dev)
{
	char str[MAX_DEVNAME];
	device_t d;
	int err;
	size_t len;

	if (!capable(CAP_DEVIO))
		return EPERM;

	if (umem_strnlen(name, MAX_DEVNAME, &len))
		return EFAULT;
	if (len == 0)
		return ENOENT;
	if (len >= MAX_DEVNAME)
		return ENAMETOOLONG;

	if (umem_copyin(name, str, len + 1) != 0)
		return EFAULT;

	if ((d = device_lookup(str)) == NULL)
		return ENXIO;

	if (d->devio->open) {
		if ((err = (d->devio->open)(d, mode)) != 0)
			return err;
	}
	return (umem_copyout(&d, dev, sizeof(device_t)));
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

	if (!capable(CAP_DEVIO))
		return EPERM;
	if (!device_valid(dev))
		return ENODEV;

	if (dev->devio->close)
		err = (dev->devio->close)(dev);

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

	if (!capable(CAP_DEVIO))
		return EPERM;
	if (!device_valid(dev))
		return ENODEV;
	if (dev->devio->read == NULL)
		return EBADF;
	if (umem_copyin(nbyte, &count, sizeof(u_long)) != 0)
		return EFAULT;
	if (vm_access(buf, count, ATTR_WRITE))
		return EFAULT;

	if ((err = (dev->devio->read)(dev, buf, &count, blkno)) != 0)
		return err;
	return (umem_copyout(&count, nbyte, sizeof(u_long)));
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

	if (!capable(CAP_DEVIO))
		return EPERM;
	if (!device_valid(dev))
		return ENODEV;
	if (dev->devio->write == NULL)
		return EBADF;
	if (umem_copyin(nbyte, &count, sizeof(u_long)) != 0)
		return EFAULT;
	if (vm_access(buf, count, ATTR_READ))
		return EFAULT;

	if ((err = (dev->devio->write)(dev, buf, &count, blkno)) != 0)
		return err;
	return (umem_copyout(&count, nbyte, sizeof(u_long)));
}

/*
 * I/O control request.
 * A command and an argument are completely device dependent.
 *
 * Note: If argument type is pointer, the driver routine must validate it.
 */
__syscall int device_ioctl(device_t dev, int cmd, u_long arg)
{
	if (!capable(CAP_DEVIO))
		return EPERM;
	if (!device_valid(dev))
		return ENODEV;
	if (dev->devio->ioctl == NULL)
		return EBADF;

	return (dev->devio->ioctl)(dev, cmd, arg);
}

/*
 * Broadcast specified event to all devices.
 * This function can be called from the kernel mode drivers.
 *
 * TODO: Take care about the call order with driver priority.
 */
static int device_broadcast(int event)
{
	device_t dev;
	list_t head, n;
	int err;

	sched_lock();
	err = 0;
	head = &device_list;
	for (n = list_first(head); n != head; n = list_next(n)) {
		dev = list_entry(n, struct device, link);
		if (dev->devio->event) {
			if ((err = (dev->devio->event)(event)) != 0)
				break;
		}
	}
	sched_unlock();
	return err;
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
	void (*drv_entry)(void const *);

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
