/*
 * Copyright (c) 2009, Kohsuke Ohtani
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

#ifndef _DEVCTL_H
#define _DEVCTL_H

/*
 * Device control code
 */
#define	_DEVC(group,num)	(u_long)(((group) << 16) | (num))

/*
 * Power management
 */
#define DEVCTL_PM_POWERDOWN	_DEVC('P', 0)	/* power down */
#define DEVCTL_PM_POWERUP	_DEVC('P', 1)	/* power up */
#define DEVCTL_PM_CHGPOLICY	_DEVC('P', 2)	/* change power policy */
#define DEVCTL_PM_LCDOFF	_DEVC('P', 3)	/* lcd off */
#define DEVCTL_PM_LCDON		_DEVC('P', 4)	/* lcd on */
#define DEVCTL_PM_CHGFREQ	_DEVC('P', 5)	/* change cpu frequency */

/*
 * Plug and play
 */
#define DEVCTL_PNP_ATTACH	_DEVC('N', 0)	/* device attach */
#define DEVCTL_PNP_DETACH	_DEVC('N', 1)	/* device detach */
#define DEVCTL_PNP_INSERT	_DEVC('N', 2)	/* media insertion */
#define DEVCTL_PNP_REMOVE	_DEVC('N', 3)	/* media removal */

/*
 * Debug
 */
#define DEVCTL_DBG_DEVSTAT	_DEVC('D', 0)	/* dump device state */
#define DEVCTL_DBG_ENTERKD	_DEVC('D', 1)	/* entering kernel debugger */
#define DEVCTL_DBG_EXITKD	_DEVC('D', 2)	/* eixt kernel debugger */

#endif /* !_DEVCTL_H */
