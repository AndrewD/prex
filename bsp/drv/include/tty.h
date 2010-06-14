/*-
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
 * (c) UNIX System Laboratories, Inc.
 * All or some portions of this file are derived from material licensed
 * to the University of California by American Telephone and Telegraph
 * Co. or Unix System Laboratories, Inc. and are reproduced herein with
 * the permission of UNIX System Laboratories, Inc.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)tty.h	8.7 (Berkeley) 1/9/95
 */

#ifndef _SYS_TTY_H_
#define _SYS_TTY_H_

#include <sys/cdefs.h>
#include <sys/termios.h>
#include <sys/syslimits.h>

#define TTYQ_SIZE	MAX_INPUT
#define TTYQ_HIWAT	(TTYQ_SIZE - 10)

struct tty_queue {
	char	tq_buf[TTYQ_SIZE];
	int	tq_head;
	int	tq_tail;
	int	tq_count;
};

/*
 * Per-tty structure.
 */
struct tty {
	struct tty_queue t_rawq;	/* raw input queue */
	struct tty_queue t_canq;	/* canonical queue */
	struct tty_queue t_outq;	/* ouput queue */
	struct termios	t_termios;	/* termios state */
	struct winsize	t_winsize;	/* window size */
	struct event	t_input;	/* event for input data ready */
	struct event	t_output;	/* event for output completion */
	void (*t_oproc)(struct tty *);	/* routine to start output */
	device_t	t_dev;		/* device */
	int		t_state;	/* driver state */
	int		t_column;	/* tty output column */
	pid_t		t_pgid;		/* foreground process group. */
	task_t		t_sigtask;	/* task to dispatch the tty signal */
	int		t_signo;	/* pending signal# */
	struct dpc	t_dpc;		/* dpc for tty */
};

#define	t_iflag		t_termios.c_iflag
#define	t_oflag		t_termios.c_oflag
#define	t_cflag		t_termios.c_cflag
#define	t_lflag		t_termios.c_lflag
#define	t_cc		t_termios.c_cc
#define	t_ispeed	t_termios.c_ispeed
#define	t_ospeed	t_termios.c_ospeed

/* These flags are kept in t_state. */
#define	TS_ASLEEP	0x00001		/* Process waiting for tty. */
#define	TS_BUSY		0x00004		/* Draining output. */
#define	TS_TIMEOUT	0x00100		/* Wait for output char processing. */
#define	TS_TTSTOP	0x00200		/* Output paused. */
#define	TS_ISIG		0x00400		/* Input is interrupted by signal. */

__BEGIN_DECLS
int	 tty_read(struct tty *, char *, size_t *);
int	 tty_write(struct tty *, char *, size_t *);
int	 tty_ioctl(struct tty *, u_long, void *);
void	 tty_input(int, struct tty *);
int	 tty_getc(struct tty_queue *);
void	 tty_done(struct tty *);
void	 tty_attach(struct tty *);
__END_DECLS

#endif /* !_SYS_TTY_H_ */
