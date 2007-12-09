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
 * task.c - task management routines
 */

/*
 * The concept of "task" looks like a container that holds three types
 * of resources - threads, memory map and objects.
 * A kernel task is the special task that has an idle thread, timer
 * thread and interrupt threads. The kernel task does not have an user
 * mode memory image.
 */

#include <kernel.h>
#include <list.h>
#include <bootinfo.h>
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
struct task kern_task = KERN_TASK(kern_task);

/*-
 * task_create() - Create new task.
 *
 * Some task data is inherited to child task from parent task.
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
 * If vm_inherit is VM_COPY, the child task will have the same memory
 * image with the parent task. Especially, text region and read-only
 * region are physically shared among them. VM_COPY is supported only
 * with MMU system. The created
 *
 * The child task initially contains no threads.
 */
__syscall int task_create(task_t parent, int vm_inherit, task_t *child)
{
	task_t task = 0;

	if (vm_inherit > VM_COPY)
		return EINVAL;
#ifndef CONFIG_MMU
	if (vm_inherit == VM_COPY)
		return EINVAL;
#endif
	sched_lock();

	if (!task_valid(parent)) {
		sched_unlock();
		return ESRCH;
	}
	if (parent != cur_task() && !capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	/*
	 * The child task ID must be set to 0 before copying parent's 
	 * memory image. So that the child task can know whether
	 * it is child.
	 */
	if (cur_task() == &kern_task) {
		*child = 0;	
	} else {
		if (umem_copyout(&task, child, sizeof(task_t)) != 0) {
			sched_unlock();
			return EFAULT;
		}
	}
	if ((task = kmem_alloc(sizeof(struct task))) == NULL) {
		sched_unlock();
		return ENOMEM;
	}
	memset(task, 0, sizeof(struct task));

	/*
	 * Setup memory map.
	 */
	switch (vm_inherit) {
	case VM_NONE:
		task->map = vm_create();
		break;
	case VM_SHARE:
		vm_reference(parent->map);
		task->map = parent->map;
		break;
	case VM_COPY:
		task->map = vm_fork(parent->map);
		break;
	}
	if (task->map == NULL) {
		kmem_free(task);
		sched_unlock();
		return ENOMEM;
	}
	/*
	 * Fill initial task data.
	 * Some task data is inherited to the child task.
	 */
	task->exc_handler = parent->exc_handler;
	task->capability = parent->capability & CAP_MASK;
	list_init(&task->objects);
	list_init(&task->threads);
	task->magic = TASK_MAGIC;
	list_insert(&kern_task.link, &task->link);

	sched_unlock();
	/*
	 * The following copy operation affects only parent's memory.
	 * So, only parent task will get the child task's ID.
	 */
	if (cur_task() == &kern_task) {
		*child = task;
	} else {
		if (umem_copyout(&task, child, sizeof(task_t)) != 0)
			return EFAULT;
   	}
	return 0;
}

/*
 * Terminate a task.
 *
 * Deallocates all resources for the specified task.
 * If terminated task is current task, this routine never returns.
 */
__syscall int task_terminate(task_t task)
{
	list_t head, n;
	thread_t th;
	object_t obj;

	sched_lock();

	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != cur_task() && !capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	task->magic = 0;

	/*
	 * Terminate all threads except a current thread. If it
	 * is terminating current task, a current thread is
	 * terminated at the bottom of this routine.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		if (th != cur_thread)
			__thread_terminate(th);
	}
	/*
	 * Delete all objects that are owned by the terminated task.
	 */
	head = &task->objects;
	for (n = list_first(head); n != head; n = list_next(n)) {
		obj = list_entry(n, struct object, task_link);
		/* Force to change object owner to delete it */
		obj->owner = cur_task();
		object_delete(obj);
	}
	/*
	 * Free all other task resources.
	 */
	vm_terminate(task->map);
	list_remove(&task->link);
	kmem_free(task);
	if (task == cur_task()) {
		cur_thread->task = NULL;
		__thread_terminate(cur_thread);
	}
	sched_unlock();
	return 0;
}

/*
 * Return current task.
 */
__syscall task_t task_self(void)
{
	return cur_task();
}

/*
 * Suspend all threads within specified task.
 */
__syscall int task_suspend(task_t task)
{
	list_t head, n;
	thread_t th;

	sched_lock();

	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != cur_task() && !capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	if (++task->sus_count != 1) {
		sched_unlock();
		return 0;
	}
	/*
	 * Suspend all threads in task. The current thread must
	 * be suspended after all other threads are suspended.
	 */
	head = &task->threads;
	for (n = list_first(head); n != head; n = list_next(n)) {
		th = list_entry(n, struct thread, task_link);
		if (th != cur_thread)
			thread_suspend(th);
	}
	if (task == cur_task())
		thread_suspend(cur_thread);

	sched_unlock();
	return 0;
}

/*
 * Resume threads within specified task.
 *
 * A thread can begin to run only when both of thread suspend
 * count and task suspend count becomes 0.
 */
__syscall int task_resume(task_t task)
{
	list_t head, n;
	thread_t th;

	ASSERT(task != cur_task());

	sched_lock();

	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != cur_task() && !capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	if (task->sus_count == 0) {
		sched_unlock();
		return EINVAL;
	}
	if (--task->sus_count == 0) {
		head = &task->threads;
		for (n = list_first(head); n != head; n = list_next(n)) {
			th = list_entry(n, struct thread, task_link);
			thread_resume(th);
		}
	}
	sched_unlock();
	return 0;
}

/*
 * Set task name.
 * 
 * A task name is used only for debug purpose. So, the parent task
 * does not have to set a name for all created (child) tasks.
 *
 * The naming service is separated from task_create() because
 * the task name can be changed at anytime.
 */
__syscall int task_name(task_t task, char *name)
{
	int err = 0;
	size_t len;

	sched_lock();

	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (task != cur_task() && !capable(CAP_TASK)) {
		sched_unlock();
		return EPERM;
	}
	if (cur_task() == &kern_task)
		strlcpy(task->name, name, MAX_TASKNAME);
	else {
		if (umem_strnlen(name, MAX_TASKNAME, &len))
			err = EFAULT;
		else if (len >= MAX_TASKNAME)
			err = ENAMETOOLONG;
		else
			err = umem_copyin(name, task->name, len + 1);
	}
	sched_unlock();
	return err;
}

/*
 * Check the task capability.
 * This is used by device driver to check the task
 * permission.
 */
int task_capable(int cap)
{
	return (int)capable(cap);
}

/*
 * Get the capability of the specified task.
 */
__syscall int task_getcap(task_t task, cap_t *cap)
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
__syscall int task_setcap(task_t task, cap_t *cap)
{
	cap_t new_cap;

	if (!capable(CAP_SETPCAP))
		return EPERM;

	sched_lock();
	if (!task_valid(task)) {
		sched_unlock();
		return ESRCH;
	}
	if (umem_copyin(cap, &new_cap, sizeof(cap_t)) != 0) {
		sched_unlock();
		return EFAULT;
	}
	task->capability = new_cap;
	sched_unlock();
	return 0;
}

#ifdef CONFIG_MMU
/*
 * Load task image for boot task. (MMU version)
 * Return 0 on success, -1 on failure.
 */
static int task_load(task_t task, struct img_info *img, void **stack)
{
	void *text, *data;

	printk("Loading task:\'%s\'\n", img->name);

	/* Create text segment */
	text = (void *)img->text;
	if (__vm_allocate(task, &text, img->text_size, 0, 1))
		return -1;
	memcpy((void *)img->text, (void *)phys_to_virt(img->phys),
	       img->text_size);

	if (vm_attribute(task, text, ATTR_READ))
		return -1;
	
	/* Copy data & BSS segment */
	if (img->data_size + img->bss_size != 0) {
		data = (void *)img->data;
		if (__vm_allocate(task, &data,
				  img->data_size + img->bss_size, 0, 1))
			return -1;

		memcpy((void *)img->data,
		       (void *)(phys_to_virt(img->phys) +
		       (img->data - img->text)),
		       img->data_size);
	}
	/* Create stack */
	*stack = (void *)(USER_MAX - USTACK_SIZE);
	if (__vm_allocate(task, stack, USTACK_SIZE, 0, 1))
		return -1;
	
	/* Free original pages */
	page_free((void *)img->phys, img->size);
	return 0;
}

#else /* !CONFIG_MMU */
/*
 * Load task image for boot task. (NOMMU version)
 * Return 0 on success, -1 on failure.
 *
 * Note: We assume that the task images are already copied to
 * the proper address by a boot loader.
 */
static int task_load(task_t task, struct img_info *img, void **stack)
{
	void *base;
	size_t size;

	printk("Loading task:\'%s\'\n", img->name);

	/* Reserve text & data area */
	base = (void *)img->text;
	size = img->text_size + img->data_size + img->bss_size;

	if (__vm_allocate(task, &base, size, 0, 0))
		return -1;
	if (img->bss_size != 0)
		memset((void *)img->data + img->data_size, 0, img->bss_size);

	/* Create stack */
	if (__vm_allocate(task, stack, USTACK_SIZE, 1, 1))
		return -1;

	return 0;
}
#endif /* !CONFIG_MMU */

/*
 * Create and setup boot tasks.
 * The scheduler has been locked, and new tasks do not run here.
 */
void task_boot(void)
{
	struct img_info *img;
	task_t task;
	thread_t th;
	void *stack;
	int i;

	img = &boot_info->tasks[0];
	for (i = 0; i < boot_info->nr_tasks; i++, img++) {

		if (task_create(&kern_task, VM_NONE, &task))
			break;

		task_name(task, img->name);

		/* Switch mapping to touch this virtual memory space */
		mmu_switch(task->map->pgd);

		if (task_load(task, img, &stack))
			break;

		if (thread_create(task, &th))
			break;
		if (thread_load(th, (void *)img->entry,
				(void *)(stack + USTACK_SIZE - sizeof(int))))
			break;

		/* Start thread */
		thread_resume(th);
	}
	if (i != boot_info->nr_tasks)
		panic("Failed to create boot tasks");

	/* Restore page mapping */
	mmu_switch(kern_task.map->pgd);

#if defined(DEBUG) && defined(CONFIG_KDUMP)
	/* boot_dump(); */
#endif
}

#if defined(DEBUG) && defined(CONFIG_KDUMP)
void task_dump(void)
{
	list_t i, j;
	task_t task;
	int nr_obj, nr_thread;

	printk("Task dump:\n");
	printk(" mod task      nr_obj nr_thr vm map   susp exc hdlr cap      name\n");
	printk(" --- --------- ------ ------ -------- ---- -------- -------- ------------\n");

	i = &kern_task.link;
	do {
		task = list_entry(i, struct task, link);

		nr_thread = 0;
		j = &task->threads;
		j = list_next(j);
		do {
			nr_thread++;
			j = list_next(j);
		} while (j != &task->threads);

		nr_obj = 0;
		j = &task->objects;
		j = list_next(j);
		do {
			nr_obj++;
			j = list_next(j);
		} while (j != &task->objects);

		printk(" %s %08x%c    %3d    %3d %08x %4d %08x %08x %s\n",
		       (task == &kern_task) ? "Knl" : "Usr",
		       task, (task == cur_task())? '*' : ' ', nr_obj,
		       nr_thread, task->map, task->sus_count,
		       task->exc_handler, task->capability,
		       task->name ? task->name : "no name");

		i = list_next(i);
	} while (i != &kern_task.link);
}

void boot_dump(void)
{
	struct img_info *img;
	int i;

	printk(" text base data base text size data size bss size   task name\n");
	printk(" --------- --------- --------- --------- ---------- ----------\n");

	img = &boot_info->tasks[0];
	for (i = 0; i < boot_info->nr_tasks; i++, img++) {
		printk("  %8x  %8x  %8d  %8d  %8d  %s\n",
		       img->text, img->data,
		       img->text_size, img->data_size, img->bss_size,
		       img->name);
	}
}
#endif

void task_init(void)
{
	/* keep it simple. ;) */
}
