/*-
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

/*
 * attack.c - invalid parameter attack.
 */

#include <sys/prex.h>

#include <stdlib.h>
#include <stdio.h>

#define NATTACKS	1000

void
attack(void)
{
	object_t *objp	= (object_t *)random();
	object_t obj	= (object_t)random();
	char *name	= (char *)random();
	void *msg	= (void *)random();
	size_t size	= (size_t)random();
	task_t self	= task_self();
	void *addr	= (void *)random();
	int attr	= random() & 7;
	thread_t t	= (thread_t)random();
	thread_t *tp	= (thread_t *)random();

	object_create(NULL, NULL);
	object_create(NULL, objp);
	object_create(name, NULL);
	object_create(name, objp);

	object_destroy(0);
	object_destroy(obj);

	object_lookup(NULL, objp);
	object_lookup(name, NULL);
	object_lookup(name, objp);

	msg_send(0, msg, size);
	msg_send(obj, NULL, size);
	msg_send(obj, msg, 0);
	msg_send(0, msg, 0);
	msg_send(0, NULL, size);
	msg_send(obj, msg, size);

	msg_receive(0, msg, size);
	msg_receive(obj, NULL, size);
	msg_receive(obj, msg, 0);
	msg_receive(0, msg, 0);
	msg_receive(0, NULL, size);
	msg_receive(obj, msg, size);

	msg_reply(0, msg, size);
	msg_reply(obj, NULL, size);
	msg_reply(obj, msg, 0);
	msg_reply(0, msg, 0);
	msg_reply(0, NULL, size);
	msg_reply(obj, msg, size);

	vm_allocate(self, addr, size, 1);
	vm_allocate(self, &addr, size, 1);

	vm_free(self, addr);
	vm_attribute(self, addr, attr);
	vm_map(self, addr, size, &addr);

	thread_create(self, tp);
	thread_suspend(t);
	thread_terminate(t);
}

int
main(int argc, char *argv[])
{
	int i;

	printf("starting invalid parameter attack.\n");

	for (i = 0; i < NATTACKS; i++)
		attack();

	printf("test complete\n");
	return 0;
}
