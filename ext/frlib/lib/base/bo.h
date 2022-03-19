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

#ifndef _R__FRLIB_LIB_BASE_BO_H
#define _R__FRLIB_LIB_BASE_BO_H

#ifndef __USE_BSD
#  define __USE_BSD
#  include <endian.h>
#  undef __USE_BSD
#else
#  include <endian.h>
#endif

#include <byteswap.h>
#include <ctype.h>
#include <stdint.h>

// Note: Don't use that Macros - might fail on ARM
#if BYTE_ORDER == BIG_ENDIAN
#define BO_HAS_BE
#elif BYTE_ORDER == LITTLE_ENDIAN
#define BO_HAS_LE
#endif



#define BO_MK_NONE	0
#define BO_MK_SWAP	1
#define BO_MK_BE		2
#define BO_MK_LE		3

#define BO_FROM_BE	BO_MK_BE
#define BO_FROM_LE	BO_MK_LE
#define BO_TO_BE		BO_MK_BE
#define BO_TO_LE		BO_MK_LE


/* for backward compatibility */
#define BO_USE_NONE	BO_MK_NONE
#define BO_USE_SWAP	BO_MK_SWAP
#define BO_USE_BE		BO_MK_BE
#define BO_USE_LE		BO_MK_LE

#ifdef __cplusplus
extern "C" {
#endif

int bo_isbe();

int bo_cv64 (uint64_t *out, uint64_t in, int bo);
int bo_cv32 (uint32_t *out, uint32_t in, int bo);
int bo_cv16 (uint16_t *out, uint16_t in, int bo);

#ifdef __cplusplus
}	/* extern "C" */
#endif


/* for backward compatibility */
#define bo_cv_64	bo_cv64
#define bo_cv_32	bo_cv32
#define bo_cv_16	bo_cv16
















#endif	/* _R__FRLIB_LIB_BASE_BO_H */

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
