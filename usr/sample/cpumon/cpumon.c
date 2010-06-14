/*
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
 * cpumon.c - CPU voltage monitoring program
 */

#include <sys/prex.h>
#include <sys/ioctl.h>
#include <stdio.h>

static struct cpufreqinfo cf_info;

int
main(int argc, char *argv[])
{
	device_t dev;
	int last_mhz = 0;
	int i, j;
	static char bar[21];

	/* Boost current prioriy */
	thread_setpri(thread_self(), 50);

	if (device_open("cpufreq", 0, &dev))
		panic("open error: cpufreq");

	/* Clear screen */
	printf("\33[2J");

	printf("CPU voltage monitor\n");
	device_ioctl(dev, CFIOC_GET_INFO, &cf_info);
	if (cf_info.freq == 0 || cf_info.volts == 0)
		panic("Invalid cpu power/speed");

	/*
	 * Setup periodic timer for 10msec period
	 */
	timer_periodic(thread_self(), 100, 10);
	for (;;) {
		/*
		 * Wait next period
		 */
		timer_waitperiod();
		device_ioctl(dev, CFIOC_GET_INFO, &cf_info);
		if (cf_info.freq != last_mhz) {
			printf("\33[s"); /* save cursor */

			/*
			 * Display speed
			 */
			printf("\nSpeed: %4dMHz  0|", cf_info.freq);
			j = cf_info.freq * 100 / cf_info.maxfreq;
			for (i = 0; i < 20; i++)
				bar[i] = (i <= j / 5) ? '*' : '-';
			bar[i] = '\0';
			printf("%s|100", bar);

			/*
			 * Display power
			 */
			printf("\nPower: %4dmV   0|", cf_info.volts);
			j = cf_info.volts * 100 / cf_info.maxvolts;
			for (i = 0; i < 20; i++)
				bar[i] = (i <= j / 5) ? '*' : '-';
			bar[i] = '\0';
			printf("%s|100", bar);

			printf("\33[u"); /* restore cursor */
			last_mhz = cf_info.freq;
		}
	}
	return 0;
}
