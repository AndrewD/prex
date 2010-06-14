/*-
 * Copyright (c) 2005-2008, Kohsuke Ohtani
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
 * object.c - object service
 */

/**
 * IPC object:
 *
 * An object represents service, state, or policies etc. To manipulate
 * objects, kernel provide 3 functions: create, destroy and lookup.
 * Prex task will create an object to provide its services to other
 * tasks. The tasks will communicate by sending a message to the
 * object each other. In typical case, a server task creates an object
 * and client tasks will send a request message to it.
 *
 * A substance of object is stored in kernel space, and so it's protected
 * from user mode code.  Usually, an object has a unique name within a
 * system. Before a task sends a message to the specific object, it must
 * obtain the object ID by looking up the name of the target object.
 *
 * A task can create a private object which does not have name. Since
 * another task can not obtain the ID of such object, the IPC operations
 * for the private object are limited to the threads in the same task.
 *
 * The object name started with '!' means that it is a protected object.
 * The protected object can be created only by the task which has
 * CAP_PROTSERV capability. Since this capability is given to the known
 * system servers, the client task can always trust the object owner.
 */

#include <kernel.h>
#include <kmem.h>
#include <sched.h>
#include <task.h>
#include <ipc.h>

/* forward declarations */
static object_t	object_find(const char *);

static struct list	object_list;	/* list of all objects */

/*
 * Create a new object.
 *
 * The ID of the new object is stored in objp on success.
 * The name of the object must be unique in the system.
 * Or, the object can be created without name by setting
 * NULL as name argument. This object can be used as a
 * private object which can be accessed only by threads in
 * same task.
 */
int
object_create(const char *name, object_t *objp)
{
	struct object *obj = 0;
	char str[MAXOBJNAME];
	int error;

	if (name == NULL)
		str[0] = '\0';
	else {
		error = copyinstr(name, str, MAXOBJNAME);
		if (error)
			return error;

		/* Check capability if name is protected object. */
		if (name[0] == '!' && !task_capable(CAP_PROTSERV))
			return EPERM;
	}
	sched_lock();

	if (curtask->nobjects >= MAXOBJECTS) {
		sched_unlock();
		return EAGAIN;
	}
	/*
	 * Check user buffer first. This can reduce the error
	 * recovery for the subsequence resource allocations.
	 */
	if (copyout(&obj, objp, sizeof(obj))) {
		sched_unlock();
		return EFAULT;
	}
	if (object_find(str) != NULL) {
		sched_unlock();
		return EEXIST;
	}
	if ((obj = kmem_alloc(sizeof(*obj))) == NULL) {
		sched_unlock();
		return ENOMEM;
	}
	if (name != NULL)
		strlcpy(obj->name, str, MAXOBJNAME);

	obj->owner = curtask;
	queue_init(&obj->sendq);
	queue_init(&obj->recvq);
	list_insert(&curtask->objects, &obj->task_link);
	curtask->nobjects++;
	list_insert(&object_list, &obj->link);
	copyout(&obj, objp, sizeof(obj));

	sched_unlock();
	return 0;
}

/*
 * Search an object in the object name space. The object
 * name must be null-terminated string.
 */
int
object_lookup(const char *name, object_t *objp)
{
	object_t obj;
	char str[MAXOBJNAME];
	int error;

	error = copyinstr(name, str, MAXOBJNAME);
	if (error)
		return error;

	sched_lock();
	obj = object_find(str);
	sched_unlock();

	if (obj == NULL)
		return ENOENT;

	if (copyout(&obj, objp, sizeof(obj)))
		return EFAULT;
	return 0;
}

int
object_valid(object_t obj)
{
	object_t tmp;
	list_t n;

	for (n = list_first(&object_list); n != &object_list;
	     n = list_next(n)) {
		tmp = list_entry(n, struct object, link);
		if (tmp == obj)
			return 1;
	}
	return 0;
}

static object_t
object_find(const char *name)
{
	object_t obj;
	list_t n;

	for (n = list_first(&object_list); n != &object_list;
	     n = list_next(n)) {
		obj = list_entry(n, struct object, link);
		if (!strncmp(obj->name, name, MAXOBJNAME))
			return obj;
	}
	return 0;
}

/*
 * Deallocate an object-- the internal version of object_destory.
 */
static void
object_deallocate(object_t obj)
{

	msg_abort(obj);
	obj->owner->nobjects--;
	list_remove(&obj->task_link);
	list_remove(&obj->link);
	kmem_free(obj);
}

/*
 * Destroy an object.
 *
 * All pending messages related to the target object are
 * automatically cancelled.
 */
int
object_destroy(object_t obj)
{

	sched_lock();
	if (!object_valid(obj)) {
		sched_unlock();
		return EINVAL;
	}
	if (obj->owner != curtask) {
		sched_unlock();
		return EACCES;
	}
	object_deallocate(obj);
	sched_unlock();
	return 0;
}

/*
 * Clean up for task termination.
 */
void
object_cleanup(task_t task)
{
	object_t obj;

	while (!list_empty(&task->objects)) {
		obj = list_entry(list_first(&task->objects),
				 struct object, task_link);
		object_deallocate(obj);
	}
}

void
object_init(void)
{

	list_init(&object_list);
}
