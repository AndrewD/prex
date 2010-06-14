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

#ifndef _IPC_H
#define _IPC_H

#include <sys/cdefs.h>
#include <types.h>
#include <sys/list.h>
#include <sys/queue.h>
#include <ipc/ipc.h>

struct object {
	struct list	link;		/* linkage for all objects in system */
	char		name[MAXOBJNAME]; /* object name */
	struct list	task_link;	/* linkage on object list in task */
	task_t		owner;		/* creator of this object */
	struct queue	sendq;		/* queue for sender threads */
	struct queue	recvq;		/* queue for receiver threads */
};

__BEGIN_DECLS
int	 object_create(const char *, object_t *);
int	 object_lookup(const char *, object_t *);
int	 object_destroy(object_t);
int	 object_valid(object_t);
void	 object_cleanup(task_t);
void	 object_init(void);

int	 msg_send(object_t, void *, size_t);
int	 msg_receive(object_t, void *, size_t);
int	 msg_reply(object_t, void *, size_t);
void	 msg_cancel(thread_t);
void	 msg_abort(object_t);
void	 msg_init(void);
__BEGIN_DECLS

#endif /* !_IPC_H */
