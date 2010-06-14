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
#define DPRINTF(a)	printf a
#else
#define DPRINTF(a)
#endif

/* Block size */
#define BSIZE		512

struct ramdisk_softc {
	device_t	dev;		/* device object */
	char		*addr;		/* base address of image */
	size_t		size;		/* image size */
};

static int ramdisk_read(device_t, char *, size_t *, int);
static int ramdisk_write(device_t, char *, size_t *, int);
static int ramdisk_probe(struct driver *);
static int ramdisk_init(struct driver *);

static struct devops ramdisk_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	ramdisk_read,
	/* write */	ramdisk_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver ramdisk_driver = {
	/* name */	"ramdisk",
	/* devops */	&ramdisk_devops,
	/* devsz */	sizeof(struct ramdisk_softc),
	/* flags */	0,
	/* probe */	ramdisk_probe,
	/* init */	ramdisk_init,
	/* shutdown */	NULL,
};

static int
ramdisk_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct ramdisk_softc *sc = device_private(dev);
	int offset = blkno * BSIZE;
	void *kbuf;
	size_t nr_read;

	DPRINTF(("ramdisk_read: buf=%x nbyte=%d blkno=%x\n",
		 buf, *nbyte, blkno));

	/* Check overrun */
	if (offset > (int)sc->size) {
		DPRINTF(("ramdisk_read: overrun!\n"));
		return EIO;
	}
	nr_read = *nbyte;
	if (offset + nr_read > (int)sc->size)
		nr_read = sc->size - offset;

	/* Translate buffer address to kernel address */
	if ((kbuf = kmem_map(buf, nr_read)) == NULL) {
		return EFAULT;
	}

	/* Copy data */
	memcpy(kbuf, sc->addr + offset, nr_read);
	*nbyte = nr_read;
	return 0;
}

static int
ramdisk_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct ramdisk_softc *sc = device_private(dev);
	int offset = blkno * BSIZE;
	void *kbuf;
	size_t nr_write;

	DPRINTF(("ramdisk_write: buf=%x nbyte=%d blkno=%x\n",
		 buf, *nbyte, blkno));

	/* Check overrun */
	if (offset > (int)sc->size)
		return EIO;
	nr_write = *nbyte;
	if (offset + nr_write > (int)sc->size)
		nr_write = sc->size - offset;

	/* Translate buffer address to kernel address */
	if ((kbuf = kmem_map(buf, nr_write)) == NULL)
		return EFAULT;

	/* Copy data */
	memcpy(sc->addr + offset, kbuf, nr_write);
	*nbyte = nr_write;
	return 0;
}

static int
ramdisk_probe(struct driver *self)
{
	struct bootinfo *bi;
	struct physmem *phys;

	machine_bootinfo(&bi);
	phys = &bi->bootdisk;
	if (phys->size == 0) {
#ifdef DEBUG
		printf("ramdisk: no bootdisk found...\n");
#endif
		return ENXIO;
	}
	return 0;
}

static int
ramdisk_init(struct driver *self)
{
	struct ramdisk_softc *sc;
	struct bootinfo *bi;
	struct physmem *phys;
	device_t dev;

	machine_bootinfo(&bi);
	phys = &bi->bootdisk;

	dev = device_create(self, "ram0", D_BLK|D_PROT);

	sc = device_private(dev);
	sc->dev = dev;
	sc->addr = (char *)ptokv(phys->base);
	sc->size = (size_t)phys->size;

#ifdef DEBUG
	printf("RAM disk at 0x%08x (%dK bytes)\n",
	       (u_int)sc->addr, sc->size/1024);
#endif
	return 0;
}
