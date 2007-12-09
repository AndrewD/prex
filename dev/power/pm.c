/*-
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
 * pm.c - power management driver (hardware independent)
 */

#include <driver.h>
#include <pm.h>
#include "dvs.h"

#ifdef CONFIG_PM

/* #define DEBUG_PM 1 */

#ifdef DEBUG_PM
#define pm_dbg(x,y...) printk("pm: "x, ##y)
#else
#define pm_dbg(x,y...)
#endif

static void dvs_timeout(u_long dummy);

int pm_open(device_t dev, int mode);
int pm_ioctl(device_t dev, int cmd, u_long arg);
int pm_close(device_t dev);
int pm_init(void);

/*
 * Driver structure
 */
struct driver pm_drv __driver_entry = {
	/* name */	"Power Management",
	/* order */	2,
	/* init */	pm_init,
};

static struct devio pm_io = {
	/* open */	pm_open,
	/* close */	pm_close,
	/* read */	NULL,
	/* write */	NULL,
	/* ioctl */	pm_ioctl,
	/* event */	NULL,
};

static device_t pm_dev;		/* Device object */
static int nr_open;		/* Open count */

/*
 * Power mangement policy
 */
static int power_policy;

/*
 * Idle timer
 */
static struct timer idle_timer;
static u_long idle_count;	/* Idling counter in sec */
static u_long suspend_timeout;	/* Time until auto suspend in sec */

#ifdef CONFIG_DVS
/*
 * Dynamic voltage scaling
 */
static int dvs_support;		/* True if dvs is supported */
static struct timer dvs_timer;
static u_long dvs_sample_period;	/* Sampling interval in msec */
static u_long avg_workload;	/* Average workload */
static u_long avg_deadline;	/* Average deadline */
static u_long last_system_ticks;
static u_long last_idle_ticks;
static int cpu_perf;		/* CPU performance in percent */
#endif

/*
 * Set system to suspend state
 * Call to all devices and architecture depended code.
 */
int pm_suspend(void)
{
	int err;

	pm_dbg("Suspend system\n");
	err = device_broadcast(EVT_SUSPEND);
	if (err)
		return err;
	system_suspend();
	return 0;
}

/*
 * Resume
 */
int pm_resume(void)
{
	pm_dbg("Resume system\n");
	device_broadcast(EVT_RESUME);
	return 0;
}

/*
 * Power off system
 * Call to all devices and architecture depended code.
 */
int pm_poweroff(void)
{
	int err;

	pm_dbg("Power off...\n");
	err = device_broadcast(EVT_SHUTDOWN);
	if (err)
		return err;
	system_poweroff();
	return 0;
}

/*
 * Reboot system.
 */
int pm_reboot(void)
{
	int err;

	pm_dbg("reboot\n");
	err = device_broadcast(EVT_SHUTDOWN);
	if (err)
		return err;

	irq_lock();

	/*
	 * Do reset.
	 */
	system_reset();
	return 0;
}

/*
 * Idle timer handler
 */
static void idle_timeout(u_long dummy)
{
	irq_lock();
	idle_count++;
	irq_unlock();
	if (idle_count >= suspend_timeout)
		pm_suspend();
	else
		timer_timeout(&idle_timer, idle_timeout, 0, 1000);
}

/*
 * Set suspend timer
 */
int pm_settimer(u_long sec)
{
	sched_lock();
	if (sec)
		timer_timeout(&idle_timer, idle_timeout, 0, 1000);
	else
		timer_stop(&idle_timer);
	idle_count = 0;
	suspend_timeout = sec;
	sched_unlock();
	return 0;
}

/*
 * Get power management timer
 */
int pm_gettimer(u_long *sec)
{
	*sec = suspend_timeout;
	return 0;
}

/*
 * Reload idle timer
 */
void pm_active(void)
{
	idle_count = 0;
}

/*
 * Set power policy
 */
int pm_setpolicy(int policy)
{
	if (policy != PM_POWERSAVE && policy != PM_PERFORMANCE)
		return EINVAL;
#ifdef CONFIG_DVS
	if (power_policy != policy) {
		if (power_policy == PM_POWERSAVE)
			timer_timeout(&dvs_timer, dvs_timeout, 0,
				      dvs_sample_period);
		else
			timer_stop(&dvs_timer);
	}
#endif
	power_policy = policy;
	return 0;
}

