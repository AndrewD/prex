/*-
 * Copyright (c) 2008, Andrew Dennison
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
 * fifo.c - fifo test program
 *
 * make BOOTTASKS='$(SRCDIR)/usr/server/fs/fs $(SRCDIR)/usr/test/fifo2/fifo' \
 *      BOOTFILES=''
 */

#include <prex/prex.h>

#include <sys/syslog.h>
#include <sys/mount.h>
#include <sys/fcntl.h>

#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <errno.h>
#include <err.h>

#define BUF_SIZE (PIPE_BUF * 2)
static const char fifo_name[] = "/tmp/fifo";
static char write_buf[BUF_SIZE];
static char read_buf[BUF_SIZE];

static char th_stack[USTACK_SIZE];

#define WR1_SIZE (PIPE_BUF / 2)
#define WR2_SIZE (BUF_SIZE - WR1_SIZE)
#define RD1_SIZE (PIPE_BUF * 3 / 4) /* > WR1_SIZE */
#define RD2_SIZE (BUF_SIZE - RD1_SIZE)

static thread_t
thread_run(char *name, void (*start)(void),
	   void *stack, size_t stack_size, int nice)
{
	thread_t th = 0;
	int rc;

	if ((rc = thread_create(task_self(), &th)) != 0)
		errx(1, "thread_create: %s", strerror(rc));

	if ((rc = thread_load(th, start, (uint8_t*)stack + stack_size)) != 0)
		errx(1, "thread_load: %s", strerror(rc));

	thread_name(th, name);

	if ((rc = thread_setprio(th, PRIO_DFLT + nice)) != 0)
		errx(1, "thread_setprio: %s", strerror(rc));

	if ((rc = thread_resume(th)) != 0)
		errx(1, "thread_resume: %s", strerror(rc));

	return th;
}

static int
validate_write(int from, int to)
{
        int rc = 0;

        for (int i = from; i < to; i++) {
                if (write_buf[i] != (i & 0x7F)) {
                        rc = EINVAL;
                        syslog(LOG_INFO, "wr[%d] = %d\n", i, write_buf[i]);
                }
        }
        return rc;
}

static int
validate_read(int from, int to)
{
        int rc = 0;

        for (int i = from; i < to; i++) {
                if (read_buf[i] != (i & 0x7F)) {
                        rc = EINVAL;
                        syslog(LOG_INFO, "rd[%d] = %d\n", i, read_buf[i]);
                }
        }
        return rc;
}

static int rd_fd, wr_fd;

static void
read_thread(void)
{
	int rc;

	memset(read_buf, 0, BUF_SIZE);

	syslog(LOG_INFO, "read_thread:\n  read1...");
	if ((rc = read(rd_fd, read_buf, RD1_SIZE)) < 0)
		err(1, "read1 %d", RD1_SIZE);
	else if (rc != RD1_SIZE)
		errx(1, "read1 %d, expected %d", rc, RD1_SIZE);

	syslog(LOG_INFO, "read1 ok\n  read2...");
	if ((rc = read(rd_fd, read_buf + RD1_SIZE, RD2_SIZE)) < 0)
		err(1, "read2 %d", RD2_SIZE);
	else if (rc != RD2_SIZE)
		errx(1, "read2 %d, expected %d", rc, RD2_SIZE);

        syslog(LOG_INFO, "read2 ok\n  close...");
	if (close(rd_fd) < 0)
		err(1, "close(%d)", rd_fd);

	syslog(LOG_INFO, "read close ok\n  read thread_terminate\n");
	thread_terminate(thread_self());
}

static void
blocking_test(void)
{
	thread_t th;
	int rc;

	for (int i = 0; i < BUF_SIZE; i++)
		write_buf[i] = i & 0x7F;

	syslog(LOG_INFO, "fifo blocking:\n wr_open...");
	if ((wr_fd = open(fifo_name, O_WRONLY, 0)) < 0)
		err(1, "open(%s)", fifo_name);

	syslog(LOG_INFO, "write open ok\n  write1...");
	if ((rc = write(wr_fd, write_buf, WR1_SIZE)) != -1)
		errx(1, "write1 %d, expected %d", rc, 0);
	else if (errno != EPIPE)
		err(1, "write1 %d", WR1_SIZE);

	syslog(LOG_INFO, "write1 ok\n  rd_open...");
	if ((rd_fd = open(fifo_name, O_RDONLY, 0)) < 0)
		err(1, "open(%s)", fifo_name);

	syslog(LOG_INFO, "read open ok\n  write1a...");
	if ((rc = write(wr_fd, write_buf, WR1_SIZE)) < 0)
		err(1, "write1a %d", WR1_SIZE);
	else if (rc != WR1_SIZE)
		errx(1, "wrote1a %d, expected %d", rc, WR1_SIZE);

	syslog(LOG_INFO, "write1a ok\n  thread_run...");
	th = thread_run("read", read_thread, th_stack, sizeof(th_stack), 1);

	syslog(LOG_INFO, "thread_run ok\n  write2...");
	rc = write(wr_fd, write_buf + WR1_SIZE, WR2_SIZE);
	if (rc < 0)
		err(1, "write2 %d", WR2_SIZE);
	else if (rc != WR2_SIZE)
		errx(1, "wrote2 %d, expected %d", rc, WR2_SIZE);

	syslog(LOG_INFO, "write2 ok\n  close...");
	if (close(wr_fd) < 0)
		err(1, "close(%d)", wr_fd);

	syslog(LOG_INFO, "write close ok\n  sleep...");
	timer_sleep(1000/*ms*/, NULL);

	syslog(LOG_INFO, "sleep done\n  data check...");
        if (validate_write(0, BUF_SIZE) || validate_read(0, BUF_SIZE))
                errx(1, "data corrupt");

        syslog(LOG_INFO, "data check ok\nblocking test complete\n");
}

