/*
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * time.c - time and date management routine
 */

#include <prex/prex.h>
#include <string.h>
#include <errno.h>
#include <stdio.h>

#define DAYSPERYEAR	(31+28+31+30+31+30+31+31+30+31+30+31)

static const int daysinmonth[] =
    { 31, 28, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31 };


static int is_leap(int year)
{
	if ((year % 4 == 0) && (year % 100 != 0))
		return 1;
	if (year % 400 == 0)
		return 1;
	return 0;
}

u_long get_time(void)
{
	device_t rtc_dev;
	int err;
	u_long sec;
	size_t len;

	if ((err = device_open("rtc", 0, &rtc_dev)) != 0) {
		printf("error to open rtc device!\n");
		return 0;
	}		
	len = sizeof(sec);
	if ((err = device_read(rtc_dev, &sec, &len, 0)) != 0) {
		printf("error in reading from rtc device\n");
		device_close(rtc_dev);
		return 0;
	}
	device_close(rtc_dev);
	return sec;
}

int cmd_time(int argc, char **argv)
{
	u_long sys_time;
	unsigned int sec, min, hour;

	
	sys_time = get_time();
	if (sys_time == 0)
		return 0;

	sec = sys_time % 60;
	min = (sys_time / 60) % 60;
	hour = (sys_time / 60 / 60) % 24;
	printf("Current time: %d:%d:%d\n", hour, min, sec);
	return 0;
}

int cmd_date(int argc, char **argv)
{
	u_long sys_time;
	unsigned int day, year, mon;
	int i;

	sys_time = get_time();
	if (sys_time == 0)
		return 0;
	sys_time = sys_time / 60 / 60 / 24;

	for (year = 1970, day = DAYSPERYEAR + is_leap(year);
	     sys_time >= day;
	     year++, day = DAYSPERYEAR + is_leap(year))
		sys_time -= day;
	     
	for (mon = 0; ; mon++) {
		i = daysinmonth[mon];
		if (mon == 1 && is_leap(year))
			i++;
		if (sys_time < i)
			break;
		sys_time -= i;
	}
	mon++;
	day = sys_time + 1;
	printf("Current date: %d/%d/%d\n", year, mon, day);

	return 0;
}
