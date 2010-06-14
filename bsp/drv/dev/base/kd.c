/*-
 * Copyright (c) 2008-2009, Kohsuke Ohtani
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
 * kd.c - kernel debugger.
 */

#include <driver.h>
#include <sys/sysinfo.h>
#include <sys/power.h>
#include <sys/dbgctl.h>
#include <sys/endian.h>

#include <devctl.h>
#include <cons.h>
#include <pm.h>

#define ARGMAX		32

static void kd_abort(void);

static int kd_null(int, char **);
static int kd_help(int, char **);
static int kd_continue(int, char **);
static int kd_reboot(int, char **);
static int kd_mstat(int, char **);
static int kd_thread(int, char **);
static int kd_task(int, char **);
static int kd_vm(int, char **);
static int kd_device(int, char **);
static int kd_driver(int, char **);
static int kd_irq(int, char **);
static int kd_trap(int, char **);
static int kd_devstat(int, char **);
static int kd_trace(int, char **);
static int kd_examine(int, char **);
static int kd_write(int, char **);

struct cmd_entry {
	const char	*cmd;
	int		(*func)(int, char **);
	const char	*usage;
};

static const struct cmd_entry cmd_table[] = {
	{ "help"	,kd_help	,"This help" },
	{ "continue"	,kd_continue	,"Continue execution [c]" },
	{ "reboot"	,kd_reboot	,"Reboot system" },
	{ "mstat" 	,kd_mstat	,"Display memory usage" },
	{ "thread"	,kd_thread	,"Display thread information" },
	{ "task"	,kd_task	,"Display task information" },
	{ "vm"		,kd_vm		,"Dump all VM segments" },
	{ "device"	,kd_device	,"Display list of devices" },
	{ "driver"	,kd_driver	,"Display list of drivers" },
	{ "irq"		,kd_irq		,"Display interrupt information" },
	{ "trap"	,kd_trap	,"Dump current trap frame" },
	{ "devstat"	,kd_devstat	,"Dump all device state" },
	{ "trace"	,kd_trace	,"Set trace flag for task" },
	{ "examine"	,kd_examine	,"Examine data (x [/fmt] [addr])" },
	{ "write"	,kd_write	,"Write data (w [/size] addr val)" },
	/* command alias section */
	{ "?"		,kd_help	,NULL },
	{ "x"		,kd_examine	,NULL },
	{ "w"		,kd_write	,NULL },
	{ "c"		,kd_continue	,NULL },
	{ NULL		,kd_null	,NULL },
};

/*
 * Errors
 */
#define KERR_SYNTAX	1
#define KERR_TOOMANY	2
#define KERR_INVAL	3
#define KERR_BADADDR	4
#define KERR_NOFUNC	5
#define KERR_NOMEM	6

static const char *kd_errname[] = {
	"",
	"Syntax error",
	"Too many arguments",
	"Invalid argument",
	"No physical memory",
	"Function not supported",
	"Out of memory",
};

#define KERR_MAX	(int)(sizeof(kd_errname) / sizeof(char *))


static dpc_t	kd_dpc;			/* dpc for debugger */

static struct abort_ops kd_abort_ops = {
	/* abort */	kd_abort,
};

static int
kd_null(int argc, char **argv)
{
	return 0;
}

static void
kd_error(int id)
{

	if (id < KERR_MAX)
		printf("%s\n", kd_errname[id]);
}

/*
 * Get taks id from its name.
 */
static task_t
kd_lookup_task(char *name)
{
	struct taskinfo ti;
	task_t task = TASK_NULL;
	int rc;

	rc = 0;
	ti.cookie = 0;
	do {
		rc = sysinfo(INFO_TASK, &ti);
		if (!rc && !strncmp(ti.taskname, name, MAXTASKNAME)) {
			task = ti.id;
		}
	} while (rc == 0);
	return task;
}


static int
kd_help(int argc, char **argv)
{
	int i = 0;

	while (cmd_table[i].cmd != NULL) {
		if (cmd_table[i].usage)
			printf(" %10s -- %s.\n", cmd_table[i].cmd,
			       cmd_table[i].usage);
		i++;
	}
	printf("\nuse `-?` to find out more about each command.\n");
	return 0;
}

static int
kd_continue(int argc, char **argv)
{

	return -1;
}

