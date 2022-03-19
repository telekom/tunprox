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
#include <stdint.h>
#include <errno.h>

#include <fr/base.h>
#include "multistream.h"







int
mstr_check (msg, msglen, prot)
	char	*msg;
	int	msglen, prot;
{
	int		isle, bo, ret;
	uint16_t	len, hcrc, chcrc;
	int		mlen, plen, head, foot;
	uint32_t	magic, fcrc, cfcrc;
	char		buf[12];

	if (!msg || msglen < 0) return RERR_PARAM;
	if (!MSTR_PROT_VALID(prot)) return RERR_PARAM;
	isle = MSTR_PROT_ISLE(prot);
	prot = MSTR_GETPROT(prot);
	if (isle) {
		bo = BO_FROM_LE;
	} else {
		bo = BO_FROM_BE;
	}
	/* check length */
	switch (prot) {
	case MSTR_PROT_0:
		if (msglen < 4) return RERR_INVALID_LEN;
		len = *(uint16_t*)(msg+2);
		head = 4;
		foot = 0;
		break;
	case MSTR_PROT_1:
		if (msglen < 16) return RERR_INVALID_LEN;
		len = *(uint16_t*)(msg+10);
		head = 12;
		foot = 4;
		/* check magic */
		magic = *(uint32_t*)msg;
		if (magic == MSTR_PROT_MAGIC) {
			bo = 0;
		} else if (magic == MSTR_PROT_REVMAGIC) {
			bo = BO_MK_SWAP;
		} else {
			return RERR_INVALID_SIGNATURE;
		}
		break;
	default:
		return RERR_PARAM;
	}
	ret = bo_cv16 (&len, len, bo);
	if (!RERR_ISOK(ret)) return ret;
	mlen = (int)(unsigned)len;
	plen = mlen % 4;
	if (plen) plen = 4 - plen;
	if (head + foot + mlen + plen != msglen) return RERR_INVALID_LEN;

	/* check crc */
	switch (prot) {
	case MSTR_PROT_1:
		hcrc = *(uint16_t*)(msg+6);
		ret = bo_cv16 (&hcrc, hcrc, bo);
		if (!RERR_ISOK(ret)) return ret;
		fcrc = *(uint32_t*)(msg+(msglen-4));
		ret = bo_cv32 (&fcrc, fcrc, bo);
		if (!RERR_ISOK(ret)) return ret;
		memcpy (buf, msg, head);
		*(uint16_t*)(buf+6) = 0;
		ret = frcrc16 (&chcrc, buf, head);
		if (!RERR_ISOK(ret)) return ret;
		if (chcrc != hcrc) return RERR_CHKSUM;
		ret = frcrc32 (&cfcrc, msg, msglen-4);
		if (!RERR_ISOK(ret)) return ret;
		if (cfcrc != fcrc) return RERR_CHKSUM;
		break;
	}

	/* count is not checked here */
	return RERR_OK;
}


int
mstr_getcount (msg, prot)
	char	*msg;
	int	prot;
{
	int		isle, ret;
	uint16_t	count;

	if (!msg) return RERR_PARAM;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		return 0;
	case MSTR_PROT_1:
		count = *(uint16_t*)(msg+4);
		isle = mstr_isle (msg, prot);
		if (!RERR_ISOK(isle)) return isle;
		ret = bo_cv16 (&count, count, isle?BO_FROM_LE:BO_FROM_BE);
		if (!RERR_ISOK(ret)) return ret;
		return (int)(unsigned)count;
	default:
		return RERR_PARAM;
	}
	return RERR_INTERNAL;	/* should never reach here */
}


int
mstr_isle (msg, prot)
	char	*msg;
	int	prot;
{
	uint32_t	magic;

	if (!msg) return RERR_PARAM;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		return MSTR_PROT_ISLE (prot);
	case MSTR_PROT_1:
		magic = *(uint32_t*)msg;
		if (magic == MSTR_PROT_MAGIC) {
#ifdef BO_HAS_BE
			return 0;
#elif defined BO_HAS_LE
			return 1;
#else
			return RERR_NOT_SUPPORTED;
#endif
		} else if (magic == MSTR_PROT_REVMAGIC) {
#ifdef BO_HAS_BE
			return 1;
#elif defined BO_HAS_LE
			return 0;
#else
			return RERR_NOT_SUPPORTED;
#endif
		} else {
			return RERR_INVALID_SIGNATURE;
		}
		break;
	default:
		return RERR_PARAM;
	}
	return RERR_INTERNAL;	/* should never reach here */
}

