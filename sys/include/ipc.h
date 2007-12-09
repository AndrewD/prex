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

#include <list.h>
#include <queue.h>

struct task;
struct thread;

#define OBJECT_MAGIC	0x4f626a3f	/* 'Obj?' */

/*
 * Object
 */
struct object {
	int		magic;		/* Magic number */
	struct list	name_link;	/* List for name hash table */
	struct list	task_link;	/* Link all objects in same task */
	char		*name;		/* Object name */
	task_t		owner;		/* Owner task of this object */
	struct queue	sendq;		/* Queue for sender threads */
	struct queue	recvq;		/* Queue for receiver threads */
};
typedef struct object *object_t;

#define object_valid(obj)  (kern_area(obj) && (obj)->magic == OBJECT_MAGIC)


/*
 * Message header
 */
struct msg_header {
	task_t	task;		/* ID of send task */
	int	code;		/* Message code */
	int	status;		/* Return status */
};

extern void object_init(void);
extern int object_create(char *name, object_t *obj);
extern int object_lookup(char *name, object_t *obj);
extern int object_delete(object_t obj);

extern int msg_send(object_t obj, void *msg, size_t size);
extern int msg_receive(object_t obj, void *msg, size_t size);
extern int msg_reply(object_t obj, void *msg, size_t size);

extern void msg_cleanup(struct thread *th);
extern void msg_cancel(struct object *obj);

#endif /* !_IPC_H */
