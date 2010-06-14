/*
 * Copyright (c) 2007-2009, Kohsuke Ohtani
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
 * cpufreq.c - CPU frequency control driver
 */

/*
 * Dynamic voltage scaling (DVS)
 *
 * DVS is widely used with mobile systems to save the processor
 * power consumption, with minimum impact on performance.
 * The basic idea is come from the fact the power consumption is
 * proportional to V^2 x f, where V is voltage and f is frequency.
 * Since processor does not always require the full performance,
 * we can reduce power consumption by lowering voltage and frequeceny.
 */

#include <sys/ioctl.h>
#include <sys/power.h>
#include <driver.h>
#include <sys/sysinfo.h>
#include <devctl.h>
#include <cpufreq.h>

/* #define DEBUG_CPUFREQ 1 */

#ifdef DEBUG_CPUFREQ
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

/*
 * DVS parameters
 */
#define SAMPLING_RATE		50	/* msec */
#define SAMPLING_TICK		mstohz(SAMPLING_RATE)
#define WEIGHT			3

struct cpufreq_softc {
	int		enable;		/* true if enabled */
	device_t	dev;		/* device object */
	struct timer	timer;		/* performance sampling timer */
	struct cpufreq_ops *ops;	/* low level h/w operations */
};

static int cpufreq_ioctl(device_t, u_long, void *);
static int cpufreq_devctl(device_t, u_long, void *);
static int cpufreq_init(struct driver *);


static struct devops cpufreq_devops= {
	/* open */	no_open,
	/* close */	no_close,
	/* read */	no_read,
	/* write */	no_write,
	/* ioctl */	cpufreq_ioctl,
	/* devctl */	cpufreq_devctl,
};

struct driver cpufreq_driver = {
	/* name */	"cpufreq",
	/* devops */	&cpufreq_devops,
	/* devsz */	sizeof(struct cpufreq_softc),
	/* flags */	0,
	/* probe */	NULL,
	/* init */	cpufreq_init,
	/* shutdown */	NULL,
};

/*
 * DVS related data
 */
static u_long	last_cputicks;
static u_long	last_idleticks;
static int	cur_speed;	/* current CPU speed (%) */
static int	max_speed;	/* maximum CPU speed (%) */
static int	min_speed;	/* minimum CPU speed (%) */
static u_long	avg_workload;	/* average workload */
static u_long	avg_deadline;	/* average deadline */
static u_long	excess_cycles;	/* cycles left over from the last interval */

/*
 * Predict max CPU speed.
 *
 * DVS Algorithm: AVG<3>
 *
 *  Computes an exponentially moving average of the previous intervals.
 *  <wight> is the relative weighting of past intervals relative to
 *  the current interval.
 *
 *   predict = (weight x current + past) / (weight + 1)
 *
 * Refernce:
 *   K.Govil, E.Chan, H.Wasserman,
 *   "Comparing Algorithm for Dynamic Speed-Setting of a Low-Power CPU".
 *   Proc. 1st Int'l Conference on Mobile Computing and Networking,
 *   Nov 1995.
 */
static void
cpufreq_predict_max_speed(u_long run_cycles, u_long idle_cycles)
{
	u_long new_workload, new_deadline;

	new_workload = run_cycles * cur_speed;
	new_deadline = (run_cycles + idle_cycles) * cur_speed;

	avg_workload = (avg_workload * WEIGHT + new_workload) / (WEIGHT + 1);
	avg_deadline = (avg_deadline * WEIGHT + new_deadline) / (WEIGHT + 1);

	max_speed = (int)(avg_workload * 100 / avg_deadline);
	if (max_speed < 50)
		max_speed = 50;

	DPRINTF(("cpufreq: new_workload=%u new_deadline=%u\n",
		 new_workload, new_deadline));
	DPRINTF(("cpufreq: avg_workload=%u avg_deadline=%u\n",
		 avg_workload, avg_deadline));
	DPRINTF(("cpufreq: max_speed=%d\n", max_speed));
}

/*
 * Predict CPU speed.
 *
 * DVS Algorithm: Weiser Style
 *
 *  If the utilization prediction x is high (over 70%), increase the
 *  speed by 20% of the maximum speed. If the utilization prediction
 *  is low (under 50%), decrease the speed by (60 - x)% of the
 *  maximum speed.
 *
 *  excess_cycles is defined as the number of uncompleted run cycles
 *  from the last interval. For example, if we find 70% activity
 *  when runnig at full speed, and their processor speed was set to
 *  50% during that interval, excess_cycles is set to 20%. This
 *  value (20%) is used to calculate the processor speed in the next
 *  interval.
 *
 * Refernce:
 *   M.Weiser, B.Welch, A.Demers, and S.Shenker,
 *   "Scheduling for Reduced CPU Energy", In Proceedings of the
 *   1st Symposium on Operating Systems Design and Implementation,
 *   pages 13-23, November 1994.
 */
