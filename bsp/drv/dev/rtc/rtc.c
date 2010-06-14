/*
 * Copyright (c) 1988 University of Utah.
 * Copyright (c) 1982, 1990, 1993
 *	The Regents of the University of California.  All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * the Systems Programming Group of the University of Utah Computer
 * Science Department.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * from: Utah $Hdr: clock.c 1.18 91/01/21$
 *
 *	@(#)clock.c	8.2 (Berkeley) 1/12/94
 */

/* Modified for Prex by Kohsuke Ohtani */

/*
 * rtc.c - machine independent RTC driver
 */

#include <sys/time.h>
#include <sys/ioctl.h>

#include <driver.h>
#include <rtc.h>

#define FEBRUARY	2
#define	days_in_year(a) 	(leapyear(a) ? 366 : 365)
#define	days_in_month(a) 	(month_days[(a) - 1])

struct rtc_softc {
	device_t	dev;		/* device object */
	struct rtc_ops	*ops;		/* rtc operations */
	void		*aux;		/* cookie data */
	time_t		boot_sec;	/* Time (sec) at system boot */
	u_long		boot_ticks;	/* Time (ticks) at system boot */
};

static int rtc_ioctl(device_t, u_long, void *);
static int rtc_init(struct driver *);

static struct devops rtc_devops = {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	rtc_ioctl,
	/* devctl */	no_devctl,
};

struct driver rtc_driver = {
	/* name */	"rtc",
	/* devops */	&rtc_devops,
	/* devsz */	sizeof(struct rtc_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	rtc_init,
	/* shutdown */	NULL,
};

static const int month_days[12] = {
	31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
};

static int
leapyear(u_int year)
{
	int rv = 0;

	if ((year & 3) == 0) {
		rv = 1;
		if ((year % 100) == 0) {
			rv = 0;
			if ((year % 400) == 0)
				rv = 1;
		}
	}
	return (rv);
}

time_t
rtc_ymdhms_to_secs(struct clock_ymdhms *dt)
{
	time_t secs;
	u_int i, year, days;

	year = dt->year;

	/*
	 * Compute days since start of time.
	 * First from years, then from months.
	 */
	days = 0;
	for (i = POSIX_BASE_YEAR; i < year; i++)
		days += days_in_year(i);
	if (leapyear(year) && dt->mon > FEBRUARY)
		days++;

	/* Months */
	for (i = 1; i < dt->mon; i++)
	  	days += days_in_month(i);
	days += (dt->day - 1);

	/* Add hours, minutes, seconds. */
	secs = (time_t)(((days * 24 + dt->hour) * 60 + dt->min)
			* 60 + dt->sec);

	return (secs);
}

/* This function uses a copy of month_days[] */
#undef	days_in_month
#define	days_in_month(a) 	(mthdays[(a) - 1])

void
rtc_secs_to_ymdhms(time_t secs, struct clock_ymdhms *dt)
{
	u_int mthdays[12];
	u_int i, days;
	u_int rsec;	/* remainder seconds */

	memcpy(mthdays, month_days, sizeof(mthdays));

	days = secs / SECDAY;
	rsec = secs % SECDAY;

	/* Day of week (Note: 1/1/1970 was a Thursday) */
	dt->dow = (days + 4) % 7;

	/* Subtract out whole years, counting them in i. */
	for (i = POSIX_BASE_YEAR; days >= days_in_year(i); i++)
		days -= days_in_year(i);
	dt->year = (u_short)i;

	/* Subtract out whole months, counting them in i. */
	if (leapyear(i))
		days_in_month(FEBRUARY) = 29;
	for (i = 1; days >= days_in_month(i); i++)
		days -= days_in_month(i);
	dt->mon = i;

	/* Days are what is left over (+1) from all that. */
	dt->day = days + 1;

	/* Hours, minutes, seconds are easy */
	dt->hour = rsec / 3600;
	rsec = rsec % 3600;
	dt->min  = rsec / 60;
	rsec = rsec % 60;
	dt->sec  = rsec;
}

static int
rtc_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct rtc_softc *sc = device_private(dev);
	struct timeval tv;
	int error = 0;
	u_long msec;

	switch (cmd) {
	case RTCIOC_GET_TIME:
		/*
		 * Calculate current time (sec/usec) from
		 * boot time and current tick count.
		 */
		msec = hztoms(timer_ticks() - sc->boot_ticks);
		tv.tv_sec = sc->boot_sec + (msec / 1000);
		tv.tv_usec = (long)((msec * 1000) % 1000000);
		if (copyout(&tv, arg, sizeof(tv)))
			return EFAULT;
		break;

	case RTCIOC_SET_TIME:
		/*
		 * TODO: We need new capability to set time.
		 */
		error = EINVAL;
		break;
	default:
		return EINVAL;
	}
	return error;
}

void
rtc_attach(struct rtc_ops *ops, void *aux)
{
	struct rtc_softc *sc;
	device_t dev;
	struct timeval tv;

	dev = device_create(&rtc_driver, "rtc", D_CHR);

	sc = device_private(dev);
	sc->dev = dev;
	sc->ops = ops;
	sc->aux = aux;

	/* Save boot time for later use. */
	ops->gettime(aux, &tv);
	sc->boot_sec = tv.tv_sec;
	sc->boot_ticks = timer_ticks();
}

static int
rtc_init(struct driver *self)
{

	return 0;
}