/*
 * Get current power policy
 */
int pm_getpolicy(int *policy)
{
	*policy = power_policy;
	return 0;
}

#ifdef CONFIG_DVS
/*
 * Timer routine for dynamic voltage scaling
 */
static void dvs_timeout(u_long dummy)
{
	struct stat_sched st;
	u_long total_ticks, idle_ticks;
	u_long new_workload, new_deadline;
	int new_perf;

	/* Get current scheduling state */
	sched_stat(&st);

	/* Get idle/total ticks from last sample */
	total_ticks = st.system_ticks - last_system_ticks;
	idle_ticks = st.idle_ticks - last_idle_ticks;

	/* Examine current workload & deadline */
	new_workload = (total_ticks - idle_ticks) * (cpu_perf + 1);
	new_deadline = total_ticks * (cpu_perf + 1);

	/* Calculate avarage data for workload & deadline */
	avg_workload =
	    (avg_workload * DVS_CONSTANT + new_workload) / (DVS_CONSTANT + 1);
	avg_deadline =
	    (avg_deadline * DVS_CONSTANT + new_deadline) / (DVS_CONSTANT + 1);

	/* Get the required performance */
	new_perf = (int)(avg_workload * 100 / avg_deadline);
	if (new_perf != cpu_perf) {
		cpu_setperf(new_perf);
		cpu_perf = cpu_getperf();
	}

/* 	printk("DVS: performance=%d\n", cpu_perf); */
/* 	printk("     total_ticks=%u idle_ticks=%u\n", total_ticks, */
/* 	       idle_ticks); */
/* 	printk("     new_workload=%u new_deadline=%u\n", new_workload, */
/* 	       new_deadline); */
/* 	printk("     avg_workload=%u avg_deadline=%u\n\n", avg_workload, avg_deadline); */

	last_system_ticks = st.system_ticks;
	last_idle_ticks = st.idle_ticks;

	timer_timeout(&dvs_timer, dvs_timeout, 0, dvs_sample_period);
}
#endif

/*
 * Open the pm device.
 *
 * The open operation is allowed to only one task. This can protect
 * the critical ioctl operation from some malicious tasks. For example,
 * the power off should be done by the privileged task like a process
 * server.
 */
int pm_open(device_t dev, int mode)
{
	if (nr_open > 0)
		return EBUSY;
	nr_open++;
	return 0;
}

int pm_close(device_t dev)
{
	if (nr_open != 1)
		return EINVAL;
	nr_open--;
	return 0;
}

int pm_ioctl(device_t dev, int cmd, u_long arg)
{
	switch (cmd) {
	case PMIOC_SET_POWER:
		switch (arg) {
		case POWER_SUSPEND:
			pm_suspend();
			break;
		case POWER_OFF:
			pm_poweroff();
			break;
		case POWER_REBOOT:
			pm_reboot();
			break;
		default:
			return EINVAL;
		}
		break;
	default:
		return EINVAL;
	}
	return 0;
}

/*
 * Initialize
 */
int pm_init(void)
{
	/* Create device object */
	pm_dev = device_create(&pm_io, "pm");
	ASSERT(pm_dev);

	nr_open = 0;
	idle_count = 0;
	suspend_timeout = 0;
	power_policy = DEFAULT_POWER_POLICY;
	timer_init(&idle_timer);
	printk("Default power policy: %s mode\n",
	       (power_policy == PM_POWERSAVE) ? "power save" : "performance");

#ifdef CONFIG_DVS
	/* 
	 * Initialize DVS
	 */
	dvs_support = 0;
	if (cpu_initperf())
		return 0;
	dvs_support = 1;
	cpu_perf = cpu_getperf();

	timer_init(&dvs_timer);
	avg_workload = 0;
	avg_deadline = 0;
	dvs_sample_period = DVS_SAMPLING_RATE;
	if (power_policy == PM_POWERSAVE)
		timer_timeout(&dvs_timer, dvs_timeout, 0, 5 * 1000);

	printk("Performance sampling period=%d msec\n", DVS_SAMPLING_RATE);
#endif
	return 0;
}

#endif /* CONFIG_PM */
