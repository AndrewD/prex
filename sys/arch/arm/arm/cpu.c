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
 * cpu.c - user memory access code for NOMMU system.
 */

#include <kernel.h>
#include <cpu.h>

#ifndef CONFIG_MMU

int
umem_copyin(const void *uaddr, void *kaddr, size_t len)
{
	if (user_area(uaddr) && user_area((u_long)uaddr + len)) {
		memcpy(kaddr, uaddr, len);
		return 0;
	}
	return EFAULT;
}

int
umem_copyout(const void *kaddr, void *uaddr, size_t len)
{
	if (user_area(uaddr) && user_area((u_long)uaddr + len)) {
		memcpy(uaddr, kaddr, len);
		return 0;
	}
	return EFAULT;
}

int
umem_strnlen(const char *uaddr, size_t maxlen, size_t *len)
{
	if (user_area(uaddr)) {
		*len = strnlen(uaddr, maxlen);
		return 0;
	}
	return EFAULT;
}
#endif
