/*-
 * Copyright (c) 2005-2007, Kohsuke Ohtani
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
 * task.c - task management routines
 */

#include <kernel.h>
#include <kmem.h>
#include <sched.h>
#include <thread.h>
#include <ipc.h>
#include <vm.h>
#include <page.h>
#include <task.h>

/*
 * Kernel task.
 * kern_task acts as a list head of all tasks in the system.
 */
struct task kern_task;

/**
 * task_create - create a new task.
 *
 * The child task will inherit some task states from its parent.
 *
 * Inherit status:
 *   Child data        Inherit from parent ?
 *   ----------------- ---------------------
 *   Task name         No
 *   Object list       No
 *   Threads           No
 *   Memory map        New/Duplicate/Share
 *   Suspend count     No
 *   Exception handler Yes
 *   Capability        Yes
 *
 * If vm_option is VM_COPY, the child task will have the same memory
 * image with the parent task. Especially, text region and read-only
 * region are physically shared among them. VM_COPY is supported only
 * with MMU system.
 * The child task initially contains no threads.
 */
int
task_create(task_t parent, int vm_option, task_t *child)
{
	task_t task;
	vm_map_t map = NULL;
	int err = 0;

	switch (vm_option) {
	case VM_NEW:
	case VM_SHARE:
#ifdef CONFIG_MMU
	case VM_COPY:
#endif
		break;
	default:
		return EINVAL;
	}
	sched_lock();
	if (!task_valid(parent)) {
		err = ESRCH;
		goto out;
	}
	if (cur_task() != &kern_task) {
		if(!task_access(parent)) {
			err = EPERM;
			goto out;
		}
		/*
		 * Set zero as child task id before copying parent's
		 * memory space. So, the child task can identify
		 * whether it is a child.
		 */
		task = 0;
		if (umem_copyout(&task, child, sizeof(task_t))) {
			err = EFAULT;
			goto out;
		}
	}

	if ((task = kmem_alloc(sizeof(struct task))) == NULL) {
		err = ENOMEM;
		goto out;
	}
	memset(task, 0, sizeof(struct task));

	/*
	 * Setup VM mapping.
	 */
	switch (vm_option) {
	case VM_NEW:
		map = vm_create();
		break;
	case VM_SHARE:
		vm_reference(parent->map);
		map = parent->map;
		break;
	case VM_COPY:
		map = vm_fork(parent->map);
		break;
	}
	if (map == NULL) {
		kmem_free(task);
		err = ENOMEM;
		goto out;
	}
	/*
	 * Fill initial task data.
	 */
	task->map = map;
	task->exc_handler = parent->exc_handler;
	task->capability = parent->capability;
	task->parent = parent;
	task->magic = TASK_MAGIC;
	list_init(&task->objects);
	list_init(&task->threads);
	list_insert(&kern_task.link, &task->link);

	if (cur_task() == &kern_task)
		*child = task;
	else
		err = umem_copyout(&task, child, sizeof(task_t));
 out:
	sched_unlock();
	return err;
}

/*
 * Terminate the specified task.
 */
int
task_terminate(task_t task)
{
	int err = 0;
	list_t head, n;
	thread_t th;
	object_t obj;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}

	/* Invalidate the task. */
	task->magic = 0;

	/*
	 * Terminate all threads except a current thread.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		if (th != cur_thread)
			thread_terminate(th);
	}
	/*
	 * Delete all objects owned by the target task.
	 */
	head = &task->objects;
	for (n = list_first(head); n != head; n = list_next(n)) {
		/*
		 * A current task may not have the right to delete
		 * target objects. So, we set the owner of the object
		 * to the current task before deleting it.
		 */
		obj = list_entry(n, struct object, task_link);
		obj->owner = cur_task();
		object_destroy(obj);
	}
	/*
	 * Release all other task related resources.
	 */
	vm_terminate(task->map);
	list_remove(&task->link);
	kmem_free(task);

	if (task == cur_task()) {
		cur_thread->task = NULL;
		thread_terminate(cur_thread);
	}
	sched_unlock();
	return err;
}

task_t
task_self(void)
{

	return cur_task();
}

/*
 * Suspend a task.
 */
