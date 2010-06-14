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

#ifndef _WSCONS_H
#define _WSCONS_H

#include <sys/cdefs.h>

/*
 * Video interface
 */
struct wscons_video_ops {
	void	(*cursor)    (void *aux, int row, int col);
	void	(*putc)      (void *aux, int row, int col, int ch);
	void	(*copyrows)  (void *aux, int srcrow, int dstrow, int nrows);
	void	(*eraserows) (void *aux, int row, int nrows);
	void	(*set_attr)  (void *aux, int attr);
	void	(*get_cursor)(void *aux, int *col, int *row);
};

/*
 * Keyboard interface
 */
struct wscons_kbd_ops {
	int	(*getc)     (void *aux);
	void	(*set_poll) (void *aux, int on);
};

__BEGIN_DECLS
void	wscons_attach_video(struct wscons_video_ops *, void *aux);
void	wscons_attach_kbd(struct wscons_kbd_ops *, void *aux);
void	wscons_kbd_input(int);
__END_DECLS

#endif /* !_WSCONS_H */
