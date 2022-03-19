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
//#define _XOPEN_SOURCE
#define __USE_XOPEN
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>



#include <fr/base/tmo.h>
#include "tmo.h"
#include "errors.h"
#include "cjg.h"
#include "crule.h"


static int globtz = TMO_TZ_SYSTEM;


#define isleap(y)	(((y)%400==0)||(((y)%4==0)&&((y)%100!=0)))
static const int _numdays[] = {31,28,31,30,31,30,31,31,30,31,30,31};
#define numdays(m,y)	(((m)<1||(m)>12)?0:(_numdays[(m)-1]+(((m)==2&&isleap(y))?1:0)))


char *
tmo_adjustdate (date, with_tz)
	char	*date;
	int	with_tz;
{
	static char	d2[32];
	int			y, m, d, h, min, sec, h2, m2;
	char			tzone[32];
	int			num, plus;
	const char	*s;

	if (!date) return NULL;
	tzone[0]=0;
	num = sscanf (date, "%d-%d-%d %d:%d:%d %31s", &y, &m, &d, &h, &min, &sec, tzone);
	tzone[31]=0;
	if (num < 6) return NULL;
	if (num==6 || !*tzone) strcpy (tzone, "+0000");
	if (m<1) m=1;
	if (m>12) m=12;
	if (d<1) d=1;
	if (d>numdays(m,y)) d=numdays(m,y);
	if (h<0) h=0;
	if (h>23) h=23;
	if (min<0) min=0;
	if (min>59) min=59;
	if (sec<0) sec=0;
	if (sec>60) sec=60;		/* accept leap seconds */
	if (*tzone=='+') {
		plus=1;
		s=tzone+1;
	} else if (*tzone=='-') {
		plus=0;
		s=tzone+1;
	} else {
		plus=1;
		s=tzone;
	}
	if (strlen (s) != 4 || !isdigit (s[0]) || !isdigit(s[1]) || 
				!isdigit(s[2]) || !isdigit(s[3]))
		s="0000";
	h2=(s[0]-'0')*10+(s[1]-'0');
	m2=(s[2]-'0')*10+(s[3]-'0');
	if (plus) {
		h2=-h2;
		m2=-m2;
	}
	min+=m2;
	if (min>59) {
		h2+=min/60;
		min%=60;
	} else if (min<0) {
		h2+=min/60-1;
		min=min%60+60;
	}
	h+=h2;
	if (h>23) {
		d+=h/24;
		h%=24;
	} else if (h<0) {
		d+=h/24-1;
		h=h%24+24;
	}
	if (d>numdays(m,y)) {
		d-=numdays(m,y);
		m++;
		if (m==13) {
			m=1;
			y++;
		}
	} else if (d<1) {
		m--;
		if (m==0) {
			m=12;
			y--;
		}
		d+=numdays(m,y);
	}
	if (with_tz) {
		sprintf (d2, "%d-%02d-%02d %02d:%02d:%02d +0000", y, m, d, h, min, sec);
	} else {
		sprintf (d2, "%d-%02d-%02d %02d:%02d:%02d", y, m, d, h, min, sec);
	}
	return d2;
}


int
tmo_parsemetadate (utc, date)
	time_t	*utc;
	char		*date;
{
	tmo_t	out;
	int	ret;

	if (!utc || !date) return RERR_PARAM;
	ret = cjg_strptime3 (&out, date, "%Y-%m-%d %T %z");
	if (!RERR_ISOK(ret)) return ret;
	*utc = out / 1000000LL;
	return RERR_OK;
}





time_t
tmo_gmktime (tm)
	struct tm	*tm;
{
	struct cjg_bd_t	tbd;
	tmo_t					out;
	int					ret;

	if (!tm) return (time_t) -1;
	bzero (&tbd, sizeof (struct cjg_bd_t));
	tbd.year = tm->tm_year+1900;
	tbd.mon = tm->tm_mon+1;
	tbd.day = tm->tm_mday;
	tbd.hour = tm->tm_hour;
	tbd.min = tm->tm_min;
	tbd.sec = tm->tm_sec;
	tbd.micro = 0;
	tbd.hasyear = 1;
	tbd.hasmon = 1;
	tbd.hasday = 1;
	tbd.hashour = 1;
	tbd.hasmin = 1;
	tbd.hassec = 1;
	tbd.hasmicro = 1;

