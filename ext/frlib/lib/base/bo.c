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
#include <unistd.h>
#include <stdint.h>

#include "bo.h"
#include "errors.h"

#define BO_DYN	1

int bo_isbe();
int
bo_isbe ()
{
	volatile int is_bigendian = 1;
	
	return ((*(char*)&is_bigendian) == 0) ? 1 : 0;
}


int
bo_cv64 (out, in, bo)
	uint64_t	*out, in;
	int		bo;
{
	if (!out) return RERR_PARAM;
	switch (bo) {
	case BO_MK_BE:
#if defined BO_DYN
		if (bo_isbe()) {
			*out = in;
		} else {
			*out = bswap_64 (in);
		}
#elif defined BO_HAS_BE
		*out = in;
#else
		*out = bswap_64 (in);
#endif
		break;
	case BO_MK_LE:
#if defined BO_DYN
		if (!bo_isbe()) {
			*out = in;
		} else {
			*out = bswap_64 (in);
		}
#elif defined BO_HAS_LE
		*out = in;
#else
		*out = bswap_64 (in);
#endif
		break;
	case BO_MK_SWAP:
		*out = bswap_64 (in);
		break;
	case BO_MK_NONE:
		*out = in;
		break;
	default:
		return RERR_PARAM;
	}
	return RERR_OK;
}

int
bo_cv32 (out, in, bo)
	uint32_t	*out, in;
	int		bo;
{
	if (!out) return RERR_PARAM;
	switch (bo) {
	case BO_MK_BE:
#if defined BO_DYN
		if (bo_isbe()) {
			*out = in;
		} else {
			*out = bswap_32 (in);
		}
#elif defined BO_HAS_BE
		*out = in;
#else
		*out = bswap_32 (in);
#endif
		break;
	case BO_MK_LE:
#if defined BO_DYN
		if (!bo_isbe()) {
			*out = in;
		} else {
			*out = bswap_32 (in);
		}
#elif defined BO_HAS_LE
		*out = in;
#else
		*out = bswap_32 (in);
#endif
		break;
	case BO_MK_SWAP:
		*out = bswap_32 (in);
		break;
	case BO_MK_NONE:
		*out = in;
		break;
	default:
		return RERR_PARAM;
	}
	return RERR_OK;
}


int
bo_cv16 (out, in, bo)
	uint16_t	*out, in;
	int		bo;
{
	if (!out) return RERR_PARAM;
	switch (bo) {
	case BO_MK_BE:
#if defined BO_DYN
		if (bo_isbe()) {
			*out = in;
		} else {
			*out = bswap_16 (in);
		}
#elif defined BO_HAS_BE
		*out = in;
#else
		*out = bswap_16 (in);
#endif
		break;
	case BO_MK_LE:
#if defined BO_DYN
		if (!bo_isbe()) {
			*out = in;
		} else {
			*out = bswap_16 (in);
		}
#elif defined BO_HAS_LE
		*out = in;
#else
		*out = bswap_16 (in);
#endif
		break;
	case BO_MK_SWAP:
		*out = bswap_16 (in);
		break;
	case BO_MK_NONE:
		*out = in;
		break;
	default:
		return RERR_PARAM;
	}
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
