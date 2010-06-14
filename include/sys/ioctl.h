/*
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * 3. Neither the name of the author nor the names of any co-contibutors
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

#ifndef _SYS_IOCTL_H_
#define _SYS_IOCTL_H_

#include <sys/cdefs.h>
#include <sys/time.h>
#include <sys/power.h>

/*
 * Ioctl's have the command encoded in the lower word, and the size of
 * any in or out parameters in the upper word.  The high 3 bits of the
 * upper word are used to encode the in/out status of the parameter.
 */
#define	IOCPARM_MASK	0xff		/* parameter length, at most 13 bits */
#define	IOCPARM_LEN(x)	(((x) >> 16) & IOCPARM_MASK)
#define	IOCBASECMD(x)	((x) & ~(IOCPARM_MASK << 16))
#define	IOCGROUP(x)	(((x) >> 8) & 0xff)

#define IOCPARM_MAX	256

#define	IOC_VOID	0x20000000	/* no parameters */
#define	IOC_OUT		0x40000000	/* copy out parameters */
#define	IOC_IN		0x80000000U	/* copy in parameters */
#define	IOC_INOUT	(IOC_IN|IOC_OUT)
#define	IOC_DIRMASK	0xe0000000

/* Prex */
#define IOC_IVAL	0x10000000	/* input argument is immediate value */
#define IOC_OVAL	0x08000000	/* return value as output */

#define	_IOC(inout,group,num,len) \
	(u_long)(inout | ((len & IOCPARM_MASK) << 16) | ((group) << 8) | (num))
#define	_IO(g,n)	_IOC(IOC_VOID,	(g), (n), 0)
#define	_IOR(g,n,t)	_IOC(IOC_OUT,	(g), (n), sizeof(t))
#define	_IOW(g,n,t)	_IOC(IOC_IN,	(g), (n), sizeof(t))
/* this should be _IORW, but stdio got there first */
#define	_IOWR(g,n,t)	_IOC(IOC_INOUT,	(g), (n), sizeof(t))

#define	_IORN(g,n,t)	_IOC((IOC_OVAL|IOC_OUT), (g), (n), sizeof(t))
#define	_IOWN(g,n,t)	_IOC((IOC_IVAL|IOC_IN),  (g), (n), sizeof(t))


/*
 * CPU frequency I/O control code
 */
#define CFIOC_GET_INFO		 _IOR('6', 0, struct cpufreqinfo)

/*
 * CPU frequency information
 */
struct cpufreqinfo {
	int	maxfreq;	/* max speed in MHz */
	int	maxvolts;	/* max power in mV */
	int	freq;		/* current speed in MHz */
	int	volts;		/* current power in mV */
};

/*
 * Power management I/O control code
 */
#define PMIOC_CONNECT		 _IOW('P', 0, int)
#define PMIOC_QUERY_EVENT	 _IOW('P', 1, int)
#define PMIOC_SET_POWER		 _IOW('P', 2, int)
#define PMIOC_GET_SUSTMR	 _IOR('P', 3, int)
#define PMIOC_SET_SUSTMR	 _IOW('P', 4, int)
#define PMIOC_GET_DIMTMR	 _IOR('P', 5, int)
#define PMIOC_SET_DIMTMR	 _IOW('P', 6, int)
#define PMIOC_GET_POLICY	 _IOR('P', 7, int)
#define PMIOC_SET_POLICY	 _IOW('P', 8, int)

/*
 * RTC I/O control code
 */
#define RTCIOC_GET_TIME		 _IOR('R', 0, struct __timeval)
#define RTCIOC_SET_TIME		 _IOW('R', 1, struct __timeval)

struct __timeval {
	long	tv_sec;		/* seconds */
	long	tv_usec;	/* and microseconds */
};

__BEGIN_DECLS
int	ioctl(int, unsigned long, ...);
__END_DECLS

#endif	/* !_SYS_IOCTL_H_ */
