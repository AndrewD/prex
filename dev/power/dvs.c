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
 * dvs.c - dynamic voltage scaling feature
 */
/*
 *  Dynamic voltage scaling (DVS) is widely used with mobile systems
 * to save the processor power consumption, with minimum impact on
 * performance. The basic idea is come from the fact the power
 * consumption is proportional to V^2 x f, where V is voltage and
 * f is frequency. Since processor does not always require the full
 * performance, we can reduce power consumption by lowering voltage
 * and frequeceny.
 */
#include <driver.h>
#include <platform.h>
#include <pm.h>
#include "dvs.h"

/* #define DEBUG_DVS 1 */

#ifdef DEBUG_DVS
#define dvs_dbg(x,y...) printk("dvs: "x, ##y)
#else
#define dvs_dbg(x,y...)
#endif

#define INTERVAL_MSEC		50
#define INTERVAL_TICK		msec_to_tick(INTERVAL_MSEC)
#define WEIGHT			20

static struct dpc dvs_dpc;	/* DVC object */
static struct hook dvs_hook;	/* Timer hook */

static int dvs_capable;		/* True if the system has dvs capability */
static int dvs_enabled;		/* True if dvs is enabled */

static int cur_speed;		/* Current CPU speed (%) */
static int max_speed;		/* Maximum CPU speed (%) */
static int min_speed;		/* Minimum CPU speed (%) */

static int run_cycles;	  /* The non-idle CPU cycles in the last interval */
static int idle_cycles;	  /* The idle CPU cycles in the last interval */
static int excess_cycles; /* The cycles left over from the last interval */

static int avg_workload;	/* Average workload */
static int avg_deadline;	/* Average deadline */

static u_long elapsed_ticks;

/*
 * Predict CPU speed
 *
 * DVS Algorithm: Weiser Style
 *
 *  If the utilization prediction x is high (over 70%), increase the speed
 *  by 20% of the maximum speed. If the utilization prediction is low
 *  (under 50%), decrease the speed by (60 - x)% of the maximum speed.
 *
 *  excess_cycles is defined as the number of uncompleted run cycles from
 *  the last interval. For example, if we find 70% activity when runnig
 *  at full speed, and their processor speed was set to 50% during that
 *  interval, excess_cycles is set to 20%. This value (20%) is used to
 *  calculate the processor speed in the next interval.
 *
 * Refernce:
 *   M.Weiser, B.Welch, A.Demers, and S.Shenker,
 *   "Scheduling for Reduced CPU Energy", In Proceedings of the
 *   1st Symposium on Operating Systems Design and Implementation,
 *   pages 13-23, November 1994.
 */
static int predict_cpu_speed(int speed)
{
	int next_excess;
	int run_percent;
	int newspeed = speed;

	run_cycles += excess_cycles;
	run_percent = (run_cycles * 100) / (idle_cycles + run_cycles);

	next_excess = run_cycles - speed * (run_cycles + idle_cycles) / 100;
	if (next_excess < 0)
		next_excess = 0;

	if (excess_cycles > idle_cycles)
		newspeed = 100;
	else if (run_percent > 70)
		newspeed = speed + 20;
	else if (run_percent < 50)
		newspeed = speed - (60 - run_percent);

	if (newspeed > max_speed)
		newspeed = max_speed;
	if (newspeed < min_speed)
		newspeed = min_speed;

	dvs_dbg("run_percent=%d next_excess=%d newspeed=%d\n\n",
		run_percent, next_excess, newspeed);

	excess_cycles = next_excess;
	return newspeed;
}

/*
 * Predict max CPU speed
 *
 * DVS Algorithm: AVG<3>
 *
 *  Computes an exponentially moving average of the previous intervals.
 *  <wight> is the relative wighting of past intervals relative to
 *  the current interval.
 *
 *   predict = (weight x current + past) / (weight + 1)
 *
 * Refernce:
 *   K.Govil, E.Chan, H.Wasserman,
 *   "Comparing Algorithm for Dynamic Speed-Setting of a Low-Power CPU".
 *   Proc. 1st Int'l Conference on Mobile Computing and Networking, Nov 1995.
 */
static int predict_max_speed(int speed)
{
	int new_workload;
	int new_deadline;
	int newspeed;

	new_workload = run_cycles * speed;
	new_deadline = (run_cycles + idle_cycles) * speed;

	avg_workload = (avg_workload * WEIGHT + new_workload) / (WEIGHT + 1);
	avg_deadline = (avg_deadline * WEIGHT + new_deadline) / (WEIGHT + 1);

	newspeed = avg_workload * 100 / avg_deadline;

	dvs_dbg("new_workload=%u new_deadline=%u\n",
		new_workload, new_deadline);
	dvs_dbg("avg_workload=%u avg_deadline=%u\n",
		avg_workload, avg_deadline);
	return newspeed;
}

/*
 * DPC routine to set CPU speed.
 */
static void dpc_adjust_speed(void *arg)
{
	int newspeed = (int)arg;

	dvs_dbg("dvs_adjust_speed: speed=%d\n", newspeed);
	cpu_setperf(newspeed);
	cur_speed = cpu_getperf();
}

/*
 * Timer hook routine called by tick handler.
 */
static void dvs_tick(void *arg)
{
	int newspeed;
	int idle = (int)arg;

	elapsed_ticks++;
	if (idle)
		idle_cycles++;
	else
		run_cycles++;

	if (elapsed_ticks >= INTERVAL_TICK) {
		/* Predict max CPU speed */
		max_speed = predict_max_speed(cur_speed);

		dvs_dbg("DVS: run_cycles=%d idle_cycles=%d cur_speed=%d max_speed=%d\n",
			run_cycles, idle_cycles, cur_speed, max_speed);
		/*
		 * Predict next CPU speed
		 */
		newspeed = predict_cpu_speed(cur_speed);
		if (newspeed != cur_speed) {
			sched_dpc(&dvs_dpc, dpc_adjust_speed,
				  (void *)newspeed);
		}
		run_cycles = 0;
		idle_cycles = 0;
		elapsed_ticks = 0;
	}
}

/*
 * Enable DVS operation
 */
void dvs_enable(void)
{
	if (!dvs_capable)
		return;

	run_cycles = 0;
	idle_cycles = 0;
	elapsed_ticks = 0;

	max_speed = 100;	/* max 100% */
	min_speed = 5;		/* min   5% */
	cur_speed = cpu_getperf();

	timer_hook(&dvs_hook, dvs_tick);
	dvs_enabled = 1;
}

/*
 * Disable DVS operation
 */
void dvs_disable(void)
{
	if (!dvs_capable)
		return;

	timer_unhook(&dvs_hook);

	/* Set CPU speed to 100% */
	cpu_setperf(100);
	dvs_enabled = 0;
}

/*
 * Initialize dvs
 */
void dvs_init(void)
{
	if (cpu_initperf())
		return;

	dvs_capable = 1;
}
