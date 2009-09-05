/*-
 * Copyright (c) 2008, Lazarenko Andrew
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
 * wdt.c - AT91x40 watchdog driver.
 */

#include <driver.h>
#include <conf/config.h>


#define DEBUG_WATCHDOG 1

#ifdef DEBUG_WATCHDOG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif


#define WD_BASE		0xFFFF8000

/* Watchdog registers */
#define WD_OMR		(*(volatile uint32_t *)(WD_BASE + 0x00))
#define WD_CMR		(*(volatile uint32_t *)(WD_BASE + 0x04))
#define WD_CR		(*(volatile uint32_t *)(WD_BASE + 0x08))
#define WD_SR		(*(volatile uint32_t *)(WD_BASE + 0x0c))

/* WD_OMR - Overflow mode register */
#define OMR_WDEN	(1 << 0)
#define OMR_RSTEN	(1 << 1)
#define OMR_IRQEN	(1 << 2)
#define OMR_EXTEN	(1 << 3)
#define OMR_OKEY	(0x234 << 4)

/* WD_CMR - Clock mode register */
#define CMR_MCK8	(0x0)
#define CMR_MCK32	(0x1)
#define CMR_MCK128	(0x2)
#define CMR_MCK1024	(0x3)
#define CMR_HPCV	(0xF << 2)
#define CMR_CKEY	(0x06E << 7)

/* WD_CR - Control register */
#define CR_RSTKEY	(0xC071 << 0)


static int wdt_init(void);
static void heartbeat(void *arg);

#ifdef CONFIG_WATCHDOG_INTERVAL
#define WDT_INTERVAL   CONFIG_WATCHDOG_INTERVAL
#else
#define WDT_INTERVAL   1000
#endif

/*
 * Driver structure
 */
struct driver wdt_drv = {
	/* name */ "AT91 Watchdog",
	/* order */ 1,
	/* init */  wdt_init,
};

static struct devio wdt_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	NULL,
	/* write */	NULL,
	/* ioctl */	NULL,
	/* event */	NULL,
};

static device_t wdt_dev;
static struct timer wdt_timer;

/*
 * Timer tick for watchdog resetting
 */
static void
heartbeat(void *arg)
{
	/* Reset the watchdog counter */
	WD_CR = CR_RSTKEY;

	timer_callout(&wdt_timer, WDT_INTERVAL / 2, &heartbeat, NULL);
}

/*
 * Init
 */
static int
wdt_init(void)
{
	uint32_t counter, hpcv;

	wdt_dev = device_create(&wdt_io, "wdt", DF_CHR);
	ASSERT(wdt_dev);

	timer_init(&wdt_timer);
	timer_callout(&wdt_timer, WDT_INTERVAL / 2, &heartbeat, 0);

	/* Config watchdog interval counter (see AT91x40 datasheet) */
	counter = ((CONFIG_MCU_FREQ / (1000)) * WDT_INTERVAL) / 1024;
	if (counter > 0xFFFF) {
		panic("wdt: Time interval not supported by H/W!");
	}
	hpcv = (counter >> 10) & CMR_HPCV;

	DPRINTF(("wdt: Counter=%x\n", counter));
	DPRINTF(("wdt: HPCV=%x\n", hpcv >> 2));

	WD_CMR = CMR_CKEY | CMR_MCK1024 | hpcv;

	DPRINTF(("wdt: Interval %d msec\n", WDT_INTERVAL));

	/* Reset the watchdog counter */
	WD_CR = CR_RSTKEY;

	/* Start the watchdog timer */
	WD_OMR = OMR_OKEY | OMR_WDEN | OMR_RSTEN;

	return 0;
}
