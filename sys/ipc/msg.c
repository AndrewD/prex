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
 * msg.c - routines to transmit a message.
 */

/*
 * Messages are sent to the specific object by using msg_send().
 * The transmission of a message is completely synchronous with 
 * this kernel. This means the thread which sent a message is blocked
 * until it receives a response from another thread. msg_receive()
 * performs reception of a message. msg_receive() is also blocked 
 * when no message is reached to the target object. The receiver
 * thread must answer the message using msg_reply() after it finishes
 * its message processing.
 *
 * The receiver thread can not receive another message until it
 * replies to the sender. In short, a thread can receive only one
 * message at once. Once the thread receives message, it can send
 * another message to different object. This mechanism allows threads
 * to redirect the sender's request to another thread.
 *
 * The message is copied from thread to thread directly without any
 * kernel buffering. If the message has a buffer, the memory region is
 * automatically mapped to the receiver's memory in kernel.
 * Since there is no page out of memory in this system, we can
 * copy the message via physical memory at anytime.
 */

#include <kernel.h>
#include <queue.h>
#include <event.h>
#include <kmem.h>
#include <sched.h>
#include <thread.h>
#include <task.h>
#include <vm.h>
#include <ipc.h>

/* Forward functions */
static thread_t dequeue_topprio(queue_t head);

/* Event for IPC operation */
static struct event ipc_event = EVENT_INIT(ipc_event, "ipc");

/*
 * Send a message.
 *
 * @obj:  object ID to send a message.
 * @msg:  pointer to the message buffer
 * @size: size of the message buffer.
 *
 * The current thread will be blocked until any other thread receives
 * the message and calls msg_reply() for the target object.
 * When new message has been reached to the object, it will be received
 * by highest priority thread waiting for that message.
 * A thread can send a message to any object if it knows the object id.
 */
__syscall int msg_send(object_t obj, void *msg, size_t size)
{
	thread_t th;
	void *kp;
	int result;

	if (!user_area(msg))
		return EFAULT;

	if (size < sizeof(struct msg_header))
		return EINVAL;

	sched_lock();

	if (!object_valid(obj)) {
		sched_unlock();
		return EINVAL;
	}
	if (obj->owner != cur_task() && !capable(CAP_IPC)) {
		sched_unlock();
		return EPERM;
	}
	/*
	 * A thread can not send a message when the thread is
	 * already receiving from the target object. This will
	 * obviously cause a deadlock.
	 */
	if (obj == cur_thread->recv_obj) {
		sched_unlock();
		return EDEADLK;
	}
	/*
	 * Translate message address to the kernel linear address.
	 * So that a receiver thread can access the message via
	 * kernel pointer. We can catch the page fault here.
	 */
	if ((kp = kmem_map(msg, size)) == NULL) {
		/* No physical address for this message */
		sched_unlock();
		return EFAULT;
	}
	/*
	 * Fill sender task ID in the message header.
	 * So, the receiver can trust this ID.
	 */
	((struct msg_header *)kp)->task = cur_task(); 

	/* Save the data for message block. */
	cur_thread->msg_addr = kp;
	cur_thread->msg_size = size;

	/*
	 * If receiver already exists, wake it up. Highest
	 * priority thread will get this message.
	 */
	if (!queue_empty(&obj->recvq)) {
		th = dequeue_topprio(&obj->recvq);
		sched_unsleep(th, 0);
	}
	/* 
	 * Sleep until we get a reply message.
	 */
	cur_thread->send_obj = obj;
	enqueue(&obj->sendq, &cur_thread->ipc_link);
	result = sched_sleep(&ipc_event);
	if (result == SLP_INTR)
		queue_remove(&cur_thread->ipc_link);
	cur_thread->send_obj = NULL;

	sched_unlock();

	/* 
	 * Check sleep result.
	 */
	switch (result) {
	case SLP_BREAK:
		return EAGAIN;	/* Receiver has been terminated */
	case SLP_INVAL:
		return EINVAL;	/* Object has been deleted */
	case SLP_INTR:
		return EINTR;	/* Exception */
	}
	return 0;
}

/*
 * Receive a message.
 *
 * A thread can receive a message from the object which was created
 * by any thread belongs to same task. If the message has not arrived
 * yet, it blocks until any message comes in.
 * The size argument specifies the "maximum" size of the message
 * buffer to receive. If the sent message is larger than this size,
 * the kernel will automatically clip the message to the receive buffer
 * size.
 *
 * When message is received, the sender thread is removed from
 * object's send queue. So, another thread can receive the subsequent
 * message from that object. This is important for the multi-thread 
 * server which receives some messages simultaneously.
 */
