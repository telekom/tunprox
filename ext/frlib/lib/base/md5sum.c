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
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <ctype.h>


#include <fr/base/errors.h>
#include <fr/base/textop.h>
#include <fr/base/slog.h>
#include <fr/base/md5sum.h>
#include <fr/base/txtenc.h>
#include <fr/base/bo.h>


static int doround (uint32_t*, uint32_t*, uint32_t*, uint32_t*, uint32_t*);
static int docalc (char*, const char*, size_t);



int
md5sum (hash, msg, msglen, flags)
	char			*hash;
	const char	*msg;
	size_t		msglen;
	int			flags;
{
	char	md5[16];
	int	ret;

	if (!hash) return RERR_PARAM;
	ret = docalc (md5, msg, msglen);
	if (!RERR_ISOK(ret)) return ret;

	return tenc_encode (hash, md5, 16, flags|TENC_F_NONL);
}




static const uint32_t md5_s[] = {
	7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,  7, 12, 17, 22,
	5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,  5,  9, 14, 20,
	4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,  4, 11, 16, 23,
	6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21,  6, 10, 15, 21};



static const uint32_t md5_k[] = {
	0xd76aa478UL, 0xe8c7b756UL, 0x242070dbUL, 0xc1bdceeeUL,
	0xf57c0fafUL, 0x4787c62aUL, 0xa8304613UL, 0xfd469501UL,
	0x698098d8UL, 0x8b44f7afUL, 0xffff5bb1UL, 0x895cd7beUL,
	0x6b901122UL, 0xfd987193UL, 0xa679438eUL, 0x49b40821UL,
	0xf61e2562UL, 0xc040b340UL, 0x265e5a51UL, 0xe9b6c7aaUL,
	0xd62f105dUL, 0x02441453UL, 0xd8a1e681UL, 0xe7d3fbc8UL,
	0x21e1cde6UL, 0xc33707d6UL, 0xf4d50d87UL, 0x455a14edUL,
	0xa9e3e905UL, 0xfcefa3f8UL, 0x676f02d9UL, 0x8d2a4c8aUL,
	0xfffa3942UL, 0x8771f681UL, 0x6d9d6122UL, 0xfde5380cUL,
	0xa4beea44UL, 0x4bdecfa9UL, 0xf6bb4b60UL, 0xbebfbc70UL,
	0x289b7ec6UL, 0xeaa127faUL, 0xd4ef3085UL, 0x04881d05UL,
	0xd9d4d039UL, 0xe6db99e5UL, 0x1fa27cf8UL, 0xc4ac5665UL,
	0xf4292244UL, 0x432aff97UL, 0xab9423a7UL, 0xfc93a039UL,
	0x655b59c3UL, 0x8f0ccc92UL, 0xffeff47dUL, 0x85845dd1UL,
	0x6fa87e4fUL, 0xfe2ce6e0UL, 0xa3014314UL, 0x4e0811a1UL,
	0xf7537e82UL, 0xbd3af235UL, 0x2ad7d2bbUL, 0xeb86d391UL
};

int md5sum_calc (struct md5sum *, const char *msg, size_t mlen);
int md5sum_get (char *out, struct md5sum*, int flags);

int
md5sum_calc (hash, msg, mlen)
	struct md5sum	*hash;
	const char		*msg;
	size_t			mlen;
{
	int	ret;

	if (!hash || (!msg && mlen > 0)) return RERR_PARAM;
	if (!msg) msg="";

	if ((ssize_t)mlen < (ssize_t)(64 - hash->idx)) {
		memcpy (hash->buf + hash->idx, msg, mlen);
		hash->idx += mlen;
		return RERR_OK;
	}
	memcpy (hash->buf + hash->idx, msg, 64-hash->idx);
	ret = doround (&hash->a0, &hash->b0, &hash->c0, &hash->d0, (uint32_t*)hash->buf);
	if (!RERR_ISOK(ret)) return ret;
	mlen-=64-hash->idx;
	msg+=64-hash->idx;
	hash->msglen += 64;
	hash->idx = 0;
	while (mlen >= 64) {
		memcpy (hash->buf, msg, 64);
		ret = doround (&hash->a0, &hash->b0, &hash->c0, &hash->d0, (uint32_t*)hash->buf);
		if (!RERR_ISOK(ret)) return ret;
		msg += 64;
		mlen -= 64;
		hash->msglen += 64;
	}
	if (mlen > 0) {
		memcpy (hash->buf, msg, mlen);
		hash->idx = mlen;
	}
	return RERR_OK;
}

