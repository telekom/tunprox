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

#ifndef _R__FRLIB_LIB_CAL_TZ_CORRECT_H
#define _R__FRLIB_LIB_CAL_TZ_CORRECT_H


#include <fr/base/tmo.h>

#ifdef __cplusplus
extern "C" {
#endif




struct ctz_info {
	int	actleapsec;		/* number of actual (in this moment) leap seconds 
									can be 0, 1 or 2 
								 */
	int	totleapsec;		/* total number of leap seconds yet */
	int	isdst;			/* is daylight saving time */
	char	*abbrname;		/* name of timezone */
	tmo_t	gmtoff;			/* offset to utc in microseconds */
};




int ctz_utc2local (	tmo_t *otstamp, tmo_t tstamp, struct ctz_info *info, 
							int tz, int flags);
int ctz_local2utc (	tmo_t *otstamp, tmo_t tstamp, int tz, int flags);










#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_CAL_TZ_CORRECT_H */

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
