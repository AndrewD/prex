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
 * fdd.c - Floppy disk drive (Intel 82078 FDC)
 */

/*
 *  State transition table:
 *
 *    State     Interrupt Timeout   Error
 *    --------- --------- --------- ---------
 *    Off       N/A       On        N/A
 *    On        N/A       Reset     N/A
 *    Reset     Recal     Off       N/A
 *    Recal     Seek      Off       Off
 *    Seek      IO        Reset     Off
 *    IO        Ready     Reset     Off
 *    Ready     N/A       Off       N/A
 *
 */

#include <driver.h>

/* #define DEBUG_FDD 1 */

#ifdef DEBUG_FDD
#define DPRINTF(a)	printf a
#else
#define DPRINTF(a)
#endif

#define FDC_IRQ		6	/* IRQ6 */
#define FDC_DMA		2	/* DMA2 */

#define SECTOR_SIZE	512
#define TRACK_SIZE	(SECTOR_SIZE * 18)
#define INVALID_TRACK	-1

/* I/O ports */
#define FDC_DOR		0x3f2	/* digital output register */
#define FDC_MSR		0x3f4	/* main status register (in) */
#define FDC_DSR		0x3f4	/* data rate select register (out) */
#define FDC_DAT		0x3f5	/* data register */
#define FDC_DIR		0x3f7	/* digital input register (in) */
#define FDC_CCR		0x3f7	/* configuration control register (out) */

/* Command bytes */
#define CMD_SPECIFY	0x03	/* specify drive timing */
#define CMD_DRVSTS	0x04
#define CMD_WRITE	0xc5	/* sector write, multi-track */
#define CMD_READ	0xe6	/* sector read */
#define CMD_RECAL	0x07	/* recalibrate */
#define CMD_SENSE	0x08	/* sense interrupt status */
#define CMD_FORMAT	0x4d	/* format track */
#define CMD_SEEK	0x0f	/* seek track */
#define CMD_VERSION	0x10	/* FDC version */

/* Floppy Drive Geometries */
#define FDG_HEADS	2
#define FDG_TRACKS	80
#define FDG_SECTORS	18
#define FDG_GAP3FMT	0x54
#define FDG_GAP3RW	0x1b

/* FDC state */
#define FDS_OFF		0	/* motor off */
#define FDS_ON		1	/* motor on */
#define FDS_RESET	2	/* reset */
#define FDS_RECAL	3	/* recalibrate */
#define FDS_SEEK	4	/* seek */
#define FDS_IO		5	/* read/write */
#define FDS_READY	6	/* ready */

struct fdd_softc {
	device_t	dev;		/* device object */
	int		isopen;		/* number of open counts */
	int		track;		/* Current track for read buffer */
	struct irp	irp;		/* I/O request packet */
	dma_t		dma;		/* DMA handle */
	irq_t		irq;		/* interrupt handle */
	timer_t		tmr;		/* timer id */
	int		stat;		/* current state */
	void		*rbuf;		/* DMA buffer for read (1 track) */
	void		*wbuf;		/* DMA buffer for write (1 sector) */
	u_char		result[7];	/* result from fdc */
};

static void	fdc_timeout(void *);

static int	fdd_open(device_t, int);
static int	fdd_close(device_t);
static int	fdd_read(device_t, char *, size_t *, int);
static int	fdd_write(device_t, char *, size_t *, int);
static int	fdd_probe(struct driver *);
static int	fdd_init(struct driver *);


static struct devops fdd_devops = {
	/* open */	fdd_open,
	/* close */	fdd_close,
	/* read */	fdd_read,
	/* write */	fdd_write,
	/* ioctl */	no_ioctl,
	/* devctl */	no_devctl,
};

struct driver fdd_driver = {
	/* name */	"fdd",
	/* devsops */	&fdd_devops,
	/* devsz */	sizeof(struct fdd_softc),
	/* flags */	0,
	/* probe */	fdd_probe,
	/* init */	fdd_init,
	/* shutdown */	NULL,
};


/*
 * Send data to FDC
 * Return -1 on failure
 */
