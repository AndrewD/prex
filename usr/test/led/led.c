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
 * led.c - led driver test program.
 */

#include <prex/prex.h>
#include <sys/ioctl.h>
#include <prex/ioctl.h>
#include <sys/types.h>
#include <stdio.h>


void
check(int err)
{

	if (err == 0) {
		printf("OK\n");
	} else {
		printf("FAIL with error %i", err);
	}
}

int
led_count(device_t dev, u_int *count)
{

	return device_ioctl(dev, LEDIOC_COUNT, count);
}

int
led_on(device_t dev, u_int mask)
{

	return device_ioctl(dev, LEDIOC_ON, &mask);
}

int
led_off(device_t dev, u_int mask)
{

	return device_ioctl(dev, LEDIOC_OFF, &mask);
}

int
led_status(device_t dev, u_int *status)
{

	return device_ioctl(dev, LEDIOC_STATUS, status);
}

int
main(int argc, char *argv[])
{
	u_int led;
	u_int count;
	u_int status;
	u_int all_mask = 0;
	device_t dev;
	int err;

	printf("led driver test program\n");

	printf("Open led device... ");
	err = device_open("led", DO_RDWR, &dev);
	check(err);
	if (err) {
		return 0;
	}
	printf("Query led count... ");
	check(led_count(dev, &count));
	printf("Count is %d\n", count);

	timer_sleep(1000, 0);

	for (led = 0; led < count; ++led) {
		printf("Turn on	 LED %d... ", led);
		check(led_on(dev, 1 << led));
		timer_sleep(1000, 0);
		printf("Turn off LED %d... ", led);
		check(led_off(dev, 1 << led));
		timer_sleep(1000, 0);

		all_mask |= (1 << led);
	}

	printf("Turn on all LEDs... ");
	check(led_on(dev, all_mask));
	printf("Get LED status... ");
	check(led_status(dev, &status));
	printf("Status is %x == %x\n", status, all_mask);

	timer_sleep(1000, 0);

	printf("Turn off all LEDs... ");
	check(led_off(dev, all_mask));
	printf("Get LED status... ");
	check(led_status(dev, &status));
	printf("Status is %x == 0\n", status);

	timer_sleep(1000, 0);

	printf("Close the device...");
	err = device_close(dev);
	check(err);

	return 0;
}
