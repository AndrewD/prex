/*-
 * Copyright (c) 2005-2006, Kohsuke Ohtani
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

/*
 * An object represents service, state, or policies etc. To manipulate
 * objects, kernel provide 3 functions: create, delete, lookup.
 * Prex task will create an object to provide its interface to other tasks.
 * The tasks will communicate by sending a message to the object each other.
 * For example, a server task creates some objects and client task will send
 * a request message to it.
 *
 * A substance of object is stored in kernel space, and it is protected
 * from user mode code. Each object data is managed with the hash table 
 * by using its name string. Usually, an object has a unique name within
 * a system. Before a task sends a message to the specific object, it must
 * obtain the object ID by looking up the name of the target object.
 * 
 * An object can be created without its name. These object can be used as 
 * private objects that are used by threads in same task.
 */

#include <kernel.h>
#include <queue.h>
#include <list.h>
#include <kmem.h>
#include <sched.h>
#include <task.h>
#include <ipc.h>

/*
 * Object hash table
 *
 * All objects are hashed by its name string. If an object has no
 * name, it is linked to index zero.
 * The scheduler must be locked when this table is modified.
 */
static struct list obj_table[NR_OBJHASH];

/*
 * Calculate the hash index for specified name string.
 * The name can be NULL if the object does not have name.
 */
static u_int object_hash(const char *name)
{
	u_int h = 0;

	if (name == NULL)
		return 0;
	while (*name)
		h = ((h << 5) + h) + *name++;
	return h & (NR_OBJHASH - 1);
}

/*
 * Find an object from the specified name.
 */
static object_t object_find(char *name)
{
	list_t head, n;
	object_t obj = NULL;

	head = &obj_table[object_hash(name)];
	for (n = list_first(head); n != head; n = list_next(n)) {
		obj = list_entry(n, struct object, name_link);
		ASSERT(obj->magic == OBJECT_MAGIC);
		if (!strncmp(obj->name, name, MAX_OBJNAME))
			break;
	}
	if (n == head)
		return NULL;
	return obj;
}

/*
 * Search an object in the object name space. The object name must
 * be null-terminated string. The object ID is returned in obj on
 * success.
 */
__syscall int object_lookup(char *name, object_t *obj)
{
	object_t o;
	size_t len;
	char str[MAX_OBJNAME];

	if (umem_strnlen(name, MAX_OBJNAME, &len))
		return EFAULT;
	if (len == 0 || len >= MAX_OBJNAME)
		return ESRCH;
	if (umem_copyin(name, str, len + 1)) 
		return EFAULT;

	sched_lock();
	o = object_find(str);
	sched_unlock();
	if (o == NULL)
		return ENOENT;
	if (umem_copyout(&o, obj, sizeof(object_t)) != 0)
		return EFAULT;
	return 0;
}

/*
 * Create a new object.
 *
 * The ID of the new object is stored in obj on success.
 * The name of the object must be unique in the system. Or, the 
 * object can be created without name by setting NULL as name
 * argument. This object can be used as a private object which
 * can be accessed only by threads in same task.
 */
__syscall int object_create(char *name, object_t *obj)
{
	int err = 0;
	object_t o = 0;
	char str[MAX_OBJNAME];
	size_t len = 0;

	if (name != NULL) {
		if (umem_strnlen(name, MAX_OBJNAME, &len))
			return EFAULT;
		if (len >= MAX_OBJNAME)
			return ENAMETOOLONG;
		if (umem_copyin(name, str, len + 1))
			return EFAULT;
		str[len] = '\0';
	}
	sched_lock();

	/*
	 * Check user buffer first. This can reduce the error
	 * recovery for the following resource allocations.
	 */
	if (umem_copyout(&o, obj, sizeof(object_t)) != 0) {
		err = EFAULT;
		goto out;
	}
	o = object_find(str);
	if (o != NULL) {
		err = EEXIST;
		goto out;
	}
	o = kmem_alloc(sizeof(struct object));
	if (o == NULL) {
		err = ENOMEM;
		goto out;
	}
	if (name != NULL) {
		o->name = kmem_alloc(len + 1);
		if (o->name == NULL) {
			kmem_free(o);
			err = ENOMEM;
			goto out;
		}
		strlcpy(o->name, str, len + 1);
	}
	queue_init(&o->sendq);
	queue_init(&o->recvq);
	o->owner = cur_task();
	o->magic = OBJECT_MAGIC;
	list_insert(&obj_table[object_hash(name)], &o->name_link);
	list_insert(&(cur_task()->objects), &o->task_link);

	if (umem_copyout(&o, obj, sizeof(object_t)) != 0)
		panic("Unexpected error");
	err = 0;
 out:
	sched_unlock();
	return err;
}

/*
 * Delete an object.
 *
 * A thread can delete the object only when the target object is created
 * by the thread of the same task. All pending messages related to the 
 * deleted object are automatically canceled.
 */
__syscall int object_delete(object_t obj)
{
	sched_lock();
	if (!object_valid(obj)) {
		sched_unlock();
		return EINVAL;
	}
	if (obj->owner != cur_task()) {
		sched_unlock();
		return EACCES;
	}
	obj->magic = 0;
	msg_cancel(obj);

	list_remove(&obj->task_link);
	list_remove(&obj->name_link);
	if (obj->name != NULL)
		kmem_free(obj->name);
	kmem_free(obj);
	sched_unlock();
	return 0;
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void object_dump(void)
{
	int i;
	list_t head, n;
	object_t obj;

	printk("Object dump:\n");
	printk(" object   owner task name\n");
	printk(" -------- ---------- ----------------\n");

	for (i = 0; i < NR_OBJHASH; i++) {
		head = &obj_table[i];
		for (n = list_first(head); n != head; n = list_next(n)) {
			obj = list_entry(n, struct object, name_link);
			printk(" %08x %08x   %s\n", obj, obj->owner,
			       (obj->name ? obj->name : "NoName"));
		}
	}
}
#endif

void object_init(void)
{
	int i;

	/* Initialize object lookup table */
	for (i = 0; i < NR_OBJHASH; i++)
		list_init(&obj_table[i]);
}
