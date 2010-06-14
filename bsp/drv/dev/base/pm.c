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
 * pm.c - power management driver
 */

#include <sys/ioctl.h>
#include <sys/capability.h>
#include <sys/power.h>
#include <sys/signal.h>
#include <driver.h>
#include <devctl.h>
#include <cons.h>
#include <pm.h>

/* #define DEBUG_PM 1 */

#ifdef DEBUG_PM
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

struct pm_softc {
	device_t	dev;		/* device object */
	int		isopen;		/* number of open counts */
	int		policy;		/* power management policy */
	int		timer_active;	/* true if pm timer is staring */
	timer_t		timer;		/* pm timer */
	u_long		idlecnt;	/* idle counter in sec */
	u_long		dimtime;	/* auto dim (lcd off) time in sec */
	u_long		sustime;	/* auto suspend time in sec */
	task_t		powtask;	/* task for power server */
	int		lcd_on;		/* true if lcd is off */
	device_t	lcd_dev;	/* lcd device */
	int		lastevt;	/* last event */
};

static int	pm_open(device_t, int);
static int	pm_close(device_t);
static int	pm_ioctl(device_t, u_long, void *);
static int	pm_init(struct driver *);
static void	pm_stop_timer(void);
static void	pm_update_timer(void);
static void	pm_timeout(void *);


static struct devops pm_devops = {
	/* open */	pm_open,
	/* close */	pm_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	pm_ioctl,
	/* devctl */	no_devctl,
};

struct driver pm_driver = {
	/* name */	"pm",
	/* devops */	&pm_devops,
	/* devsz */	sizeof(struct pm_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	pm_init,
	/* unload */	NULL,
};

/*
 * Pointer to the PM state.  There can be only one PM instance.
 */
static struct pm_softc *pm_softc;

static int
pm_open(device_t dev, int mode)
{
	struct pm_softc *sc = pm_softc;

	if (!task_capable(CAP_POWERMGMT))
		return EPERM;

	if (sc->isopen > 0)
		return EBUSY;

	sc->isopen++;
	return 0;
}

static int
pm_close(device_t dev)
{
	struct pm_softc *sc = pm_softc;

	if (!task_capable(CAP_POWERMGMT))
		return EPERM;

	if (sc->isopen != 1)
		return EINVAL;

	sc->isopen--;
	return 0;
}

static int
pm_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct pm_softc *sc = pm_softc;
	int error = 0;
	int policy, state, event;

	if (!task_capable(CAP_POWERMGMT))
		return EPERM;

	switch (cmd) {

	case PMIOC_CONNECT:
		/* Connection request from the power server */
		if (copyin(arg, &sc->powtask, sizeof(task_t)))
			return EFAULT;
		DPRINTF(("pm: connect power server\n"));
		break;

	case PMIOC_QUERY_EVENT:
		event = sc->lastevt;
		sc->lastevt = PME_NO_EVENT;
		if (copyout(&event, arg, sizeof(int)))
			return EFAULT;
		DPRINTF(("pm: query event=%d\n", event));
		break;

	case PMIOC_SET_POWER:
		if (copyin(arg, &state, sizeof(int)))
			return EFAULT;

		switch (state) {
		case PWR_SUSPEND:
		case PWR_OFF:
		case PWR_REBOOT:
			pm_set_power(state);
			break;
		default:
			error = EINVAL;
			break;
		}
		break;

	case PMIOC_GET_POLICY:
		if (copyout(&sc->policy, arg, sizeof(int)))
			return EFAULT;
		DPRINTF(("pm: get policy %d\n", sc->policy));
		break;

	case PMIOC_SET_POLICY:
		if (copyin(arg, &policy, sizeof(int)))
			return EFAULT;
		if (policy != PM_POWERSAVE && policy != PM_PERFORMANCE)
			return EINVAL;

		DPRINTF(("pm: set policy %d\n", policy));

		if (policy == sc->policy) {
			/* same policy */
			break;
		}
		/* Call devctl() routine for all devices */
		device_broadcast(DEVCTL_PM_CHGPOLICY, &policy, 1);

		sc->policy = policy;
		if (policy == PM_POWERSAVE)
			pm_update_timer();
		else
			pm_stop_timer();
		break;

	case PMIOC_GET_SUSTMR:
		if (copyout(&sc->sustime, arg, sizeof(u_long)))
			return EFAULT;
		break;

	case PMIOC_SET_SUSTMR:
		if (copyin(arg, &sc->sustime, sizeof(u_long)))
			return EFAULT;
		DPRINTF(("pm: set sustmr=%d\n", sc->sustime));
		pm_update_timer();
		break;

	case PMIOC_GET_DIMTMR:
		if (copyout(&sc->dimtime, arg, sizeof(u_long)))
			return EFAULT;
		break;

	case PMIOC_SET_DIMTMR:
		if (copyin(arg, &sc->dimtime, sizeof(u_long)))
			return EFAULT;
		DPRINTF(("pm: set dimtmr=%d\n", sc->dimtime));
		pm_update_timer();
		break;

	default:
		return EINVAL;
	}
	return error;
}


