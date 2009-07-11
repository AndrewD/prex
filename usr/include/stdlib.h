/*-
 * Copyright (c) 1990, 1993
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
 *	@(#)stdlib.h	8.5 (Berkeley) 5/19/95
 */

#ifndef _STDLIB_H_
#define _STDLIB_H_

#if !defined(_SIZE_T)
#define _SIZE_T
typedef	unsigned int	size_t;		/* size of something in bytes */
#endif

#if !defined(_WCHAR_T)
#define _WCHAR_T
typedef int		wchar_t;
#endif

typedef struct {
	int quot;		/* quotient */
	int rem;		/* remainder */
} div_t;

typedef struct {
	long quot;		/* quotient */
	long rem;		/* remainder */
} ldiv_t;

#ifndef NULL
#define	NULL	0
#endif

#define	EXIT_FAILURE	1
#define	EXIT_SUCCESS	0

#define	RAND_MAX	0x7fffffff

extern int __mb_cur_max;
#define	MB_CUR_MAX	__mb_cur_max

#include <sys/cdefs.h>

__BEGIN_DECLS
void	 abort(void) __noreturn;
int	 abs(int);
int	 atexit(void (*)(void));
double	 atof(const char *);
int	 atoi(const char *);
long	 atol(const char *);
void	*bsearch(const void *, const void *, size_t,
	    size_t, int (*)(const void *, const void *));
void	*calloc(size_t, size_t);
div_t	 div(int, int);
void	 exit(int) __noreturn;
void	 free(void *);
char	*getenv(const char *);
long	 labs(long);
ldiv_t	 ldiv(long, long);
void	 mstat(void);
void	 mchk(void);
void	*malloc(size_t);
void	 qsort(void *, size_t, size_t,
	    int (*)(const void *, const void *));
int	 rand(void);
void	*realloc(void *, size_t);
void	 srand(unsigned);
double	 strtod(const char *, char **);
long	 strtol(const char *, char **, int);
unsigned long
	 strtoul(const char *, char **, int);
int	 system(const char *);

/* These are currently just stubs. */
int	 mblen(const char *, size_t);
size_t	 mbstowcs(wchar_t *, const char *, size_t);
int	 wctomb(char *, wchar_t);
int	 mbtowc(wchar_t *, const char *, size_t);
size_t	 wcstombs(char *, const wchar_t *, size_t);

#ifndef _ANSI_SOURCE
int	 putenv(const char *);
int	 setenv(const char *, const char *, int);
#endif

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define alloca(x) __builtin_alloca(x)	/* built-in for gcc */

					/* getcap(3) functions */
char	*getbsize(int *, long *);
char	*cgetcap(char *, char *, int);
int	 cgetclose(void);
int	 cgetent(char **, char **, char *);
int	 cgetfirst(char **, char **);
int	 cgetmatch(char *, char *);
int	 cgetnext(char **, char **);
int	 cgetnum(char *, char *, long *);
int	 cgetset(char *);
int	 cgetstr(char *, char *, char **);
int	 cgetustr(char *, char *, char **);

int	 daemon(int, int);
char	*devname(int, int);
int	 getloadavg(double [], int);

char	*group_from_gid(unsigned long, int);
int	 heapsort(void *, size_t, size_t,
	    int (*)(const void *, const void *));
char	*initstate(unsigned long, char *, long);
int	 mergesort(void *, size_t, size_t,
	    int (*)(const void *, const void *));
int	 radixsort(const unsigned char **, int, const unsigned char *,
	    unsigned);
int	 sradixsort(const unsigned char **, int, const unsigned char *,
	    unsigned);
long	 random(void);
char	*realpath(const char *, char resolved_path[]);
char	*setstate(char *);
void	 srandom(unsigned long);
char	*user_from_uid(unsigned long, int);
#ifndef __STRICT_ANSI__
long long
	 strtoq(const char *, char **, int);
unsigned long long
	 strtouq(const char *, char **, int);
#endif
void	 unsetenv(const char *);
#endif

void	*malloc_r(size_t);
void	 free_r(void *);

__END_DECLS

#endif /* _STDLIB_H_ */
