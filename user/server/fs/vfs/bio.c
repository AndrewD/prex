/*
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
 * bio.c - buffered I/O operations
 */

/*
 * Our buffer cache works as a write-through cache, and so it does
 * not require flushing cache by an external helper program like an
 * update daemon. This design is suitable to prevent the data loss
 * by unexpected power failure with the battery-powered devices.
 */

#include <prex/prex.h>
#include <sys/list.h>
#include <sys/syslog.h>
#include <sys/param.h>
#include <sys/buf.h>

#include <limits.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

#include "vfs.h"

/* Macros to clear/set/test flags. */
#define	SET(t, f)	(t) |= (f)
#define	CLR(t, f)	(t) &= ~(f)
#define	ISSET(t, f)	((t) & (f))

/*
 * Global lock to access all buffer headers and lists.
 */
#if NR_FS_THREADS > 1
static mutex_t bio_lock = MUTEX_INITIALIZER;
#define BIO_LOCK()	mutex_lock(&bio_lock)
#define BIO_UNLOCK()	mutex_unlock(&bio_lock)
#else
#define BIO_LOCK()
#define BIO_UNLOCK()
#endif

static void *buf_base;		/* Pointer to the chunk of buffers */
static struct buf buf_table[NR_BUFFERS];
static struct list free_list = LIST_INIT(free_list);
static int nr_free;
static sem_t free_sem;

/*
 * Insert buffer to the head of free list
 */
static void insqfree_head(struct buf *bp)
{
	list_insert(&free_list, &bp->b_link);
	nr_free++;
	sem_post(&free_sem);
	bio_printf("insqfree_head: free=%d\n", nr_free);
}

/*
 * Insert buffer to the tail of free list
 */
static void insqfree_tail(struct buf *bp)
{
	list_insert(list_prev(&free_list), &bp->b_link);
	nr_free++;
	sem_post(&free_sem);
	bio_printf("insqfree_tail: free=%d\n", nr_free);
}

/*
 * Remove buffer
 */
static void remqfree(struct buf *bp)
{
	bio_printf("remqfree: free=%d\n", nr_free);
	sem_wait(&free_sem, 0);
	ASSERT(!list_empty(&free_list));
	list_remove(&bp->b_link);
	nr_free--;
}

/*
 * Remove buffer from free list
 */
static struct buf *remqfree_head(void)
{
	struct buf *bp;
	list_t n;

	bio_printf("remqfree_head: free=%d\n", nr_free);
	sem_wait(&free_sem, 0);

	ASSERT(!list_empty(&free_list));
	n = list_first(&free_list);
	bp = list_entry(n, struct buf, b_link);
	list_remove(&bp->b_link);
	nr_free--;
	return bp;
}

/*
 * Determine if a block is in the cache.
 */
static struct buf *incore(dev_t dev, int blkno)
{
	struct buf *bp;
	int i;

	for (i = 0; i < NR_BUFFERS; i++) {
		bp = &buf_table[i];
		if (bp->b_blkno == blkno && bp->b_dev == dev &&
		    !ISSET(bp->b_flags, B_INVAL))
			return bp;
	}
	return NULL;
}

/*
 * Get a buffer for the given block.
 *
 * The block is selected from the buffer list with LRU algorithm.
 * If the appropriate block already exists in the block list, return it.
 * Otherwise, the least recently used block is used.
 */
struct buf *getblk(dev_t dev, int blkno)
{
	struct buf *bp;

	bio_printf("getblk: dev=%x blkno=%d\n", dev, blkno);
 start:
	BIO_LOCK();
	bp = incore(dev, blkno);
	if (bp != NULL) {
		bio_printf("getblk: found in cache! bp=%x\n", bp);

		/* Block exists in cache */
		if (ISSET(bp->b_flags, B_BUSY)) {
			bio_printf("getblk: but busy...\n");
			BIO_UNLOCK();
			mutex_lock(&bp->b_lock);
			mutex_unlock(&bp->b_lock);
			bio_printf("getblk: scan again...\n");
			goto start;
		}
		remqfree(bp);
	} else {
		bio_printf("getblk: find new buf\n");

		bp = remqfree_head();
		SET(bp->b_flags, B_BUSY);
		if (ISSET(bp->b_flags, B_DELWRI))
			bwrite(bp);
		bp->b_flags = B_BUSY;
	}
	bp->b_dev = dev;
	bp->b_blkno = blkno;
	BIO_UNLOCK();
	SET(bp->b_flags, B_BUSY);
	mutex_lock(&bp->b_lock);
	bio_printf("getblk: done bp=%x\n", bp);
	return bp;
}

