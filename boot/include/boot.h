/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

#ifndef _BOOT_H
#define _BOOT_H

#include <config.h>
#include <types.h>
#include <platform.h>
#include <arch.h>
#include <bootinfo.h>

/* #define DEBUG_BOOT		1 */
/* #define DEBUG_BOOT_IMAGE	1 */

#define PAGE_SIZE	CONFIG_PAGE_SIZE

/* Macro to adjust alignment */
#define PAGE_MASK	(PAGE_SIZE-1)
#define PAGE_ALIGN(n)	((((u_long)(n)) + PAGE_MASK) & ~PAGE_MASK)

/* Page mapping */
#define phys_to_virt(p_addr)	(void *)((u_long)(p_addr) + PAGE_OFFSET)
#define virt_to_phys(v_addr)	(void *)((u_long)(v_addr) - PAGE_OFFSET)

/* stdarg */
typedef __builtin_va_list va_list;
#define va_start(v,l)	__builtin_stdarg_start((v),l)
#define va_end		__builtin_va_end
#define va_arg		__builtin_va_arg
#define __va_copy(d,s)	__builtin_va_copy((d),(s))

extern u_long load_base;
extern u_long load_start;

/* stdlib */
extern long atol(char *nptr);

/* string */
extern char *strncpy(char *dest, const char *src, size_t count);
extern int strncmp(const char *src, const char *tgt, size_t count);
extern size_t strnlen(const char *str, size_t count);
extern void *memcpy(void *dest, const void *src, size_t count);
extern void *memset(void *dest, int ch, size_t count);

/* debug */
extern void panic(const char *msg);
extern void printk(const char *fmt, ...);

/* elf */
extern int elf_load(char *img, struct img_info *info);

/* main */
extern void reserve_memory(u_long start, size_t size);

#endif /* !_BOOT_H */
