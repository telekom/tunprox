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
#include <ctype.h>
#include <sys/types.h>



#include "txtenc.h"
#include "errors.h"




static char	base64map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklm"
							"nopqrstuvwxyz0123456789+/";

int
tenc_b64encode_alloc (out, in, inlen, flags)
	char			**out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	char	*obuf;
	int	ret;

	if (!out || !in) return RERR_PARAM;
	obuf = malloc (inlen * 3 / 2 + 3);
	if (!obuf) return RERR_NOMEM;
	*out = obuf;
	ret = tenc_b64encode (obuf, in, inlen, flags);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		*out = NULL;
		return ret;
	}
	return RERR_OK;
}


int
tenc_b64encode (out, in, inlen, flags)
	char			*out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	char	*s, *so;
	int	i, nl;

	if (!out || !in) return RERR_PARAM;
	nl = !(flags & TENC_F_NONL);

	for (i=0, s=(char*)in, so=out; i<(ssize_t)inlen/3; i++, s+=3, so+=4) {
		if (nl && i%18==0 && i) {
			*so = '\n';
			so++;
		}
		so[0] = base64map[(s[0]&0xfc)>>2];
		so[1] = base64map[((s[0]&0x03)<<4)|((s[1]&0xf0)>>4)];
		so[2] = base64map[((s[1]&0x0f)<<2)|((s[2]&0xc0)>>6)];
		so[3] = base64map[s[2]&0x3f];
	}
	if ((inlen%3)==0) {
		if (nl) {
			*so = '\n';
			so++;
		}
		*so = 0;
		return RERR_OK;
	} else if (nl && i%18==0 && i) {
		*so = '\n';
		so++;
	}
	i=inlen%3;
	so[0] = base64map[(s[0]&0xfc)>>2];
	if (i==1) {
		so[1] = base64map[(s[0]&0x03)<<4];
		if (!(flags & TENC_F_NOEQ)) {
			so[2] = '=';
			so[3] = '=';
			so+=4;
		} else {
			so+=2;
		}
	} else {	/* i==2 */
		so[1] = base64map[((s[0]&0x03)<<4)|((s[1]&0xf0)>>4)];
		so[2] = base64map[(s[1]&0x0f)<<2];
		if (!(flags & TENC_F_NOEQ)) {
			so[3] = '=';
			so+=4;
		} else {
			so+=3;
		}
	}
	if (nl) {
		*so = '\n';
		so++;
	}
	*so = 0;

	return RERR_OK;
}

static const unsigned   rb64table[] = {/*43*/ 62, 0, 0, 0, /*47*/ 63, 52, 53,
							54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
							0, /*A*/ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
							12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
							24, 25, 0, 0, 0, 0, 0, 0, /*97*/ 26, 27, 28, 29,
							30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
							42, 43, 44, 45, 46, 47, 48, 49, 50, 51};
#define RB64TAB(c)	(rb64table[(c)-43])
#define VALID_B64(c) (((c)>='A'&&(c)<='Z')||((c)>='a'&&(c)<='z')||\
						((c)>='0'&&(c)<='9')||(c)=='+'||(c)=='/'||(c)=='=')
#define VALID_B64STR(s) (VALID_B64((s)[0])&&VALID_B64((s)[1])&&\
						VALID_B64((s)[2])&&VALID_B64((s)[3]))

int
tenc_b64decode_alloc (out, olen, in, flags)
	char			**out;
	size_t		*olen;
	const char	*in;
	int			flags;
{
	char	*obuf;
	int	ret;

	if (!out || !in) return RERR_PARAM;
	obuf = malloc ((strlen (in) * 3) / 4 + 2);
	if (!obuf) return RERR_NOMEM;
	*out = obuf;
	ret = tenc_b64decode (obuf, olen, in, flags);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		*out = NULL;
		return ret;
	}
	return RERR_OK;
}