#undef WR1_SIZE
#undef WR2_SIZE
#undef RD1_SIZE
#undef RD2_SIZE

#define WR1_SIZE (PIPE_BUF / 2)
#define RD1_SIZE (PIPE_BUF * 3 / 4) /* > WR1_SIZE, expect WR1_SIZE */
#define WR2_SIZE (PIPE_BUF)
#define RD2_SIZE (PIPE_BUF - 100) /* < WR2_SIZE */

#define WR3_SIZE (BUF_SIZE - WR1_SIZE - WR2_SIZE)
#define RD3_SIZE (BUF_SIZE - WR1_SIZE - RD2_SIZE)


static void
nonblock_test(void)
{
	int rc;
	char *rd = read_buf, *wr = write_buf;

	memset(read_buf, 0, BUF_SIZE);

	syslog(LOG_INFO, "fifo non-blocking test start...\n");

	syslog(LOG_INFO, "ok\n rd open...");
	if ((rd_fd = open(fifo_name, O_RDONLY | O_NONBLOCK, 0)) < 0)
		err(1, "open(%s)", fifo_name);

	syslog(LOG_INFO, "ok\n  read1 expecting EOF...");
	if ((rc = read(rd_fd, rd, RD1_SIZE)) < 0)
		err(1, "read1 %d", RD1_SIZE);
	else if (rc != 0)
		errx(1, "read1 %d, expected %d", rc, 0);

	syslog(LOG_INFO, "ok\n wr open...");
	if ((wr_fd = open(fifo_name, O_WRONLY | O_NONBLOCK, 0)) < 0)
		err(1, "open(%s)", fifo_name);

	syslog(LOG_INFO, "ok\n  read1a expecting EAGAIN...");
	if ((rc = read(rd_fd, rd, RD1_SIZE)) != -1)
		errx(1, "read1a %d, expected %d", rc, 0);
	else if (errno != EAGAIN)
		err(1, "read1a %d", RD1_SIZE);

	syslog(LOG_INFO, "ok\n  write1...");
	if ((rc = write(wr_fd, wr, WR1_SIZE)) < 0)
		err(1, "write1 %d", WR1_SIZE);
	else if (rc != WR1_SIZE)
		errx(1, "write1 %d, expected %d", rc, WR1_SIZE);
	wr += rc;

	syslog(LOG_INFO, "ok\n  read1b...");
	if ((rc = read(rd_fd, rd, RD1_SIZE)) < 0)
		err(1, "read1b %d", RD1_SIZE);
	else if (rc != WR1_SIZE) /* expect what was written */
		errx(1, "read1b %d, expected %d", rc, WR1_SIZE);
	rd += rc;

	syslog(LOG_INFO, "ok\n  write2...");
	if ((rc = write(wr_fd, wr, WR2_SIZE)) < 0)
		err(1, "write2 %d", WR2_SIZE);
	else if (rc != WR2_SIZE)
		errx(1, "write2 %d, expected %d", rc, WR2_SIZE);
	wr += rc;

	syslog(LOG_INFO, "ok\n  read2...");
	if ((rc = read(rd_fd, rd, RD2_SIZE)) < 0)
		err(1, "read2 %d", RD2_SIZE);
	else if (rc != RD2_SIZE)
		errx(1, "read2 %d, expected %d", rc, RD2_SIZE);
	rd += rc;

	syslog(LOG_INFO, "ok\n  write3...");
	rc = write(wr_fd, wr, WR3_SIZE);
	if (rc < 0)
		err(1, "write3 %d", WR3_SIZE);
	else if (rc != WR3_SIZE)
		errx(1, "write3 %d, expected %d", rc, WR3_SIZE);
	wr += rc;

	syslog(LOG_INFO, "ok\n  close wr...");
	if (close(wr_fd) < 0)
		err(1, "close(%d)", wr_fd);

	syslog(LOG_INFO, "ok\n  read3...");
	if ((rc = read(rd_fd, rd, RD3_SIZE)) < 0)
		err(1, "read3 %d", RD3_SIZE);
	else if (rc != RD3_SIZE)
		errx(1, "read3 %d, expected %d", rc, RD3_SIZE);
	rd += rc;

	syslog(LOG_INFO, "ok\n  close rd...");
	if (close(rd_fd) < 0)
		err(1, "close(%d)", rd_fd);

	syslog(LOG_INFO, "ok\n  data check...");
        if (validate_write(0, BUF_SIZE) || validate_read(0, BUF_SIZE))
                errx(1, "data corrupt");

	syslog(LOG_INFO, "ok\nnon-blocking test complete\n");
}

/*
 * Main routine
 */
int
main(int argc, char *argv[])
{
	syslog(LOG_INFO, "\nfifo: fs test program\n");

	/* Wait 1 sec until loading fs server */
	timer_sleep(1000, 0);

	/*
	 * Prepare to use a file system.
	 */
	fslib_init();

	/*
	 * Mount file systems
	 */
	mount("", "/", "ramfs", 0, NULL);
	mkdir("/dev", 0);
	mount("", "/dev", "devfs", 0, NULL);		/* device */
	mkdir("/tmp", 0);
	/*
	 * Prepare stdio
	 */
	open("/dev/tty", O_RDWR);	/* stdin */
	dup(0);				/* stdout */
	dup(0);				/* stderr */

	if (mkfifo(fifo_name, 0) < 0)
		err(1, "mkfifo(%s)", fifo_name);

	blocking_test();
	nonblock_test();

	/* sleep a bit */
	timer_sleep(2000/*ms*/, NULL);

	/*
	 * Disconnect from a file system.
	 */
	fslib_exit();
	return 0;
}
