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

#ifndef _R__FRLIB_LIB_BASE_TXTENC_H
#define _R__FRLIB_LIB_BASE_TXTENC_H


#include <ctype.h>



#ifdef __cplusplus
extern "C" {
#endif




#define TENC_FMT_DEF	0x00L
#define TENC_FMT_BIN	0x01L
#define TENC_FMT_HEX	0x02L
#define TENC_FMT_B32	0x03L
#define TENC_FMT_B64	0x04L
#define TENC_M_FMT	0x0fL

#define TENC_F_NONL	0x100L
#define TENC_F_NOEQ	0x200L
#define TENC_F_CAPS	0x400L
#define TENC_F_OLD32	0x800L
#define TENC_F_OLEN	0x1000L	/* write at most olen bytes - decode only */




int tenc_b64encode (char *out, const char *in, size_t inlen, int flags);
int tenc_b64encode_alloc (char **out, const char *in, size_t inlen, int flags);
int tenc_b64decode (char *out, size_t *olen, const char *in, int flags);
int tenc_b64decode_alloc (char **out, size_t *olen, const char *in, int flags);

int tenc_b32encode (char *out, const char *in, size_t inlen, int flags);
int tenc_b32encode_alloc (char **out, const char *in, size_t inlen, int flags);
/* the following 2 functions are not implemented yet */
int tenc_b32decode (char *out, size_t *olen, const char *in, int flags);
int tenc_b32decode_alloc (char **out, size_t *olen, const char *in, int flags);

int tenc_hexencode (char *out, const char *in, size_t inlen, int flags);
int tenc_hexencode_alloc (char **out, const char *in, size_t inlen, int flags);
int tenc_hexdecode (char *out, size_t *olen, const char *in, int flags);
int tenc_hexdecode_alloc (char **out, size_t *olen, const char *in, int flags);

int tenc_encode (char *out, const char *in, size_t inlen, int flags);
int tenc_encode_alloc (char **out, const char *in, size_t inlen, int flags);
int tenc_decode (	char *out, size_t *olen, const char *in, size_t inlen,
						int flags);
int tenc_decode_alloc (	char **out, size_t *olen, const char *in, size_t inlen,
								int flags);






#ifdef __cplusplus
}	/* extern "C" */
#endif












#endif	/* _R__FRLIB_LIB_BASE_TXTENC_H */
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