static void
pm_stop_timer(void)
{
	struct pm_softc *sc = pm_softc;
	int s;

	DPRINTF(("pm: stop timer\n"));

	s = splhigh();
	if (sc->timer_active) {
		timer_stop(&sc->timer);
		sc->idlecnt = 0;
		sc->timer_active = 0;
	}
	splx(s);
}

static void
pm_update_timer(void)
{
	struct pm_softc *sc = pm_softc;
	int s;

	if (sc->policy != PM_POWERSAVE)
		return;

	s = splhigh();
	sc->idlecnt = 0;
	if (sc->timer_active) {
		if (sc->sustime == 0 && sc->dimtime == 0)
			timer_stop(&sc->timer);
	} else {
		if (sc->sustime != 0 || sc->dimtime != 0) {
			DPRINTF(("pm: start timer\n"));
			timer_callout(&sc->timer, 1000, &pm_timeout, sc);
		      	sc->timer_active = 1;
		}
	}
}


static int
pm_suspend(void)
{
	int error;

	DPRINTF(("pm: suspend system...\n"));

	pm_stop_timer();
	error = device_broadcast(DEVCTL_PM_POWERDOWN, NULL, 1);
	if (error) {
		device_broadcast(DEVCTL_PM_POWERUP, NULL, 1);
		return error;
	}
	machine_powerdown(PWR_SUSPEND);
	return 0;
}

static int
pm_resume(void)
{

	DPRINTF(("pm: resume...\n"));

	device_broadcast(DEVCTL_PM_POWERUP, NULL, 1);
	pm_update_timer();
	return 0;
}

static int
pm_poweroff(void)
{

	DPRINTF(("pm: power off...\n"));

	pm_stop_timer();
	device_broadcast(DEVCTL_PM_POWERDOWN, NULL, 1);
	driver_shutdown();

#ifdef CONFIG_CONS
	cons_puts("\nThe system is halted. You can turn off power.");
#endif
	machine_powerdown(PWR_OFF);

	/* NOTREACHED */
	return 0;
}

static int
pm_reboot(void)
{

	DPRINTF(("pm: rebooting...\n"));

	pm_stop_timer();
	device_broadcast(DEVCTL_PM_POWERDOWN, NULL, 1);
	driver_shutdown();
	machine_powerdown(PWR_REBOOT);

	/* NOTREACHED */
	return 0;
}

static void
pm_lcd_off(void)
{
	struct pm_softc *sc = pm_softc;

	DPRINTF(("pm: LCD off\n"));

	if (sc->lcd_dev != NODEV && sc->lcd_on) {
		device_control(sc->lcd_dev, DEVCTL_PM_LCDOFF, NULL);
		if (sc->sustime == 0)
			pm_stop_timer();
		sc->lcd_on = 0;
	}
}

