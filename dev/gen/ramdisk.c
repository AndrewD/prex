/*-
 * Copyright (c) 2006, Kohsuke Ohtani
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
 * ramdisk.c - RAM disk driver
 */

#include <driver.h>

/* #define DEBUG_RAMDISK 1 */

#ifdef DEBUG_RAMDISK
#define rd_printf(fmt, ...)	printk("%s: " fmt, __FUNCTION__ , ## __VA_ARGS__)
#else
#define rd_printf(fmt, ...)	do {} while (0)
#endif

/* Block size */
#define BSIZE		512

static int ramdisk_open(device_t dev, int mode);
static int ramdisk_close(device_t dev);
static int ramdisk_read(device_t dev, char *buf, size_t *nbyte, int blkno);
static int ramdisk_write(device_t dev, char *buf, size_t *nbyte, int blkno);
static int ramdisk_init(void);

/*
 * Driver structure
 */
struct driver ramdisk_drv __driver_entry = {
	/* name */	"RAM disk",
	/* order */	6,
	/* init */	ramdisk_init,
};

static struct devio ramdisk_io = {
	.open = ramdisk_open,
	.close = ramdisk_close,
	.read = ramdisk_read,
	.write = ramdisk_write,
};

static device_t ramdisk_dev;	/* Device object */

static char *img_start;
static size_t img_size;
static int open;

static int
ramdisk_open(device_t dev, int mode)
{
	if (img_size == 0)
		return EIO;

	open++;
	return 0;
}

static int
ramdisk_close(device_t dev)
{
	if (open <= 0)
		return EBADF;

	if (--open == 0) {	/* free memory when ramdisk closed */
		char *end = (void *)PAGE_ALIGN(img_start + img_size);
		char *addr = (void *)PAGE_TRUNC(img_start);
		size_t size = end - addr;
		printk("freeing RAM disk at 0x%p (%dK bytes)\n",
		       img_start, img_size/1024);
		page_free(addr, size);
		img_size = 0;
	}
	return 0;
}

static int
ramdisk_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	void *kbuf;
	size_t nr_read;

	rd_printf("read buf=%x nbyte=%d blkno=%x\n", buf, *nbyte, blkno);

	/* Check overrun */
	if ((size_t)blkno * BSIZE > img_size) {
		rd_printf("Overrun!\n", );
		return EIO;
	}
	nr_read = *nbyte;
	if ((size_t)blkno * BSIZE + nr_read > img_size)
		nr_read = img_size - blkno * BSIZE;

	/* Translate buffer address to kernel address */
	kbuf = kmem_map(buf, nr_read);
	if (kbuf == NULL)
		return EFAULT;

	/* Copy data */
	memcpy(kbuf, img_start + blkno * BSIZE, nr_read);
	*nbyte = nr_read;
	return 0;
}

/*
 * Data written to this device is discarded.
 */
static int
ramdisk_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	void *kbuf;
	size_t nr_write;

	rd_printf("write buf=%x nbyte=%d blkno=%x\n", buf, *nbyte, blkno);

	/* Check overrun */
	if ((size_t)blkno * BSIZE > img_size)
		return EIO;
	nr_write = *nbyte;
	if ((size_t)blkno * BSIZE + nr_write > img_size)
		nr_write = img_size - blkno * BSIZE;

	/* Translate buffer address to kernel address */
	kbuf = kmem_map(buf, nr_write);
	if (kbuf == NULL)
		return EFAULT;

	/* Copy data */
	memcpy(img_start + blkno * BSIZE, kbuf, nr_write);
	*nbyte = nr_write;
	return 0;
}

/*
 * Initialize
 */
static int
ramdisk_init(void)
{
	struct boot_info *boot_info;
	struct mem_map *rd;

	machine_bootinfo(&boot_info);
	rd = (struct mem_map *)&boot_info->ram_disk;
	img_start = (char *)phys_to_virt((void *)rd->start);
	img_size = rd->size;
	if (img_size == 0)
		return -1;
	printk("RAM disk at 0x%p (%dK bytes)\n", img_start, img_size/1024);

	/* Create device object */
	ramdisk_dev = device_create(&ramdisk_io, "ram0", DF_BLK);
	ASSERT(ramdisk_dev);
	return 0;
}
