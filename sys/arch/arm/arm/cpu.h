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

/*
 * ARM Processor Status Register
 */
#define PSR_MODE	0x0000001f
#define PSR_USR_MODE	0x00000010
#define PSR_FIQ_MODE	0x00000011
#define PSR_IRQ_MODE	0x00000012
#define PSR_SVC_MODE	0x00000013
#define PSR_ABT_MODE	0x00000017
#define PSR_UND_MODE	0x0000001b
#define PSR_SYS_MODE	0x0000001f

#define PSR_THUMB	0x00000020
#define PSR_FIQ_DIS	0x00000040
#define PSR_IRQ_DIS	0x00000080


#ifdef __gba__
#define PSR_APP_MODE	PSR_SYS_MODE
#else
#define PSR_APP_MODE	PSR_USR_MODE
#endif

#ifndef __ASSEMBLY__

#endif /* !__ASSEMBLY__ */

#endif /* !_CPU_H */
