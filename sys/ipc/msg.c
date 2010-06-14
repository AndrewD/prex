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
 * msg.c - routines to transmit a message.
 */

/**
 * IPC transmission:
 *
 * Messages are sent to the specific object by using msg_send.  The
 * transmission of a message is completely synchronous with this
 * kernel. This means the thread which sent a message is blocked until
 * it receives a response from another thread.  msg_receive performs
 * reception of a message. msg_receive is also blocked when no message
 * is reached to the target object.  The receiver thread must answer the
 * message using msg_reply after processing the message.
 *
 * The receiver thread can not receive an additional message until it
 * replies to the sender. In short, a thread can receive only one
 * message at once. In other hand, once the thread receives a message,
 * it can send another message to different object. This mechanism
 * allows threads to redirect the sender's request to another thread.
 *
 * A message is copied from thread to thread directly without any kernel
 * buffering. The message buffer in sender's memory space is automatically
 * mapped to the receiver's memory by kernel. Since there is no page
 * out of memory in this system, we can copy the message data via physical
 * memory at anytime.
 */

#include <kernel.h>
#include <sched.h>
#include <task.h>
#include <kmem.h>
#include <thread.h>
#include <task.h>
#include <event.h>
#include <ipc.h>

/* forward declarations */
static thread_t	msg_dequeue(queue_t);
static void	msg_enqueue(queue_t, thread_t);

static struct event ipc_event;		/* event for IPC operation */

/*
 * Send a message.
 *
 * The current thread will be blocked until any other thread
 * receives and reply the message.  A thread can send a
 * message to any object if it knows the object id.
 */
int
msg_send(object_t obj, void *msg, size_t size)
{
	struct msg_header *hdr;
	thread_t t;
	void *kmsg;
	int rc;

	if (!user_area(msg))
		return EFAULT;

	if (size < sizeof(struct msg_header))
		return EINVAL;

	sched_lock();

	if (!object_valid(obj)) {
		sched_unlock();
		return EINVAL;
	}
	/*
	 * A thread can not send a message when it is
	 * already receiving from the target object.
	 * It will obviously cause a deadlock.
	 */
	if (obj == curthread->recvobj) {
		sched_unlock();
		return EDEADLK;
	}
	/*
	 * Translate message address to the kernel linear
	 * address.  So that a receiver thread can access
	 * the message via kernel pointer. We can catch
	 * the page fault here.
	 */
	if ((kmsg = kmem_map(msg, size)) == NULL) {
		sched_unlock();
		return EFAULT;
	}
	curthread->msgaddr = kmsg;
	curthread->msgsize = size;

	/*
	 * The sender ID is filled in the message header
	 * by the kernel. So, the receiver can trust it.
	 */
	hdr = (struct msg_header *)kmsg;
	hdr->task = curtask;

	/*
	 * If receiver already exists, wake it up.
	 * The highest priority thread can get the message.
	 */
	if (!queue_empty(&obj->recvq)) {
		t = msg_dequeue(&obj->recvq);
		sched_unsleep(t, 0);
	}
	/*
	 * Sleep until we get a reply message.
	 * Note: Do not touch any data in the object
	 * structure after we wakeup. This is because the
	 * target object may be deleted while we are sleeping.
	 */
	curthread->sendobj = obj;
	msg_enqueue(&obj->sendq, curthread);
	rc = sched_sleep(&ipc_event);
	if (rc == SLP_INTR)
		queue_remove(&curthread->ipc_link);
	curthread->sendobj = NULL;

	sched_unlock();

	/*
	 * Check sleep result.
	 */
	switch (rc) {
	case SLP_BREAK:
		return EAGAIN;	/* Receiver has been terminated */
	case SLP_INVAL:
		return EINVAL;	/* Object has been deleted */
	case SLP_INTR:
		return EINTR;	/* Exception */
	default:
		/* DO NOTHING */
		break;
	}
	return 0;
}

/*
 * Receive a message.
 *
 * A thread can receive a message from the object which was
 * created by any thread belongs to same task. If the message
 * has not reached yet, it blocks until any message comes in.
 *
 * The size argument specifies the "maximum" size of the message
 * buffer to receive. If the sent message is larger than this
 * size, the kernel will automatically clip the message to this
 * maximum buffer size.
 *
 * When a message is received, the sender thread is removed from
 * object's send queue. So, another thread can receive the
 * subsequent message from that object. This is important for
 * the multi-thread server which must receive multiple messages
 * simultaneously.
 */
