/*-
 * Copyright (c) 2008, Lazarenko Andrew
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

#ifndef _EBI_H
#define _EBI_H

/*
 * ebi.h - External Bus Interface (EBI) definition for AT91x40
 *
 */

/*
 * Data Bus Width
 */
#define EBI_DBW_16	(1 << 0)
#define EBI_DBW_8	(2 << 0)

/*
 * Number of Wait States
 */
#define EBI_NWS_1	(0 << 2)
#define EBI_NWS_2	(1 << 2)
#define EBI_NWS_3	(2 << 2)
#define EBI_NWS_4	(3 << 2)
#define EBI_NWS_5	(4 << 2)
#define EBI_NWS_6	(5 << 2)
#define EBI_NWS_7	(6 << 2)
#define EBI_NWS_8	(7 << 2)

/*
 * Wait State Enable
 */
#define EBI_WSE		(1 << 5)

/*
 * Page Size
 */
#define EBI_PAGES_1M	(0 << 7)
#define EBI_PAGES_4M	(1 << 7)
#define EBI_PAGES_16M	(2 << 7)
#define EBI_PAGES_64M	(3 << 7)

/*
 * Data Float Output Time
 */
#define EBI_TDF_0	(0 << 9)
#define EBI_TDF_1	(1 << 9)
#define EBI_TDF_2	(2 << 9)
#define EBI_TDF_3	(3 << 9)
#define EBI_TDF_4	(4 << 9)
#define EBI_TDF_5	(5 << 9)
#define EBI_TDF_6	(6 << 9)
#define EBI_TDF_7	(7 << 9)

/*
 * Byte Access Type
 */
#define EBI_BAT_BYTE_WRITE	(0 << 12)
#define EBI_BAT_BYTE_SELECT	(1 << 12)

/*
 * Chip Select Enable
 */
#define EBI_CSEN	(1 << 13)

/*
 * Base Address
 */
#define EBI_BA_MASK	0xfff00000

/*
 * Remap Command
 */
#define EBI_RCB		(1 << 0)

/*
 * Address Line Enable
 */
#define EBI_ALE_16M	(0 << 0)
#define EBI_ALE_8M	(4 << 0)
#define EBI_ALE_4M	(5 << 0)
#define EBI_ALE_2M	(6 << 0)
#define EBI_ALE_1M	(7 << 0)

/*
 * Data Read Protocol
 */
#define EBI_DRP_STANDARD	(0 << 4)
#define EBI_DRP_EARLY		(1 << 4)

#endif /* _EBI_H */
