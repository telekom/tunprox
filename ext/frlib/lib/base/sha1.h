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

#ifndef _R__FRLIB_LIB_BASE_SHA1SUM_H
#define _R__FRLIB_LIB_BASE_SHA1SUM_H


#include "txtenc.h"
#include <stdlib.h>

#ifdef __cplusplus
extern "C" {
#endif



#define SHA1SUM_F_NONE	TENC_FMT_BIN
#define SHA1SUM_F_B64	TENC_FMT_B64
#define SHA1SUM_F_B32	TENC_FMT_B32
#define SHA1SUM_F_HEX	TENC_FMT_HEX
#define SHA1SUM_F_BIN	TENC_FMT_BIN
#define SHA1SUM_F_NOEQ	TENC_FMT_NOEQ
#define SHA1SUM_F_CAPS	TENC_FMT_CAPS



/*  hash  should be at least 29 bytes long for b64 
								 and 41 for hex 
								 and 20 for binary */



int sha1sum (char *hash, const char *msg, size_t msglen, int flags);











#ifdef __cplusplus
}	/* extern "C" */
#endif












#endif	/* _R__FRLIB_LIB_BASE_SHA1SUM_H */

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