int
msg_receive(object_t obj, void *msg, size_t size)
{
	thread_t t;
	size_t len;
	int rc, error = 0;

	if (!user_area(msg))
		return EFAULT;

	sched_lock();

	if (!object_valid(obj)) {
		sched_unlock();
		return EINVAL;
	}
	if (obj->owner != curtask) {
		sched_unlock();
		return EACCES;
	}
	/*
	 * Check if this thread finished previous receive
	 * operation.  A thread can not receive different
	 * messages at once.
	 */
	if (curthread->recvobj) {
		sched_unlock();
		return EBUSY;
	}
	curthread->recvobj = obj;

	/*
	 * If no message exists, wait until message arrives.
	 */
	while (queue_empty(&obj->sendq)) {
		/*
		 * Block until someone sends a message.
		 */
		msg_enqueue(&obj->recvq, curthread);
		rc = sched_sleep(&ipc_event);
		if (rc != 0) {
			/*
			 * Receive is failed due to some reasons.
			 */
			switch (rc) {
			case SLP_INVAL:
				error = EINVAL;	/* Object has been deleted */
				break;
			case SLP_INTR:
				queue_remove(&curthread->ipc_link);
				error = EINTR;	/* Got exception */
				break;
			default:
				panic("msg_receive");
				break;
			}
			curthread->recvobj = NULL;
			sched_unlock();
			return error;
		}

		/*
		 * Check the existence of the sender thread again.
		 * Even if this thread is woken by the sender thread,
		 * the message may be received by another thread.
		 * This may happen when another high priority thread
		 * becomes runnable before we receive the message.
		 */
	}

	t = msg_dequeue(&obj->sendq);

	/*
	 * Copy out the message to the user-space.
	 */
	len = MIN(size, t->msgsize);
	if (len > 0) {
		if (copyout(t->msgaddr, msg, len)) {
			msg_enqueue(&obj->sendq, t);
			curthread->recvobj = NULL;
			sched_unlock();
			return EFAULT;
		}
	}
	/*
	 * Detach the message from the target object.
	 */
	curthread->sender = t;
	t->receiver = curthread;

	sched_unlock();
	return error;
}

/*
 * Send a reply message.
 *
 * The target object must be the object that we are receiving.
 * Otherwise, this function will be failed.
 */
int
msg_reply(object_t obj, void *msg, size_t size)
{
	thread_t t;
	size_t len;

	if (!user_area(msg))
		return EFAULT;

	sched_lock();

	if (!object_valid(obj) || obj != curthread->recvobj) {
		sched_unlock();
		return EINVAL;
	}
	/*
	 * Check if sender still exists
	 */
	if (curthread->sender == NULL) {
		/* Clear receive state */
		curthread->recvobj = NULL;
		sched_unlock();
		return EINVAL;
	}
	/*
	 * Copy a message to the sender's buffer.
	 */
	t = curthread->sender;
	len = MIN(size, t->msgsize);
	if (len > 0) {
		if (copyin(msg, t->msgaddr, len)) {
			sched_unlock();
			return EFAULT;
		}
	}
	/*
	 * Wakeup sender with no error.
	 */
	sched_unsleep(t, 0);
	t->receiver = NULL;

	/* Clear transmit state */
	curthread->sender = NULL;
	curthread->recvobj = NULL;

	sched_unlock();
	return 0;
}

/*
 * Cancel pending message operation of the specified thread.
 * This is called when the thread is terminated.
 *
 * We have to handle the following conditions to prevent deadlock.
 *
 * If the terminated thread is sending a message:
 *  1. A message is already received.
 *     -> The receiver thread will reply to the invalid thread.
 *
 *  2. A message is not received yet.
 *     -> The thread remains in send queue of the object.
 *
 * When the terminated thread is receiving a message.
 *  3. A message is already sent.
 *     -> The sender thread will wait for reply forever.
 *
 *  4. A message is not sent yet.
 *     -> The thread remains in receive queue of the object.
 */
void
msg_cancel(thread_t t)
{

	sched_lock();

	if (t->sendobj != NULL) {
		if (t->receiver != NULL)
			t->receiver->sender = NULL;
		else
			queue_remove(&t->ipc_link);
	}
	if (t->recvobj != NULL) {
		if (t->sender != NULL) {
			sched_unsleep(t->sender, SLP_BREAK);
			t->sender->receiver = NULL;
		} else
			queue_remove(&t->ipc_link);
	}
	sched_unlock();
}

/*
 * Abort all message operations relevant to the specified object.
 * This is called when the target object is deleted.
 */
void
msg_abort(object_t obj)
{
	queue_t q;
	thread_t t;

	sched_lock();

	/*
	 * Force wakeup all threads in the send queue.
	 */
	while (!queue_empty(&obj->sendq)) {
		q = dequeue(&obj->sendq);
		t = queue_entry(q, struct thread, ipc_link);
		sched_unsleep(t, SLP_INVAL);
	}
	/*
	 * Force wakeup all threads waiting for receive.
	 */
	while (!queue_empty(&obj->recvq)) {
		q = dequeue(&obj->recvq);
		t = queue_entry(q, struct thread, ipc_link);
		sched_unsleep(t, SLP_INVAL);
	}
	sched_unlock();
}

/*
 * Dequeue thread from the IPC queue.
 * The most highest priority thread will be chosen.
 */
static thread_t
msg_dequeue(queue_t head)
{
	queue_t q;
	thread_t t, top;

	q = queue_first(head);
	top = queue_entry(q, struct thread, ipc_link);

	while (!queue_end(head, q)) {
		t = queue_entry(q, struct thread, ipc_link);
		if (t->priority < top->priority)
			top = t;
		q = queue_next(q);
	}
	queue_remove(&top->ipc_link);
	return top;
}

static void
msg_enqueue(queue_t head, thread_t t)
{

	enqueue(head, &t->ipc_link);
}

void
msg_init(void)
{

	event_init(&ipc_event, "ipc");
}
