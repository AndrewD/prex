/*-
 * Copyright (c) 2007, Kohsuke Ohtani
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
 * cpufreq.c - CPU frequency control
 */

#include <sys/ioctl.h>
#include <driver.h>
#include <pm.h>
#include "dvs.h"

/* #define DEBUG_CPUFREQ 1 */

#ifdef DEBUG_CPUFREQ
#define cf_printf(fmt, args...) printk("cpufreq: " fmt, ## args)
#else
#define cf_printf(fmt...)	 do {} while (0)
#endif

static int cpufreq_open(device_t dev, int mode);
static int cpufreq_ioctl(device_t dev, int cmd, u_long arg);
static int cpufreq_close(device_t dev);
static int cpufreq_init(void);

/*
 * Driver structure
 */
struct driver cpufreq_drv = {
	/* name */	"CPU Frequency Control",
	/* order */	3,		/* Must larger than pm driver */
	/* init */	cpufreq_init,
};

static struct devio cpufreq_io = {
	/* open */	cpufreq_open,
	/* close */	cpufreq_close,
	/* read */	NULL,
	/* write */	NULL,
	/* ioctl */	cpufreq_ioctl,
	/* event */	NULL,
};

static device_t cpufreq_dev;		/* Device object */

/*
 * Frequecy control policy
 */
static int cpufreq_policy;

static int
cpufreq_open(device_t dev, int mode)
{

	return 0;
}

static int
cpufreq_close(device_t dev)
{

	return 0;
}

static int
cpufreq_ioctl(device_t dev, int cmd, u_long arg)
{

	return 0;
}

void
cpufreq_setpolicy(int policy)
{

	switch (cpufreq_policy) {
	case CPUFREQ_ONDEMAND:
		if (policy == PM_POWERSAVE)
			dvs_enable();
		else
			dvs_disable();

		break;
	case CPUFREQ_MAXSPEED:
		break;
	case CPUFREQ_MINSPEED:
		break;
	}
}

static int
cpufreq_init(void)
{
	int policy;

	/* Create device object */
	cpufreq_dev = device_create(&cpufreq_io, "cpufreq", DF_CHR);
	ASSERT(cpufreq_dev);

	dvs_init();

	policy = pm_getpolicy();
	if (policy == PM_POWERSAVE)
		dvs_enable();
	return 0;
}

