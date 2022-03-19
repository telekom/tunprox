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
#include <stdint.h>


#include "errors.h"
#include "bo.h"
#include "utf8.h"

#if 0
static int envcase=0;
static int hasenvcase=0;
static int checkenvcase ();
#define ENVCASE	(hasenvcase?envcase:checkenvcase())

static int locase=0;
static int haslocase=0;
static int checklocase ();
#define LOCASE		(haslocase?locase:checklocase())

#define ICASE(f)	(((f)&FU8C_F_ICASE)?(1):\
						(((f)&FU8C_F_LOCASE)?LOCASE:\
						(((f)&FU8C_F_ENVCASE)?ENVCASE:\
						(0))))

#define MARKZEROBYTE(v) (((v)-0x0101010101010101ULL) & ~(v) & \
									0x8080808080808080ULL);
#define PROPHIBIT(x) { \
		(x) |= (x) >> 1; \
		(x) |= (x) >> 2; \
		(x) |= (x) >> 4; \
	}

#if defined BO_HAS_BE
		/* for big endian propagate byte to the right */
# define PROPBYTE(x) {  \
		(x) |= (x) >> 8;  \
		(x) |= (x) >> 16; \
		(x) |= (x) >> 32; \
	}
#elif defined BO_HAS_LE
		/* for little endian make the same but to the left */
# define PROPBYTE(x) {  \
		(x) |= (x) << 8;  \
		(x) |= (x) << 16; \
		(x) |= (x) << 32; \
	}
#else
# warning unknown byte order
#endif

#endif


ssize_t
fu8_blen (str)
	const char	*str;
{
	if (!str) return RERR_PARAM;
	return strlen (str);
}


ssize_t
fu8_clen (str)
	const char	*str;
{
	uint64_t	*p;
	uint64_t	v, vsum, x;
	int		i, n;
	ssize_t	sum;

	if (!str) return RERR_PARAM;
	/* first we need to calculate the first up to 7 bytes, in case the string
	 * does not start on 8-byte boundary. This not only speeds up the 
	 * following calculation. if not done, the following even might crash
	 * when the string terminates in the page boundary, but we read over
	 * the boundary.
	 */
	n = (int)(((size_t)str) & 0x07UL);	/* identical to % 8 */
	if (n != 0) {
		/* we start reading n bytes left of the string, it is guaranteed to
		 * be in the same page, because a page always starts on a 8-byte
		 * boundary
		 */
		p = (uint64_t*)(str-n);
		v = *p;
#ifdef BO_HAS_BE
		/* set first n bytes to 0xff 
		 * this guarantees, that they don't start with 10... and are neither all 0
		 */
		v |= (((int64_t)(1LL))<<63) >> ((n<<3)-1);	/* 8*n-1 */
#elif defined BO_HAS_LE
		/* set last n bytes to 0xff */
		v |= ~((((int64_t)(1LL))<<63) >> (63 - (n<<3)));	/* 8*(8-n)-1 */
#else
		return RERR_NOT_SUPPORTED
#endif
		/* the first bytes now are counted, so we need to substract them,
		 * thus initialize sum with -n
		 */
		sum = -n;
	} else {		/* we are on 8-byte boundary */
		p=(uint64_t*)str;
		v = *p;
		sum = 0;
	}
	p++;
	while (1) {
		vsum = 0;
		for (i=255; i; i--, p++) {
			/* at beginning v is already set */
			/* check wether we have a 0 byte */
			if ((x=((v-0x0101010101010101ULL) & ~v & 0x8080808080808080ULL))) {
				goto lastword;
			}
			/* the following makes all bytes 0 that start with the bits 10...
			 * other bytes have at least one 1 in the first two bits
			 */
			v = (v & 0xc0c0c0c0c0c0c0c0ULL) ^ 0x8080808080808080ULL;
			/* the following makes the first bit 1 and all other 0 if
			 * one of the first two bits has a 1
			 * hence if the byte doesn't start with 10... the first bit
			 * will be set
			 */
			v = ((v & 0x4040404040404040ULL) << 1) | (v & 0x8080808080808080ULL);
			/* now shift seven bits to the right, so that the lower bit is
			 * 1 if the byte does not start with 10
			 */
			v >>= 7;
			/* now add to vsum */
			vsum += v;
			/* adjust v - for next round */
			v = *p;
		}
		/* now sum up all bytes in vsum */
		vsum = ((vsum & 0xff00ff00ff00ff00ULL) >> 8) 
				+ (vsum & 0x00ff00ff00ff00ffULL);
		vsum = ((vsum & 0xffff0000ffff0000ULL) >> 16)
				+ (vsum & 0x0000ffff0000ffffULL);
		vsum = ((vsum & 0xffffffff00000000ULL) >> 32)
				+ (vsum & 0x00000000ffffffffULL);
		/* add it to sum */
		sum += (ssize_t) vsum;
	}
lastword:
	/* set the first bit of each byte to 1 starting from the first 0 byte 
	 * to the right
	 */
#ifdef BO_HAS_BE
	x |= x >> 8;
	x |= x >> 16;
	x |= x >> 32;
#elif defined BO_HAS_LE
	/* for little endian make the same but to the left */
	x |= x << 8;
	x |= x << 16;
	x |= x << 32;
#else
	return RERR_NOT_SUPPORTED
#endif
	/* now invert */
	x = ~x;
	/* calculate the number of byte not starting with 10.. */
	v = (v & 0xc0c0c0c0c0c0c0c0ULL) ^ 0x8080808080808080ULL;
	v = ((v & 0x4040404040404040ULL) << 1) | (v & 0x8080808080808080ULL);
	/* now and the two above - so each byte in the string (not after
	 * the 0 byte) that does not start with 10.. has a 1 bit in
	 * its first position
	 */
	v &= x;
	/* now shift */
	v >>= 7;
	/* and add to vsum, we still can without overflowing */
	vsum += v;

	if (vsum) {
		/* now sum up all bytes in vsum */
		vsum = ((vsum & 0xff00ff00ff00ff00ULL) >> 8) 
				+ (vsum & 0x00ff00ff00ff00ffULL);
		vsum = ((vsum & 0xffff0000ffff0000ULL) >> 16)
				+ (vsum & 0x0000ffff0000ffffULL);
		vsum = ((vsum & 0xffffffff00000000ULL) >> 32)
				+ (vsum & 0x00000000ffffffffULL);
		/* add it to sum */
		sum += (ssize_t) vsum;
	}
	/* we are done - sum contains the string length in utf8 chars */
	return sum;
}


