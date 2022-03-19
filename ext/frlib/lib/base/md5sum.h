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

#ifndef _R__FRLIB_LIB_BASE_MD5SUM_H
#define _R__FRLIB_LIB_BASE_MD5SUM_H

#include <fr/base/txtenc.h>
#include <stdint.h>
#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif




/*  hash  should be at least 25 bytes long for b64 
								 and 33 for hex 
								 and 16 for binary */

int md5sum (char *hash, const char *msg, size_t msglen, int flag);





/* the following exists for backward compatibility only
 * bette use the TENC_ macros directly
 */

#define MD5SUM_FLAG_NONE	TENC_FMT_DEF
#define MD5SUM_FLAG_B64		TENC_FMT_B64
#define MD5SUM_FLAG_HEX		TENC_FMT_HEX
#define MD5SUM_FLAG_BIN		TENC_FMT_BIN
#define MD5SUM_MASK_FORMAT	TENC_M_FMT
#define MD5SUM_FLAG_NOEQ	TENC_F_NOEQ
#define MD5SUM_FLAG_CAPS	TENC_F_CAPS



struct md5sum {
	char		buf[64];
	int		idx;
	uint32_t a0,
				b0,
				c0,
				d0;
	uint64_t	msglen;
};

#define MD5SUM_SINIT	{ \
				.a0 = 0x67452301U, \
				.b0 = 0xEFCDAB89U, \
				.c0 = 0x98BADCFEU, \
				.d0 = 0x10325476U, \
		}
#define MD5SUM_INIT ((struct md5sum)MD5SUM_SINIT)

int md5sum_calc (struct md5sum *, const char *msg, size_t mlen);
int md5sum_get (char *out, struct md5sum*, int flags);










#ifdef __cplusplus
}	/* extern "C" */
#endif













#endif	/* _R__FRLIB_LIB_BASE_MD5SUM_H */

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