__syscall int msg_receive(object_t obj, void *msg, size_t size)
{
	thread_t th;
	int err, result;
	size_t len;

	err = 0;
	if (!user_area(msg))
		return EFAULT;

	sched_lock();

	if (!object_valid(obj)) {
		err = EINVAL;
		goto out;
	}
	if (obj->owner != cur_task()) {
		err = EACCES;
		goto out;
	}
	/*
	 * Check if this thread finished previous receive
	 * operation. A thread can not receive different 
	 * messages at once.
	 */
	if (cur_thread->recv_obj) {
		err = EBUSY;
		goto out;
	}
	cur_thread->recv_obj = obj;

	/* 
	 * If no message exists, wait until message arrives.
	 */
	while (queue_empty(&obj->sendq)) {
		/*
		 * Sleep until message comes in.
		 */
		enqueue(&obj->recvq, &cur_thread->ipc_link);
		result = sched_sleep(&ipc_event);
		if (result == 0) {
			/*
			 * Even if this thread is woken by the sender thread,
			 * the message may be received by another thread 
			 * before this thread runs. This can occur when higher
			 * priority thread becomes runnable at that time. So,
			 * it is necessary to check the existence of the
			 * sender here again. The following line must be
			 * "continue" instead of "break" to check the queue
			 * again.
			 */
			continue;
		} 
		/*
		 * Receive is failed by some reason.
		 */
		switch (result) {
		case SLP_INVAL:
			err = EINVAL;	/* Object has been deleted */
			break;
		case SLP_INTR:
			queue_remove(&cur_thread->ipc_link);
			err = EINTR;	/* Exception */
			break;
		default:
			panic("Unexpected error for msg receive");
		}
		
		cur_thread->recv_obj = NULL;
		goto out;
	}

	th = dequeue_topprio(&obj->sendq);
	/*
	 * Copy message to user space.
	 * The smaller buffer size is used as copy length
	 * between sender and receiver thread.
	 */
	len = min(size, th->msg_size);
	if (len > 0) {
		if (umem_copyout(th->msg_addr, msg, len)) {
			enqueue(&obj->sendq, &th->ipc_link);
			cur_thread->recv_obj = NULL;
			err = EFAULT;
			goto out;
		}
	}
	/*
	 * Detach the message from the target object. 
	 */
	cur_thread->sender = th;
	th->receiver = cur_thread;
 out:

	sched_unlock();
	return err;
}

/*
 * Send a reply message.
 *
 * The target object must be an appropriate object that current thread
 * has been received from. Otherwise, this function will be failed.
 *
 * Since the target object may already be deleted, we can not access
 * the data of the object within this routine.
 */
__syscall int msg_reply(object_t obj, void *msg, size_t size)
{
	thread_t th;
	int err;
	size_t len;

	err = 0;
	if (!user_area(msg))
		return EFAULT;

	sched_lock();

	if (!object_valid(obj) || obj != cur_thread->recv_obj) {
		sched_unlock();
		return EINVAL;
	}
	/*
	 * Check if sender still exists 
	 */
	if (cur_thread->sender == NULL) {
		err = EINVAL;
		goto out;
	}
	/*
	 * Copy message to the sender's buffer.
	 */
	th = cur_thread->sender;
	len = min(size, th->msg_size);
	if (len > 0) {
		if (umem_copyin(msg, th->msg_addr, len)) {
			sched_unlock();
			return EFAULT;
		}
	}
	/*
	 * Wakeup sender with no error.
	 */
	sched_unsleep(th, 0);
	th->receiver = NULL;
 out:
	/* Clear transmit state */
	cur_thread->sender = NULL;
	cur_thread->recv_obj = NULL;

	sched_unlock();
	return err;
}

/*
 * Clean up pending message operation of specified thread in
 * order to prevent deadlock.
 * This is called when the thread is killed.
 * It is necessary to deal with the following conditions.
 *
 * If killed thread is sender:
 *  1. Killed after message is received
 *     -> The received thread will reply to the invalid thread.
 *
 *  2. Killed before message is received
 *     -> The thread remains in send queue of the object.
 *
 * When thread is receiver:
 *  3. Killed after message is sent
 *     -> The sender thread continues waiting for reply forever.
 *
 *  4. Killed before message is sent
 *     -> The thread remains in receive queue of the object.
 */
void msg_cleanup(thread_t th)
{
	sched_lock();

	if (th->send_obj) {
		if (th->receiver) {
			th->receiver->sender = NULL;
		} else {
			queue_remove(&th->ipc_link);
		}
	}
	if (th->recv_obj) {
		if (th->sender) {
			sched_unsleep(th->sender, SLP_BREAK);
			th->sender->receiver = NULL;
		} else {
			queue_remove(&th->ipc_link);
		}
	}
	sched_unlock();
}

/*
 * Cancel all message operation relevant to the specified object.
 * This is called when target object is deleted.
 * All threads in message queue are woken to avoid deadlock.
 * If the message has already been received, send/reply operation
 * continue processing normally.
 */
void msg_cancel(object_t obj)
{
	queue_t head, q;
	thread_t th;

	sched_lock();

	/* Force wakeup all thread in the send queue */
	head = &obj->sendq;
	for (q = queue_first(head); !queue_end(head, q); q = queue_next(q)) {
		th = queue_entry(q, struct thread, ipc_link);
		sched_unsleep(th, SLP_INVAL);
	}
	/* Force wakeup all thread waiting for receive */
	head = &obj->recvq;
	for (q = queue_first(head); !queue_end(head, q); q = queue_next(q)) {
		th = queue_entry(q, struct thread, ipc_link);
		sched_unsleep(th, SLP_INVAL);
	}
	sched_unlock();
}

/*
 * Remove the top priority thread from specified queue.
 */
static thread_t dequeue_topprio(queue_t head)
{
	queue_t q;
	thread_t th, top;

	q = queue_first(head);
	top = queue_entry(q, struct thread, ipc_link);
	while (!queue_end(head, q)) {
		th = queue_entry(q, struct thread, ipc_link);
		if (th->prio < top->prio)
			top = th;
		q = queue_next(q);
	}
	queue_remove(&top->ipc_link);
	return top;
}