ssize_t
fu8_nthchar (str, nth)
	const char	*str;
	ssize_t		nth;
{
	uint64_t			*p;
	uint64_t			v, vsum;
	int				i, n;
	size_t			upto;
	unsigned char	*s;

	if (!str || nth < 0) return RERR_PARAM;
	if (nth < 8) {
		p = (uint64_t*)str;
		goto lastword;
	}
	/* first we need to calculate the first up to 7 bytes, in case the string
	 * does not start on 8-byte boundary. This not only speeds up the 
	 * following calculation. if not done, the following even might crash
	 * when the string terminates in the page boundary, but we read over
	 * the boundary.
	 */
	n = (int)(((size_t)str) & 0x07UL);	/* identical to % 8 */
	if (n != 0) {
		/* we start reading n bytes left of the string, it is guaranteed to
		 * be in the same page, because a page always starts on a 8-byte
		 * boundary
		 */
		p = (uint64_t*)(str-n);
		v = *p;
#ifdef BO_HAS_BE
		/* set first n bytes to 0xff 
		 * this guarantees, that they don't start with 10... and are neither all 0
		 */
		v |= (((int64_t)(1LL))<<63) >> ((n<<3)-1);	/* 8*n-1 */
#elif defined BO_HAS_LE
		/* set last n bytes to 0xff */
		v |= ~((((int64_t)(1LL))<<63) >> (63 - (n<<3)));	/* 8*(8-n)-1 */
#else
		return RERR_NOT_SUPPORTED
#endif
		/* the first bytes now are counted, so we need to substract them,
		 * thus initialize add n to nth;
		 */
		nth += n;
	} else {		/* we are on 8-byte boundary */
		p=(uint64_t*)str;
		v = *p;
	}
	p++;
	while (1) {
		vsum = 0;
		upto = ((size_t)nth) >> 3;
		if (!upto) goto lastword;
		if (upto > 255) upto = 255;
		for (i=(int)upto; i; i--, p++) {
			/* at beginning v is already set */
			/* check wether we have a 0 byte */
			if ((v-0x0101010101010101ULL) & ~v & 0x8080808080808080ULL) {
				/* when we are here, the string is terminated, and the nth char
				 * cannot be here
				 */
				return RERR_NOT_FOUND;
			}
			/* the following makes all bytes 0 that start with the bits 10...
			 * other bytes have at least one 1 in the first two bits
			 */
			v = (v & 0xc0c0c0c0c0c0c0c0ULL) ^ 0x8080808080808080ULL;
			/* the following makes the first bit 1 and all other 0 if
			 * one of the first two bits has a 1
			 * hence if the byte doesn't start with 10... the first bit
			 * will be set
			 */
			v = ((v & 0x4040404040404040ULL) << 1) | (v & 0x8080808080808080ULL);
			/* now shift seven bits to the right, so that the lower bit is
			 * 1 if the byte does not start with 10
			 */
			v >>= 7;
			/* now add to vsum */
			vsum += v;
			/* adjust v - for next round */
			v = *p;
		}
		/* now sum up all bytes in vsum */
		vsum = ((vsum & 0xff00ff00ff00ff00ULL) >> 8) 
				+ (vsum & 0x00ff00ff00ff00ffULL);
		vsum = ((vsum & 0xffff0000ffff0000ULL) >> 16)
				+ (vsum & 0x0000ffff0000ffffULL);
		vsum = ((vsum & 0xffffffff00000000ULL) >> 32)
				+ (vsum & 0x00000000ffffffffULL);
		/* subtract vsum from nth */
		nth -= (ssize_t) vsum;
	}
lastword:
	/* the rest we search in traditional way */
	for (s = (unsigned char*)p; nth && *s; s++) {
		if ((*s & 0xc0) != 0x80) nth--;
	}
	for (; *s; s++) {
		if ((*s & 0xc0) != 0x80) break;
	}
	if (!*s) return RERR_NOT_FOUND;

	return (ssize_t)((char*)s - str);
}