	ret = cjg_compose (&out, &tbd, CRUL_T_UTC, 0);
	if (!RERR_ISOK(ret)) return (time_t) -1;
	out /= 1000000LL;
	return (time_t) out;
}


time_t
tmo_getrfctime (str)
	char	*str;
{
	int	ret;
	tmo_t	out;

	ret = cjg_strptime3 (&out, str, "%r");
	if (!RERR_ISOK(ret)) return (time_t) -1;
	out /= 1000000LL;
	return (time_t) out;
}



static const char 	* dformats[] = {
						"%d %b %Y %H:%M:%S",
						"%d %b %Y %H:%M",
						"%d %b %Y",
						"%d. %b %Y %H:%M:%S",
						"%d. %b %Y %H:%M",
						"%d. %b %Y",
						"%d %B %Y %H:%M:%S",
						"%d %B %Y %H:%M",
						"%d %B %Y",
						"%d. %B %Y %H:%M:%S",
						"%d. %B %Y %H:%M",
						"%d. %B %Y",
						"%d %m %Y %H:%M:%S",
						"%d %m %Y %H:%M",
						"%d %m %Y",
						"%d/%m/%Y %H:%M:%S",
						"%d/%m/%Y %H.%M",
						"%d/%m/%Y",
						"%d.%m.%Y %H:%M:%S",
						"%d.%m.%Y %H:%M",
						"%d.%m.%Y",
						"%Y-%m-%d %H:%M:%S",
						"%Y-%m-%d %H:%M",
						"%Y-%m-%d",
						"%m/%d/%Y %H:%M:%S",
						"%m/%d/%Y %H:%M",
						"%m/%d/%Y",
						"%m %d %Y %H:%M:%S",
						"%m %d %Y %H:%M",
						"%m %d %Y",
						NULL };

time_t
tmo_parsefreedate (str, ende)
	char	*str;
	int	ende;
{
	char			**sp;
	struct tm	tm;

	if (!str) return -2;
	if (!*str) return 0;
	if (!strcasecmp (str, "now")) return time (NULL);
	if (ende && !strcasecmp (str, "max")) return -1;
	bzero (&tm, sizeof (struct tm));
	tm.tm_year=70;
	tm.tm_mon=0;
	tm.tm_mday=1;
	if (ende) {
		tm.tm_hour=23;
		tm.tm_min=59;
		tm.tm_sec=59;
	}
	for (sp=(char **)dformats; *sp; sp++) {
		bzero (&tm, sizeof (struct tm));
		tm.tm_year=70;
		tm.tm_mon=0;
		tm.tm_mday=1;
		if (ende) {
			tm.tm_hour=23;
			tm.tm_min=59;
			tm.tm_sec=59;
		}
		if (strptime (str, *sp, &tm)) break;
	}
	if (!*sp) return -2;
	return tmo_gmktime (&tm);
}




int
tmo_strptime (str, fmt, tm)
	char			*str;
	const char	*fmt;
	struct tm	*tm;
{
	int					ret;
	struct cjg_bd_t	tbd;

	if (!str || !fmt || !tm) return RERR_PARAM;
	ret = cjg_strptime (str, fmt, &tbd, 0);
	if (!RERR_ISOK(ret)) return ret;
	if (tbd.hasyear) tm->tm_year=tbd.year - 1900;
	if (tbd.hasmon) tm->tm_mon =tbd.mon - 1;
	if (tbd.hasday) tm->tm_mday=tbd.day;
	if (tbd.hashour) tm->tm_hour=tbd.hour;
	if (tbd.hasmin) tm->tm_min =tbd.min;
	if (tbd.hassec) tm->tm_sec =tbd.sec;
	if (tbd.haswday) tm->tm_wday = (tbd.wday == 6) ? 0 : tbd.wday + 1;
	if (tbd.hasyday) tm->tm_yday = tbd.yday;
	return RERR_OK;
}






int
tmo_gettimestr (tstr)
	const char	*tstr;
{
	tmo_t	out;

	out = tmo_gettimestr64 (tstr);
	out /= 1000000LL;
	return (int)out;
}


tmo_t
tmo_gettimestr64 (tstr)
	const char	*tstr;
{
	return cjg_gettimestr (tstr);
}


int
tmo_compose (tstamp, tbd)
	tmo_t					*tstamp;
	struct tmo_bd_t	*tbd;
{
	return tmo_compose2 (tstamp, tbd, TMO_TZ_GLOBAL);
}

