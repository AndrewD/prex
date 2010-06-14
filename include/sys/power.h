/*
 * Copyright (c) 2008, Kohsuke Ohtani
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

#ifndef _SYS_POWER_H
#define _SYS_POWER_H

#include <conf/config.h>

/*
 * Power policy
 */
#define PM_PERFORMANCE		0
#define PM_POWERSAVE		1

/*
 * Power state
 */
#define PWR_ON			0
#define PWR_SUSPEND		1
#define PWR_OFF			2
#define PWR_REBOOT		3

/*
 * Power management events
 */
#define PME_NO_EVENT		0
#define PME_PWRBTN_PRESS	1
#define PME_SLPBTN_PRESS	2
#define PME_LCD_OPEN		3
#define PME_LCD_CLOSE		4
#define PME_AC_ATTACH		5
#define PME_AC_DETACH		6
#define PME_LOW_BATTERY		7
#define PME_USER_ACTIVITY	8

/*
 * Default power policy
 */
#ifdef CONFIG_PM_POWERSAVE
#define DEFAULT_POWER_POLICY	PM_POWERSAVE
#else
#define DEFAULT_POWER_POLICY	PM_PERFORMANCE
#endif

#endif	/* !_SYS_POWER_H */
