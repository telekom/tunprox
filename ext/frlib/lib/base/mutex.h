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

#ifndef _R__FRLIB_LIB_BASE_MUTEX_H
#define _R__FRLIB_LIB_BASE_MUTEX_H





#include "tmo.h"


#ifdef __cplusplus
extern "C" {
#endif


#define MUTEX_F_NONE		0x00
/* reserve the lower 9 bits for mode */
#define MUTEX_M_MODMASK	0x1f
#define MUTEX_F_NOUNDO	0x20


int mutex_create (int key, int flags);
int mutex_destroy (int id);
int mutex_rdlock (int id, tmo_t tmout, int flags);
int mutex_wrlock (int id, tmo_t tmout, int flags);
int mutex_unlock (int id, int flags);
int mutex_funlock (int id);
int mutex_islocked (int id);




#ifdef __cplusplus
}	/* extern "C" */
#endif


























#endif	/* _R__FRLIB_LIB_BASE_MUTEX_H */

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
