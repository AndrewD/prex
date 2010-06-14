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
 * pl030.c - ARM PrimeCell PL030 RTC
 */

#include <sys/time.h>
#include <sys/ioctl.h>

#include <driver.h>
#include <rtc.h>

#define RTC_BASE	CONFIG_PL030_BASE

#define RTC_DR		(RTC_BASE + 0x00)
#define RTC_MR		(RTC_BASE + 0x04)
#define RTC_STAT	(RTC_BASE + 0x08)
#define RTC_EOI		(RTC_BASE + 0x08)
#define RTC_LR		(RTC_BASE + 0x0c)
#define RTC_CR		(RTC_BASE + 0x10)

static int	pl030_init(struct driver *);
static int	pl030_gettime(void *, struct timeval *);
static int	pl030_settime(void *, struct timeval *);


struct driver pl030_driver = {
	/* name */	"pl030",
	/* devops */	NULL,
	/* devsz */	0,
	/* flags */	0,
	/* probe */	NULL,
	/* init */	pl030_init,
	/* shutdown */	NULL,
};

struct rtc_ops pl030_ops = {
	/* gettime */	pl030_gettime,
	/* settime */	pl030_settime,
};

static int
pl030_gettime(void *aux, struct timeval *tv)
{

	tv->tv_usec = 0;
	tv->tv_sec = bus_read_32(RTC_DR);
	printf("******* sec=%d\n", tv->tv_sec);
	return 0;
}

static int
pl030_settime(void *aux, struct timeval *ts)
{
	return 0;
}

static int
pl030_init(struct driver *self)
{

	rtc_attach(&pl030_ops, NULL);
	return 0;
}