static int
fdc_out(int dat)
{
	int i;

	for (i = 0; i < 100000; i++) {
		if ((bus_read_8(FDC_MSR) & 0xc0) == 0x80) {
			delay_usec(1);
			bus_write_8(FDC_DAT, (u_char)(dat & 0xff));
			delay_usec(1);
			return 0;
		}
	}
	DPRINTF(("fdc: timeout msr=%x\n", bus_read_8(FDC_MSR)));
	return -1;
}

/* Return number of result bytes */
static int
fdc_result(struct fdd_softc *sc)
{
	int i, msr, index = 0;

	for (i = 0; i < 50000; i++) {	/* timeout=500msec */
		msr = bus_read_8(FDC_MSR);
		if ((msr & 0xd0) == 0x80) {
			return index;
		}
		if ((msr & 0xd0) == 0xd0) {
			if (index > 6) {
				DPRINTF(("fdc: overrun\n"));
				return -1;
			}
			sc->result[index++] = bus_read_8(FDC_DAT);
		}
		delay_usec(10);
	}
	DPRINTF(("fdc: timeout\n"));
	return -1;
}

/*
 * Stop motor. (No interrupt)
 */
static void
fdc_off(struct fdd_softc *sc)
{
	DPRINTF(("fdc: motor off\n"));

	sc->stat = FDS_OFF;
	timer_stop(&sc->tmr);
	bus_write_8(FDC_DOR, 0x0c);
	delay_usec(1);
}

/*
 * Start motor and wait 250msec. (No interrupt)
 */
static void
fdc_on(struct fdd_softc *sc)
{
	DPRINTF(("fdc: motor on\n"));

	sc->stat = FDS_ON;
	bus_write_8(FDC_DOR, 0x1c);
	delay_usec(1);

	timer_callout(&sc->tmr, 250, &fdc_timeout, sc);
}

static void
fdc_error(struct fdd_softc *sc, int error)
{
	struct irp *irp = &sc->irp;

	DPRINTF(("fdc: error=%d\n", error));

	dma_stop(sc->dma);
	irp->error = error;
	sched_wakeup(&irp->iocomp);
	fdc_off(sc);
}

/*
 * Reset FDC and wait an intterupt.
 * Timeout is 500msec.
 */
static void
fdc_reset(struct fdd_softc *sc)
{
	DPRINTF(("fdc: reset\n"));

	sc->stat = FDS_RESET;
	timer_callout(&sc->tmr, 500, &fdc_timeout, sc);
	bus_write_8(FDC_DOR, 0x18);	/* motor0 enable, DMA enable */
	delay_usec(20);			/* wait 20 usec while reset */
	bus_write_8(FDC_DOR, 0x1c);	/* clear reset */
	delay_usec(1);
}

/*
 * Recalibrate FDC and wait an interrupt.
 * Timeout is 5sec.
 */
static void
fdc_recal(struct fdd_softc *sc)
{
	DPRINTF(("fdc: recalibrate\n"));

	sc->stat = FDS_RECAL;
	timer_callout(&sc->tmr, 5000, &fdc_timeout, sc);
	fdc_out(CMD_RECAL);
	fdc_out(0);		/* Drive 0 */
}

/*
 * Seek FDC and wait an interrupt.
 * Timeout is 4sec.
 */
static void
fdc_seek(struct fdd_softc *sc)
{
	struct irp *irp = &sc->irp;
	int head, track;

	DPRINTF(("fdc: seek\n"));
	sc->stat = FDS_SEEK;
	head = (irp->blkno % (FDG_SECTORS * FDG_HEADS)) / FDG_SECTORS;
	track = irp->blkno / (FDG_SECTORS * FDG_HEADS);

	timer_callout(&sc->tmr, 4000, &fdc_timeout, sc);

	fdc_out(CMD_SPECIFY);	/* specify command parameter */
	fdc_out(0xd1);		/* Step rate = 3msec, Head unload time = 16msec */
	fdc_out(0x02);		/* Head load time = 2msec, Dma on (0) */

	fdc_out(CMD_SEEK);
	fdc_out(head << 2);
	fdc_out(track);
}

