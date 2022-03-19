/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for more details governing rights and limitations under the License.
 *
 * The Original Code is part of the frlib.
 *
 * The Initial Developer of the Original Code is
 * Frank Reker <frank@reker.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <string.h>
#include <strings.h>

#include <stdint.h>

#include "sha1.h"
#include "errors.h"
#include "bo.h"
#include "txtenc.h"



static uint32_t leftrotate (uint32_t, unsigned);
static int doround (	const char*, uint32_t*, uint32_t*, uint32_t*, uint32_t*,
							uint32_t*);
static int sha1calc (char*, size_t, const char*);



int
sha1sum (hash, msg, msglen, flags)
	char			*hash;
	const char	*msg;
	size_t		msglen;
	int			flags;
{
	char	sha1[20];
	int	ret;

	if (!hash) return RERR_PARAM;
	ret = sha1calc (sha1, msglen, msg);
	if (!RERR_ISOK(ret)) return ret;

	ret = tenc_encode (hash, sha1, 20, flags|TENC_F_NONL);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
sha1calc (hash, len, msg)
	char			*hash;
	const char	*msg;
	size_t		len;
{
	uint32_t		h0, h1, h2, h3, h4;
	uint64_t		xlen;
	size_t		blen, last, i;
	char			lblock[64];
	const char	*block;
	int			ret;

	if (!hash || !msg) return RERR_PARAM;

	/* do initialization */
	h0 = 0x67452301L;
	h1 = 0xEFCDAB89L;
	h2 = 0x98BADCFEL;
	h3 = 0x10325476L;
	h4 = 0xC3D2E1F0L;
	blen = len / 64;
	last = len % 64;
	xlen = len;
	xlen *= 8;
	ret = bo_cv64 (&xlen, xlen, BO_TO_BE);
	if (!RERR_ISOK(ret)) return ret;

	/* elab all full blocks */
	for (i=0; i<blen; i++) {
		block = msg + i * 64;
		ret = doround (block, &h0, &h1, &h2, &h3, &h4);
		if (!RERR_ISOK(ret)) return ret;
	}

	/* elab last block */
	block = msg + i * 64;
	if (last > 0) {
		memcpy (lblock, block, last);
	}
	lblock[last] = 0x80;
	last++;
	if (last <= 56) {
		for (i=last; i<56; i++) lblock[i]=0;
		*(uint64_t*)(void*)(lblock+56) = (uint64_t)xlen;
		ret = doround (lblock, &h0, &h1, &h2, &h3, &h4);
		if (!RERR_ISOK(ret)) return ret;
	} else {
		for (i=last; i<64; i++) lblock[i]=0;
		ret = doround (lblock, &h0, &h1, &h2, &h3, &h4);
		if (!RERR_ISOK(ret)) return ret;
		for (i=0; i<56; i++) lblock[i]=0;
		*(uint64_t*)(void*)(lblock+56) = (uint64_t)xlen;
		ret = doround (lblock, &h0, &h1, &h2, &h3, &h4);
		if (!RERR_ISOK(ret)) return ret;
	}

	/* convert h0 thru h4 to big endian */
	ret = bo_cv32 (&h0, h0, BO_TO_BE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (&h1, h1, BO_TO_BE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (&h2, h2, BO_TO_BE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (&h3, h3, BO_TO_BE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (&h4, h4, BO_TO_BE);
	if (!RERR_ISOK(ret)) return ret;

	/* create hash */
	*(uint32_t*)(void*)(hash+0) = (uint32_t)h0;
	*(uint32_t*)(void*)(hash+4) = (uint32_t)h1;
	*(uint32_t*)(void*)(hash+8) = (uint32_t)h2;
	*(uint32_t*)(void*)(hash+12) = (uint32_t)h3;
	*(uint32_t*)(void*)(hash+16) = (uint32_t)h4;

	return RERR_OK;
}


#define DOROUND	\
		tmp = leftrotate (a, 5) + f + e + k + w[i]; \
		e = d; \
		d = c; \
		c = leftrotate (b, 30); \
		b = a; \
		a = tmp;

static
int
doround (block, h0, h1, h2, h3, h4)
	const char	*block;
	uint32_t	*h0, *h1, *h2, *h3, *h4;
{
	uint32_t	a, b, c, d, e, f, k, tmp;
	uint32_t	w[80];
	int		i, ret;

	if (!block || !h0 || !h1 || !h2 || !h3 || !h4) return RERR_PARAM;

	/* create w - vector */
	for (i=0; i<16; i++) {
		ret = bo_cv32 (&(w[i]), *(uint32_t*)(void*)(block + i*4), BO_FROM_BE);
		if (!RERR_ISOK(ret)) return ret;
	}
	for (i=16; i<80; i++) {
		w[i] = leftrotate (w[i-3] ^ w[i-8] ^ w[i-14] ^ w[i-16], 1);
	}

	/* initialize */
	a = *h0;
	b = *h1;
	c = *h2;
	d = *h3;
	e = *h4;

	/* main loop */
	k = 0x5A827999L;
	for (i=0; i<20; i++) {
		f = (b & c) | (~b & d);
		DOROUND;
	}
	k = 0x6ED9EBA1L;
	for (i=20; i<40; i++) {
		f = b ^ c ^ d;
		DOROUND;
	}
	k = 0x8F1BBCDCL;
	for (i=40; i<60; i++) {
		f = (b & c) | (b & d) | (c & d);
		DOROUND;
	}
	k = 0xCA62C1D6L;
	for (i=60; i<80; i++) {
		f = b ^ c ^ d;
		DOROUND;
	}

	/* add to hash */
	*h0 += a;
	*h1 += b;
	*h2 += c;
	*h3 += d;
	*h4 += e;

	return RERR_OK;
}



static
uint32_t
leftrotate (a, n)
	uint32_t		a;
	unsigned		n;
{
	n %= 32;
	return (a << n) | (a >> (32-n));
}

























/*
 * Overrides for XEmacs and vim so that we get a uniform tabbing style.
 * XEmacs/vim will notice this stuff at the end of the file and automatically
 * adjust the settings for this buffer only.  This must remain at the end
 * of the file.
 * ---------------------------------------------------------------------------
 * Local variables:
 * c-indent-level: 3
 * c-basic-offset: 3
 * tab-width: 3
 * End:
 * vim:tw=0:ts=3:wm=0:
 */
