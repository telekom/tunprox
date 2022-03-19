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

#ifndef _R__FRLIB_LIB_CAL_TMO_H
#define _R__FRLIB_LIB_CAL_TMO_H

#include <time.h>
#include <sys/types.h>

#include <fr/base/tmo.h>
#include <fr/cal/cjg.h>



/*****************************************************
 *  !!!  ATTENTION  !!!                              *
 * ------------------------------------------------- *
 * these functions are depricated                    *
 * this file does exist for backward compatibility   *
 * only.                                             *
 * consider to use the cjg functions directly!       *
 *****************************************************/


#ifdef __cplusplus
extern "C" {
#endif


char * tmo_adjustdate (char *date, int with_tz);
int tmo_parsemetadate (time_t *utc, char *date);
time_t tmo_getrfctime (char*);

time_t tmo_gmktime (struct tm *);
time_t tmo_parsefreedate (char *str, int ende);

int tmo_strptime (char *str, const char *fmt, struct tm*);

int tmo_gettimestr (const char *tstr);
tmo_t tmo_gettimestr64 (const char *tstr);



#define TMO_TIMESTRFORM_DIFF	0
#define TMO_TIMESTRFORM_ABS	1
#define TMO_TIMESTRFORM_TFORM	2


struct cjg_bd_t;
#define tmo_bd_t	cjg_bd_t



#define TMO_TZ_GLOBAL	0
#define TMO_TZ_UTC		1
#define TMO_TZ_SYSTEM	2


int tmo_tzset (int tz);
int tmo_breakdown (struct tmo_bd_t *tbd, tmo_t tstamp);
int tmo_breakdown_daytime (struct tmo_bd_t *tbd, tmo_t tstamp);
int tmo_breakdown2 (struct tmo_bd_t *tbd, tmo_t tstamp, int tz);
int tmo_breakdown2_daytime (struct tmo_bd_t *tbd, tmo_t tstamp, int tz);
int tmo_breakdown_diff (struct tmo_bd_t *tbd, tmo_t tstamp);
int tmo_prttimestr64 (char *tstr, int tlen, tmo_t tstamp, int tform);
int tmo_strftime (char *tstr, int tlen, char *fmt, tmo_t tstamp);
int tmo_astrftime (char **tstr, char *fmt, tmo_t tstamp);
int tmo_strftime2 (char *tstr, int tlen, char *fmt, tmo_t tstamp, int tz);
int tmo_astrftime2 (char **tstr, char *fmt, tmo_t tstamp, int tz);
int tmo_compose (tmo_t *tstamp, struct tmo_bd_t *tbd);
int tmo_compose2 (tmo_t *tstamp, struct tmo_bd_t *tbd, int tz);
int tmo_compose_diff (tmo_t *tstamp, struct tmo_bd_t *tbd);
int tmo_getweekday (tmo_t tstamp);
int tmo_getdayofyear (tmo_t tstamp);
int tmo_getdaysince1970 (tmo_t tstamp);
tmo_t tmo_getyear (tmo_t tstamp);




#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_CAL_TMO_H */

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
