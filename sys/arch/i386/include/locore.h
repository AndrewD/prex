/*-
 * Copyright (c) 2007, Kohsuke Ohtani
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

#ifndef _I386_LOCORE_H
#define _I386_LOCORE_H

#include <arch.h>

extern void intr_0(void);
extern void intr_1(void);
extern void intr_2(void);
extern void intr_3(void);
extern void intr_4(void);
extern void intr_5(void);
extern void intr_6(void);
extern void intr_7(void);
extern void intr_8(void);
extern void intr_9(void);
extern void intr_10(void);
extern void intr_11(void);
extern void intr_12(void);
extern void intr_13(void);
extern void intr_14(void);
extern void intr_15(void);
extern void trap_default(void);
extern void trap_0(void);
extern void trap_1(void);
extern void trap_2(void);
extern void trap_3(void);
extern void trap_4(void);
extern void trap_5(void);
extern void trap_6(void);
extern void trap_7(void);
extern void trap_8(void);
extern void trap_9(void);
extern void trap_10(void);
extern void trap_11(void);
extern void trap_12(void);
extern void trap_13(void);
extern void trap_14(void);
extern void trap_15(void);
extern void trap_16(void);
extern void trap_17(void);
extern void trap_18(void);
extern void syscall_entry(void);
extern void syscall_ret(void);
extern void cpu_switch(struct kern_regs *, struct kern_regs *);
extern void known_fault1(void);
extern void known_fault2(void);
extern void known_fault3(void);
extern void umem_fault(void);

#endif /* !_I386_LOCORE_H */