int
tmo_compose2 (tstamp, tbd, tz)
	tmo_t					*tstamp;
	struct tmo_bd_t	*tbd;
	int					tz;
{
	int	rul;

	if (tz==TMO_TZ_GLOBAL) tz=globtz;
	switch (tz) {
	case TMO_TZ_UTC:
		rul = CRUL_T_UTC;
		break;
	case TMO_TZ_SYSTEM:
	default:
		rul = CRUL_T_CJG;
		break;
	}
	return cjg_compose (tstamp, tbd, rul, 0);
}
	

int
tmo_breakdown (tbd, tstamp)
	struct tmo_bd_t	*tbd;
	tmo_t					tstamp;
{
	return tmo_breakdown2 (tbd, tstamp, TMO_TZ_GLOBAL);
}

int
tmo_breakdown2 (tbd, tstamp, tz)
	struct tmo_bd_t	*tbd;
	tmo_t					tstamp;
	int					tz;
{
	int	rul;

	if (tz==TMO_TZ_GLOBAL) tz=globtz;
	switch (tz) {
	case TMO_TZ_UTC:
		rul = CRUL_T_UTC;
		break;
	case TMO_TZ_SYSTEM:
	default:
		rul = CRUL_T_CJG;
		break;
	}
	return cjg_breakdown (tbd, tstamp, rul, 0);
}


int
tmo_breakdown_daytime (tbd, tstamp)
	struct tmo_bd_t	*tbd;
	tmo_t					tstamp;
{
	return tmo_breakdown2_daytime (tbd, tstamp, TMO_TZ_GLOBAL);
}

int
tmo_breakdown2_daytime (tbd, tstamp, tz)
	struct tmo_bd_t	*tbd;
	tmo_t					tstamp;
	int					tz;
{
	return tmo_breakdown2 (tbd, tstamp, tz);
}



int
tmo_compose_diff (tstamp, tbd)
	tmo_t					*tstamp;
	struct tmo_bd_t	*tbd;
{
	return cjg_compose2 (tstamp, tbd, CJG_F_ISDELTA);
}

int
tmo_breakdown_diff (tbd, tstamp)
	struct tmo_bd_t	*tbd;
	tmo_t					tstamp;
{
	return cjg_breakdown2 (tbd, tstamp, CJG_F_ISDELTA);
}


int
tmo_prttimestr64 (tstr, tlen, tstamp, tform)
	char	*tstr;
	int	tlen, tform;
	tmo_t	tstamp;
{
	return cjg_prttimestr (tstr, tlen, tstamp, tform);
}


int
tmo_getweekday (tstamp)
	tmo_t	tstamp;
{
	return cjg_getweekday (tstamp);
}


int
tmo_getdayofyear (tstamp)
	tmo_t	tstamp;
{
	return cjg_getdayofyear (tstamp);
}


tmo_t
tmo_getyear (tstamp)
	tmo_t	tstamp;
{
	return cjg_getyear (tstamp);
}


int
tmo_getdaysince1970 (tstamp)
	tmo_t	tstamp;
{
	return cjg_getdaysince1970 (tstamp);
}


int
tmo_strftime (tstr, tlen, fmt, tstamp)
	char	*tstr, *fmt;
	int	tlen;
	tmo_t	tstamp;
{
	return tmo_strftime2 (tstr, tlen, fmt, tstamp, TMO_TZ_GLOBAL);
}

int
tmo_strftime2 (tstr, tlen, fmt, tstamp, tz)
	char	*tstr, *fmt;
	int	tlen, tz;
	tmo_t	tstamp;
{
	struct cjg_bd_t	tbd;
	int					ret;

	ret = tmo_breakdown2 (&tbd, tstamp, tz);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_strftime (tstr, tlen, fmt, &tbd, 0);
}


int
tmo_astrftime (tstr, fmt, tstamp)
	char	**tstr, *fmt;
	tmo_t	tstamp;
{
	return tmo_astrftime2 (tstr, fmt, tstamp, TMO_TZ_GLOBAL);
}

int
tmo_astrftime2 (tstr, fmt, tstamp, tz)
	char	**tstr, *fmt;
	tmo_t	tstamp;
	int	tz;
{
	struct cjg_bd_t	tbd;
	int					ret;

	ret = tmo_breakdown2 (&tbd, tstamp, tz);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_astrftime (tstr, fmt, &tbd, 0);
}


int
tmo_tzset (tz)
	int	tz;
{
	globtz = tz;
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
