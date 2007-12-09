/*-
 * Copyright (c) 2005, Kohsuke Ohtani
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
 * main.c - driver main routine
 */
#include <driver.h>

#if DEBUG
void driver_dump(void)
{
	driver_t drv;

	printk("driver_dump:\n");
	printk(" order init     name\n");
	printk(" ----- -------- -------------------------\n");
	for (drv = &__driver_table; drv <= &__driver_table_end; drv++)
		printk(" %d     %08x %s\n", drv->order, drv->init, drv->name);
}
#endif

void driver_main(void)
{
	struct driver *drv;
	int order, err;

	printk("Prex driver module build:" __DATE__ "\n");

	/*
	 * Call init routine for all device drivers with init order.
	 * Smaller value will be run first.
	 */
	for (order = 0; order < 16; order++) {
		for (drv = &__driver_table; drv != &__driver_table_end;
		     drv++) {
			ASSERT(drv->order < 16);
			if (drv->order == order) {
				if (drv->init) {
					printk("Initializing %s\n", drv->name);
					err = drv->init();
				}
			}
		}
	}
}
