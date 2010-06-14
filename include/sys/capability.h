/*
 * Copyright (c) 2007-2008, Kohsuke Ohtani
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

#ifndef _SYS_CAPABILITY_H
#define _SYS_CAPABILITY_H

/*
 * Type of capability
 */
typedef uint32_t	cap_t;

/*
 * Task capabilities
 */

#define CAP_NICE	0x00000001	/* Allow changing scheduling parameters */
#define CAP_RAWIO	0x00000002	/* Allow raw I/O operations */
#define CAP_KILL	0x00000004	/* Allow raising exception to another task */
#define CAP_SETPCAP	0x00000008	/* Allow setting capability */
#define CAP_TASKCTRL	0x00000010	/* Allow controlling another task's execution */
#define CAP_EXTMEM	0x00000020	/* Allow touching another task's memory */
#define CAP_PROTSERV	0x00000040	/* Allow operations as protected server */
#define CAP_NETWORK	0x00000080	/* Allow accessing network service */
#define CAP_POWERMGMT	0x00000100	/* Allow power management operation */
#define CAP_DISKADMIN	0x00000200	/* Allow mount, umount, etc. */
#define CAP_USERFILES	0x00000400	/* Allow accessing user files */
#define CAP_SYSFILES	0x00000800	/* Allow accessing system files */

/*
 * Default capability set
 */
#define CAPSET_BOOT	0x00000043	/* capabilities for boot tasks */

#endif /* !_SYS_CAPABILITY_H */
