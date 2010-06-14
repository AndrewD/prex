/*
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
 * pmctrl.c - power management utility
 *
 * Required capabilities:
 *      CAP_POWERMGMT
 */

#include <sys/prex.h>
#include <sys/ioctl.h>
#include <ipc/pow.h>
#include <ipc/ipc.h>

#include <unistd.h>
#include <err.h>
#include <errno.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <signal.h>

static void pmctrl_help(int, char **);
static void pmctrl_off(int, char **);
static void pmctrl_reboot(int, char **);
static void pmctrl_suspend(int, char **);
static void pmctrl_info(int, char **);
static void pmctrl_policy(int, char **);
static void pmctrl_sustime(int, char **);
static void pmctrl_dimtime(int, char **);
static void pmctrl_battery(int, char **);
static void pmctrl_null(int, char **);

struct cmdtab {
	const char	*cmd;
	void		(*func)(int, char **);
	const char	*usage;
};

static const struct cmdtab cmdtab[] = {
	{ "off"		,pmctrl_off	,"Power off." },
	{ "reboot"	,pmctrl_reboot	,"Reboot system." },
	{ "suspend"	,pmctrl_suspend	,"Suspend system." },
	{ "info"	,pmctrl_info	,"Disaplay power management information." },
	{ "policy"	,pmctrl_policy	,"Set power policy." },
	{ "sustime"	,pmctrl_sustime	,"Set timeout for suspend timer." },
	{ "dimtime"	,pmctrl_dimtime	,"Set timeout for dim timer." },
	{ "battery"	,pmctrl_battery	,"Show current battery level." },
	{ "-?"		,pmctrl_help	,"This help." },
	{ NULL		,pmctrl_null	,NULL },
};

static object_t powobj;

static void
pmctrl_null(int argc, char **argv)
{
}

static void
pmctrl_help(int argc, char **argv)
{
	int i = 0;

	fprintf(stderr, "usage: pmctrl command\n");
	fprintf(stderr, "commands:\n");
	while (cmdtab[i].cmd != NULL) {
		if (cmdtab[i].usage)
			fprintf(stderr, " %-8s -- %s\n", cmdtab[i].cmd,
			       cmdtab[i].usage);
		i++;
	}
}

/*
 * User confirmation is required for some actions
 * due to security reason.
 */
static void
pmctrl_confirm(char *action)
{
	int ch, checkch;

	printf("Do you want to %s the system now? (y/n) ", action);
	checkch = ch = getchar();
	while (ch != '\n' && ch != EOF)
		ch = getchar();
	if (checkch != 'y')
		exit(1);
}

static void
pmctrl_off(int argc, char **argv)
{
	struct msg m;

	pmctrl_confirm("shutdown");

	printf("Shutdown system...\n");
	m.hdr.code = POW_SET_POWER;
	m.data[0] = PWR_OFF;
	msg_send(powobj, &m, sizeof(m));

	fprintf(stderr, "Shutdown failed!\n");
}

static void
pmctrl_reboot(int argc, char **argv)
{
	struct msg m;

	pmctrl_confirm("reboot");

	printf("Reboot system...\n");
	m.hdr.code = POW_SET_POWER;
	m.data[0] = PWR_REBOOT;
	msg_send(powobj, &m, sizeof(m));

	fprintf(stderr, "Reboot failed!\n");
}

static void
pmctrl_suspend(int argc, char **argv)
{
	struct msg m;

	pmctrl_confirm("suspend");

	printf("Suspend system...\n");
	m.hdr.code = POW_SET_POWER;
	m.data[0] = PWR_SUSPEND;
	msg_send(powobj, &m, sizeof(m));

	fprintf(stderr, "Suspend failed!\n");
}

static void
pmctrl_info(int argc, char **argv)
{
	struct msg m;
	int policy;
	int timeout;

	m.hdr.code = POW_GET_POLICY;
	msg_send(powobj, &m, sizeof(m));
	policy = m.data[0];
	printf("Power policy   : %s mode\n",
	       policy == PM_PERFORMANCE ? "high performance" : "power save");

	m.hdr.code = POW_GET_SUSTMR;
	msg_send(powobj, &m, sizeof(m));
	timeout = m.data[0];
	printf("Suspend timeout: %d sec\n", timeout);

	m.hdr.code = POW_GET_DIMTMR;
	msg_send(powobj, &m, sizeof(m));
	timeout = m.data[0];
	printf("Dim timeout    : %d sec\n", timeout);
}

static void
pmctrl_policy(int argc, char **argv)
{
	struct msg m;

	if (argc != 3) {
		fprintf(stderr, "Usage: pmctrl policy high|save\n");
		exit(1);
	}

	if (!strcmp(argv[2], "high"))
		m.data[0] = PM_PERFORMANCE;
	else if (!strcmp(argv[2], "save"))
		m.data[0] = PM_POWERSAVE;
	else {
		fprintf(stderr, "Invalid policy\n");
		exit(1);
	}
	m.hdr.code = POW_SET_POLICY;
	msg_send(powobj, &m, sizeof(m));
}

static void
pmctrl_sustime(int argc, char **argv)
{
	struct msg m;
	int timeout;

	if (argc < 3)
		fprintf(stderr, "Usage: pmctrl sustime sec\n");
	else {
		timeout = atoi(argv[2]);

		m.hdr.code = POW_SET_SUSTMR;
		m.data[0] = timeout;
		msg_send(powobj, &m, sizeof(m));
	}
}

static void
pmctrl_dimtime(int argc, char **argv)
{
	struct msg m;
	int timeout;

	if (argc < 3)
		fprintf(stderr, "Usage: pmctrl dimtime sec\n");
	else {
		timeout = atoi(argv[2]);

		m.hdr.code = POW_SET_DIMTMR;
		m.data[0] = timeout;
		msg_send(powobj, &m, sizeof(m));
	}
}

static void
pmctrl_battery(int argc, char **argv)
{

	fprintf(stderr, "Not supported...\n");
}

int
main(int argc, char *argv[])
{
	int i = 0;
	int found = 0;

	if (argc < 2) {
		pmctrl_help(1, NULL);
		exit(1);
	}

	if (object_lookup("!pow", &powobj) != 0) {
		fprintf(stderr, "No power server found\n");
		exit(1);
	}

	while (cmdtab[i].cmd != NULL) {
		if (!strncmp(argv[1], cmdtab[i].cmd, LINE_MAX)) {
			(cmdtab[i].func)(argc, argv);
			found = 1;
			break;
		}
		i++;
	}
	if (!found)
		pmctrl_help(1, NULL);
	exit(1);
}
