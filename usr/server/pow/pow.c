/*-
 * Copyright (c) 2009, Kohsuke Ohtani
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
 * pow.c - power server
 */

#include <sys/prex.h>
#include <sys/mount.h>
#include <sys/ioctl.h>
#include <ipc/proc.h>
#include <ipc/pow.h>
#include <ipc/exec.h>
#include <ipc/ipc.h>

#include <unistd.h>
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <signal.h>
#include <errno.h>

/* #define DEBUG_POW 1 */

#ifdef DEBUG_POW
#define DPRINTF(a) dprintf a
#else
#define DPRINTF(a)
#endif

/*
 * Action for each power event.
 */
struct power_action {
	int	pwrbtn;		/* state for power button press */
	int	slpbtn;		/* state for sleep button press */
	int	lcdclose;	/* state for LCD close */
	int	lowbatt;	/* state for low battery */
};

static int pow_noop(struct msg *);
static int pow_set_power(struct msg *);
static int pow_get_policy(struct msg *);
static int pow_set_policy(struct msg *);
static int pow_get_sustmr(struct msg *);
static int pow_set_sustmr(struct msg *);
static int pow_get_dimtmr(struct msg *);
static int pow_set_dimtmr(struct msg *);
static int pow_battery_lvl(struct msg *);
static int pow_debug(struct msg *);

static void set_power_state(int);
static void shutdown_server(const char *);

/*
 * Message mapping
 */
struct msg_map {
	int	code;
	int	(*func)(struct msg *);
};

static const struct msg_map powermsg_map[] = {
	{POW_SET_POWER,	pow_set_power},
	{POW_GET_POLICY,	pow_get_policy},
	{POW_SET_POLICY,	pow_set_policy},
	{POW_GET_SUSTMR,	pow_get_sustmr},
	{POW_SET_SUSTMR,	pow_set_sustmr},
	{POW_GET_DIMTMR,	pow_get_dimtmr},
	{POW_SET_DIMTMR,	pow_set_dimtmr},
	{POW_BATTERY_LVL,	pow_battery_lvl},
	{STD_DEBUG,		pow_debug},
	{0,			pow_noop},
};

static struct power_action pmact;
static device_t pmdev;

static int
pow_noop(struct msg *msg)
{
	return 0;
}

static int
pow_set_power(struct msg *msg)
{
	int state;

	state = msg->data[0];
	set_power_state(state);
	return 0;
}

static int
pow_get_policy(struct msg *msg)
{
	int policy;

	device_ioctl(pmdev, PMIOC_GET_POLICY, &policy);
	msg->data[0] = policy;
	return 0;
}

static int
pow_set_policy(struct msg *msg)
{
	int policy;

	policy = msg->data[0];
	device_ioctl(pmdev, PMIOC_SET_POLICY, &policy);
	return 0;
}

static int
pow_get_sustmr(struct msg *msg)
{
	int timeout;

	device_ioctl(pmdev, PMIOC_GET_SUSTMR, &timeout);
	msg->data[0] = timeout;
	return 0;
}

static int
pow_set_sustmr(struct msg *msg)
{
	int timeout;

	timeout = msg->data[0];
	device_ioctl(pmdev, PMIOC_SET_SUSTMR, &timeout);
	return 0;
}

static int
pow_get_dimtmr(struct msg *msg)
{
	int timeout;

	device_ioctl(pmdev, PMIOC_GET_DIMTMR, &timeout);
	msg->data[0] = timeout;
	return 0;
}

static int
pow_set_dimtmr(struct msg *msg)
{
	int timeout;

	timeout = msg->data[0];
	device_ioctl(pmdev, PMIOC_SET_DIMTMR, &timeout);
	return 0;
}

static int
pow_battery_lvl(struct msg *msg)
{
	/* TODO: Get current battery level from battery driver. */
	return 0;
}

static void
set_power_state(int state)
{

	if (pmdev != NODEV) {
		DPRINTF(("set_power_state: state=%d\n", state));

		sync();
		if (state == PWR_OFF || state == PWR_REBOOT) {
			kill(-1, SIGTERM);
			shutdown_server("!exec");
			shutdown_server("!fs");
			shutdown_server("!proc");
		}
		device_ioctl(pmdev, PMIOC_SET_POWER, &state);
	}
}

static void
exception_handler(int sig)
{

	if (sig == SIGPWR) {
		DPRINTF(("SIGPWR!\n"));
	}
	exception_return();
}

static void
power_thread(void)
{
	int sig, event, state;

	DPRINTF(("power_thread: start\n"));

	for (;;) {
		/*
		 * Wait signals from PM driver.
		 */
		exception_wait(&sig);
		DPRINTF(("power_thread: sig=%d\n", sig));

		if (sig == SIGPWR) {
			/*
			 * Query PM events.
			 */
			device_ioctl(pmdev, PMIOC_QUERY_EVENT, &event);
			DPRINTF(("power_thread: event=%d\n", event));

			/*
			 * Do action for current power settings.
			 */
			state = PWR_ON;
			switch (event) {
			case PME_PWRBTN_PRESS:
				state = pmact.pwrbtn;
				break;
			case PME_LOW_BATTERY:
				state = pmact.lowbatt;
				break;
			case PME_SLPBTN_PRESS:
				state = pmact.slpbtn;
				break;
			case PME_LCD_CLOSE:
				state = pmact.lcdclose;
				break;
			}
			if (state != PWR_ON)
				set_power_state(state);

		}
	}
}

