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
 * led.c - AT91x40 LED driver
 */

#include <driver.h>
#include <sys/ioctl.h>
#include <prex/ioctl.h>
#include "pio.h"

static int led_init(void);
static int led_ioctl(file_t, u_long, void *);

static uint32_t led_to_pio(u_int mask);
static u_int pio_to_led(uint32_t pio);

static void led_on(u_int mask);
static void led_off(u_int mask);
static u_int led_status(void);

/*
 * Driver structure
 */
struct driver led_drv __driver_entry = {
	/* name */ "LED",
	/* order */ 5,
	/* init */  led_init,
};

static struct devio led_io = {
	/* open */	NULL,
	/* close */	NULL,
	/* read */	NULL,
	/* write */	NULL,
	/* ioctl */	led_ioctl,
	/* event */	NULL,
};

static device_t led_dev;


#define NR_LEDS	       4

static u_int led_pin[NR_LEDS] = {
	1,	/* Red */
	0,	/* Yellow */
	2,	/* Green */
	21	/* Status */
};

static uint32_t
led_to_pio(u_int mask)
{
	u_int led;
	uint32_t pio;

	pio = 0;

	for (led = 0; led < NR_LEDS; ++led) {
		if (mask & (1 << led)) {
			pio |= (1 << led_pin[led]);
		}
	}

	return pio;
}

static u_int
pio_to_led(uint32_t pio)
{
	u_int led;
	u_int mask;

	mask = 0;

	for (led = 0; led < NR_LEDS; ++led) {
		if (pio & (1 << led_pin[led])) {
			mask |= (1 << led);
		}
	}

	return mask;
}


static void
led_on(u_int mask)
{

	pio_set(led_to_pio(mask));
}

static void
led_off(u_int mask)
{

	pio_clear(led_to_pio(mask));
}

static u_int
led_status(void)
{

	return pio_to_led(pio_get());
}

static int
led_ioctl(file_t file, u_long cmd, void *arg)
{
	u_int mask;
	u_int count;

	switch (cmd) {
	/*
	 * Switch LEDs on/off
	 */
	case LEDIOC_ON:
		if (umem_copyin(arg, &mask, sizeof(mask))) {
			return EFAULT;
		}
		led_on(mask);
		break;
	case LEDIOC_OFF:
		if (umem_copyin(arg, &mask, sizeof(mask))) {
			return EFAULT;
		}
		led_off(mask);
		break;
	/*
	 * Query LEDs status
	 */
	case LEDIOC_STATUS:
		mask = led_status();
		if (umem_copyout(&mask, arg, sizeof(mask))) {
			return EFAULT;
		}
		break;
	/*
	 * Query LED count
	 */
	case LEDIOC_COUNT:
		count = NR_LEDS;
		if (umem_copyout(&count, arg, sizeof(count))) {
			return EFAULT;
		}
		break;
	default:
		return EINVAL;
	}

	return 0;
}

static int
led_init(void)
{
	u_int led;
	uint32_t led_mask;

	led_dev = device_create(&led_io, "led", DF_CHR, NULL);
	ASSERT(led_dev);

	led_mask = 0;
	for (led = 0; led < NR_LEDS; ++led) {
		led_mask |= (1 << led_pin[led]);
	}

	/* Enable PIO control for LED pins */
	pio_enable(led_mask);

	/* Turn off all LEDs (before config output mode) */
	pio_clear(led_mask);

	/* Set LED pins to output mode */
	pio_setout(led_mask);

	return 0;
}