int
mstr_getmsglen (msg, prot)
	char	*msg;
	int	prot;
{
	int		isle, ret;
	uint16_t	len;

	if (!msg) return RERR_PARAM;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		len = *(uint16_t*)(msg+2);
		break;
	case MSTR_PROT_1:
		len = *(uint16_t*)(msg+10);
		break;
	default:
		return RERR_PARAM;
	}
	isle = mstr_isle (msg, prot);
	if (!RERR_ISOK(isle)) return isle;
	ret = bo_cv16 (&len, len, isle?BO_FROM_LE:BO_FROM_BE);
	if (!RERR_ISOK(ret)) return ret;
	return (int)(unsigned)len;
}


char*
mstr_getmsg (msg, prot)
	char	*msg;
	int	prot;
{
	if (!msg) return NULL;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		return msg+4;
		break;
	case MSTR_PROT_1:
		return msg+12;
		break;
	}
	return NULL;
}


int
mstr_getchan (msg, prot)
	char	*msg;
	int	prot;
{
	int		isle, ret;
	uint16_t	chan;

	if (!msg) return RERR_PARAM;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		chan = *(uint16_t*)msg;
		break;
	case MSTR_PROT_1:
		chan = *(uint16_t*)(msg+8);
		break;
	default:
		return RERR_PARAM;
	}
	isle = mstr_isle (msg, prot);
	if (!RERR_ISOK(isle)) return isle;
	ret = bo_cv16 (&chan, chan, isle?BO_FROM_LE:BO_FROM_BE);
	if (!RERR_ISOK(ret)) return ret;
	return (int)(unsigned)chan;
}


int
mstr_guessprot (msg, msglen)
	char	*msg;
	int	msglen;
{
	int		ret, prot, isle, guess;
	uint32_t	magic;
	char		buf[12];
	uint16_t	hcrc, chcrc, len, revlen, chan, revchan;

	if (!msg || msglen < 4) return RERR_PARAM;
	magic = *(uint32_t*)msg;
	if (magic == MSTR_PROT_MAGIC || magic == MSTR_PROT_REVMAGIC) {
		prot = MSTR_PROT_1;
#ifdef BO_HAS_BE
		isle = (magic == MSTR_PROT_REVMAGIC);
#elif defined BO_HAS_LE
		isle = (magic == MSTR_PROT_MAGIC);
#else
		return RERR_NOT_SUPPORTED;
#endif
		if (msglen >= 12) {
			memcpy (buf, msg, 12);
			hcrc = *(uint16_t*)(buf+6);
			*(uint16_t*)(buf+6) = 0;
			ret = bo_cv16 (&hcrc, hcrc, isle?BO_FROM_LE:BO_FROM_BE);
			if (!RERR_ISOK(ret)) return ret;
			ret = frcrc16 (&chcrc, buf, 12);
			if (!RERR_ISOK(ret)) return ret;
			if (hcrc != chcrc) prot = MSTR_PROT_0;
		}
	} else {
		prot = MSTR_PROT_0;
	}
	if (prot == MSTR_PROT_0) {
		/* do some heuristics to check endiness */
		chan = *(uint16_t*)msg;
		len = *(uint16_t*)(msg+2);
		ret = bo_cv16 (&revchan, chan, BO_MK_SWAP);
		if (!RERR_ISOK(ret)) return ret;
		ret = bo_cv16 (&revlen, len, BO_MK_SWAP);
		if (!RERR_ISOK(ret)) return ret;
		guess = (int)(unsigned)chan - (int)(unsigned)revchan;
		guess *= 4;
		guess += (int)(unsigned)len - (int)(unsigned)revlen;
		/* if > 0 - need to swap */
#ifdef BO_HAS_BE
		isle = guess > 0;
#elif defined BO_HAS_LE
		isle = !(guess > 0);
#else
		return RERR_NOT_SUPPORTED;
#endif
	}
	if (isle) prot |= MSTR_PROT_F_LE;
	return ret;
}