int
tenc_b64decode (out, olen, in, flags)
	char			*out;
	size_t		*olen;
	const char	*in;
	int			flags;
{
	char		*s, *so;
	size_t	i;

	if (!in || !out) return RERR_PARAM;

	if ((flags & TENC_F_OLEN) && !olen) return RERR_PARAM;
	for (i=0,s=(char*)in,so=out; *s; i+=3,s+=4,so+=3) {
		if ((flags & TENC_F_OLEN) && (size_t)(so - out + 2) >= *olen) break;
		for (; isspace(*s); s++);
		if (!*s) break;
		if (!VALID_B64STR(s)) return RERR_NOT_BASE64;
		so[0] = (RB64TAB(s[0]) << 2) | (RB64TAB(s[1]) >> 4);
		if (s[2] && (s[2] != '=')) {
			so[1] = ((RB64TAB(s[1]) & 0x0f) << 4) | (RB64TAB(s[2]) >> 2);
		} else {
			i++; so++;
			break;
		}
		if (s[3] && (s[3] != '=')) {
			so[2] = ((RB64TAB(s[2]) & 0x03) << 6) | RB64TAB(s[3]);
		} else {
			i+=2; so+=2;
			break;
		}
	}
	*so = 0;
	if (olen) *olen = i;

	return RERR_OK;
}


static const char b32map_smal[] = "abcdefghjklmnpqrstuvwxyz23456789";
static const char b32map_caps[] = "ABCDEFGHJKLMNPQRSTUVWXYZ23456789";
static const char b32map_old[] = "ABCDEFGHIJKLMNPOQRSTUVWXYZ012345";

int
tenc_b32encode_alloc (out, in, inlen, flags)
	char			**out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	char	*obuf;
	int	ret;

	if (!out || !in) return RERR_PARAM;
	*out = NULL;
	obuf = malloc (inlen * 2 + 3);
	if (!obuf) return RERR_NOMEM;
	ret = tenc_b32encode (obuf, in, inlen, flags);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		return ret;
	}
	*out = realloc (obuf, strlen (obuf)+1);
	if (!*out) *out = obuf;
	return RERR_OK;
}

int
tenc_b32encode (out, in, inlen, flags)
	char			*out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	char			*s, *so;
	size_t		i;
	int			j, nl;
	const char	*base32map;

	if (!in || !out) return RERR_PARAM;
	nl = !(flags & TENC_F_NONL);
	base32map = (flags & TENC_F_CAPS) ? b32map_caps : b32map_smal;
	if (flags & TENC_F_OLD32) base32map = b32map_old;

	for (i=0,s=(char*)in,so=out; i < inlen/5; i++, s+=5, so+=8) {
		if (nl && i%9==0 && i) {
			*so = '\n';
			so++;
		}
		so[0] = base32map[(s[0]&0xf8) >> 3];
		so[1] = base32map[((s[0]&0x07) << 2) | ((s[1]&0xc0) >> 6)];
		so[2] = base32map[(s[1]&0x3e) >> 1];
		so[3] = base32map[((s[1]&0x01) << 4) | ((s[2]&0xf0) >> 4)];
		so[4] = base32map[((s[2]&0x0f) << 1) | ((s[3]&0x80) >> 7)];
		so[5] = base32map[(s[3]&0x7c) >> 2];
		so[6] = base32map[((s[3]&0x03) << 3) | ((s[4]&0xe0) >> 5)];
		so[7] = base32map[s[4] & 0x1f];
	}
	if ((inlen%5)==0) {
		if (nl) {
			*so = '\n';
			so++;
		}
		*so = 0;
		return RERR_OK;
	} else if (nl && i%9==0 && i) {
		*so = '\n';
		so++;
	}
	*so = 0;
	j = inlen%5;
	so[0] = base32map[(s[0]&0xf8) >> 3];
	if (j==1) {
		so[1] = base32map[s[0] & 0x07];
		so+=2;
		goto finish;
	}
	so[1] = base32map[((s[0]&0x07) << 2) | ((s[1]&0xc0) >> 6)];
	so[2] = base32map[(s[1]&0x3e) >> 1];
	if (j==2) {
		so[3] = base32map[s[1] & 0x01];
		so+=4;
		goto finish;
	}
	so[3] = base32map[((s[1]&0x01) << 4) | ((s[2]&0xf0) >> 4)];
	if (j==3) {
		so[4] = base32map[s[2] & 0x0f];
		so+=5;
		goto finish;
	}
	so[4] = base32map[((s[2]&0x0f) << 1) | ((s[3]&0x80) >> 7)];
	so[5] = base32map[(s[3]&0x7c) >> 2];
	so[6] = base32map[s[3] & 0x03];
	so+=7;
