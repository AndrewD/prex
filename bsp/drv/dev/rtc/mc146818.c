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
 * mc146818.c - mc146818 and compatible RTC
 */

#include <sys/time.h>
#include <sys/ioctl.h>

#include <driver.h>
#include <rtc.h>


#define RTC_INDEX	(CONFIG_MC146818_BASE + 0)
#define RTC_DATA	(CONFIG_MC146818_BASE + 1)

#define RTC_SEC		0x00
#define RTC_MIN		0x02
#define RTC_HOUR	0x04
#define RTC_DOW		0x06	/* day of week */
#define RTC_DAY		0x07
#define RTC_MON		0x08
#define RTC_YEAR	0x09
#define RTC_STS_A	0x0a
#define RTC_UIP		  0x80
#define RTC_STS_B	0x0b
#define RTC_BCD		  0x04

static int	mc146818_init(struct driver *);
static int	mc146818_gettime(void *, struct timeval *);
static int	mc146818_settime(void *, struct timeval *);


struct driver mc146818_driver = {
	/* name */	"mc146818",
	/* devops */	NULL,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	NULL,
	/* init */	mc146818_init,
	/* shutdown */	NULL,
};

struct rtc_ops mc146818_ops = {
	/* gettime */	mc146818_gettime,
	/* settime */	mc146818_settime,
};

static u_char
mc_read(u_char index)
{
	u_char val;
	int s;

	s = splhigh();
	bus_write_8(RTC_INDEX, index);
	val = bus_read_8(RTC_DATA);
	splx(s);
	return val;
}

#if 0
static void
mc_write(u_char index, u_char val)
{
	int s;

	s = splhigh();
	bus_write_8(RTC_INDEX, index);
	bus_write_8(RTC_DATA, val);
	splx(s);
}
#endif

static int
mc146818_gettime(void *aux, struct timeval *tv)
{
	struct clock_ymdhms cy;
	int i;

	/* Wait until data ready */
	for (i = 0; i < 1000000; i++)
		if (!(mc_read(RTC_STS_A) & RTC_UIP))
			break;

	cy.nsec = 0;
	cy.sec = mc_read(RTC_SEC);
	cy.min = mc_read(RTC_MIN);
	cy.hour = mc_read(RTC_HOUR);
	cy.dow = mc_read(RTC_DOW);
	cy.day = mc_read(RTC_DAY);
	cy.mon = mc_read(RTC_MON);
	cy.year = mc_read(RTC_YEAR);

	if (!(mc_read(RTC_STS_B) & RTC_BCD)) {
		cy.sec = FROMBCD(cy.sec);
		cy.min = FROMBCD(cy.min);
		cy.hour = FROMBCD(cy.hour);
		cy.day = FROMBCD(cy.day);
		cy.mon = FROMBCD(cy.mon);
		cy.year = FROMBCD(cy.year);
	}
	if (cy.year < 80)
		cy.year += 2000;
	else
		cy.year += 1900;
#ifdef DEBUG
	printf("rtc: system time was %d/%d/%d %d:%d:%d\n",
		cy.year, cy.mon, cy.day, cy.hour, cy.min, cy.sec);
#endif
	tv->tv_usec = 0;
	tv->tv_sec = rtc_ymdhms_to_secs(&cy);
	return 0;
}

static int
mc146818_settime(void *aux, struct timeval *ts)
{
	return 0;
}

static int
mc146818_init(struct driver *self)
{

	rtc_attach(&mc146818_ops, NULL);
	return 0;
}
