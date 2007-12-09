/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * debug.c - loader debug functions
 */
#include <boot.h>

/*
 * printk - print formated string
 */
void printk(const char *fmt, ...)
{
#ifdef DEBUG
	static const char digits[16] = "0123456789abcdef";
	va_list ap;
	char buf[10];
	char *s;
	unsigned r, u;
	int c;

	va_start(ap, fmt);
	while ((c = *fmt++)) {
		if (c == '%') {
			c = *fmt++;
			switch (c) {
			case 'c':
				putc(va_arg(ap, int));
				continue;
			case 's':
				for (s = va_arg(ap, char *); *s; s++)
					putc((int)*s);
				continue;
			case 'u':
			case 'x':
				r = c == 'u' ? 10U : 16U;
				u = va_arg(ap, unsigned);
				s = buf;
				do
					*s++ = digits[u % r];
				while (u /= r);
				while (--s >= buf)
					putc((int)*s);
				continue;
			}
		}
		putc((int)c);
	}
	va_end(ap);
#endif /* DEBUG */
}

/*
 * Show error and hang up.
 */
void panic(const char *msg)
{
	printk("Error: %s\n", msg);
	for (;;);
}