static int
kd_reboot(int argc, char **argv)
{

#ifdef CONFIG_PM
	pm_set_power(PWR_REBOOT);
#else
	machine_powerdown(PWR_REBOOT);
#endif
	return 0;
}

static int
kd_mstat(int argc, char **argv)
{
	struct meminfo info;

	/* Get memory information from kernel. */
	sysinfo(INFO_MEMORY, &info);

	printf("Memory usage:\n");
	printf(" Used     :%8ld KB\n", (info.total - info.free) / 1024);
	printf(" Free     :%8ld KB\n", info.free / 1024);
	printf(" Total    :%8ld KB\n", info.total / 1024);
	printf(" Bootdisk :%8ld KB\n", info.bootdisk / 1024);
	return 0;
}

static int
kd_thread(int argc, char **argv)
{
	const char state[][4] = \
		{ "RUN", "SLP", "SUS", "S&S", "EXT" };
	const char pol[][5] = { "FIFO", "RR  " };
	struct threadinfo ti;
	int rc;

	printf("Thread list:\n");
	printf(" thread   task         stat pol  pri base     time "
	       "suscnt sleep event\n");
	printf(" -------- ------------ ---- ---- --- ---- -------- "
	       "------ ------------\n");

	rc = 0;
	ti.cookie = 0;
	do {
		/* Get thread information from kernel. */
		rc = sysinfo(INFO_THREAD, &ti);
		if (!rc) {
			printf(" %08lx %12s %s%c %s  %3d  %3d %8d %6d %s\n",
			       ti.id, ti.taskname, state[ti.state],
			       ti.active ? '*' : ' ',
			       pol[ti.policy], ti.priority, ti.basepri,
			       ti.time, ti.suscnt, ti.slpevt);
		}
	} while (rc == 0);

	return 0;
}

static int
kd_task(int argc, char **argv)
{
	struct taskinfo ti;
	int rc;

	printf("Task list:\n");
	printf(" task      name     nthreads flags    suscnt capability   vmsize\n");
	printf(" --------- -------- -------- -------- ------ ---------- --------\n");

	rc = 0;
	ti.cookie = 0;
	do {
		/* Get task information from kernel. */
		rc = sysinfo(INFO_TASK, &ti);
		if (!rc) {
			printf(" %08lx%c %8s %8d %08x %6d   %08x %8d\n",
			       ti.id, ti.active ? '*' : ' ', ti.taskname,
			       ti.nthreads, ti.flags, ti.suscnt,
			       ti.capability, ti.vmsize);
		}
	} while (rc == 0);

	return 0;
}

static void
kd_vm_region(task_t task)
{
	struct vminfo vi;
	char flags[6];
	int rc;

	printf(" virtual  physical     size flags\n");
	printf(" -------- -------- -------- -----\n");

	rc = 0;
	vi.cookie = 0;
	do {
		/* Get task information from kernel. */
		vi.task = task;
		rc = sysinfo(INFO_VM, &vi);
		if (!rc) {
			if (vi.flags != VF_FREE) {
				strlcpy(flags, "-----", sizeof(flags));
				if (vi.flags & VF_READ)
					flags[0] = 'R';
				if (vi.flags & VF_WRITE)
					flags[1] = 'W';
				if (vi.flags & VF_EXEC)
					flags[2] = 'E';
				if (vi.flags & VF_SHARED)
					flags[3] = 'S';
				if (vi.flags & VF_MAPPED)
					flags[4] = 'M';
				printf(" %08lx %08lx %8x %s\n",
				       (long)vi.virt, (long)vi.phys,
				       vi.size, flags);
			}
		}
	} while (rc == 0);
}

static int
kd_vm(int argc, char **argv)
{
	struct taskinfo ti;
	int rc;

	printf("VM information:\n");

	rc = 0;
	ti.cookie = 0;
	do {
		/* Get task information from kernel. */
		rc = sysinfo(INFO_TASK, &ti);
		if (!rc) {
			if (ti.vmsize != 0) {
				printf("\ntask=%08lx name=%s total=%dK bytes\n",
				       ti.id, ti.taskname, ti.vmsize / 1024);
				kd_vm_region(ti.id);
			}
		}
	} while (rc == 0);

	return 0;
}