finish:
#if 0
	so[0] = '0';
	so[1] = '0' + j;
	so+=2;
#endif
	if (nl) {
		*so = '\n';
		so++;
	}
	*so = 0;

	return RERR_OK;
}



int
tenc_b32decode_alloc (out, olen, in, flags)
	char			**out;
	size_t		*olen;
	const char	*in;
	int			flags;
{
	char	*obuf;
	int	ret;

	if (!out || !in) return RERR_PARAM;
	obuf = malloc ((strlen (in) * 5) / 8 + 2);
	if (!obuf) return RERR_NOMEM;
	*out = obuf;
	ret = tenc_b32decode (obuf, olen, in, flags);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		*out = NULL;
		return ret;
	}
	return RERR_OK;
}


int
tenc_b32decode (out, olen, in, flags)
	char			*out;
	size_t		*olen;
	const char	*in;
	int			flags;
{
	return RERR_NOT_SUPPORTED;
}


int
tenc_hexencode_alloc (out, in, inlen, flags)
	char			**out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	char	*obuf;
	int	ret;

	if (!out || !in) return RERR_PARAM;
	*out = NULL;
	obuf = malloc (inlen * 2 + 1);
	if (!obuf) return RERR_NOMEM;
	ret = tenc_hexencode (obuf, in, inlen, flags);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		return ret;
	}
	*out = obuf;
	return RERR_OK;
}


#if 0
# define D2HC(n,a)	(((n)<0||(n)>15)?'0':(((n)<10)?('0'+(n)):\
							((a)+(n)-10)))
#else
# define D2HC(n,a)	(((n)<10)?('0'+(n)):((a)+(n)-10))
#endif
#define D2HL(n,a)	(D2HC(((n)&0x0f),(a)))
#define D2HH(n,a)	(D2HC((((n)&0xf0)>>4),(a)))

int
tenc_hexencode (out, in, inlen, flags)
	char			*out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	char		*s, *so;
	size_t	i;
	char		a;
	int		nl;

	a = (flags & TENC_F_CAPS) ? 'A' : 'a';
	nl = !(flags & TENC_F_NONL);

	for (i=0, s=(char*)in, so=out; i<inlen; i++, s++, so+=2) {
		if (nl && i%36==0 && i) {
			*so = '\n';
			so++;
		}
		so[0] = D2HH (*s, a);
		so[1] = D2HL (*s, a);
	}
	if (nl && i) {
		*so = '\n';
		so++;
	}
	*so = 0;
	return RERR_OK;
}



int
tenc_hexdecode_alloc (out, olen, in, flags)
	char			**out;
	size_t		*olen;
	const char	*in;
	int			flags;
{
	char	*obuf;
	int	ret;

	if (!out || !in) return RERR_PARAM;
	obuf = malloc (strlen (in) / 2 + 1);
	if (!obuf) return RERR_NOMEM;
	*out = obuf;
	ret = tenc_hexdecode (obuf, olen, in, flags);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		*out = NULL;
		return ret;
	}
	return RERR_OK;
}


#define H2DC(c)	((((c)>='0')&&((c)<='9'))?((c)-'0'):\
						((((c)>='a')&&((c)<='f'))?((c)-'a'+10):\
						((((c)>='A')&&((c)<='F'))?((c)-'A'+10):\
						(0))))
#define H2D2(a,b)	((H2DC(a)<<4)|(H2DC(b)))
#define ISHEX(c)	((((c)>='0')&&((c)<='9'))||\
						 (((c)>='a')&&((c)<='f'))||\
						 (((c)>='A')&&((c)<='F')))
