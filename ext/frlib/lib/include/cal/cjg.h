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

#ifndef _R__FRLIB_LIB_CAL_GREG_H
#define _R__FRLIB_LIB_CAL_GREG_H

#include <stdarg.h>
#include <stdint.h>
#include <fr/base/tmo.h>


#ifdef __cplusplus
extern "C" {
#endif


/* general flags - used by several functions */
#define CJG_F_NONE		0x00
#define CJG_F_ISDELTA	0x01	/* is delta time */
#define CJG_F_NODELTA	0x02	/* force to be absolute time */
#define CJG_F_NOZERO		0x04	/* don't zero breakdown structure before 
											filling it */
#define CJG_F_DAYTIME	0x08	/* stop after breaking down to time and
											days1970 */
#define CJG_F_FORCERULE	0x10	/* force compose to use rule, even if it
											has a gmtoff available */
#define CJG_F_NANO		0x20	/* timestamp is in nano seconds not micro */


/* the following reforms are predifined */
#define CJG_REF_DEF		0	/* use default settings */
#define CJG_REF_JUL		1	/* use always julian calendar */
#define CJG_REF_GREG		2	/* use always gregorian calendar */
#define CJG_REF_1582		3	/* use 1582'th reform */
#define CJG_REF_1752		4	/* use 1752'th reform */
#define CJG_REF_SWEDEN	5	/* use special case sweden */
#define CJG_REF_DATE		6	/* use explicit date (first day in greg cal) */
#define CJG_REF_MAX		6

/* tform param for cjg_prttimestr */
#define CJG_TSTR_T_DDELTA	0
#define CJG_TSTR_T_D			1
#define CJG_TSTR_T_T			2


struct cjg_bd_t {
	tmo_t		year;			/* year +1 -> 1 AD    -1 -> 1 BC, year 0 does not 
									exist */
	int		mon;			/* month between 1 and 12 */
	int		day;			/* day of month between 1 and 31 */
	int		hour;			/* hour of day 0 - 23*/
	int		min;			/* minute 0 - 59 */
	int		sec;			/* second 0 - 61 (consider leap seconds) */
	int		micro;		/* microsecond 0 - 999999 */
	int32_t	nano;			/* nanoseconds 0 - 999999999 */
	/* the following fields are produced by breakdown, but not used for compose */
	tmo_t		tstamp;		/* micro/nano seconds since 1970 - the non broken down
									info */
	tmo_t		micro1970;	/* micro seconds since 1970 */
	tmo_t		nano1970;	/* nano secons since 1970 */
	tmo_t		days1970;	/* days since 1970 */
	int		yday;			/* day of year */
	int		wday;			/* weekday 0=mon, 6=sun*/
	int		weeknum;		/* iso week number */
	tmo_t		wyear;		/* year to which week belongs */
	union {
		struct {
			uint32_t	isleap:2,	/* is leap year - can be 2 for 1712 sweden */
						isdst:1,		/* is daylight saving time */
						isjul:1,		/* is julian calendar */
						isdelta:1,	/* is delta time */
						isnano:1,	/* tstamp is in nano seconds */
						reserved:11,
						/* the following indicates which information we have, 
							primarily set by strptime and used by compose */
						hasyear:1,
						hasmon:1,
						hasday:1,
						hashour:1,
						hasmin:1,
						hassec:1,
						hasmicro:1,
						hasnano:1,
						hasmicro1970:1,
						hasdays1970:1,
						hasyday:1,
						haswday:1,
						hasweeknum:1,
						haswyear:1,
						hasgmtoff:1;
		};
		/* the following two are for convinience */
		struct {
			uint32_t	reserved2:17,
						hasinfo:15;
		};
		uint32_t		info;
	};
	int	numleapsec;	/* number of leap seconds so far */
	tmo_t	gmtoff;		/* offset of gmt in microseconds - east is positive */
	char	*tzname;		/* abbreviation of timezone name */
	int	tz;			/* timezone used for breakdown */
	int	rul;			/* rule used for breakdown */
	int	flags;		/* flags used for breakdown */
};



int cjg_breakdown (struct cjg_bd_t *tbd, tmo_t tstamp, int rule, int flags);
int cjg_compose (tmo_t *tstamp, struct cjg_bd_t *tbd, int rule, int flags);

int cjg_strftime (char *tstr, int tlen, const char *fmt, struct cjg_bd_t *tbd, 
						int flags);
int cjg_astrftime (char **tstr, const char *fmt, struct cjg_bd_t *tbd, int flags);

int cjg_strptime (const char *tstr, const char *fmt, struct cjg_bd_t *tbd, 
						int flags);


int cjg_isleap (tmo_t year, int rul);


/* the following functions are handy, but do use only default tz and reform */

int cjg_breakdown2 (struct cjg_bd_t *tbd, tmo_t tstamp, int flags);
int cjg_compose2 (tmo_t *tstamp, struct cjg_bd_t *tbd, int flags);

int cjg_strftime2 (char *tstr, int tlen, const char *fmt, tmo_t tstamp, int flags);
int cjg_astrftime2 (char **tstr, const char *fmt, tmo_t tstamp, int flags);
int cjg_strptime2 (tmo_t *tstamp, const char *tstr, const char *fmt, int flags);


/* for those, who don't even want to type a 0 flags */
int cjg_breakdown3 (struct cjg_bd_t *tbd, tmo_t tstamp);
int cjg_compose3 (tmo_t *tstamp, struct cjg_bd_t *tbd);

int cjg_strftime3 (char *tstr, int tlen, const char *fmt, tmo_t tstamp);
int cjg_astrftime3 (char **tstr, const char *fmt, tmo_t tstamp);
int cjg_strptime3 (tmo_t *tstamp, const char *tstr, const char *fmt);


int cjg_getweekday (tmo_t tstamp);			/* usec only */
int cjg_getdayofyear (tmo_t tstamp);		/* usec only */
int cjg_getdaysince1970 (tmo_t tstamp);	/* usec only */
tmo_t cjg_getyear (tmo_t tstamp);			/* usec only */

tmo_t cjg_gettimestr (const char *tstr);
int cjg_gettimestr2 (tmo_t *out, const char *tstr);
int cjg_prttimestr (char *tstr, int tlen, tmo_t tstamp, int tform);

/* the corresponding nano sec functions */
tmo_t cjg_gettimestrns (const char *tstr);
int cjg_gettimestr2ns (tmo_t *out, const char *tstr);
int cjg_prttimestrns (char *tstr, int tlen, tmo_t tstamp, int tform);

int cjg_isleap2 (tmo_t year);



/* set rules */

/* if reform is CJG_REF_DATE, the function expects 
 *		(int year, int mon, int day)
 * as ...
 */
int cjg_rulesetreform (int rul, int reform, ...);

/* old style form, but has still a usage, so leave it */
int cjg_defreform (int reform);



/* used internally for crul */

struct cjg_rule {
	int	reform;
	tmo_t	year;
	int	mon, day;
};

int cjg_rulesetdef (struct cjg_rule*);
int cjg_ruledel (struct cjg_rule*);























#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_CAL_GREG_H */

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