static void
pm_lcd_on(void)
{
	struct pm_softc *sc = pm_softc;

	DPRINTF(("pm: LCD on\n"));

	if (sc->lcd_dev != NODEV && !sc->lcd_on) {
		device_control(sc->lcd_dev, DEVCTL_PM_LCDON, NULL);
		pm_update_timer();
		sc->lcd_on = 1;
	}
}

static void
pm_timeout(void *arg)
{
	struct pm_softc *sc = arg;
	int s, reload;

	s = splhigh();
	sc->idlecnt++;
	splx(s);

	DPRINTF(("pm: idlecnt=%d\n", sc->idlecnt));

	if (sc->sustime != 0 && sc->idlecnt >= sc->sustime) {
#ifdef CONFIG_CONS
		cons_puts("\nThe system is about to suspend...");
#endif
		pm_suspend();
	} else {
		reload = 0;
		if (sc->dimtime != 0 && sc->idlecnt >= sc->dimtime) {
			pm_lcd_off();
			if (sc->sustime != 0)
				reload = 1;
		} else
			reload = 1;

		if (reload)
			timer_callout(&sc->timer, 1000, &pm_timeout, sc);
	}
}

/*
 * PM service for other drivers.
 */
int
pm_set_power(int state)
{
	int error;

	switch (state) {
	case PWR_ON:
		error = pm_resume();
		break;
	case PWR_SUSPEND:
		error = pm_suspend();
		break;
	case PWR_OFF:
		error = pm_poweroff();
		break;
	case PWR_REBOOT:
		error = pm_reboot();
		break;
	default:
		error = EINVAL;
	}
	return error;
}

/*
 * PM event notification.
 */
void
pm_notify(int event)
{
	struct pm_softc *sc = pm_softc;
	int s;

	if (event == PME_USER_ACTIVITY) {
		/*
		 * Reload suspend timer for user activity.
		 */
		s = splhigh();
		sc->idlecnt = 0;
		splx(s);

		if (!sc->lcd_on)
			pm_lcd_on();
		return;
	}

	DPRINTF(("pm: notify %d\n", event));

	if (sc->powtask != TASK_NULL) {
		/*
		 * Power server exists.
		 */
		switch (event) {
		case PME_PWRBTN_PRESS:
		case PME_SLPBTN_PRESS:
		case PME_LOW_BATTERY:
		case PME_LCD_CLOSE:
			/*
			 * Post an exception to the power server.
			 * Then, the power server will query PM event.
			 */
			sc->lastevt = event;
			DPRINTF(("pm: post %d\n", event));
			exception_post(sc->powtask, SIGPWR);
			break;
		case PME_LCD_OPEN:
			sc->lastevt = PME_NO_EVENT;
			pm_lcd_on();
			break;
		}
	} else {
		/*
		 * No power server.
		 * Map power event to default action.
		 */
		switch (event) {
		case PME_PWRBTN_PRESS:
			pm_poweroff();
			break;
		case PME_SLPBTN_PRESS:
		case PME_LOW_BATTERY:
			pm_suspend();
			break;
		case PME_LCD_OPEN:
			pm_lcd_on();
			break;
		case PME_LCD_CLOSE:
			pm_lcd_off();
			break;
		}
	}
}

void
pm_attach_lcd(device_t dev)
{

	ASSERT(pm_softc != NULL);

	pm_softc->lcd_dev = dev;
}

static int
pm_init(struct driver *self)
{
	struct pm_softc *sc;
	device_t dev;

	/* Create device object */
	dev = device_create(self, "pm", D_CHR|D_PROT);

	sc = device_private(dev);
	sc->dev = dev;
	sc->isopen = 0;
	sc->policy = DEFAULT_POWER_POLICY;
	sc->idlecnt = 0;
	sc->dimtime = 0;
	sc->sustime = 0;
	sc->timer_active = 0;
	sc->powtask = TASK_NULL;
	sc->lcd_dev = NODEV;
	sc->lcd_on = 1;
	sc->lastevt = PME_NO_EVENT;

	pm_softc = sc;

	DPRINTF(("Power policy: %s mode\n",
		 (sc->policy == PM_POWERSAVE) ? "power save" : "performance"));
	return 0;
}