int
mstr_create (omsg, omsglen, msg, msglen, prot, stream, cnt)
	char		**omsg, *msg;
	int		*omsglen, msglen, prot, stream;
	uint16_t	cnt;
{
	char	*buf;
	int	buflen, ret;

	if (!omsg) return RERR_PARAM;
	buflen = mstr_getsendmsglen (msglen, prot);
	if (!RERR_ISOK(buflen)) return buflen;
	buf = malloc (buflen);
	if (!buf) return RERR_NOMEM;
	ret = mstr_create2 (buf, omsglen, msg, msglen, prot, stream, cnt);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	*omsg = buf;
	return RERR_OK;
}

int
mstr_getsendmsglen (msglen, prot)
	int	msglen, prot;
{
	int	buflen, plen;

	if (msglen < 0) return RERR_PARAM;
	if (msglen > 65535) return RERR_OUTOFRANGE;
	plen = msglen % 4;
	if (plen) plen = 4 - plen;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		buflen = 4 + msglen + plen;
		break;
	case MSTR_PROT_1:
		buflen = 12 + msglen + plen + 4;
		break;
	default:
		return RERR_INVALID_PROT;
	}
	return buflen;
}

int
mstr_create3 (omsg, msg, msglen, prot, stream, cnt)
	char		*omsg, *msg;
	int		msglen, prot, stream;
	uint16_t	cnt;
{
	return mstr_create2 (omsg, NULL, msg, msglen, prot, stream, cnt);
}

int
mstr_create2 (buf, omsglen, msg, msglen, prot, stream, cnt)
	char		*buf, *msg;
	int		*omsglen, msglen, prot, stream;
	uint16_t	cnt;
{
	int		plen, bo, ret;
	uint32_t	magic, fcrc;
	uint16_t	hcrc, len, chan;

	if (!buf || !msg || msglen < 0 || stream < 0) return RERR_PARAM;
	if (stream > 65535 || msglen > 65535) return RERR_OUTOFRANGE;
	if (omsglen) {
		ret = *omsglen = mstr_getsendmsglen (msglen, prot);
		if (!RERR_ISOK(ret)) return ret;
	}
	plen = msglen % 4;
	if (plen) plen = 4 - plen;
	if (MSTR_PROT_ISLE (prot)) {
		bo = BO_TO_LE;
	} else {
		bo = BO_TO_BE;
	}
	ret = bo_cv32 (&magic, MSTR_PROT_MAGIC, bo);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv16 (&cnt, cnt, bo);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv16 (&chan, (uint16_t)stream, bo);
	if (!RERR_ISOK(ret)) return ret;
	ret = bo_cv16 (&len, (uint16_t)msglen, bo);
	if (!RERR_ISOK(ret)) return ret;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		*(uint16_t*)buf = chan;
		*(uint16_t*)(buf+2) = len;
		memcpy (buf+4, msg, msglen);
		if (plen) bzero (buf+4+msglen, plen);
		break;
	case MSTR_PROT_1:
		*(uint32_t*)buf = magic;
		*(uint16_t*)(buf+4) = cnt;
		*(uint16_t*)(buf+6) = (uint16_t)0;
		*(uint16_t*)(buf+8) = chan;
		*(uint16_t*)(buf+10) = len;
		memcpy (buf+12, msg, msglen);
		if (plen) bzero (buf+12+msglen, plen);
		ret = frcrc16 (&hcrc, buf, 12);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		ret = bo_cv16 (&hcrc, hcrc, bo);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		*(uint16_t*)(buf+6) = hcrc;
		ret = frcrc32 (&fcrc, buf, 12+msglen+plen);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		ret = bo_cv32 (&fcrc, fcrc, bo);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		*(uint32_t*)(buf+12+msglen+plen) = fcrc;
		break;
	default:
		return RERR_INVALID_PROT;
	}
	return RERR_OK;
}