int
md5sum_get (out, hash, flags)
	char				*out;
	struct md5sum	*hash;
	int				flags;
{
	int	i, ret;
	char	buf[16];

	if (!out || !hash) return RERR_PARAM;

	hash->msglen += hash->idx;
	hash->msglen *= 8;
	hash->buf[hash->idx] = 0x80;
	hash->idx++;
	if (hash->idx > 56) {
		for (i=hash->idx; i < 64; i++) hash->buf[i]=0;
		ret = doround (&hash->a0, &hash->b0, &hash->c0, &hash->d0, (uint32_t*)hash->buf);
		if (!RERR_ISOK(ret)) return ret;
		hash->idx = 0;
	}
	for (i=hash->idx; i < 56; i++) hash->buf[i]=0;
	ret = bo_cv64 ((uint64_t*)(hash->buf+56), hash->msglen, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = doround (&hash->a0, &hash->b0, &hash->c0, &hash->d0, (uint32_t*)hash->buf);
	if (!RERR_ISOK(ret)) return ret;
	/* copy hash */
	ret = bo_cv32 (((uint32_t*)buf)+0, hash->a0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (((uint32_t*)buf)+1, hash->b0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (((uint32_t*)buf)+2, hash->c0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (((uint32_t*)buf)+3, hash->d0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;

	return tenc_encode (out, buf, 16, flags|TENC_F_NONL);
}

static
int
docalc (hash, msg, mlen)
	char			*hash;
	const char	*msg;
	size_t		mlen;
{
	char		buf[64];
	uint32_t a0 = 0x67452301U,
				b0 = 0xEFCDAB89U,
				c0 = 0x98BADCFEU,
				d0 = 0x10325476U;
	uint64_t	msglen = mlen * 8;
	int		i, ret;

	if (!hash || (!msg && mlen > 0)) return RERR_PARAM;
	if (!msg) msg="";

	while (mlen >= 64) {
		memcpy (buf, msg, 64);
		doround (&a0, &b0, &c0, &d0, (uint32_t*)buf);
		msg += 64;
		mlen -= 64;
	}
	memcpy (buf, msg, mlen);
	buf[mlen] = 0x80;
	for (i=mlen+1; i < 56; i++) buf[i]=0;
	if (i > 56) {
		for (; i < 64; i++) buf[i]=0;
		doround (&a0, &b0, &c0, &d0, (uint32_t*)buf);
		mlen=0;
		for (i=0; i < 56; i++) buf[i]=0;
	}
	ret = bo_cv64 ((uint64_t*)(buf+56), msglen, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = doround (&a0, &b0, &c0, &d0, (uint32_t*)buf);
	if (!RERR_ISOK(ret)) return ret;

	/* copy hash */
	ret = bo_cv32 (((uint32_t*)hash)+0, a0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (((uint32_t*)hash)+1, b0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (((uint32_t*)hash)+2, c0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv32 (((uint32_t*)hash)+3, d0, BO_MK_LE);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
doround (a0, b0, c0, d0, msg)
	uint32_t	*a0, *b0, *c0, *d0, *msg;
{
	uint32_t	a, b, c, d, f;
	int		i, g, n;
	uint32_t	t1, t2;
	int		ret;

	if (!a0 || !b0 || !c0 || !d0 || !msg) return RERR_PARAM;
	a = *a0; b = *b0; c = *c0; d = *d0;

	/* convert to little endian */
	for (i=0; i<16; i++) {
		ret = bo_cv32 (msg+i, msg[i], BO_MK_LE);
		if (!RERR_ISOK(ret)) return ret;
	}

#define LEFTROT32(a,n) (((a)<<(n)) | (((uint32_t)a)>>(32-(n))))
#define ROUND	do { \
		t1 = d;\
		d = c;\
		c = b;\
		t2 = a + f + md5_k[i] + msg[g]; \
		n = md5_s[i]; \
		b += LEFTROT32 (t2, n);\
		a = t1;\
	} while (0)
	for (i=0; i<16; i++) {
		/* f = (b & c) | (~b & d); */
		f = d ^ (b & (c ^ d));
		g = i;
		ROUND;
	}
	for (; i<32; i++) {
		/* f = (b & d) | (c & ~d); */
		f =  c ^ (d & (b ^ c));
		g = (5*i+1) & 0x0f;
		ROUND;
	}
	for (; i<48; i++) {
		f = b ^ c ^ d;
		g = (3*i + 5) & 0x0f;
		ROUND;
	}
	for (; i<64; i++) {
		f = c ^ (b | ~d);
		g = (7 * i) & 0x0f;
		ROUND;
	}

	*a0 += a;
	*b0 += b;
	*c0 += c;
	*d0 += d;

	return RERR_OK;
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
