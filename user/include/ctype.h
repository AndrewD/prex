/*
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

#ifndef _CTYPE_H
#define _CTYPE_H

#define _U  0x01		/* upper */
#define _L  0x02		/* lower */
#define _D  0x04		/* digit */
#define _C  0x08		/* cntrl */
#define _P  0x10		/* punct */
#define _S  0x20		/* white space (space/lf/tab) */
#define _X  0x40		/* hex digit */
#define _SP 0x80		/* hard space (0x20) */

extern unsigned char _ctype[];

#define isalnum(c) ((int)((_ctype+1)[c]&(_U|_L|_D)))
#define isalpha(c) ((int)((_ctype+1)[c]&(_U|_L)))
#define iscntrl(c) ((int)((_ctype+1)[c]&(_C)))
#define isdigit(c) ((int)((_ctype+1)[c]&(_D)))
#define isgraph(c) ((int)((_ctype+1)[c]&(_P|_U|_L|_D)))
#define islower(c) ((int)((_ctype+1)[c]&(_L)))
#define isprint(c) ((int)((_ctype+1)[c]&(_P|_U|_L|_D|_SP)))
#define ispunct(c) ((int)((_ctype+1)[c]&(_P)))
#define isspace(c) ((int)((_ctype+1)[c]&(_S)))
#define isupper(c) ((int)((_ctype+1)[c]&(_U)))
#define isxdigit(c) ((int)((_ctype+1)[c]&(_D|_X)))

#if !defined(_ANSI_SOURCE) && !defined(_POSIX_SOURCE)
#define isascii(c) (((unsigned) c)<=0x7f)
#define toascii(c) (((unsigned) c)&0x7f)
#endif

#define tolower(c) (isupper(c)?((c)-('A'-'a')):(c))
#define toupper(c) (islower(c)?((c)-('a'-'A')):(c))

#endif /* !_CTYPE_H */
