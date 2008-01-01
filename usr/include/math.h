/*-
 * Copyright (c) 1985, 1990, 1993
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
 *	@(#)math.h	8.1 (Berkeley) 6/2/93
 */

#ifndef	_MATH_H_
#define	_MATH_H_

#if defined(vax) || defined(tahoe)		/* DBL_MAX from float.h */
#define	HUGE_VAL	1.701411834604692294E+38
#else
#define	HUGE_VAL	1e500			/* IEEE: positive infinity */
#endif

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#if defined(vax) || defined(tahoe)
/*
 * HUGE for the VAX and Tahoe converts to the largest possible F-float value.
 * This implies an understanding of the conversion behavior of atof(3).  It
 * was defined to be the largest float so that overflow didn't occur when it
 * was assigned to a single precision number.  HUGE_VAL is strongly preferred.
 */
#define	HUGE	1.701411733192644270E+38		
#else
#define	HUGE	HUGE_VAL
#endif

#define	M_E		2.7182818284590452354	/* e */
#define	M_LOG2E		1.4426950408889634074	/* log 2e */
#define	M_LOG10E	0.43429448190325182765	/* log 10e */
#define	M_LN2		0.69314718055994530942	/* log e2 */
#define	M_LN10		2.30258509299404568402	/* log e10 */
#define	M_PI		3.14159265358979323846	/* pi */
#define	M_PI_2		1.57079632679489661923	/* pi/2 */
#define	M_PI_4		0.78539816339744830962	/* pi/4 */
#define	M_1_PI		0.31830988618379067154	/* 1/pi */
#define	M_2_PI		0.63661977236758134308	/* 2/pi */
#define	M_2_SQRTPI	1.12837916709551257390	/* 2/sqrt(pi) */
#define	M_SQRT2		1.41421356237309504880	/* sqrt(2) */
#define	M_SQRT1_2	0.70710678118654752440	/* 1/sqrt(2) */
#endif /* !_ANSI_SOURCE && !_POSIX_SOURCE */

#include <sys/cdefs.h>

__BEGIN_DECLS
double	acos(double);
double	asin(double);
double	atan(double);
double	atan2(double, double);
double	ceil(double);
double	cos(double);
double	cosh(double);
double	exp(double);
double	fabs(double);
double	floor(double);
double	fmod(double, double);
	double	frexp(double, int *);
double	ldexp(double, int);
double	log(double);
double	log10(double);
	double	modf(double, double *);
double	pow(double, double);
double	sin(double);
double	sinh(double);
double	sqrt(double);
double	tan(double);
double	tanh(double);

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
double	acosh(double);
double	asinh(double);
double	atanh(double);
	double	cabs();	/* we can't describe cabs()'s argument properly */
double	cbrt(double);
double	copysign(double, double);
double	drem(double, double);
double	erf(double);
double	erfc(double);
double	expm1(double);
int	finite(double);
double	hypot(double, double);
#if defined(vax) || defined(tahoe)
double	infnan(int);
#endif
int	isinf(double);
int	isnan(double);
double	j0(double);
double	j1(double);
double	jn(int, double);
double	lgamma(double);
double	log1p(double);
double	logb(double);
double	rint(double);
double	scalb(double, int);
double	y0(double);
double	y1(double);
double	yn(int, double);
#endif

__END_DECLS

#endif /* _MATH_H_ */