static int
kd_device(int argc, char **argv)
{
	struct devinfo di;
	char flags[6];
	int rc;

	printf("Device list:\n");
	printf(" device   name         flags\n");
	printf(" -------- ------------ -----\n");

	rc = 0;
	di.cookie = 0;
	do {
		/* Get device information from kernel. */
		rc = sysinfo(INFO_DEVICE, &di);
		if (!rc) {
			strlcpy(flags, "-----", sizeof(flags));
			if (di.flags & D_CHR)
				flags[0] = 'C';
			if (di.flags & D_BLK)
				flags[1] = 'B';
			if (di.flags & D_REM)
				flags[2] = 'R';
			if (di.flags & D_PROT)
				flags[3] = 'P';
			if (di.flags & D_TTY)
				flags[4] = 'T';

			printf(" %08lx %12s %s\n", di.id, di.name, flags);
		}
	} while (rc == 0);

	return 0;
}

static int
kd_driver(int argc, char **argv)
{

	driver_dump();
	return 0;
}

static int
kd_irq(int argc, char **argv)
{
	struct irqinfo ii;
	int rc;

	printf("Interrupt table:\n");
	printf(" vector count    pending IST pri thread\n");
	printf(" ------ -------- ----------- --- --------\n");

	rc = 0;
	ii.cookie = 0;
	do {
		rc = sysinfo(INFO_IRQ, &ii);
		if (!rc) {
			printf("   %4d %8d    %8d %3d %08lx\n",
			       ii.vector, ii.count, ii.istreq,
			       ii.priority, (long)ii.thread);
		}
	} while (rc == 0);

	return 0;
}

static int
kd_trap(int argc, char **argv)
{

	printf("Trap frame:\n");

	dbgctl(DBGC_DUMPTRAP, NULL);
	return 0;
}

static int
kd_devstat(int argc, char **argv)
{

	printf("Device state:\n");

	device_broadcast(DEVCTL_DBG_DEVSTAT, NULL, 1);
	return 0;
}

static int
kd_trace(int argc, char **argv)
{
	task_t task;

	if (argc != 2 || !strncmp(argv[1], "-?", 2)) {
		printf("usage: trace taskname\n");
		return 0;
	}

	task = kd_lookup_task(argv[1]);
	if (task == TASK_NULL)
		return KERR_INVAL;

	printf("Toggle trace flag: %s (%08lx)\n", argv[1], (long)task);
	dbgctl(DBGC_TRACE, (void *)task);

	return 0;
}

static int
kd_examine(int argc, char **argv)
{
	char *p;
	u_char *kp;
	static u_long len = 16;
	static u_long cnt;
	static vaddr_t addr;
	static int size = 4;
	static char fmt = '*';

	p = argv[1];
	switch (argc) {
	case 1:
		/* Use previous address and format */
		p = NULL;
		break;
	case 2:
		p = argv[1];
		len = 16;
		break;
	case 3:
		if (*p != '/')
			return KERR_INVAL;
		p++;
		switch (*p) {
		case 'c':
		case 'b':
		case 'h':
		case 'w':
			fmt = *p;
			p++;
			break;
		}
		len = strtoul(p, NULL, 16);
		if (len == ULONG_MAX)
			return KERR_INVAL;
		p = argv[2];
		break;
	default:
		return KERR_SYNTAX;
	}
	if (p != NULL) {
		addr = strtoul(p, NULL, 16);
		if (addr == ULONG_MAX)
			return KERR_INVAL;
	}

	if ((kp = kmem_map((void *)addr, (size_t)len)) == NULL)
		return KERR_BADADDR;

	for (cnt = 0; cnt < len;) {
		if ((cnt % 16) == 0)
			printf("\n%08lx: ", (long)addr);

		switch (fmt) {
		case 'c':
			printf("%c", *kp);
			size = 1;
			break;
		case 'b':
			printf("%02x ", *kp);
			size = 1;
			break;
		case 'h':
			printf("%04x ", *(u_short *)kp);
			size = 2;
			break;
		case 'w':
		default:
			printf("%08lx ", *(u_long *)kp);
			size = 4;
			break;
		}
		addr += size;
		kp += size;
		cnt += size;
	}
	return 0;
}

