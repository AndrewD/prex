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
 * ipl.h - Interrupt priority level
 */

#ifndef _SYS_IPL_H
#define _SYS_IPL_H
#ifdef KERNEL

/*
 * Interrupt priority levels
 */
#define IPL_NONE	0	/* nothing (lowest) */
#define IPL_COMM	1	/* serial, parallel */
#define IPL_BLOCK	2	/* FDD, IDE */
#define IPL_NET		3	/* network */
#define IPL_DISPLAY	4	/* screen */
#define IPL_INPUT	5	/* keyboard, mouse */
#define IPL_AUDIO	6	/* audio */
#define IPL_BUS		7	/* USB, PCCARD */
#define IPL_RTC		8	/* RTC Alarm */
#define IPL_PROFILE	9	/* profiling timer */
#define IPL_CLOCK	10	/* system Clock Timer */
#define IPL_HIGH	11	/* everything */

#define NIPLS		12	/* number of IPLs */

#endif /* !KERNEL */
#endif /* !_SYS_IPL_H */