/*
 * Read/write data and wait an interrupt.
 * Timeout is 2sec.
 */
static void
fdc_io(struct fdd_softc *sc)
{
	struct irp *irp = &sc->irp;
	int head, track, sect;
	u_long io_size;
	int read;

	DPRINTF(("fdc: read/write\n"));
	sc->stat = FDS_IO;

	head = (irp->blkno % (FDG_SECTORS * FDG_HEADS)) / FDG_SECTORS;
	track = irp->blkno / (FDG_SECTORS * FDG_HEADS);
	sect = irp->blkno % FDG_SECTORS + 1;
	io_size = irp->blksz * SECTOR_SIZE;
	read = (irp->cmd == IO_READ) ? 1 : 0;

	DPRINTF(("fdc: hd=%x trk=%x sec=%x size=%d read=%d\n",
		 head, track, sect, io_size, read));

	timer_callout(&sc->tmr, 2000, &fdc_timeout, sc);

	dma_setup(sc->dma, irp->buf, io_size, read);

	/* Send command */
	fdc_out(read ? CMD_READ : CMD_WRITE);
	fdc_out(head << 2);
	fdc_out(track);
	fdc_out(head);
	fdc_out(sect);
	fdc_out(2);		/* sector size = 512 bytes */
	fdc_out(FDG_SECTORS);
	fdc_out(FDG_GAP3RW);
	fdc_out(0xff);
}

/*
 * Wake up iorequester.
 * FDC motor is set to off after 5sec.
 */
static void
fdc_ready(struct fdd_softc *sc)
{
	struct irp *irp = &sc->irp;

	DPRINTF(("fdc: wakeup requester\n"));

	sc->stat = FDS_READY;
	sched_wakeup(&irp->iocomp);
	timer_callout(&sc->tmr, 5000, &fdc_timeout, sc);
}

/*
 * Timeout handler
 */
static void
fdc_timeout(void *arg)
{
	struct fdd_softc *sc = arg;
	struct irp *irp = &sc->irp;

	DPRINTF(("fdc: stat=%d\n", sc->stat));

	switch (sc->stat) {
	case FDS_ON:
		fdc_reset(sc);
		break;
	case FDS_RESET:
	case FDS_RECAL:
		DPRINTF(("fdc: reset/recal timeout\n"));
		fdc_error(sc, EIO);
		break;
	case FDS_SEEK:
	case FDS_IO:
		DPRINTF(("fdc: seek/io timeout retry=%d\n", irp->nr_retry));
		if (++irp->ntries <= 3)
			fdc_reset(sc);
		else
			fdc_error(sc, EIO);
		break;
	case FDS_READY:
		fdc_off(sc);
		break;
	default:
		panic("fdc: unknown timeout");
	}
}

/*
 * Interrupt service routine
 * Do not change the fdc_stat in isr.
 */
static int
fdc_isr(void *arg)
{
	struct fdd_softc *sc = arg;
	struct irp *irp = &sc->irp;

	DPRINTF(("fdc_stat=%d\n", sc->stat));

	timer_stop(&sc->tmr);

	switch (sc->stat) {
	case FDS_IO:
		dma_stop(sc->dma);
		/* Fall through */
	case FDS_RESET:
	case FDS_RECAL:
	case FDS_SEEK:
		if (irp->cmd == IO_NONE) {
			DPRINTF(("fdc: invalid interrupt\n"));
			timer_stop(&sc->tmr);
			break;
		}
		return INT_CONTINUE;
	case FDS_OFF:
		break;
	default:
		DPRINTF(("fdc: unknown interrupt\n"));
		break;
	}
	return 0;
}

/*
 * Interrupt service thread
 * This is called when command completion.
 */
