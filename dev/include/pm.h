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

#ifndef _PM_H
#define _PM_H

#include <ioctl.h>

/*
 * Power Management Policy
 */
#define PM_PERFORMANCE		0
#define PM_POWERSAVE		1

/*
 * Power state
 */
#define POWER_ON		0
#define POWER_SUSPEND		1
#define POWER_OFF		2
#define POWER_REBOOT		3

/*
 * I/O control code
 */
#define PMIOC_SET_POWER		_IOW('P', 0, int)
#define PMIOC_SET_TIMER		_IOW('P', 1, int)
#define PMIOC_GET_TIMER		_IOR('P', 2, int)
#define PMIOC_SET_POLICY	_IOW('P', 3, int)
#define PMIOC_GET_POLICY	_IOR('P', 4, int)

extern void system_suspend(void);
extern void system_poweroff(void);

extern int pm_suspend(void);
extern int pm_poweroff(void);
extern void pm_active(void);

#endif /* !_PM_H */
