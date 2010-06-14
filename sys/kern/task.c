/*-
 * Copyright (c) 2005-2009, Kohsuke Ohtani
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
 * task.c - task management routines.
 */

#include <kernel.h>
#include <kmem.h>
#include <sched.h>
#include <thread.h>
#include <ipc.h>
#include <sync.h>
#include <vm.h>
#include <exception.h>
#include <task.h>
#include <hal.h>
#include <sys/bootinfo.h>

struct task		kernel_task;	/* kernel task */
static struct list	task_list;	/* list for all tasks */
static int		ntasks;		/* number of tasks in system */

/**
 * task_create - create a new task.
 *
 * vm_option:
 *   VM_NEW:   The child task will have fresh memory image.
 *   VM_SHARE: The child task will share whole memory image with parent.
 *   VM_COPY:  The parent's memory image is copied to the child's one.
 *             VM_COPY is supported only with MMU system.
 *
 * Note: The child task initially contains no threads.
 */
int
task_create(task_t parent, int vm_option, task_t *childp)
{
	struct task *task;
	vm_map_t map = NULL;

	ASSERT(parent != NULL);

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
	if (ntasks >= MAXTASKS)
		return EAGAIN;

	sched_lock();
	if (!task_valid(parent)) {
		sched_unlock();
		return ESRCH;
	}
	if ((curtask->flags & TF_SYSTEM) == 0) {
		if (!task_access(parent)) {
			sched_unlock();
			return EPERM;
		}
		/*
		 * It's important to set zero as task id before
		 * copying parent's memory space. Otherwise, we
		 * have to switch VM space to copy it.
		 */
		task = 0;
		if (copyout(&task, childp, sizeof(task))) {
			sched_unlock();
			return EFAULT;
		}
	}

	if ((task = kmem_alloc(sizeof(*task))) == NULL) {
		sched_unlock();
		return ENOMEM;
	}
	memset(task, 0, sizeof(*task));

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
		map = vm_dup(parent->map);
		break;
	}
	if (map == NULL) {
		kmem_free(task);
		sched_unlock();
		return ENOMEM;
	}

	/*
	 * Fill initial task data.
	 */
	task->map = map;
	task->handler = parent->handler;
	task->capability = parent->capability;
	task->parent = parent;
	task->flags = TF_DEFAULT;
	strlcpy(task->name, "*noname", MAXTASKNAME);
	list_init(&task->threads);
	list_init(&task->objects);
	list_init(&task->mutexes);
	list_init(&task->conds);
	list_init(&task->sems);
	list_insert(&task_list, &task->link);
	ntasks++;

	if (curtask->flags & TF_SYSTEM)
		*childp = task;
	else {
		/*
		 * No page fault here because we have already
		 * checked it.
		 */
		copyout(&task, childp, sizeof(task));
	}

	sched_unlock();
	return 0;
}

/*
 * Terminate the specified task.
 */
int
task_terminate(task_t task)
{
	list_t head, n;
	thread_t t;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}

	list_remove(&task->link);
	task->handler = EXC_DFL;

	/*
	 * Clean up all resources owned by the target task.
	 */
	timer_stop(&task->alarm);
	object_cleanup(task);
	mutex_cleanup(task);
	cond_cleanup(task);
	sem_cleanup(task);

	/*
	 * Terminate each thread in the task.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		t = list_entry(n, struct thread, task_link);
		if (t != curthread)
			thread_destroy(t);
	}
	if (task == curtask)
		thread_destroy(curthread);

	vm_terminate(task->map);
	task->map = NULL;
	kmem_free(task);
	ntasks--;
	sched_unlock();
	return 0;
}

/*
 * Return the current task.
 */
task_t
task_self(void)
{

	return curthread->task;
}

/*
 * Suspend a task.
 */
int
task_suspend(task_t task)
{
	list_t head, n;
	thread_t t;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}

	if (++task->suscnt == 1) {
		/*
		 * Suspend all threads within the task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			t = list_entry(n, struct thread, task_link);
			thread_suspend(t);
		}
	}
	sched_unlock();
	return 0;
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
	thread_t t;

	ASSERT(task != curtask);

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}
	if (task->suscnt == 0) {
		sched_unlock();
		return EINVAL;
	}

	if (--task->suscnt == 0) {
		/*
		 * Resume all threads in the target task.
		 */
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			t = list_entry(n, struct thread, task_link);
			thread_resume(t);
		}
	}
	sched_unlock();
	return 0;
}

/*
 * Set task name.
 *
 * The naming service is separated from task_create() because
 * the task name can be changed at anytime by exec().
 */
int
task_setname(task_t task, const char *name)
{
	char str[MAXTASKNAME];
	int error;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}

	if (curtask->flags & TF_SYSTEM)
		strlcpy(task->name, name, MAXTASKNAME);
	else {
		error = copyinstr(name, str, MAXTASKNAME);
		if (error) {
			sched_unlock();
			return error;
		}
		strlcpy(task->name, str, MAXTASKNAME);
	}
	sched_unlock();
	return 0;
}

