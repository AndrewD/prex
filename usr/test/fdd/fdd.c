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
 * fdd.c - fdd driver test program.
 */

#include <sys/prex.h>
#include <stdio.h>
#include <string.h>
#include <ctype.h>

int
test_read(int sector)
{
	device_t fdd;
	size_t size;
	int error, i, j;
	static unsigned char disk_buf[512];
	unsigned char ch;

	printf("open fd0\n");
	error = device_open("fd0", 0, &fdd);
	if (error) {
		printf("open failed\n");
		return 0;
	}
	printf("opened\n");

	printf("fdd read: sector=%d buf=%x\n", sector, (u_int)disk_buf);
	size = 512;
	error = device_read(fdd, disk_buf, &size, sector);
	if (error) {
		printf("read failed\n");
		device_close(fdd);
		return 0;
	}
	printf("read comp: sector=%d buf=%x\n", sector, (u_int)disk_buf);

	for (i = 0; i < (512 / 16); i++) {
		for (j = 0; j < 16; j++)
			printf("%02x ", disk_buf[i * 16 + j]);
		printf("    ");
		for (j = 0; j < 16; j++) {
			ch = disk_buf[i * 16 + j];
			if (isprint(ch))
				putchar(ch);
			else
				putchar('.');

		}
		printf("\n");
	}
	printf("\n");
	error = device_close(fdd);
	if (error)
		printf("close failed\n");

	return 0;
}

int
test_write(int sector)
{
	device_t fdd;
	size_t size;
	int error;
	static unsigned char disk_buf[512];

	printf("open fd0\n");
	error = device_open("fd0", 0, &fdd);
	if (error) {
		printf("open failed\n");
		return 0;
	}
	printf("opened\n");

	size = 512;
	error = device_read(fdd, disk_buf, &size, sector);
	if (error) {
		printf("read failed\n");
		device_close(fdd);
		return 0;
	}
	printf("read comp sector=%d\n", sector);

	size = 512;
	error = device_write(fdd, disk_buf, &size, sector);
	if (error) {
		printf("write failed\n");
		device_close(fdd);
		return 0;
	}
	printf("write comp sector=%d\n", sector);

	error = device_close(fdd);
	if (error)
		printf("close failed\n");
	return 0;
}

int
main(int argc, char *argv[])
{
	test_read(0);
	test_read(1);
	test_read(2);

	test_write(1);

	return 0;
}
