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

#ifndef _R__FRLIB_LIB_BASE_DLRT_H
#define _R__FRLIB_LIB_BASE_DLRT_H

#include <time.h>
#include <stdlib.h>
#include <unistd.h>

#ifdef __cplusplus
extern "C" {
#endif

#include <fr/base/dlstub.h>




DLSTUB_MKSTUB_A2 (nanosleep, int, const struct timespec*, struct timespec*)
DLSTUB_MKSTUB_A2 (clock_gettime, int, clockid_t, struct timespec*)
DLSTUB_MKSTUB_A3 (shmopen, int, const char*, int, mode_t)
DLSTUB_MKSTUB_A1 (shmunlink, int, const char*)








#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_DLRT_H */
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