int
mstr_termprot0be (data, dlen, tlen)
	char	*data;
	int	dlen, *tlen;
{
	uint16_t	len;
	int		ret, mlen, plen;

	if (!data || dlen < 0) return RERR_PARAM;
	if (tlen) *tlen = 0;
	if (dlen < 4) return 0;
	len = *(uint16_t*)(data+2);
	ret = bo_cv16 (&len, len, BO_FROM_BE);
	if (!RERR_ISOK(ret)) return ret;
	mlen = (int)(unsigned)len;
	plen = mlen % 4;
	if (plen) plen = 4 - plen;
	mlen += 4 + plen;
	if (dlen < mlen) return 0;
	return mlen;
}

int
mstr_termprot0le (data, dlen, tlen)
	char	*data;
	int	dlen, *tlen;
{
	uint16_t	len;
	int		ret, mlen, plen;

	if (!data || dlen < 0) return RERR_PARAM;
	if (tlen) *tlen = 0;
	if (dlen < 4) return 0;
	len = *(uint16_t*)(data+2);
	ret = bo_cv16 (&len, len, BO_FROM_LE);
	if (!RERR_ISOK(ret)) return ret;
	mlen = (int)(unsigned)len;
	plen = mlen % 4;
	if (plen) plen = 4 - plen;
	mlen += 4 + plen;
	if (dlen < mlen) return 0;
	return mlen;
}


int
mstr_termprot1 (data, dlen, tlen)
	char	*data;
	int	dlen, *tlen;
{
	int	len, plen, i, ret;
	char	*s;

	if (!data || dlen < 0) return RERR_PARAM;
	if (tlen) *tlen = 0;
	if (dlen < 16) return 0;
	/* find beginning */
	for (i=0, s=data; i+12<=dlen; i++, s++) {
		if ((*(uint32_t*)s == MSTR_PROT_MAGIC) || 
					(*(uint32_t*)s == MSTR_PROT_REVMAGIC)) {
			ret = mstr_checkheader (data, dlen, MSTR_PROT_1);
			if (RERR_ISOK(ret)) goto found;
		}
	}
	return 0;
found:
	if (s != data) return s - data;		/* rubbish, needs to be thrown away by callee */
	len = mstr_getmsglen (data, MSTR_PROT_1);
	plen = len % 4;
	if (plen) plen = 4 - plen;
	len += plen + 16;
	if (dlen < len) return 0;
	return len;
}


int
mstr_checkheader (data, dlen, prot)
	const char	*data;
	int			dlen, prot;
{
	uint32_t	magic;
	char		buf[12];
	uint16_t	hcrc, chcrc;
	int		ret;

	if (!data || dlen < 0) return RERR_PARAM;
	switch (MSTR_GETPROT(prot)) {
	case MSTR_PROT_0:
		if (dlen < 4) return RERR_INVALID_LEN;
		break;
	case MSTR_PROT_1:
		if (dlen < 12) return RERR_INVALID_LEN;
		magic = *(uint32_t*)data;
		if (magic != MSTR_PROT_MAGIC && magic != MSTR_PROT_REVMAGIC) {
			return RERR_INVALID_SIGNATURE;
		}
		memcpy (buf, data, 12);
		hcrc = *(uint16_t*)(buf + 6);
		*(uint16_t*)(buf+6) = (uint16_t)0;
		ret = frcrc16 (&chcrc, buf, 12);
		if (!RERR_ISOK(ret)) return ret;
		if (magic == MSTR_PROT_REVMAGIC) {
			ret = bo_cv16 (&hcrc, hcrc, BO_MK_SWAP);
			if (!RERR_ISOK(ret)) return ret;
		}
		if (hcrc != chcrc) return RERR_CHKSUM;
		break;
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}


#if 0
#define MSTR_PROT2_MAX16	((uint16_t)0xfffeUL)

uint16_t
mstr_encode_p2 (num)
	uint16_t	num;
{
	uint16_t	x;

	x = num & 0x00ff;
	x += ((num & 0xff00) >> 8) * 2;
	x += ((x & 0xff00) >> 8) * 2;
	x += 0x0101;
	num += x;
	if (num & 0xff == 0xff) num++;
	if (num & 0xff == 0) num++;
	return num;
}


uint16_t
mstr_decode_p2 (num)
	uint16_t	num;
{
	num -= 0x0101;
	num -= ((num & 0xff00) >> 8) * 2;
	return num;
}
#endif










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