/* slow - needs to be changed */
int
fu8_verify (str, flags)
	const char	*str;
	int			flags;
{
	if (!str) return RERR_PARAM;
	for (; *str; str++) {
		if (!(*str & 0x80)) continue;
		if (!(*str & 0x40)) return RERR_UTF8;
		if (!(*str & 0x20)) {
			if ((str[1] & 0xc0) != 0x80) return RERR_UTF8;
			str++;
			continue;
		}
		if (!(*str & 0x10)) {
			if ((str[1] & 0xc0) != 0x80) return RERR_UTF8;
			if ((str[2] & 0xc0) != 0x80) return RERR_UTF8;
			str += 2;
			continue;
		}
		if ((unsigned char)*str > 0xf5) return RERR_UTF8;
		if ((str[1] & 0xc0) != 0x80) return RERR_UTF8;
		if ((str[2] & 0xc0) != 0x80) return RERR_UTF8;
		if ((str[3] & 0xc0) != 0x80) return RERR_UTF8;
		str += 3;
	}
	return RERR_OK;
}



#if 0

#define FU8C_F_NONE		0x000
#define FU8C_F_NONORM	0x001	/* strings are not normalized */
#define FU8C_F_ICASE		0x002	/* case insensitive */
#define FU8C_F_LOCASE	0x004	/* case as locales say */
#define FU8C_F_ENVCASE	0x008	/* use env var STRCASE */
#define FU8C_F_NOCOMP	0x010	/* do not honour compose chars - faster */
#define FU8C_F_LOCALE	0x020	/* use locales */
#define FU8C_F_MAXLEN	0x040	/* make use of max length (n) */
#define FU8C_F_LEN1		0x080	/* max len == fu8_blen(str1) */
#define FU8C_F_LEN2		0x100	/* max len == fu8_blen(str2) */
#define FU8C_F_CLEN		0x200	/* max len is in chars not bytes */
#define FU8C_F_CESU8		0x400	/* honour cesu8 strings */


int
fu8_cmp (str1, str2, n, flags)
	const char	*str1, *str2;
	int			flags;
	size_t		n;
{
	uint64_t	*p1, *p2;
	uint64_t	v1,v2,x1,x2;
	int		iscase;

	if (!str1 && !str2) return 0;
	if (!str1) return -1;
	if (!str2) return 1;
	iscase = ICASE(flags);
	p1 = (uint64_t*)(void*)(char*)str1;
	p2 = (uint64_t*)(void*)(char*)str2;
	for (; 1; p1++, p2++) {
		v1 = *p1; v2 = *p2;
		if (v1 == v2) {
			if (MARKZEROBYTE(v1)) {
				return 0;
			}
			continue;
		}
		/* check for last word */
		x1 = MARKZEROBYTE (v1);
		x2 = MARKZEROBYTE (v2);
		if (x1 || x2) goto lastword;
	}

lastword:
	if (x1) {
		PROPHIBIT (x1);
		PROPBYTE (x1);
	}
	if (x2) {
		PROPHIBIT (x2);
		PROPBYTE (x2);
	}
	/* invert */
	x1 = ~x1;
	x2 = ~x2;
	/* and with v */
	v1 &= x1;
	v2 &= x2;
	if (flags & FU8C_F_LEN1) {
		v2 &= x1;
	}
	if (flags & FU8C_F_LEN2) {
		v1 &= x2;
	}
	if (v1 == v2) return 0;
}













/* *********************
 * helper functions
 * *********************/

static
int
checkenvcase ()
{
	char	*s;

	s = getenv ("STRCASE");
	if (s) {
		sswitch (s) {
		sicase ("ignore")
		sicase ("insensitive")
		sicase ("icase")
			envcase = 1;
			break;
		sicase ("case")
		sicase ("sensitive")
			envcase = 0;
			break;
		sdefault
			envcase = cf_isyes (s);
			break;
		} esac;
	} else {
		envcase = 0;
	}
	hasenvcase = 1;
	return envcase;
}


static
int
checklocase ()
{
	/* to be done */
	locase = 0;
	haslocase = 1;
	return locase;
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
