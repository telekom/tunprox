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
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */
#ifndef _R__FRLIB_LIB_BASE_IPOOL_H
#define _R__FRLIB_LIB_BASE_IPOOL_H

#include <stdint.h>


#ifdef __cplusplus
extern "C" {
#endif


#define IPOOL_F_MASK_INVERT	0x01
#define IPOOL_F_MASK_REGULAR	0x02

#define IPOOL_F_PRT_IP			0x10
#define IPOOL_F_PRT_HEX			0x20

struct ipool_range {
	uint64_t	b, e;
};
struct ipool {
	struct ipool_range	*list;
	int						len;
};
#define IPOOL_NULL ((struct ipool) {NULL, 0})



/* note str will be modified */
int ipool_add (struct ipool*, char* str, int flags);
int ipool_addfile (struct ipool*, const char *file, int flags);
int ipool_add_range (struct ipool*, uint64_t, uint64_t);
int ipool_subtract_range (struct ipool*, uint64_t, uint64_t);
int ipool_subtract_pool (struct ipool*, struct ipool*);
int ipool_choose (uint64_t*, struct ipool*, int seq);
int ipool_print (struct ipool*, int flags);
void ipool_print_num (uint64_t, int flags);




#ifdef __cplusplus
}	/* extern "C" */
#endif




#endif	/* _R__FRLIB_LIB_BASE_IPOOL_H */

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