/*
 * Run specified routine as a thread.
 */
static int
run_thread(void (*entry)(void))
{
	task_t self;
	thread_t t;
	void *stack, *sp;
	int error;

	self = task_self();
	if ((error = thread_create(self, &t)) != 0)
		return error;
	if ((error = vm_allocate(self, &stack, DFLSTKSZ, 1)) != 0)
		return error;

	sp = (void *)((u_long)stack + DFLSTKSZ - sizeof(u_long) * 3);
	if ((error = thread_load(t, entry, sp)) != 0)
		return error;

	return thread_resume(t);
}

static void
pow_init(void)
{
	task_t self;

	/*
	 * Set default power actions
	 */
	pmact.pwrbtn = PWR_OFF;
	pmact.slpbtn = PWR_SUSPEND;
	pmact.lcdclose = PWR_SUSPEND;
	pmact.lowbatt = PWR_OFF;

	/*
	 * Connect to the pm driver to get all power events.
	 */
	if (device_open("pm", 0, &pmdev) != 0) {
		/*
		 * Bad config...
		 */
		sys_panic("pow: no pm driver");
	}
	self = task_self();
	device_ioctl(pmdev, PMIOC_CONNECT, &self);

	/*
	 * Setup exception to receive signals from pm driver.
	 */
	exception_setup(exception_handler);

	/*
	 * Start power thread.
	 */
	if (run_thread(power_thread))
		sys_panic("pow_init");
}

static void
register_process(void)
{
	struct msg m;
	object_t obj;
	int error;

	error = object_lookup("!proc", &obj);
	if (error)
		sys_panic("pow: no proc found");

	m.hdr.code = PS_REGISTER;
	msg_send(obj, &m, sizeof(m));
}

/*
 * Wait until specified server starts.
 */
static void
wait_server(const char *name, object_t *pobj)
{
	int i, error = 0;

	/* Give chance to run other servers. */
	thread_yield();

	/*
	 * Wait for server loading. timeout is 1 sec.
	 */
	for (i = 0; i < 100; i++) {
		error = object_lookup((char *)name, pobj);
		if (error == 0)
			break;

		/* Wait 10msec */
		timer_sleep(10, 0);
		thread_yield();
	}
	if (error)
		sys_panic("pow: server not found");
}

static void
shutdown_server(const char *name)
{
	struct msg m;
	object_t obj;
	int error;

	DPRINTF(("pow: shutdown %s\n", name));
	error = object_lookup((char *)name, &obj);
	if (error != 0)
		return;

	m.hdr.code = STD_SHUTDOWN;
	error = msg_send(obj, &m, sizeof(m));
	if (error)
		sys_panic("pow: shutdown error");
}

static int
pow_debug(struct msg *msg)
{
	return 0;
}

/*
 * Main routine for power server.
 */
int
main(int argc, char *argv[])
{
	static struct msg msg;
	const struct msg_map *map;
	object_t obj;
	struct bind_msg bm;
	object_t execobj, procobj;
	int error;

	sys_log("Starting power server\n");

	/* Boost thread priority. */
	thread_setpri(thread_self(), PRI_POW);

	/*
	 * Wait until all required system servers
	 * become available.
	 */
	wait_server("!proc", &procobj);
	wait_server("!exec", &execobj);

	/*
	 * Request to bind a new capabilities for us.
	 */
	bm.hdr.code = EXEC_BINDCAP;
	strlcpy(bm.path, "/boot/pow", sizeof(bm.path));
	msg_send(execobj, &bm, sizeof(bm));

	/*
	 * Register to process server
	 */
	register_process();

	/*
	 * Initialize power service.
	 */
	pow_init();

	/*
	 * Create an object to expose our service.
	 */
	error = object_create("!pow", &obj);
	if (error)
		sys_panic("fail to create object");

	/*
	 * Message loop
	 */
	for (;;) {
		/*
		 * Wait for an incoming request.
		 */
		error = msg_receive(obj, &msg, sizeof(msg));
		if (error)
			continue;

		DPRINTF(("pow: msg code=%x task=%x\n",
			 msg.hdr.code, msg.hdr.task));


		/* Check client's capability. */
		if (task_chkcap(msg.hdr.task, CAP_POWERMGMT) != 0) {
			map = NULL;
			error = EPERM;
		} else {
			error = EINVAL;
			map = &powermsg_map[0];
			while (map->code != 0) {
				if (map->code == msg.hdr.code) {
					error = (*map->func)(&msg);
					break;
				}
				map++;
			}
		}
		/*
		 * Reply to the client.
		 */
		msg.hdr.status = error;
		msg_reply(obj, &msg, sizeof(msg));
#ifdef DEBUG_POWER
		if (map != NULL && error != 0)
			DPRINTF(("pow: msg code=%x error=%d\n",
				 map->code, error));
#endif
	}
}
