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
 * ctype.c - character type table
 */
#include <ctype.h>

const unsigned char _ctype[] = {0x00,                  /* EOF     */
_C,_C,_C,_C,_C,_C,_C,_C,                               /* 0-7     */
_C,_C|_S,_C|_S,_C|_S,_C|_S,_C|_S,_C,_C,                /* 8-15    */
_C,_C,_C,_C,_C,_C,_C,_C,                               /* 16-23   */
_C,_C,_C,_C,_C,_C,_C,_C,                               /* 24-31   */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,                           /* 32-39   */
_P,_P,_P,_P,_P,_P,_P,_P,                               /* 40-47   */
_D,_D,_D,_D,_D,_D,_D,_D,                               /* 48-55   */
_D,_D,_P,_P,_P,_P,_P,_P,                               /* 56-63   */
_P,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U|_X,_U,             /* 64-71   */
_U,_U,_U,_U,_U,_U,_U,_U,                               /* 72-79   */
_U,_U,_U,_U,_U,_U,_U,_U,                               /* 80-87   */
_U,_U,_U,_P,_P,_P,_P,_P,                               /* 88-95   */
_P,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L|_X,_L,             /* 96-103  */
_L,_L,_L,_L,_L,_L,_L,_L,                               /* 104-111 */
_L,_L,_L,_L,_L,_L,_L,_L,                               /* 112-119 */
_L,_L,_L,_P,_P,_P,_P,_C,                               /* 120-127 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                       /* 128-143 */
0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,                       /* 144-159 */
_S|_SP,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,   /* 160-175 */
_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,_P,       /* 176-191 */
_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,_U,       /* 192-207 */
_U,_U,_U,_U,_U,_U,_U,_P,_U,_U,_U,_U,_U,_U,_U,_L,       /* 208-223 */
_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,_L,       /* 224-239 */
_L,_L,_L,_L,_L,_L,_L,_P,_L,_L,_L,_L,_L,_L,_L,_L};      /* 240-255 */