/*
 * Release a buffer on to the free lists.
 */
void brelse(struct buf *bp)
{
	bio_printf("brelse: bp=%x dev=%x blkno=%d\n",
		   bp, bp->b_dev, bp->b_blkno);
	ASSERT(ISSET(bp->b_flags, B_BUSY));
	BIO_LOCK();
	CLR(bp->b_flags, B_BUSY);
	mutex_unlock(&bp->b_lock);
	if (ISSET(bp->b_flags, B_INVAL))
		insqfree_head(bp);
	else
		insqfree_tail(bp);
	BIO_UNLOCK();
}

/*
 * Block read with cache.
 *
 * @dev:   device id to read from.
 * @blkno: block number.
 * @buf:   buffer to read.
 *
 * An actual read operation is done only when the cached buffer
 * is dirty.
 */
int bread(dev_t dev, int blkno, struct buf **bpp)
{
	struct buf *bp;
	size_t size;
	int err;

	bio_printf("bread: dev=%x blkno=%d\n", dev, blkno);
	bp = getblk(dev, blkno);

	if (!ISSET(bp->b_flags, (B_DONE | B_DELWRI))) {
		bio_printf("bread: i/o read\n");
		size = BSIZE;
		err = device_read((device_t)dev, bp->b_data, &size, blkno);
		if (err) {
			bio_printf("bread: i/o error\n");
			brelse(bp);
			return err;
		}
	}
	BIO_LOCK();
	CLR(bp->b_flags, B_INVAL);
	SET(bp->b_flags, (B_READ | B_DONE));
	BIO_UNLOCK();
	bio_printf("bread: done bp=%x\n\n", bp);
	*bpp = bp;
	return 0;
}

/*
 * Block write with cache.
 *
 * @buf:   buffer to write.
 *
 * The data is copied to cache block.
 */
int bwrite(struct buf *bp)
{
	size_t size;
	int err;

	bio_printf("bwrite: dev=%x blkno=%d\n", bp->b_dev, bp->b_blkno);
	BIO_LOCK();
	CLR(bp->b_flags, (B_READ | B_DONE | B_DELWRI));
	BIO_UNLOCK();

	size = BSIZE;
	err = device_write((device_t)bp->b_dev, bp->b_data, &size, bp->b_blkno);
	if (err)
		return err;
	BIO_LOCK();
	SET(bp->b_flags, B_DONE);
	BIO_UNLOCK();
	brelse(bp);
	return 0;
}

/*
 * Delayed write.
 *
 * The data is copied to cache block.
 */
void bdwrite(struct buf *bp)
{
	BIO_LOCK();
	SET(bp->b_flags, B_DELWRI);
	CLR(bp->b_flags, B_DONE);
	BIO_UNLOCK();
	brelse(bp);
}

/*
 * Invalidate buffer for specified device.
 * This is called when unmount.
 */
void binval(dev_t dev)
{
	struct buf *bp;
	int i;

	BIO_LOCK();
	for (i = 0; i < NR_BUFFERS; i++) {
		bp = &buf_table[i];
		if (bp->b_dev == dev) {
			if (ISSET(bp->b_flags, B_BUSY))
				brelse(bp);
			bp->b_flags = B_INVAL;
		}
	}
	BIO_UNLOCK();
}

/*
 * Initialize
 */
void bio_init(void)
{
	int i;
	char *p;

	if (vm_allocate(task_self(), &buf_base, BSIZE * NR_BUFFERS, 1) != 0)
		panic("bio_init: failed");
	/*
	 * Build buffer list.
	 */
	p = buf_base;
	for (i = 0; i < NR_BUFFERS; i++) {
		buf_table[i].b_flags = B_INVAL;
		buf_table[i].b_data = p;
		list_insert(&free_list, &buf_table[i].b_link);
		p += BSIZE;
	}
	sem_init(&free_sem, NR_BUFFERS);
	nr_free = NR_BUFFERS;
}
