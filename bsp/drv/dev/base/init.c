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
 * init.c - driver initialization routine
 */

#include <driver.h>
#include <cons.h>
#include <dki.h>
#include <kd.h>
#include <conf/drvtab.h>

#ifdef DEBUG
#define DPRINTF(a) printf a
#else
#define DPRINTF(a)
#endif

#define NDRIVERS	(int)(sizeof(driver_table) / sizeof(driver_table[0]))


/*
 * Call xxx_probe routine for all drivers.
 */
void
driver_probe(void)
{
	struct driver *dp;
	int i;

	DPRINTF(("Probing devices...\n"));

	for (i = 0; i < NDRIVERS; i++) {
		dp = driver_table[i];
		ASSERT(dp != NULL);

		if (dp->probe != NULL) {
			if (dp->probe(dp) == 0)
				dp->flags |= DS_ALIVE;
		} else {
			/*
			 * No probe method. Mark it alive.
			 */
			dp->flags |= DS_ALIVE;
		}
	}
}

/*
 * Call xxx_init routine for all living drivers.
 */
void
driver_init(void)
{
	struct driver *dp;
	int i;

	for (i = 0; i < NDRIVERS; i++) {
		dp = driver_table[i];

		/* All drivers must have init method */
		ASSERT(dp->init != NULL);

		if (dp->flags & DS_ALIVE) {
			DPRINTF(("Initializing %s\n", dp->name));
			if (dp->init(dp) == 0)
				dp->flags |= DS_ACTIVE;
		}
	}
}

/*
 * Call xxx_unload routine for all active drivers.
 */
void
driver_shutdown(void)
{
	struct driver *dp;
	int i;

	DPRINTF(("Shutting down...\n"));
	for (i = NDRIVERS - 1; i >= 0; i--) {
		dp = driver_table[i];
		if (dp == NULL)
			break;
		/*
		 * Process only active drivers.
		 */
		if (dp->flags & DS_ACTIVE) {
			DPRINTF(("Unloading %s\n", dp->name));
			if (dp->unload != NULL)
				dp->unload(dp);
		}
	}
}

#if defined(DEBUG) && defined(CONFIG_KD)
void
driver_dump(void)
{
	struct driver *dp;
	int i = 0;

	printf("Driver table:\n");
	printf(" probe    init     unload   devops   flags    name\n");
	printf(" -------- -------- -------- -------- -------- -----------\n");

	for (i = 0; i < NDRIVERS; i++) {
		dp = driver_table[i];
		printf(" %08lx %08lx %08lx %08lx %08lx %s\n",
		       (long)dp->probe, (long)dp->init, (long)dp->unload,
		       (long)dp->devops,(long) dp->flags, dp->name);
	}
}
#endif
