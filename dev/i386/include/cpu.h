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

#ifndef _CPU_H
#define _CPU_H

#include <sys/cdefs.h>
#include <sys/types.h>

#define BREAKPOINT() __asm__ ("int $3"::)

/*
 * I/O instructions
 */
static __inline void
outb(unsigned char value, int port)
{
	__asm__ __volatile__(
		"outb %b0, %w1"
		::"a" (value),"Nd" (port));
}

static __inline void
outw(unsigned short value, int port)
{
	__asm__ __volatile__(
		"outw %w0, %w1"
		::"a" (value),"Nd" (port));
}

static __inline void
outl(unsigned long value, int port)
{
	__asm__ __volatile__(
		"outl %0, %w1"
		::"a" (value),"Nd" (port));
}

static __inline unsigned char
inb(int port)
{
	unsigned char _val;
	__asm__ __volatile__(
		"inb %w1, %b0"
		:"=a" (_val)
		:"Nd" (port));
	return _val;
}

static __inline unsigned short
inw(int port)
{
	unsigned char _val;
	__asm__ __volatile__(
		"inw %w1, %w0"
		:"=a" (_val)
		:"Nd" (port));
	return _val;
}

static __inline unsigned long
inl(int port)
{
	unsigned char _val;
	__asm__ __volatile__(
		"inl %w1, %0"
		:"=a" (_val)
		:"Nd" (port));
	return _val;
}

static __inline void
outb_p(unsigned char value, int port)
{
	__asm__ __volatile__(
		"outb %b0, %w1\n\t"
		"outb %%al, $0x80\n\t"
		::"a" (value),"Nd" (port));
}

static __inline unsigned char
inb_p(int port)
{
	unsigned char _val;
	__asm__ __volatile__(
		"inb %w1, %b0\n\t"
		"outb %%al, $0x80\n\t"
		:"=a" (_val)
		:"Nd" (port));
	return _val;
}


/*
 * Macro to read/write the machine specific register (MSR)
 */
#define rdmsr(msr,val1,val2) \
	__asm__ __volatile__("rdmsr" \
			  : "=a" (val1), "=d" (val2) \
			  : "c" (msr))

#define wrmsr(msr,val1,val2) \
	__asm__ __volatile__("wrmsr" \
			  : /* no outputs */ \
			  : "c" (msr), "a" (val1), "d" (val2))

/*
 * Get cpu id
 */
static __inline void
cpuid(u_int eax, u_int *p)
{
	__asm__ __volatile__(
		"cpuid\n\t"
		"movl %%eax, 0(%2)\n\t"
		"movl %%ebx, 4(%2)\n\t"
		"movl %%ecx, 8(%2)\n\t"
		"movl %%edx, 12(%2)\n\t"
		: "=a" (eax)
		: "0" (eax), "S" (p)
		: "bx", "cx", "dx");
}

#endif	/* !_CPU_H */
