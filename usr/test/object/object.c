/*-
 * Copyright (c) 2009, Kohsuke Ohtani
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
 * object.c - test for object security.
 */

#include <sys/prex.h>
#include <ipc/ipc.h>

#include <stdio.h>


int
main(int argc, char *argv[])
{
	object_t obj;
	int error;

	printf("test for object security\n");

	/*
	 * Try to make normal object.
	 */
	error = object_create("test", &obj);
	if (error)
		panic("Failed to create an object.\n");

	error = object_destroy(obj);
	if (error)
		panic("Failed to destroy an object.\n");

	/*
	 * We can not use an object name that started with '!'.
	 */
	error = object_create("!test", &obj);
	if (error == 0)
		panic("Oops! We could create protected object!");

	/*
	 * Find process object and destroy it!
	 */
	error = object_lookup("!proc", &obj);
	if (error)
		panic("Could not find a process object!");

	error = object_destroy(obj);
	if (error == 0)
		panic("Oops! We could destroy a process object!");

	printf("test ok\n");
	return 0;
}