int
tenc_hexdecode (out, olen, in, flags)
	char			*out;
	size_t		*olen;
	const char	*in;
	int			flags;
{
	char		*s, *so, a, b;

	if (!out || !in) return RERR_PARAM;
	if ((flags & TENC_F_OLEN)) {
		if (!olen) return RERR_PARAM;
		if (*olen < 1) return RERR_OK;
	}
	for (s=(char*)in,so=out; *s; s++, so++) {
		if ((flags & TENC_F_OLEN) && (size_t)(so - out) >= *olen) break;
		for (; isspace(*s); s++);
		if (!*s) break;
		a = *s;
		for (s++; *s && isspace(*s); s++);
		if (!*s) break;
		b = *s;
		if (!ISHEX(a) || !ISHEX(b)) return RERR_INVALID_VAL;
		*so = H2D2(a,b);
	}
	if ((flags & TENC_F_OLEN) && (size_t)(so - out) >= *olen) so--;
	*so = 0;
	if (olen) *olen = (so - out);
	return RERR_OK;
}


int
tenc_encode_alloc (out, in, inlen, flags)
	char			**out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	if (!in || !out) return RERR_PARAM;

	switch (flags & TENC_M_FMT) {
	case TENC_FMT_BIN:
		*out = malloc (inlen+1);
		if (!*out) return RERR_NOMEM;
		memcpy (*out, in, inlen);
		return RERR_OK;
	case TENC_FMT_HEX:
		return tenc_hexencode_alloc (out, in, inlen, flags);
	case TENC_FMT_B32:
		return tenc_b32encode_alloc (out, in, inlen, flags);
	case TENC_FMT_B64:
		return tenc_b64encode_alloc (out, in, inlen, flags);
	default:
		return RERR_INVALID_FORMAT;
	}
	return RERR_INTERNAL;
}



int
tenc_encode (out, in, inlen, flags)
	char			*out;
	const char	*in;
	size_t		inlen;
	int			flags;
{
	if (!in || !out) return RERR_PARAM;

	switch (flags & TENC_M_FMT) {
	case TENC_FMT_BIN:
		memcpy (out, in, inlen);
		return RERR_OK;
	case TENC_FMT_HEX:
		return tenc_hexencode (out, in, inlen, flags);
	case TENC_FMT_B32:
		return tenc_b32encode (out, in, inlen, flags);
	case TENC_FMT_B64:
		return tenc_b64encode (out, in, inlen, flags);
	default:
		return RERR_INVALID_FORMAT;
	}
	return RERR_INTERNAL;
}


int
tenc_decode_alloc (out, olen, in, inlen, flags)
	char			**out;
	size_t		*olen, inlen;
	const char	*in;
	int			flags;
{
	if (!in || !out) return RERR_PARAM;

	switch (flags & TENC_M_FMT) {
	case TENC_FMT_BIN:
		*out = malloc (inlen+1);
		if (!*out) return RERR_NOMEM;
		memcpy (*out, in, inlen);
		if (olen) *olen = inlen;
		return RERR_OK;
	case TENC_FMT_HEX:
		return tenc_hexdecode_alloc (out, olen, in, flags);
	case TENC_FMT_B32:
		return tenc_b32decode_alloc (out, olen, in, flags);
	case TENC_FMT_B64:
		return tenc_b64decode_alloc (out, olen, in, flags);
	default:
		return RERR_INVALID_FORMAT;
	}
	return RERR_INTERNAL;
}


int
tenc_decode (out, olen, in, inlen, flags)
	char			*out;
	size_t		*olen, inlen;
	const char	*in;
	int			flags;
{
	if (!in || !out) return RERR_PARAM;

	switch (flags & TENC_M_FMT) {
	case TENC_FMT_BIN:
		memcpy (out, in, inlen);
		if (olen) *olen = inlen;
		return RERR_OK;
	case TENC_FMT_HEX:
		return tenc_hexdecode (out, olen, in, flags);
	case TENC_FMT_B32:
		return tenc_b32decode (out, olen, in, flags);
	case TENC_FMT_B64:
		return tenc_b64decode (out, olen, in, flags);
	default:
		return RERR_INVALID_FORMAT;
	}
	return RERR_INTERNAL;
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