int
task_suspend(task_t task)
{
	list_t head, n;
	thread_t th;
	int err = 0;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
	} else if (!task_access(task)) {
		err = EPERM;
	} else if (++task->suspend_count == 1) {
		/*
		 * Suspend all threads within the task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, struct thread, task_link);
			thread_suspend(th);
		}
	}
	sched_unlock();
	return err;
}

/*
 * Resume a task.
 *
 * A thread in the task will begin to run only when both
 * thread suspend count and task suspend count become 0.
 */
int
task_resume(task_t task)
{
	list_t head, n;
	thread_t th;
	int err = 0;

	ASSERT(task != cur_task());

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
	} else if (!task_access(task)) {
		err = EPERM;
	} else if (task->suspend_count == 0) {
		err = EINVAL;
	} else if (--task->suspend_count == 0) {
		/*
		 * Resume all threads in the target task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, struct thread, task_link);
			thread_resume(th);
		}
	}
	sched_unlock();
	return err;
}

/*
 * Set task name.
 *
 * The naming service is separated from task_create() because
 * the task name can be changed at anytime by exec().
 */
int
task_name(task_t task, const char *name)
{
	size_t len;
	int err = 0;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
	} else if (!task_access(task)) {
		err = EPERM;
	} else {
		if (cur_task() == &kern_task)
			strlcpy(task->name, name, MAXTASKNAME);
		else {
			if (umem_strnlen(name, MAXTASKNAME, &len))
				err = EFAULT;
			else if (len >= MAXTASKNAME)
				err = ENAMETOOLONG;
			else
				err = umem_copyin((void *)name, task->name,
						  len + 1);
		}
	}
	sched_unlock();
	return err;
}

/*
 * Get the capability of the specified task.
 */
int
task_getcap(task_t task, cap_t *cap)
{
	cap_t cur_cap;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	cur_cap = task->capability;
	sched_unlock();

	return umem_copyout(&cur_cap, cap, sizeof(cap_t));
}

/*
 * Set the capability of the specified task.
 */
int
task_setcap(task_t task, cap_t *cap)
{
	cap_t new_cap;
	int err = 0;

	if (!task_capable(CAP_SETPCAP))
		return EPERM;

	sched_lock();
	if (!task_valid(task)) {
		err = ESRCH;
	} else if (umem_copyin(cap, &new_cap, sizeof(cap_t))) {
		err = EFAULT;
	} else {
		task->capability = new_cap;
	}
	sched_unlock();
	return err;
}

/*
 * Check if the current task can access the specified task.
 */
int
task_access(task_t task)
{

	return (task != &kern_task &&
		(task == cur_task() || task->parent == cur_task() ||
		 task_capable(CAP_TASK)));
}

/*
 * Create and setup boot tasks.
 */
void
task_bootstrap(void)
{
	struct module *m;
	task_t task;
	thread_t th;
	void *stack;
	int i;

	m = &boot_info->tasks[0];
	for (i = 0; i < boot_info->nr_tasks; i++, m++) {
		/*
		 * Create a new task.
		 */
		if (task_create(&kern_task, VM_NEW, &task))
			break;
		task_name(task, m->name);
		if (vm_load(task->map, m, &stack))
			break;

		/*
		 * Create and start a new thread.
		 */
		if (thread_create(task, &th))
			break;
		thread_name(th, "main");
		stack = (void *)((u_long)stack + USTACK_SIZE - sizeof(int));
		*(u_long *)stack = 0; /* arg = 0 */
		if (thread_load(th, (void (*)(void))m->entry, stack))
			break;
		thread_resume(th);
	}
	if (i != boot_info->nr_tasks)
		panic("task_boot");
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void
task_dump(void)
{
	list_t i, j;
	task_t task;
	int nobjs, nthreads;

	printk("Task dump:\n");
	printk(" mod task      nobjs  nthrds vm map   susp exc hdlr "
	       "cap      name\n");
	printk(" --- --------- ------ ------ -------- ---- -------- "
	       "-------- ------------\n");

	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);

		nthreads = 0;
		j = &task->threads;
		j = list_next(j);
		do {
			nthreads++;
			j = list_next(j);
		} while (j != &task->threads);

		nobjs = 0;
		j = &task->objects;
		j = list_next(j);
		do {
			nobjs++;
			j = list_next(j);
		} while (j != &task->objects);

		printk(" %s %08x%c    %3d    %3d %08x %4d %08x %08x %s\n",
		       (task == &kern_task) ? "Knl" : "Usr",
		       task, (task == cur_task()) ? '*' : ' ', nobjs,
		       nthreads, task->map, task->suspend_count,
		       task->exc_handler, task->capability,
		       task->name ? task->name : "no name");

		i = list_next(i);
	} while (i != &kern_task.link);
}

void
boot_dump(void)
{
	struct module *m;
	int i;

	printk(" text base data base  bss base "
	       "text size data size bss size   task name\n");
	printk(" --------- --------- --------- "
	       "--------- --------- ---------- ----------\n");

	m = &boot_info->driver;
	printk("  %8x  %8x  %8x  %8d  %8d  %8d  %s\n",
	       m->text, m->data, m->bss,
	       m->textsz, m->datasz, m->bsssz,
	       m->name);

	m = &boot_info->tasks[0];
	for (i = 0; i < boot_info->nr_tasks; i++, m++) {
		printk("  %8x  %8x  %8x  %8d  %8d  %8d  %s\n",
		       m->text, m->data, m->bss,
		       m->textsz, m->datasz, m->bsssz,
		       m->name);
	}
}

void ksym_dump(void)
{
	struct module *m;
	int i;

	printk(" ksym addr name			 task name\n");
	printk(" --------- --------------------- ----------\n");

	m = &boot_info->kernel;
	for (i = 0; i < 2; i++, m++) {
		struct kernel_symbol *ksym, *ksym_end;
		if (m->ksym == 0 || m->ksymsz == 0)
			continue;

		ksym_end = (void *)(m->ksym + m->ksymsz);
		for (ksym = (void *)m->ksym; ksym < ksym_end; ksym++) {
			printk("  %8x  %20s  %s\n",
			       ksym->value, ksym->name, m->name);
		}
	}
}
#endif

void
task_init(void)
{
	/*
	 * Create a kernel task as a first task in the system.
	 *
	 * Note: We assume the VM mapping for a kernel task has
	 * already been initialized in vm_init().
	 */
	strlcpy(kern_task.name, "kernel", MAXTASKNAME);
	list_init(&kern_task.link);
	list_init(&kern_task.objects);
	list_init(&kern_task.threads);
	kern_task.capability = 0xffffffff;
	kern_task.magic = TASK_MAGIC;
}
