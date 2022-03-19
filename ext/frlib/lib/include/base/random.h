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


#ifndef _R__FRLIB_LIB_BASE_RANDOM_H
#define _R__FRLIB_LIB_BASE_RANDOM_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif





int frnd_init ();
uint32_t frnd_get32 ();
uint32_t frnd_get32limit (uint32_t limit);
uint32_t frnd_get32range (uint32_t min, uint32_t max);
char frnd_getletter ();
char frnd_getalnum ();
void frnd_getbytes (char *buf, size_t len);



#define frnd_getchar(min,max)	((unsigned char) frnd_get32range \
										((uint32_t)(min), (uint32_t)(max)))
#define frnd_getupper() (frnd_getchar ('A','Z'))
#define frnd_getlower() (frnd_getchar ('a','z'))
#define frnd_getdigit() (frnd_getchar ('0','9'))


int frnd_ishuffle (int *arr, uint32_t num);




#ifdef __cplusplus
}	/* extern "C" */
#endif














#endif	/* _R__FRLIB_LIB_BASE_RANDOM_H */

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