static int
kd_write(int argc, char **argv)
{
	vaddr_t addr;
	int size = 4;
	u_char *kp;
	char *p, *pa, *pv;
	u_long val;
	int i;

	if (argc < 3)
		return KERR_INVAL;

	pa = argv[1];
	pv = argv[2];

	if (argc == 4) {
		p = argv[1];
		if (*p != '/')
			return KERR_INVAL;
		p++;
		switch (*p) {
		case 'b':
			size = 1;
			break;
		case 'h':
			size = 2;
			break;
		case 'w':
			size = 4;
			break;
		default:
			return KERR_INVAL;
		}
		pa = argv[2];
		pv = argv[3];
	}
	addr = strtoul(pa, NULL, 16);
	if (addr == ULONG_MAX)
		return KERR_INVAL;

	val = strtoul(pv, NULL, 16);
	if (val == ULONG_MAX)
		return KERR_INVAL;

	if ((kp = kmem_map((void *)addr, (size_t)size)) == NULL)
		return KERR_BADADDR;

#if BYTE_ORDER == LITTLE_ENDIAN
	for (i = 0; i < size; i++) {
#else /* BYTE_ORDER == BIG_ENDIAN */
	for (i = size; i-- > 0;) {
#endif /* BYTE_ORDER */
		/* FIXME: need to check alignment...  */
		*(char *)((char *)addr + i) = (char)(val & 0xff);
		val >>= 8;
	}

	return 0;
}

static int
kd_dispatch(int argc, char **argv)
{
	int i = 0;
	int error = 0;

	while (cmd_table[i].cmd != NULL) {
		if (!strncmp(argv[0], cmd_table[i].cmd, LINE_MAX)) {
			error = (cmd_table[i].func)(argc, argv);
			break;
		}
		i++;
	}
	if (cmd_table[i].cmd == NULL)
		error = KERR_SYNTAX;

	if (error > 0 && error <= KERR_MAX)
		kd_error(error);

	if (error == -1)
		return -1;
	return 0;
}

static int
kd_parse_line(char *line)
{
	static char *args[ARGMAX];
	char *p, *word = NULL;
	int argc = 0;
	int rc = 0;

	if (line[0] != ' ' && line[0] != '\t')
		word = line;

	p = line;
	while (*p) {
		if (word == NULL) {
			/* Skip white space. */
			if (*p != ' ' && *p != '\t')
				word = p;
		} else {
			if (*p == ' ' || *p == '\t') {
				*p = '\0';
				args[argc++] = word;
				word = NULL;
				if (argc >= ARGMAX - 1) {
					kd_error(KERR_TOOMANY);
					return 0;
				}
			}
		}
		p++;
	}
	if (word)
		args[argc++] = word;
	args[argc] = NULL;

	if (argc) {
		if (kd_dispatch(argc, args))
			rc = 1;
	}
	return rc;
}

static void
kd_read_line(char *line)
{
	int c, pos = 0;
	char *p = line;

	for (;;) {
		c = cons_getc();

		switch(c) {
		case '\n':
		case '\r':
			*p = '\0';
			printf("\n");
			return;
		case '\b':
		case 127:
			if (pos > 0) {
				*p = '\0';
				p--;
				pos--;
				printf("\b");
				printf(" ");
				printf("\b");
			}
			break;
		default:
			*p = (char)c;
			p++;
			pos++;
			if (pos > LINE_MAX) {
				*p = '\0';
				return;
			}
			printf("%c", c);
			break;
		}
	}
}

void
kd_invoke(void *arg)
{
	static char line[LINE_MAX];
	int s;

	printf("\n-------------------------------\n");
	printf(" Entering debugger.\n");
	printf(" Type 'help' to list commands.\n");
	printf("-------------------------------\n");

	s = spl0();

	/* Set input device to polling mode. */
	cons_pollc(1);

	for (;;) {
		printf("\n[kd] ");
		kd_read_line(line);
		if (kd_parse_line(line))
			break;
	}
	cons_pollc(0);
	splx(s);
}

/*
 * User can enter kd by pressing ctrl+k key at any time.
 */
void
kd_enter(void)
{

	/* Call back in DPC level */
	sched_dpc(&kd_dpc, &kd_invoke, NULL);
}

/*
 * Callback handler for abort.
 */
void
kd_abort(void)
{

	kd_invoke(NULL);
}

void
kd_init(void)
{

	/*
	 * Install abort handler to catch assert & panic events.
	 */
	dbgctl(DBGC_SETABORT, &kd_abort_ops);
}