/*
 * Set the capability of the specified task.
 */
int
task_setcap(task_t task, cap_t cap)
{

	if (!task_capable(CAP_SETPCAP))
		return EPERM;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (!task_access(task)) {
		sched_unlock();
		return EPERM;
	}
	task->capability = cap;
	sched_unlock();
	return 0;
}

/*
 * task_chkcap - system call to check task capability.
 */
int
task_chkcap(task_t task, cap_t cap)
{
	int error = 0;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if ((task->capability & cap) == 0) {
		DPRINTF(("Denying capability by %s: task=%s cap=%08x\n",
			 curtask->name, task->name, cap));
		if (task->flags & TF_AUDIT)
			panic("audit failed");
		error = EPERM;
	}
	sched_unlock();
	return error;
}

/*
 * Check if the current task has specified capability.
 * Returns true on success, or false on error.
 */
int
task_capable(cap_t cap)
{
	int capable = 1;

	if ((curtask->capability & cap) == 0) {
		DPRINTF(("Denying capability by kernel: task=%s cap=%08x\n",
			 curtask->name, cap));
		if (curtask->flags & TF_AUDIT)
			panic("audit failed");
		capable = 0;
	}
	return capable;
}

/*
 * Return true if the specified task is valid.
 */
int
task_valid(task_t task)
{
	task_t tmp;
	list_t n;

	for (n = list_first(&task_list); n != &task_list; n = list_next(n)) {
		tmp = list_entry(n, struct task, link);
		if (tmp == task)
			return 1;
	}
	return 0;
}

/*
 * Check if the current task can access the specified task.
 * Return true on success, or false on error.
 */
int
task_access(task_t task)
{

	if (task->flags & TF_SYSTEM) {
		/* Do not access the kernel task. */
		return 0;
	} else {
		if (task == curtask || task->parent == curtask ||
		    task == curtask->parent ||	/* XXX: fork on nommu */
		    task_capable(CAP_TASKCTRL))
			return 1;
	}
	return 0;
}

int
task_info(struct taskinfo *info)
{
	u_long target = info->cookie;
	u_long i = 0;
	task_t task;
	list_t n;

	sched_lock();
	n = list_first(&task_list);
	do {
		if (i++ == target) {
			task = list_entry(n, struct task, link);
			info->cookie = i;
			info->id = task;
			info->flags = task->flags;
			info->suscnt = task->suscnt;
			info->capability = task->capability;
			info->vmsize = task->map->total;
			info->nthreads = task->nthreads;
			info->active = (task == curtask) ? 1 : 0;
			strlcpy(info->taskname, task->name, MAXTASKNAME);
			sched_unlock();
			return 0;
		}
		n = list_next(n);
	} while (n != &task_list);
	sched_unlock();
	return ESRCH;
}

/*
 * Create and setup boot tasks.
 */
void
task_bootstrap(void)
{
	struct module *mod;
	struct bootinfo *bi;
	task_t task;
	thread_t t;
	void *stack, *sp;
	int i, error = 0;

	machine_bootinfo(&bi);
	mod = &bi->tasks[0];

	for (i = 0; i < bi->nr_tasks; i++) {
		/*
		 * Create a new task.
		 */
		if ((error = task_create(&kernel_task, VM_NEW, &task)) != 0)
			break;
		if ((error = vm_load(task->map, mod, &stack)) != 0)
			break;
		task_setname(task, mod->name);

		/*
		 * Set the default capability.
		 * We give CAP_SETPCAP to the exec server.
		 */
		task->capability = CAPSET_BOOT;
		if (!strncmp(task->name, "exec", MAXTASKNAME))
			task->capability |= CAP_SETPCAP;

		/*
		 * Create and start a new thread.
		 */
		if ((error = thread_create(task, &t)) != 0)
			break;
		sp = (char *)stack + DFLSTKSZ - (sizeof(int) * 3);
		error = thread_load(t, (void (*)(void))mod->entry, sp);
		if (error)
			break;
		t->priority = PRI_REALTIME;
		t->basepri = PRI_REALTIME;
		thread_resume(t);

		mod++;
	}
	if (error) {
		DPRINTF(("task_bootstrap: error=%d\n", error));
		panic("unable to load boot task");
	}
}

/*
 * Initialize task.
 */
void
task_init(void)
{

	list_init(&task_list);

	/*
	 * Create a kernel task as first task.
	 */
	strlcpy(kernel_task.name, "kernel", MAXTASKNAME);
	kernel_task.flags = TF_SYSTEM;
	kernel_task.nthreads = 0;
	list_init(&kernel_task.threads);
	list_init(&kernel_task.objects);
	list_init(&kernel_task.mutexes);
	list_init(&kernel_task.conds);
	list_init(&kernel_task.sems);

	list_insert(&task_list, &kernel_task.link);
	ntasks = 1;
}