static void
fdc_ist(void *arg)
{
	struct fdd_softc *sc = arg;
	struct irp *irp = &sc->irp;
	int i;

	DPRINTF(("fdc_stat=%d\n", sc->stat));
	if (irp->cmd == IO_NONE)
		return;

	switch (sc->stat) {
	case FDS_RESET:
		/* clear output buffer */
		for (i = 0; i < 4; i++) {
			fdc_out(CMD_SENSE);
			fdc_result(sc);
		}
		fdc_recal(sc);
		break;
	case FDS_RECAL:
		fdc_out(CMD_SENSE);
		fdc_result(sc);
		if ((sc->result[0] & 0xf8) != 0x20) {
			DPRINTF(("fdc: recal error\n"));
			fdc_error(sc, EIO);
			break;
		}
		fdc_seek(sc);
		break;
	case FDS_SEEK:
		fdc_out(CMD_SENSE);
		fdc_result(sc);
		if ((sc->result[0] & 0xf8) != 0x20) {
			DPRINTF(("fdc: seek error\n"));
			if (++irp->ntries <= 3)
				fdc_reset(sc);
			else
				fdc_error(sc, EIO);
			break;
		}
		fdc_io(sc);
		break;
	case FDS_IO:
		fdc_result(sc);
		if ((sc->result[0] & 0xd8) != 0x00) {
			if (++irp->ntries <= 3)
				fdc_reset(sc);
			else
				fdc_error(sc, EIO);
			break;
		}
		DPRINTF(("fdc: i/o complete\n"));
		fdc_ready(sc);
		break;
	case FDS_OFF:
		/* Ignore */
		break;
	default:
		ASSERT(0);
	}
}


static int
fdd_open(device_t dev, int mode)
{
	struct fdd_softc *sc = device_private(dev);

	if (sc->isopen > 0)
		return EBUSY;

	sc->isopen++;
	sc->irp.cmd = IO_NONE;
	return 0;
}

static int
fdd_close(device_t dev)
{
	struct fdd_softc *sc = device_private(dev);

	if (sc->isopen != 1)
		return EINVAL;

	sc->isopen--;
	sc->irp.cmd = IO_NONE;

	fdc_off(sc);
	return 0;
}

/*
 * Common routine for read/write
 */
static int
fdd_rw(struct fdd_softc *sc, int cmd, char *buf, u_long blksz, int blkno)
{
	struct irp *irp = &sc->irp;
	int error;

	DPRINTF(("fdd_rw: cmd=%x buf=%x blksz=%d blkno=%x\n",
		 cmd, buf, blksz, blkno));

	irp->cmd = cmd;
	irp->ntries = 0;
	irp->blkno = blkno;
	irp->blksz = blksz;
	irp->buf = buf;
	irp->error = 0;

	sched_lock();
	if (sc->stat == FDS_OFF)
		fdc_on(sc);
	else
		fdc_seek(sc);

	if (sched_sleep(&irp->iocomp) == SLP_INTR)
		error = EINTR;
	else
		error = irp->error;

	sched_unlock();
	return error;
}

/*
 * Read
 *
 * Error:
 *  EINTR   ... Interrupted by signal
 *  EIO     ... Low level I/O error
 *  ENXIO   ... Write protected
 *  EFAULT  ... No physical memory is mapped to buffer
 */
static int
fdd_read(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct fdd_softc *sc = device_private(dev);
	char *kbuf;
	int track, sect, error;
	u_int i, nr_sect;

	DPRINTF(("fdd_read: buf=%x nbyte=%d blkno=%x\n", buf, *nbyte, blkno));

	/* Check overrun */
	if (blkno > FDG_HEADS * FDG_TRACKS * FDG_SECTORS)
		return EIO;

	/* Translate buffer address to kernel address */
	if ((kbuf = kmem_map(buf, *nbyte)) == NULL)
		return EFAULT;

	nr_sect = *nbyte / SECTOR_SIZE;
	error = 0;
	for (i = 0; i < nr_sect; i++) {
		/* Translate the logical sector# to logical track#/sector#. */
		track = blkno / FDG_SECTORS;
		sect = blkno % FDG_SECTORS;

		/*
		 * If target sector does not exist in buffer,
		 * read 1 track (18 sectors) at once.
		 */
		if (track != sc->track) {
			error = fdd_rw(sc, IO_READ, sc->rbuf, FDG_SECTORS,
				       track * FDG_SECTORS);
			if (error != 0) {
				sc->track = INVALID_TRACK;
				break;
			}
			sc->track = track;
		}
		memcpy(kbuf, (char *)sc->rbuf + sect * SECTOR_SIZE,
		       SECTOR_SIZE);
		blkno++;
		kbuf += SECTOR_SIZE;
	}
	*nbyte = i * SECTOR_SIZE;
	return error;
}