static int
cpufreq_predict_cpu_speed(u_long run_cycles, u_long idle_cycles)
{
	u_long next_excess;
	u_int run_percent;
	u_int new_speed = cur_speed;

	run_cycles += excess_cycles;
	run_percent = (int)((run_cycles * 100) / (idle_cycles + run_cycles));

	next_excess = run_cycles -
		cur_speed * (run_cycles + idle_cycles) / 100;
	if (next_excess < 0)
		next_excess = 0;

	if (excess_cycles > idle_cycles)
		new_speed = 100;
	else if (run_percent > 70)
		new_speed = cur_speed + 20;
	else if (run_percent < 50)
		new_speed = cur_speed - (60 - run_percent);

	if (new_speed > max_speed)
		new_speed = max_speed;
	if (new_speed < min_speed)
		new_speed = min_speed;

	DPRINTF(("cpufreq: run_percent=%d next_excess=%d new_speed=%d\n\n",
		 run_percent, next_excess, new_speed));

	excess_cycles = next_excess;

	return new_speed;
}

/*
 * Timer callback routine.
 */
static void
cpufreq_timeout(void *arg)
{
	struct cpufreq_softc *sc = arg;
	struct timerinfo info;
	int new_speed;
	u_long idle_cycles, run_cycles;

	/*
	 * Get run/idle cycles.
	 */
	sysinfo(INFO_TIMER, &info);
	idle_cycles = info.idleticks - last_idleticks;
	run_cycles = info.cputicks - last_cputicks - idle_cycles;

	DPRINTF(("cpufreq: run_cycles=%d idle_cycles=%d cur_speed=%d\n",
		 run_cycles, idle_cycles, cur_speed));

	/*
	 * Predict max CPU speed.
	 */
	cpufreq_predict_max_speed(run_cycles, idle_cycles);

	/*
	 * Predict next CPU speed.
	 */
	new_speed = cpufreq_predict_cpu_speed(run_cycles, idle_cycles);
	if (new_speed != cur_speed) {
		sc->ops->setperf(new_speed);
		cur_speed = sc->ops->getperf();
	}

	last_cputicks = info.cputicks;
	last_idleticks = info.idleticks;

	timer_callout(&sc->timer, SAMPLING_RATE, &cpufreq_timeout, sc);
}

/*
 * Enable DVS operation
 */
static void
cpufreq_enable(struct cpufreq_softc *sc)
{
	struct timerinfo info;

	ASSERT(sc->ops != NULL);

	DPRINTF(("cpufreq: enable\n"));

	if (sc->enable)
		return;
	sc->enable = 1;

	/*
	 * Initialize DVS parameters.
	 */
	sysinfo(INFO_TIMER, &info);
	last_cputicks = info.cputicks;
	last_idleticks = info.idleticks;

	max_speed = 100;	/* max 100% */
	min_speed = 5;		/* min   5% */
	cur_speed = sc->ops->getperf();

	excess_cycles = 0;
	avg_workload = SAMPLING_TICK * 100;
	avg_deadline = SAMPLING_TICK * 100;

	timer_callout(&sc->timer, SAMPLING_RATE, &cpufreq_timeout, sc);
}

/*
 * Disable DVS operation
 */
static void
cpufreq_disable(struct cpufreq_softc *sc)
{

	DPRINTF(("cpufreq: disable\n"));

	if (!sc->enable)
		return;
	sc->enable = 0;

	timer_stop(&sc->timer);

	/* Set CPU speed to 100% */
	sc->ops->setperf(100);
	cur_speed = 100;
}

static int
cpufreq_ioctl(device_t dev, u_long cmd, void *arg)
{
	struct cpufreq_softc *sc = device_private(dev);
	struct cpufreqinfo info;

	if (sc->ops == NULL)
		return EINVAL;

	switch (cmd) {
	case CFIOC_GET_INFO:
		sc->ops->getinfo(&info);
		if (copyout(&info, arg, sizeof(info)))
			return EFAULT;
		break;
	default:
		return EINVAL;
	}
	return 0;
}

static int
cpufreq_devctl(device_t dev, u_long cmd, void *arg)
{
	struct cpufreq_softc *sc = device_private(dev);
	int error = 0;
	int policy;

	DPRINTF(("cpufreq: devctl cmd=%d\n", cmd));

	if (sc->ops == NULL)
		return 0;

	switch (cmd) {
	case DEVCTL_PM_CHGPOLICY:
		DPRINTF(("cpufreq: change policy\n"));
		policy = *(int *)arg;
		DPRINTF(("cpufreq: policy=%d\n", policy));
		if (policy == PM_POWERSAVE)
			cpufreq_enable(sc);
		else
			cpufreq_disable(sc);
		break;
	}
	return error;
}

void
cpufreq_attach(struct cpufreq_ops *ops)
{
	struct cpufreq_softc *sc;
	device_t dev;
	int policy;

	DPRINTF(("cpufreq: attach ops=%x\n", ops));

	dev = device_create(&cpufreq_driver, "cpufreq", D_CHR|D_PROT);

	sc = device_private(dev);
	sc->dev = dev;
	sc->enable = 0;
	sc->ops = ops;
	cur_speed = 100;

	policy = DEFAULT_POWER_POLICY;
	if (policy == PM_POWERSAVE)
		cpufreq_enable(sc);
}

static int
cpufreq_init(struct driver *self)
{

	return 0;
}
