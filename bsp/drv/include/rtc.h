/*	$OpenBSD: time.h,v 1.25 2007/05/09 17:42:19 deraadt Exp $	*/
/*	$NetBSD: time.h,v 1.18 1996/04/23 10:29:33 mycroft Exp $	*/

/*
 * Copyright (c) 1982, 1986, 1993
 *	The Regents of the University of California.  All rights reserved.
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
 *	@(#)time.h	8.2 (Berkeley) 7/10/94
 */

/* Modified for Prex by Kohsuke Ohtani */

#ifndef _RTC_H
#define _RTC_H

#include <sys/cdefs.h>

struct rtc_ops {
	int	(*gettime)(void *aux, struct timeval *tv);
	int	(*settime)(void *aux, struct timeval *tv);
};

/*
 * "POSIX time" to/from "YY/MM/DD/hh/mm/ss"
 */
struct clock_ymdhms {
        u_short		year;
        u_char		mon;
        u_char		day;
        u_char		dow;	/* Day of week */
        u_char		hour;
        u_char		min;
        u_char		sec;
        u_char		nsec;
};

/*
 * BCD to decimal and decimal to BCD.
 */
#define FROMBCD(x)      (u_char)(((x) >> 4) * 10 + ((x) & 0xf))
#define TOBCD(x)        (u_char)(((x) / 10 * 16) + ((x) % 10))

/* Some handy constants. */
#define SECDAY          86400L
#define SECYR           (SECDAY * 365)

/* Traditional POSIX base year */
#define POSIX_BASE_YEAR 1970

__BEGIN_DECLS
time_t	rtc_ymdhms_to_secs(struct clock_ymdhms *dt);
void	rtc_secs_to_ymdhms(time_t secs, struct clock_ymdhms *dt);
void	rtc_attach(struct rtc_ops *ops, void *aux);
__END_DECLS

#endif /* !_RTC_H */
