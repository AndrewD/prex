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
#include <platform.h>
#include <pm.h>
#include "dvs.h"

#ifdef CONFIG_PM

/* #define DEBUG_PM 1 */

#ifdef DEBUG_PM
#define pm_dbg(x,y...) printk("pm: "x, ##y)
#else
#define pm_dbg(x,y...)
#endif


#ifdef CONFIG_PM_POWERSAVE
#define DEFAULT_POWER_POLICY	PM_POWERSAVE
#else
#define DEFAULT_POWER_POLICY	PM_PERFORMANCE
#endif

static int pm_open(device_t dev, int mode);
static int pm_ioctl(device_t dev, int cmd, u_long arg);
static int pm_close(device_t dev);
static int pm_init(void);

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

/*
 * Set system to suspend state
 * Call to all devices and architecture depended code.
 */
int pm_suspend(void)
{
	int err;

	pm_dbg("Suspend system\n");
	err = device_broadcast(EVT_SUSPEND, 1);
	if (err)
		return err;
	platform_suspend();
	return 0;
}

/*
 * Resume
 */
int pm_resume(void)
{
	pm_dbg("Resume system\n");
	device_broadcast(EVT_RESUME, 1);
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
	err = device_broadcast(EVT_SHUTDOWN, 1);
	if (err)
		return err;
	platform_poweroff();
	return 0;
}

/*
 * Reboot system.
 */
int pm_reboot(void)
{
	int err;

	pm_dbg("reboot\n");
	err = device_broadcast(EVT_SHUTDOWN, 1);
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
static int pm_setpolicy(int policy)
{
	if (policy != PM_POWERSAVE && policy != PM_PERFORMANCE)
		return EINVAL;
#ifdef CONFIG_CPUFREQ
	cpufreq_setpolicy(policy);
#endif
	power_policy = policy;
	return 0;
}

/*
 * Get current power policy
 */
int pm_getpolicy(void)
{
	return power_policy;
}

/*
 * Open the pm device.
 *
 * The open operation is allowed to only one task. This can protect
 * the critical ioctl operation from some malicious tasks. For example,
 * the power off should be done by the privileged task like a process
 * server.
 */
static int pm_open(device_t dev, int mode)
{
	if (nr_open > 0)
		return EBUSY;
	nr_open++;
	return 0;
}

static int pm_close(device_t dev)
{
	if (nr_open != 1)
		return EINVAL;
	nr_open--;
	return 0;
}

static int pm_ioctl(device_t dev, int cmd, u_long arg)
{
	int err = 0;
	int policy;

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
	case PMIOC_SET_POLICY:
		err = pm_setpolicy(arg);
		break;
	case PMIOC_GET_POLICY:
		policy = pm_getpolicy();
		if (umem_copyout(&policy, (int *)arg, sizeof(int)))
			return EFAULT;
		break;
	default:
		return EINVAL;
	}
	return err;
}

/*
 * Initialize
 */
static int pm_init(void)
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
	return 0;
}

#endif /* CONFIG_PM */