/*
 * Write
 *
 * Error:
 *  EINTR   ... Interrupted by signal
 *  EIO     ... Low level I/O error
 *  ENXIO   ... Write protected
 *  EFAULT  ... No physical memory is mapped to buffer
 */
static int
fdd_write(device_t dev, char *buf, size_t *nbyte, int blkno)
{
	struct fdd_softc *sc = device_private(dev);
	char *kbuf, *wbuf;
	int track, sect, error;
	u_int i, nr_sect;

	DPRINTF(("fdd_write: buf=%x nbyte=%d blkno=%x\n", buf, *nbyte, blkno));

	/* Check overrun */
	if (blkno > FDG_HEADS * FDG_TRACKS * FDG_SECTORS)
		return EIO;

	/* Translate buffer address to kernel address */
	if ((kbuf = kmem_map(buf, *nbyte)) == NULL)
		return EFAULT;

	nr_sect = *nbyte / SECTOR_SIZE;
	error = 0;
	for (i = 0; i < nr_sect; i++) {
		/* Translate the logical sector# to track#/sector#. */
		track = blkno / FDG_SECTORS;
		sect = blkno % FDG_SECTORS;

		/*
		 * If target sector exists in read buffer, use it as
		 * write buffer to keep the cache cohrency.
		 */
		if (track == sc->track)
			wbuf = (char *)sc->rbuf + sect * SECTOR_SIZE;
		else
			wbuf = sc->wbuf;

		memcpy(wbuf, kbuf, SECTOR_SIZE);
		error = fdd_rw(sc, IO_WRITE, wbuf, 1, blkno);
		if (error != 0) {
			sc->track = INVALID_TRACK;
			break;
		}
		blkno++;
		kbuf += SECTOR_SIZE;
	}
	*nbyte = i * SECTOR_SIZE;

	DPRINTF(("fdd_write: error=%d\n", error));
	return error;
}

static int
fdd_probe(struct driver *self)
{

	if (bus_read_8(FDC_MSR) == 0xff) {
		printf("Floppy drive not found!\n");
		return ENXIO;
	}
	return 0;
}

static int
fdd_init(struct driver *self)
{
	struct fdd_softc *sc;
	struct irp *irp;
	device_t dev;
	char *buf;
	int i;

	dev = device_create(self, "fd0", D_BLK|D_PROT);
	sc = device_private(dev);
	sc->dev = dev;
	sc->isopen = 0;

	/* Initialize I/O request packet */
	irp = &sc->irp;
	irp->cmd = IO_NONE;
	event_init(&irp->iocomp, "fdd i/o");

	/*
	 * Allocate physical pages for DMA buffer.
	 * Buffer: 1 track for read, 1 sector for write.
	 */
	buf = dma_alloc(TRACK_SIZE + SECTOR_SIZE);
	ASSERT(buf != NULL);
	sc->rbuf = buf;
	sc->wbuf = buf + TRACK_SIZE;
	sc->dma = dma_attach(FDC_DMA);

	/*
	 * Attach IRQ.
	 */
	sc->irq = irq_attach(FDC_IRQ, IPL_BLOCK, 0, fdc_isr, fdc_ist, sc);

	sc->stat = FDS_OFF;
	sc->track = INVALID_TRACK;

	/* Reset FDC */
	bus_write_8(FDC_DOR, 0x08);
	delay_usec(20);
	bus_write_8(FDC_DOR, 0x0C);
	delay_usec(1);

	/* Data rate 500k bps */
	bus_write_8(FDC_CCR, 0x00);

	/* Clear output buffer */
	for (i = 0; i < 4; i++) {
		fdc_out(CMD_SENSE);
		fdc_result(sc);
	}
	return 0;
}
