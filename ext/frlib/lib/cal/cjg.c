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
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <time.h>
#include <stdarg.h>
#include <ctype.h>
#include <errno.h>

#include <fr/base/tmo.h>
#include <fr/base/errors.h>
#include <fr/base/textop.h>
#include <fr/base/roman.h>

#include "cjg.h"
#include "tz.h"
#include "tzcorrect.h"
#include "crule.h"
#include "romkal.h"
#include "strcase.h"


static int def_reform = CJG_REF_1582;

static int compday (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compdayymd (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compdayyd (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compdayweek (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compjulymd (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compjulyd (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compjulweek (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compgregymd (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compgregyd (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int compgregweek (tmo_t*, struct cjg_bd_t*, struct cjg_rule*);
static int delta_compose (tmo_t*, struct cjg_bd_t*, int);
static int breakdown_daytime (struct cjg_bd_t*, tmo_t, int, int);
static int breakgreg (struct cjg_bd_t*, tmo_t, struct cjg_rule*, int);
static int breakjul (struct cjg_bd_t*, tmo_t, struct cjg_rule*, int);
static int delta_break (struct cjg_bd_t*, tmo_t, int);
static int isjulian1 (tmo_t, int, int, struct cjg_rule*);
static int isjulian2 (tmo_t, struct cjg_rule*);
static int isjulian3 (tmo_t, int, struct cjg_rule*);
static int isjulian4 (tmo_t, int, int, struct cjg_rule*);
static int isrefyear (tmo_t, struct cjg_rule*);
static int breakweeknum (struct cjg_bd_t*, struct cjg_rule*);
static int isleapfunc (tmo_t, struct cjg_rule*);
static int _xprtf (char*, int, const char*, ...);
static int _prtnum (char*, int, int, int, tmo_t);
static int _prtstr (char*, int, int, int, const char*);
static tmo_t _div (tmo_t, tmo_t);
static tmo_t _mod (tmo_t, tmo_t);
static int loaddef ();
static int _scanum (tmo_t*, const char*, int);
static int _checkarray (const char*, const char**);
static int ptstamp_parsetform (struct cjg_bd_t*, const char*);
static int ptstamp_parse (struct cjg_bd_t*, const char*, int);
static int prt_deltatimestr (char*, int, struct cjg_bd_t*, int);



#define ISLEAP(y)				(((y)%400==0)||(((y)%4==0)&&((y)%100!=0)))
#define ISLEAPJUL(y)			((y)%4==0)
#define ISLEAPNEG(y)			((((y)+1)%400==0)||((((y)+1)%4==0)&&(((y)+1)%100!=0)))
#define ISLEAPNEGJUL(y)		(((y)+1)%4==0)
#define NUMLEAP(y)			(((y)-1)/4-((y)-1)/100+((y)-1)/400+1)
#define NUMLEAPJUL(y)		(((y)-1)/4+1)
#define NUMLEAPNEG(y)		(((y)+1)/4-((y)+1)/100+((y)+1)/400+1)
#define NUMLEAPNEGJUL(y)	(((y)+1)/4)
#define YEARDAYS(y)			((y)*365+NUMLEAP(y))
#define YEARDAYSJUL(y)		((y)*365+NUMLEAPJUL(y))
#define YEARDAYSNEG(y)		(((y)+1)*365+NUMLEAPNEG(y))
#define YEARDAYSNEGJUL(y)	(((y)+1)*365+NUMLEAPNEGJUL(y))
static int daysmon[] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 273, 
									304, 334 };
#define DAYSMON(m)			(((m)<1)?0:(((m)>12)?365:daysmon[m]))
#define WDAY_OFFSET			5

#define ISSWEDEN1(y,m,d,r)	(((r)==CJG_REF_SWEDEN)&&\
										(((y)<1712)||(((y)==1712)&&((m)<=2)))&&\
										(((y)>1700)||(((y)==1700)&&((m)>2))))
#define ISSWEDEN2(d,r)		(((r)==CJG_REF_SWEDEN)&&\
										((d)<(YEARDAYSJUL(1712)+DAYSMON(3)+1+1-1))&&\
										((d)>(YEARDAYSJUL(1700)+DAYSMON(2)+28-1)))




int
cjg_isleap2 (year)
	tmo_t	year;
{
	return cjg_isleap (year, CRUL_T_CJG);
}

int
cjg_isleap (year, rul)
	tmo_t	year;
	int	rul;
{
	struct crul_t	*crul;
	int				ret;

	if (rul==0) rul=CRUL_T_CJG;
	ret = crul_get (&crul, rul);
	if (!RERR_ISOK(ret) || crul->caltype != CRUL_T_CJG) {
		ret = crul_get (&crul, CRUL_T_CJG);
		if (!RERR_ISOK(ret)) return 0;
	}
	return isleapfunc (year, &(crul->cjg));
}

int
cjg_compose3 (tstamp, tbd)
	tmo_t					*tstamp;
	struct cjg_bd_t	*tbd;
{
	return cjg_compose (tstamp, tbd, CRUL_T_CJG, 0);
}

int
cjg_compose2 (tstamp, tbd, flags)
	tmo_t					*tstamp;
	struct cjg_bd_t	*tbd;
	int					flags;
{
	return cjg_compose (tstamp, tbd, CRUL_T_CJG, flags);
}

int
cjg_compose (tstamp, tbd, rul, flags)
	tmo_t					*tstamp;
	struct cjg_bd_t	*tbd;
	int					rul, flags;
{
	tmo_t				days, out, xout;
	int				ret;
	struct crul_t	*crul;
	int				tz, isdelta;

	if (!tbd) return RERR_PARAM;
	/* check for delta time first */
	isdelta = tbd->isdelta;
	if (flags & CJG_F_ISDELTA) isdelta = 1;
	if (flags & CJG_F_NODELTA) isdelta = 0;
	if (isdelta) return delta_compose (tstamp, tbd, flags);

	ret = crul_get (&crul, rul);
	if (!RERR_ISOK(ret)) return ret;
	tz = crul->tz;
	ret = compday (&days, tbd, &(crul->cjg));
	if (!RERR_ISOK(ret)) return ret;
	out = days * 86400LL + tbd->hour * 3600LL + tbd->min * 60LL + 
			tbd->sec;
	out *= 1000000LL;
	out += tbd->micro;
	if ((tbd->hasgmtoff || (!tbd->hasinfo && tbd->gmtoff!=0)) 
				&& !(flags & CJG_F_FORCERULE)) {
		/* do simple correction */
		out -= tbd->gmtoff;
	} else {
		ret = ctz_local2utc (&xout, out, tz, flags);
		if (!RERR_ISOK(ret)) return ret;
		out = xout;
	}
	if (flags & CJG_F_NANO) {
		out *= 1000LL;
		out += tbd->nano % 1000LL;
	}
	if (tstamp) *tstamp = out;
	return RERR_OK;	
}


int
cjg_breakdown3 (tbd, tstamp)
	struct cjg_bd_t	*tbd;
	tmo_t					tstamp;
{
	return cjg_breakdown (tbd, tstamp, CRUL_T_CJG, 0);
}

int
cjg_breakdown2 (tbd, tstamp, flags)
	struct cjg_bd_t	*tbd;
	tmo_t					tstamp;
	int					flags;
{
	return cjg_breakdown (tbd, tstamp, CRUL_T_CJG, flags);
}

int
cjg_breakdown (tbd, tstamp, rul, flags)
	struct cjg_bd_t	*tbd;
	tmo_t					tstamp;
	int					rul, flags;
{
	int				ret;
	tmo_t				days;
	struct crul_t	*crul;
	int				tz;

	if (!tbd) return RERR_PARAM;
	if (!(flags & CJG_F_NOZERO)) {
		bzero (tbd, sizeof (struct cjg_bd_t));
	}
	if ((flags & CJG_F_ISDELTA) && !(flags &CJG_F_NODELTA)) {
		return delta_break (tbd, tstamp, flags);
	}
	ret = crul_get (&crul, rul);
	if (!RERR_ISOK(ret)) return ret;
	tbd->tstamp = tstamp;
	if (flags & CJG_F_NANO) {
		tbd->nano1970 = tstamp;
		if (tstamp < 0) {
			tbd->nano = tstamp % 1000LL;
			tstamp /= 1000LL;
			if (tbd->nano != 0) {
				tstamp--;
				tbd->nano += 1000LL;
			}
		} else {
			tbd->nano = tstamp % 1000LL;
			tstamp /= 1000LL;
		}
		tbd->micro1970 = tstamp;
		tbd->isnano = 1;
		tbd->hasnano = 1;
	} else {
		tbd->micro1970 = tstamp;
		tbd->nano1970 = tstamp * 1000LL;
		tbd->nano = 0;
		tbd->isnano = 0;
		tbd->hasnano = 0;
	}
	tbd->rul = rul;
	tbd->tz = tz = crul->tz;
	tbd->flags = flags;
	ret = breakdown_daytime (tbd, tstamp, tz, flags);
	if (!RERR_ISOK(ret)) return ret;
	tbd->nano += tbd->micro * 1000LL;
	if (flags & CJG_F_DAYTIME) return RERR_OK;
	days = tbd->days1970;
	days += YEARDAYS(1970LL);
	tbd->wday = ((days + WDAY_OFFSET) % 7LL + 7LL) % 7LL;
	ret = isjulian2 (days, &(crul->cjg));
	if (!RERR_ISOK(ret)) return ret;
	if (ret) {
		tbd->isjul = 1;
		ret = breakjul (tbd, days, &(crul->cjg), flags);
	} else {
		tbd->isjul = 0;
		ret = breakgreg (tbd, days, &(crul->cjg), flags);
	}
	if (!RERR_ISOK(ret)) return ret;
	ret = isleapfunc (tbd->year, &(crul->cjg));
	if (!RERR_ISOK(ret)) return ret;
	tbd->isleap = ret;
	ret = breakweeknum (tbd, &(crul->cjg));
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


/******************************************
 * static functions for breakdown/compose 
 ******************************************/

static
int
compday (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	if (!tbd || !odays) return RERR_PARAM;
	if (tbd->hasinfo) {
		if (tbd->hasdays1970) {
			*odays = tbd->days1970;
			return RERR_OK;
		}
		if (tbd->hasyear && tbd->hasmon && tbd->hasday) {
			return compdayymd (odays, tbd, rul);
		}
		if (tbd->hasyear && tbd->hasyday) {
			return compdayyd (odays, tbd, rul);
		}
		if (tbd->haswyear && tbd->hasweeknum && tbd->haswday) {
			return compdayweek (odays, tbd, rul);
		}
		return RERR_INSUFFICIENT_DATA;
	}
	/* do some heuristics */
	if (tbd->days1970 != 0) {
		*odays = tbd->days1970;
		return RERR_OK;
	}
	if (tbd->day > 0 && tbd->mon > 0) {
		return compdayymd (odays, tbd, rul);
	}
	if (tbd->yday > 0) {
		return compdayyd (odays, tbd, rul);
	}
	if (tbd->weeknum > 0) {
		return compdayweek (odays, tbd, rul);
	}
	/* we still might have days1970 set to 0 */
	*odays = 0;
	return RERR_OK;
}

static
int
compdayymd (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	int	ret;

	if (!tbd) return RERR_PARAM;
	ret = isjulian1 (tbd->year, tbd->mon, tbd->day, rul);
	if (!RERR_ISOK(ret)) return ret;
	if (ret) {
		ret = compjulymd (odays, tbd, rul);
	} else {
		ret = compgregymd (odays, tbd, rul);
	}
	return ret;
}


static
int
compdayyd (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	int	ret;

	if (!tbd) return RERR_PARAM;
	ret = isjulian3 (tbd->year, tbd->yday, rul);
	if (!RERR_ISOK(ret)) return ret;
	if (ret) {
		ret = compjulyd (odays, tbd, rul);
	} else {
		ret = compgregyd (odays, tbd, rul);
	}
	return ret;
}

static
int
compdayweek (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	int	ret;

	if (!tbd) return RERR_PARAM;
	ret = isjulian4 (tbd->wyear, tbd->weeknum, tbd->wday, rul);
	if (!RERR_ISOK(ret)) return ret;
	if (ret) {
		ret = compjulweek (odays, tbd, rul);
	} else {
		ret = compgregweek (odays, tbd, rul);
	}
	return ret;
}

static
int
compjulymd (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	tmo_t	days;

	if (!tbd || !rul) return RERR_PARAM;
	if (tbd->year < 0) {
		days = YEARDAYSNEGJUL(tbd->year) - YEARDAYS(1970LL);
		days += DAYSMON(tbd->mon);
		if (ISLEAPNEGJUL(tbd->year) && (tbd->mon>2)) days++;
		days -= 2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
	} else {
		days = YEARDAYSJUL(tbd->year) - YEARDAYS(1970LL);
		days += DAYSMON(tbd->mon);
		if (ISLEAPJUL(tbd->year) && (tbd->mon>2)) days++;
		days -= 2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
		if (ISSWEDEN1 (tbd->year, tbd->mon, tbd->day, rul->reform)) days--;
	}
	days += tbd->day - 1;
	if (odays) *odays = days;
	return RERR_OK;
}

static
int
compjulyd (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	tmo_t	days;

	if (!tbd || !rul) return RERR_PARAM;
	if (tbd->year < 0) {
		days = YEARDAYSNEGJUL(tbd->year) - YEARDAYS(1970LL);
		days -= 2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
	} else {
		days = YEARDAYSJUL(tbd->year) - YEARDAYS(1970LL);
		days -= 2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
		if (ISSWEDEN1 (tbd->year, tbd->yday>60?3:1, 1, rul->reform)) days--;
	}
	days += tbd->yday - 1;
	if (odays) *odays = days;
	return RERR_OK;
}

static
int
compjulweek (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	tmo_t	days;
	int	fday, yday;

	if (!tbd) return RERR_PARAM;
	if (tbd->wyear < 0) {
		days = YEARDAYSNEGJUL(tbd->wyear);
		days -= 2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
	} else {
		days = YEARDAYSJUL(tbd->wyear);
		days -= 2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
	}
	/* now correct year begin */
	fday = ((days + WDAY_OFFSET) % 7LL + 7LL) % 7LL;
	days -= YEARDAYS(1970LL);
	if (fday <= 3) {
		yday = -fday;
	} else {
		yday = 7 - fday;
	}
	/* calc day of year */
	yday += (tbd->weeknum - 1) * 7;
	yday += tbd->wday;
	/* correct special case sweden */
	if (yday <= 0) {
		if (ISSWEDEN1 (tbd->year-1, 12, 1, rul->reform)) days--;
	} else {
		if (ISSWEDEN1 (tbd->year, yday>60?3:1, 1, rul->reform)) days--;
	}
	/* add week info */
	days += yday;
	if (odays) *odays = days;
	return RERR_OK;
}

static
int
compgregymd (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	tmo_t	days;

	if (!tbd) return RERR_PARAM;
	if (tbd->year < 0) {
		days = YEARDAYSNEG(tbd->year) - YEARDAYS(1970LL);
		days += DAYSMON(tbd->mon);
		if (ISLEAPNEG(tbd->year) && (tbd->mon>2)) days++;
	} else {
		days = YEARDAYS(tbd->year) - YEARDAYS(1970LL);
		days += DAYSMON(tbd->mon);
		if (ISLEAP(tbd->year) && (tbd->mon>2)) days++;
	}
	days += tbd->day - 1;
	if (odays) *odays = days;
	return RERR_OK;
}

static
int
compgregyd (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	tmo_t	days;
	int	ret;

	if (!tbd) return RERR_PARAM;
	if (tbd->year < 0) {
		days = YEARDAYSNEG(tbd->year) - YEARDAYS(1970LL);
	} else {
		days = YEARDAYS(tbd->year) - YEARDAYS(1970LL);
	}
	days += tbd->yday - 1;
	ret = isrefyear (tbd->year, rul);
	if (!RERR_ISOK(ret)) return ret;
	days += ret;
	if (odays) *odays = days;
	return RERR_OK;
}

static
int
compgregweek (odays, tbd, rul)
	tmo_t					*odays;
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	tmo_t	days;
	int	fday;
	int	ret;

	if (!tbd) return RERR_PARAM;
	if (tbd->wyear < 0) {
		days = YEARDAYSNEG(tbd->wyear);
	} else {
		days = YEARDAYS(tbd->wyear);
	}
	/* now correct year begin */
	fday = ((days + WDAY_OFFSET) % 7LL + 7LL) % 7LL;
	days -= YEARDAYS(1970LL);
	if (fday <= 3) {
		days -= fday;
	} else {
		days += 7 - fday;
	}
	/* add week info */
	days += (tbd->weeknum - 1) * 7;
	days += tbd->wday;
	ret = isrefyear (tbd->wyear, rul);
	if (!RERR_ISOK(ret)) return ret;
	days += ret;
	if (odays) *odays = days;
	return RERR_OK;
}

static
int
delta_compose (tstamp, tbd, flags)
	tmo_t					*tstamp;
	struct cjg_bd_t	*tbd;
	int					flags;
{
	tmo_t	out, day;

	if (!tbd) return RERR_PARAM;
	if (tbd->hasinfo) {
		if (tbd->hasdays1970) {
			day = tbd->days1970;
		} else {
			day = tbd->day;
		}
	if (tbd->hasmicro1970) {
		out = tbd->micro1970;
		goto finish;
	} 
	} else {
		if (tbd->days1970) {
			day = tbd->days1970;
		} else {
			day = tbd->day;
		}
		if (tbd->micro1970) {
			out = tbd->micro1970;
			goto finish;
		} 
	}
	out = tbd->year * 31565925000000LL +
				tbd->mon * 2630493750000LL +
				day * 86400000000LL +
				tbd->hour * 3600000000LL +
				tbd->min * 60000000LL +
				tbd->sec * 1000000LL +
				tbd->micro;
finish:
	if (flags & CJG_F_NANO) {
		out *= 1000LL;
		out += tbd->nano % 1000LL;
	}
	if (tstamp) *tstamp = out;
	return RERR_OK;	
}

static
int
breakdown_daytime (tbd, tstamp, tz, flags)
	struct cjg_bd_t	*tbd;
	tmo_t					tstamp;
	int					tz, flags;
{
	int					ret;
	struct ctz_info	info;

	if (!tbd) return RERR_PARAM;
	ret = ctz_utc2local (&tstamp, tstamp, &info, tz, flags);
	if (!RERR_ISOK(ret)) return ret;
	tbd->days1970 = tstamp / 86400000000LL;
	tstamp = tstamp % 86400000000LL;
	if (tstamp < 0) {
		tbd->days1970--;
		tstamp += 86400000000LL;
	}
	tbd->micro = tstamp % 1000000LL;
	tstamp /= 1000000LL;
	tbd->sec = tstamp % 60LL;
	tstamp /= 60LL;
	tbd->min = tstamp % 60LL;
	tstamp /= 60LL;
	tbd->hour = tstamp;
	tbd->sec += info.actleapsec;
	tbd->isdst = info.isdst;
	tbd->numleapsec = info.totleapsec;
	tbd->gmtoff = info.gmtoff;
	tbd->tzname = info.abbrname;

	return RERR_OK;
}

static
int
breakgreg (tbd, days, rul, flags)
	struct cjg_bd_t	*tbd;
	tmo_t					days;
	int					flags;
	struct cjg_rule	*rul;
{
	int	ret;

	if (!tbd) return RERR_PARAM;
	if (days < 0) {
		tbd->year = (days / YEARDAYS(400LL) - 1) * 400LL;
		days %= YEARDAYS (400LL);
		days += YEARDAYS (400LL);
	} else {
		tbd->year = (days / YEARDAYS(400LL)) * 400LL;
		days %= YEARDAYS(400LL);
	}
	if (days >= 366LL) {
		days --;
		tbd->year += (days / (YEARDAYS(100LL)-1)) * 100LL;
		days %= YEARDAYS(100LL)-1;
		if (days >= 365LL) {
			days ++;
			tbd->year += (days / YEARDAYS(4LL)) * 4LL;
			days %= YEARDAYS(4LL);
			if (days >= 366LL) {
				days --;
				tbd->year += days / 365LL;
				days %= 365LL;
			}
		}
	}
	if (tbd->year < 0) {
		tbd->year --;	/* year 0 does not exist */
		tbd->isleap = ISLEAPNEG (tbd->year);
	} else {
		tbd->isleap = ISLEAP (tbd->year);
	}
	tbd->yday = days+1;
	ret = isrefyear (tbd->year, rul);
	if (!RERR_ISOK(ret)) return ret;
	tbd->yday -= ret;
	if (tbd->isleap && days == 59) {
		tbd->mon = 2;
		tbd->day = 29;
	} else {
		if (tbd->isleap && days >= 59) days--;
		for (tbd->mon = 12; tbd->mon > 1 && daysmon[tbd->mon] > days; tbd->mon--);
		days -= daysmon[tbd->mon];
		tbd->day = days + 1;
	}
	return RERR_OK;
}

static
int
breakjul (tbd, days, rul, flags)
	struct cjg_bd_t	*tbd;
	tmo_t					days;
	int					flags;
	struct cjg_rule	*rul;
{
	if (!tbd || !rul) return RERR_PARAM;
	if (days < 2) {
		days+=2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
		tbd->year = days / YEARDAYSJUL(4LL);
		days %= YEARDAYSJUL(4LL);
		if (days < 0) {
			days += YEARDAYSJUL(4LL);
			tbd->year --;
		}
		tbd->year *= 4LL;
		if (days >= 366LL) {
			days --;
			tbd->year += days / 365LL;
			days %= 365LL;
		}
		tbd->year --;	/* year 0 does not exist */
		tbd->isleap = ISLEAPNEGJUL (tbd->year);
	} else {
		if (ISSWEDEN2 (days,rul->reform)) days++;
		days+=2;	/* epoch is 2 days after 1.1.1BC due to shift in greg. cal. */
		tbd->year = (days / YEARDAYSJUL(4LL)) * 4LL;
		days %= YEARDAYSJUL(4LL);
		if (days >= 366LL) {
			days --;
			tbd->year += days / 365LL;
			days %= 365LL;
		}
		tbd->isleap = ISLEAPJUL (tbd->year);
		if (tbd->year == 0) tbd->year--;
	}
	tbd->yday = days+1;
	if (tbd->isleap && days == 59) {
		tbd->mon = 2;
		tbd->day = 29;
	} else {
		if (tbd->isleap && days >= 59) days--;
		for (tbd->mon = 12; tbd->mon > 1 && daysmon[tbd->mon] > days; tbd->mon--);
		days -= daysmon[tbd->mon];
		tbd->day = days + 1;
	}
	return RERR_OK;
}


static
int
isrefyear (year, rul)
	tmo_t					year;
	struct cjg_rule	*rul;
{
	int	reform;

	if (!rul) return RERR_PARAM;
	reform = rul->reform;
	if (reform == CJG_REF_DEF) reform = def_reform;
	switch (reform) {
	case CJG_REF_JUL:
		return 0;
	case CJG_REF_GREG:
		return 0;
	case CJG_REF_1582:
		return (year==1582) ? 10 : 0;
	case CJG_REF_1752:
	case CJG_REF_SWEDEN:
		/* here ignore special case sweden */
		return (year==1752) ? 11 : 0;
	case CJG_REF_DATE:
		return (year == rul->year) ? _div (year, 400) - 2 : 0;
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;
}

static
int
isjulian1 (year, mon, day, rul)
	tmo_t					year;
	int					mon, day;
	struct cjg_rule	*rul;
{
	int	reform;

	if (!rul) return RERR_PARAM;
	reform = rul->reform;
	if (reform == CJG_REF_DEF) reform = def_reform;
	switch (reform) {
	case CJG_REF_JUL:
		return 1;
	case CJG_REF_GREG:
		return 0;
	case CJG_REF_1582:
		return (year<1582)||((year==1582)&&((mon<10)||((mon==10)&&day<15)));
	case CJG_REF_1752:
	case CJG_REF_SWEDEN:
		/* here ignore special case sweden */
		return (year<1752)||((year==1752)&&((mon<9)||((mon==9)&&day<14)));
	case CJG_REF_DATE:
		return (year<rul->year)||((year==rul->year)&&((mon<rul->mon)||
					((mon==rul->mon)&&day<rul->day)));
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;
}

static
int
isjulian2 (days1970, rul)
	tmo_t					days1970;
	struct cjg_rule	*rul;
{
	int	reform, isleap;
	tmo_t	days;

	if (!rul) return RERR_PARAM;
	reform = rul->reform;
	if (reform == CJG_REF_DEF) reform = def_reform;
	switch (reform) {
	case CJG_REF_JUL:
		return 1;
	case CJG_REF_GREG:
		return 0;
	case CJG_REF_1582:
		return days1970 < (YEARDAYS(1582)+DAYSMON(10)+15-1);
	case CJG_REF_1752:
	case CJG_REF_SWEDEN:
		/* here ignore special case sweden */
		return days1970 < (YEARDAYS(1752)+DAYSMON(9)+1+14-1);
	case CJG_REF_DATE:
		if (rul->mon <= 2) {
			isleap = 0;
		} else if (rul->year < 0) {
			isleap = ISLEAPNEGJUL(rul->year);
		} else {
			isleap = ISLEAPJUL(rul->year);
		}
		if (rul->year < 0) {
			days = YEARDAYSNEG (rul->year);
		} else {
			days = YEARDAYS (rul->year);
		}
		days += DAYSMON(rul->mon) + isleap + rul->day - 1;
		return days1970 < days;
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;
}

static
int
isjulian3 (year, yday, rul)
	tmo_t					year;
	int					yday;
	struct cjg_rule	*rul;
{
	int	reform, xday, delta;

	if (!rul) return RERR_PARAM;
	reform = rul->reform;
	if (reform == CJG_REF_DEF) reform = def_reform;
	switch (reform) {
	case CJG_REF_JUL:
		return 1;
	case CJG_REF_GREG:
		return 0;
	case CJG_REF_1582:
		return (year<1582)||((year==1582)&&(yday<278));
	case CJG_REF_1752:
	case CJG_REF_SWEDEN:
		/* here ignore special case sweden */
		return (year<1752)||((year==1752)&&(yday<247));
	case CJG_REF_DATE:
		if (rul->year < 0) {
			delta = (rul->year + 1 - (rul->mon < 3)) / 100 - 1;
			delta -= delta / 4 - 1;
			delta -= 2;
			xday = DAYSMON(rul->mon) + ((rul->mon > 2) && ISLEAPNEGJUL(rul->year))
					+ rul->day - delta;
		} else {
			delta = (rul->year - (rul->mon < 3)) / 100;
			delta -= delta / 4;
			delta -= 2;
			xday = DAYSMON(rul->mon) + ((rul->mon > 2) && ISLEAPJUL(rul->year)) 
					+ rul->day - delta;
		}
		return (year<rul->year)||((year==rul->year)&&(yday < xday));
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;
}

static
int
isjulian4 (wyear, weeknum, wday, rul)
	tmo_t					wyear;
	int					wday, weeknum;
	struct cjg_rule	*rul;
{
	int					reform, xday, xweek;
	int					xyear, delta, ret;
	struct cjg_bd_t	tbd;

	if (!rul) return RERR_PARAM;
	reform = rul->reform;
	if (reform == CJG_REF_DEF) reform = def_reform;
	switch (reform) {
	case CJG_REF_JUL:
		return 1;
	case CJG_REF_GREG:
		return 0;
	case CJG_REF_1582:
		return (wyear<1582)||((wyear==1582)&&((weeknum<40)||((weeknum==40)&&(wday<4))));
	case CJG_REF_1752:
	case CJG_REF_SWEDEN:
		/* here ignore special case sweden */
		return (wyear<1752)||((wyear==1752)&&((weeknum<36)||((weeknum==36)&&(wday<3))));
	case CJG_REF_DATE:
		bzero (&tbd, sizeof (struct cjg_bd_t));
		if (rul->year < 0) {
			delta = (rul->year + 1 - (rul->mon < 3)) / 100 - 1;
			delta -= delta / 4 - 1;
			delta -= 2;
			xday = DAYSMON(rul->mon) + ((rul->mon > 2) && ISLEAPNEGJUL(rul->year)) 
					+ rul->day - delta;
		} else {
			delta = (rul->year - (rul->mon < 3)) / 100;
			delta -= delta / 4;
			delta -= 2;
			xday = DAYSMON(rul->mon) + ((rul->mon > 2) && ISLEAPJUL(rul->year)) 
					+ rul->day - delta;
		}
		tbd.year = rul->year;
		tbd.mon = rul->mon;
		tbd.day = rul->day;
		tbd.isjul=0;
		tbd.yday = xday;
		if (rul->year < 0) {
			xday += YEARDAYSNEG(rul->year);
		} else {
			xday += YEARDAYS(rul->year);
		}
		tbd.wday = ((xday + WDAY_OFFSET) % 7LL + 7LL) % 7LL;
		xday -= YEARDAYS(1970LL);
		tbd.days1970 = xday;
		ret = breakweeknum (&tbd, rul);
		if (!RERR_ISOK(ret)) return ret;
		xday = tbd.wday;
		xyear = tbd.wyear;
		xweek = tbd.weeknum;
		return (wyear<xyear)||((wyear==xyear)&&((weeknum<xweek)||((weeknum==xweek)
								&&(wday<xday))));
	default:
		return RERR_NOT_SUPPORTED;
	}
	return RERR_INTERNAL;
}

static
int
breakweeknum (tbd, rul)
	struct cjg_bd_t	*tbd;
	struct cjg_rule	*rul;
{
	int	days, wday, weeknum, wyear, ret;

	if (!tbd || !rul) return RERR_PARAM;
	days = tbd->yday - 1;
	wday = days + 7 - tbd->wday;
	wday %= 7;
	wday = 7 - wday;	/* this is the weekday of 1. jan */
	days += wday;
	weeknum = (days / 7) + 1;
	if (wday > 3) weeknum--;
	wyear = tbd->year;
	if (weeknum == 0) {
		/* week belongs to year before */
		wyear --;
		if (wyear == 0) wyear--;
		ret = isleapfunc (wyear, rul);
		if (!RERR_ISOK(ret)) return ret;
		wday += 6 - ret;
		wday %= 7;	/* weekday of 1. jan of year before */
		days = tbd->yday - 1 + 365 + ret + wday;
		weeknum = (days / 7) + 1;
		if (wday > 3) weeknum--;
	} else if (weeknum >= 52) {
		/* _may_ belong to next year */
		wday += 1 + tbd->isleap;
		wday %= 7;	/* weekday of 1. jan of next year */
		days = tbd->yday - 1 - 365 - tbd->isleap + wday;
		if (days >= 0) {
			/* week does belong to next year ! */
			weeknum = 1;
			wyear ++;
		}
	}
	tbd->weeknum = weeknum;
	tbd->wyear = wyear;
	return RERR_OK;
}

static
int
delta_break (tbd, tstamp, flags)
	struct cjg_bd_t	*tbd;
	tmo_t					tstamp;
	int					flags;
{
	int	neg=0;

	if (!tbd) return RERR_PARAM;
	tbd->tstamp = tstamp;
	if (flags & CJG_F_NANO) {
		tbd->nano1970 = tstamp;
	} else {
		tbd->nano1970 = tstamp * 1000LL;
	}
	if (tstamp < 0) {
		tstamp *= -1;
		neg=1;
	}
	if (flags & CJG_F_NANO) {
		tbd->nano = tstamp % 1000LL;
		tstamp /= 1000LL;
		tbd->isnano = 1;
		tbd->hasnano = 1;
	} else {
		tbd->nano = 0;
		tbd->isnano = 0;
		tbd->hasnano = 0;
	}

	/* don't use the following, would create problems for composing */
	/* tbd->days1970 = tstamp / 86400000000LL; */
	tbd->micro1970 = neg ? (-1)*tstamp : tstamp;
	tbd->hasmicro1970 = 1;
	tbd->year = tstamp / 31565925000000LL;
	tstamp %= 31565925000000LL;
	tbd->day = tstamp / 86400000000LL;
	tstamp %= 86400000000LL;
	tbd->micro = tstamp % 1000000LL;
	tstamp /= 1000000LL;
	tbd->sec = tstamp % 60;
	tstamp /= 60;
	tbd->min = tstamp % 60;
	tbd->hour = tstamp / 60;
	tbd->nano += tbd->micro * 1000LL;
	if (neg) {
		tbd->year *= -1;
		tbd->mon *= -1;
		tbd->day *= -1;
		tbd->hour *= -1;
		tbd->min *= -1;
		tbd->sec *= -1;
		tbd->micro *= -1;
		tbd->nano *= -1;
	}
	tbd->hasyear = 1;
	tbd->hasmon = 0;
	tbd->hasday = 1;
	tbd->hasdays1970 = 0;
	tbd->hashour = 1;
	tbd->hasmin = 1;
	tbd->hassec = 1;
	tbd->hasmicro = 1;
	tbd->isdelta = 1;

	return RERR_OK;
}

static
int
isleapfunc (year, rul)
	tmo_t					year;
	struct cjg_rule	*rul;
{
	int	ret;
	if (!rul) return RERR_PARAM;
	ret = isjulian1 (year, 2, 29, rul);
	if (!RERR_ISOK(ret)) return ret;
	if (ret) {
		if (rul->reform == CJG_REF_SWEDEN) {
			if (year == 1712) return 2;
			if (year == 1700) return 0;
		}
		if (year < 0) return ISLEAPNEGJUL (year);
		return ISLEAPJUL (year);
	} else {
		if (year < 0) return ISLEAPNEG (year);
		return ISLEAP (year);
	}
	return 0;
}


/*************************
 * crule functions
 *************************/

int
cjg_rulesetdef (rul)
	struct cjg_rule	*rul;
{
	if (!rul) return RERR_PARAM;
	bzero (rul, sizeof (struct cjg_rule));
	rul->reform = CJG_REF_DEF;
	return RERR_OK;
}

int
cjg_ruledel (rul)
	struct cjg_rule	*rul;
{
	/* nothing to be done */
	return RERR_OK;
}

int
cjg_rulesetreform (
	int	rul,
	int	reform,
	...)
{
	struct crul_t		*crul;
	struct cjg_rule	*p;
	int					ret;
	va_list				ap;

	if (reform < 0 || reform > CJG_REF_MAX) return RERR_PARAM;
	ret = crul_get (&crul, rul);
	if (!RERR_ISOK(ret)) return ret;
	if (crul->caltype != CRUL_T_CJG) return RERR_INVALID_TYPE;
	p = &(crul->cjg);
	p->reform = reform;
	if (reform == CJG_REF_DATE) {
		va_start (ap, reform);
		p->year = (tmo_t) va_arg (ap, int);
		p->mon = va_arg (ap, int);
		p->day = va_arg (ap, int);
		va_end (ap);
		if (p->day < 1) p->day = 1;
		if (p->day > 31) p->day = 31;
		if (p->mon < 1) p->mon = 1;
		if (p->mon > 12) p->mon = 12;
		if (p->year == 0) p->year = -1;
	}
	return RERR_OK;
}


/* old style */
int
cjg_defreform (reform)
	int	reform;
{
	if (reform < 0 || reform > CJG_REF_MAX || reform == CJG_REF_DATE) 
		return RERR_PARAM;
	def_reform = reform;
	return RERR_OK;
}


/**************************
 * short hand functions
 **************************/


int
cjg_getweekday (tstamp)
	tmo_t	tstamp;
{
	struct cjg_bd_t	tbd;
	int					ret;

	ret = cjg_breakdown3 (&tbd, tstamp);
	if (!RERR_ISOK(ret)) return ret;
	return tbd.wday;
}


int
cjg_getdayofyear (tstamp)
	tmo_t	tstamp;
{
	struct cjg_bd_t	tbd;
	int					ret;

	ret = cjg_breakdown3 (&tbd, tstamp);
	if (!RERR_ISOK(ret)) return ret;
	return tbd.yday;
}


tmo_t
cjg_getyear (tstamp)
	tmo_t	tstamp;
{
	struct cjg_bd_t	tbd;
	int					ret;

	ret = cjg_breakdown3 (&tbd, tstamp);
	if (!RERR_ISOK(ret)) return ret;
	return tbd.year;
}

int
cjg_getdaysince1970 (tstamp)
	tmo_t	tstamp;
{
	struct cjg_bd_t	tbd;
	int					ret;

	ret = breakdown_daytime (&tbd, tstamp, CTZ_TZ_DEFAULT, 0);
	if (!RERR_ISOK(ret)) return ret;
	return tbd.days1970;
}

int
cjg_gettimestr2 (out, tstr)
	tmo_t			*out;
	const char	*tstr;
{
	int	ret;
	char	*s;

	ret = cjg_strptime3 (out, tstr, "%D");
	if (!RERR_ISOK(ret)) return 0;
	if ((s = index (tstr, 'T')) && s-tstr <= ret) {
		return CJG_TSTR_T_T;
	} else if ((s = index (tstr, 'D')) && s-tstr <= ret) {
		return CJG_TSTR_T_D;
	} else {
		return CJG_TSTR_T_DDELTA;
	}
}


tmo_t
cjg_gettimestr (tstr)
	const char	*tstr;
{
	int	ret;
	tmo_t	out;

	ret = cjg_strptime3 (&out, tstr, "%D");
	if (!RERR_ISOK(ret)) return 0;
	return out;
}


int
cjg_prttimestr (tstr, tlen, tstamp, tform)
	char	*tstr;
	int	tlen;
	tmo_t	tstamp;
	int	tform;
{
	struct cjg_bd_t	tbd;
	int					ret, flags=0;
	const char			*fmt="";

	flags = (tform == CJG_TSTR_T_DDELTA) ? CJG_F_ISDELTA : 0;
	switch (tform) {
	case CJG_TSTR_T_DDELTA:
		flags = CJG_F_ISDELTA;
		/* fall thru */
	case CJG_TSTR_T_D:
		fmt = "%D";
		break;
	case CJG_TSTR_T_T:
		fmt = "%f";
		break;
	default:
		return RERR_PARAM;
	}
	ret = cjg_breakdown (&tbd, tstamp, CRUL_T_CJG, flags);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_strftime (tstr, tlen, fmt, &tbd, flags);
}


int
cjg_gettimestr2ns (out, tstr)
	tmo_t			*out;
	const char	*tstr;
{
	int	ret;
	char	*s;

	ret = cjg_strptime2 (out, tstr, "%D", CJG_F_NANO);
	if (!RERR_ISOK(ret)) return 0;
	if ((s = index (tstr, 'T')) && s-tstr <= ret) {
		return CJG_TSTR_T_T;
	} else if ((s = index (tstr, 'D')) && s-tstr <= ret) {
		return CJG_TSTR_T_D;
	} else {
		return CJG_TSTR_T_DDELTA;
	}
}


tmo_t
cjg_gettimestrns (tstr)
	const char	*tstr;
{
	int	ret;
	tmo_t	out;

	ret = cjg_strptime2 (&out, tstr, "%D", CJG_F_NANO);
	if (!RERR_ISOK(ret)) return 0;
	return out;
}


int
cjg_prttimestrns (tstr, tlen, tstamp, tform)
	char	*tstr;
	int	tlen;
	tmo_t	tstamp;
	int	tform;
{
	struct cjg_bd_t	tbd;
	int					ret, flags;
	const char			*fmt="";

	flags = CJG_F_NANO;
	switch (tform) {
	case CJG_TSTR_T_DDELTA:
		flags |= CJG_F_ISDELTA;
		/* fall thru */
	case CJG_TSTR_T_D:
		fmt = "%D";
		break;
	case CJG_TSTR_T_T:
		fmt = "%f";
		break;
	default:
		return RERR_PARAM;
	}
	ret = cjg_breakdown (&tbd, tstamp, CRUL_T_CJG, flags);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_strftime (tstr, tlen, fmt, &tbd, flags);
}


/*********************** 
 * strftime functions
 ***********************/


int
cjg_strftime3 (tstr, tlen, fmt, tstamp)
	char			*tstr;
	const char	*fmt;
	int			tlen;
	tmo_t			tstamp;
{
	return cjg_strftime2 (tstr, tlen, fmt, tstamp, 0);
}

int
cjg_strftime2 (tstr, tlen, fmt, tstamp, flags)
	char			*tstr;
	const char	*fmt;
	int			tlen;
	tmo_t			tstamp;
	int			flags;
{
	struct cjg_bd_t	tbd;
	int					ret;

	if (!fmt) return RERR_PARAM;
	ret = cjg_breakdown2 (&tbd, tstamp, flags);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_strftime (tstr, tlen, fmt, &tbd, flags);
}

int
cjg_astrftime3 (tstr, fmt, tstamp)
	char			**tstr;
	const char	*fmt;
	tmo_t			tstamp;
{
	return cjg_astrftime2 (tstr, fmt, tstamp, 0);
}

int
cjg_astrftime2 (tstr, fmt, tstamp, flags)
	char			**tstr;
	const char	*fmt;
	tmo_t			tstamp;
	int			flags;
{
	struct cjg_bd_t	tbd;
	int					ret;

	if (!tstr || !fmt) return RERR_PARAM;
	ret = cjg_breakdown2 (&tbd, tstamp, flags);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_astrftime (tstr, fmt, &tbd, flags);
}


int
cjg_astrftime (tstr, fmt, tbd, flags)
	char					**tstr;
	const char			*fmt;
	struct cjg_bd_t	*tbd;
	int					flags;
{
	int	len;

	if (!tstr || !fmt || !tbd) return RERR_PARAM;
	len = cjg_strftime (NULL, 0, fmt, tbd, flags);
	if (len < 0) return len;
	*tstr = malloc (len+2);
	if (!*tstr) return RERR_NOMEM;
	return cjg_strftime (*tstr, len+2, fmt, tbd, flags);
}


#define MOD_ALTNUM		0x001
#define MOD_ROMAN			0x002
#define MOD_ABS			0x004
#define MOD_OPTIONAL		0x008		/* for scanning */
/* nopad ovewrites spacepad, spacepad ovewrites zeropad */
#define MOD_NOPAD			0x008
#define MOD_SPACEPAD		0x010
#define MOD_ZEROPAD		0x020
#define MOD_UPPERCASE	0x040
#define MOD_LOWERCASE	0x080
#define MOD_LOCAL			0x100
#define MOD_NEG			0x200		/* print -0 */
#define MOD_ALTFUNC		0x400
static const char	*wdayabbr[] = {
	"Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun", NULL };
#define WDAYABBR(wday) (wdayabbr[((wday)<0)?0:(((wday)>6)?6:(wday))])
static const char *wdayfull[] = {
	"Monday", "Tuesday", "Wednsday", "Thursday", "Friday", "Saturday", 
	"Sunday", NULL };
#define WDAYFULL(wday) (wdayfull[((wday)<0)?0:(((wday)>6)?6:(wday))])
static const char *monabbr[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug",
	"Sep", "Oct", "Nov", "Dec", NULL};
#define MONABBR(mon) (monabbr[((mon)<1)?0:(((mon)>12)?11:((mon)-1))])
static const char *monfull[] = {
	"January", "February", "March", "April", "May", "June", "July",
	"August", "September", "October", "November", "December", NULL };
#define MONFULL(mon) (monfull[((mon)<1)?0:(((mon)>12)?11:((mon)-1))])

static const int	mypow[] = { 1, 10, 100, 1000, 10000, 100000, 1000000 };


static const char *defdatetimerep = "%a %d %b %Y %T %Z";
static const char *defdaterep = "%Y-%m-%d";
static const char *deftimerep = "%H:%M:%S";
static int	hasdef = 0;

int
cjg_strftime (tstr, tlen, fmt, tbd, flags)
	char					*tstr;
	const char			*fmt;
	int					tlen, flags;
	struct cjg_bd_t	*tbd;
{
	int			neg, ret, len, wlen, isdelta;
	int			width, prec, mod, savlen;
	const char	*s;
	char			c, xfmt[64], *savstr;
	tmo_t			n;

	if ((!tstr && tlen > 0) || !fmt) return RERR_PARAM;
	if (tlen < 0) return tlen;
	if (!hasdef) loaddef ();
	isdelta = (flags & CJG_F_ISDELTA) && !(flags & CJG_F_NODELTA);
	savlen = tlen;
	savstr = tstr;
	wlen=0;
	for (; *fmt; fmt++) {
		if (*fmt != '%') {
			for (s=fmt; *s && *s != '%'; s++);
			len = s-fmt;
			if (len <= tlen) {
				strncpy (tstr, fmt, len);
				tstr += len;
				tlen -= len;
			} else if (tlen > 0) {
				strncpy (tstr, fmt, tlen);
				tstr += tlen;
				tlen = 0;
			}
			wlen+=len;
			fmt = s-1;
			continue;
		}
		fmt++;
		mod=0;
		while (1) {
			switch (*fmt) {
			case 'E':
				/* we: parse, but ignore for now */
				/* alternative local rep for num */
				mod |= MOD_ALTNUM;
				break;
			case 'O':
				/* roman numbers */
				mod |= MOD_ROMAN;
				break;
			case '+':
				/* we: parse, but ignore */
				/* date format */
				break;
			case '-':
				/* no padding */
				mod |= MOD_NOPAD;
				break;
			case '_':
				/* space padding */
				mod |= MOD_SPACEPAD;
				break;
			case '0':
				/* zero padding */
				mod |= MOD_ZEROPAD;
				break;
			case '^':
				/* upper case */
				mod |= MOD_UPPERCASE;
				break;
			case '#':
				/* we: lower case */
				/* case swap */
				mod |= MOD_LOWERCASE;
				break;
			case 'L':
				/* we only: local behaviour swapped */
				mod |= MOD_LOCAL;
				break;
			case '~':
				/* alternative functionality */
				mod |= MOD_ALTFUNC;
				break;
			default:
				goto out;
			}
			fmt++;
		}
out:
		for (width=0; *fmt >= '0' && *fmt <= '9'; fmt++) {
			width*=10;
			width+=*fmt-'0';
		}
		prec=0;
		if (*fmt == '.') {
			for (fmt++; *fmt >= '0' && *fmt <= '9'; fmt++) {
				prec*=10;
				prec+=*fmt-'0';
			}
		}
		switch (*fmt) {
		case '%':
			/* lit. % */
			ret = _xprtf (tstr, tlen, "%%");
			break;
		case 'a':
			/* abbr. weekday */
			ret = _prtstr (tstr, tlen, mod, width, WDAYABBR(tbd->wday));
			break;
		case 'A':
			/* full weekday */
			ret = _prtstr (tstr, tlen, mod, width, WDAYFULL(tbd->wday));
			break;
		case 'b':
			/* abbr. month name */
			ret = _prtstr (tstr, tlen, mod, width, MONABBR(tbd->mon));
			break;
		case 'B':
			/* full month name */
			ret = _prtstr (tstr, tlen, mod, width, MONFULL(tbd->mon));
			break;
		case 'c':
			/* standard fmt */
			/* E */
			ret = cjg_strftime (tstr, tlen, defdatetimerep, tbd, flags);
			break;
		case 'C':
			/* century 2 digit */
			/* E */
			/* + not */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, 
								_div (tbd->year,100LL));
			break;
		case 'd':
			/* day of month 2 digit */
			/* O */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, tbd->day);
			break;
		case 'D':
			/* we: %Y:%m:%dD%H:%M:%Q%z */
			/* %m/%d%Y */
			if (isdelta) {
				ret = prt_deltatimestr (tstr, tlen, tbd, prec);
			} else {
				sprintf (xfmt, "%%Y:%%m:%%dD%%H:%%M:%%.%dQ%%z", prec);
				ret = cjg_strftime (tstr, tlen, xfmt, tbd, flags);
			}
			break;
		case 'e':
			/* we: %Y%m%dT%H%M%S */
			/* day of month with space */
			/* O */
			ret = cjg_strftime (tstr, tlen, "%Y%m%dT%H%M%S", tbd, flags);
			break;
		case 'f':
			/* we only: %Y%m%dT%H%M%Q */
			sprintf (xfmt, "%%Y%%m%%dT%%H%%M%%.%dQ", prec);
			ret = cjg_strftime (tstr, tlen, xfmt, tbd, flags);
			break;
		case 'F':
			/* %Y-%m-%d */
			/* + not */
			ret = cjg_strftime (tstr, tlen, "%Y-%m-%d", tbd, flags);
			break;
		case 'g':
			/* week based year without century */
			/* + not */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, _mod (tbd->wyear,100LL));
			break;
		case 'G':
			/* week based year */
			/* + not */
			ret = _prtnum (tstr, tlen, mod, width, tbd->wyear);
			break;
#if 0
		case 'h':
			/* we: not */
			/* idem zu b */
			break;
#endif
		case 'H':
			/* 24-hour 2 digit */
			/* O */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, tbd->hour);
			break;
		case 'I':
			/* 12-hour 2 digit */
			/* O */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			n = tbd->hour%12;
			if (n==0) n=12;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, n);
			break;
		case 'j':
			/* we: day of year - no pad */
			/* day of year 3 digit */
			ret = _prtnum (tstr, tlen, mod, width, tbd->yday);
			break;
		case 'J':
			/* we only: days since 1970 */
			ret = _prtnum (tstr, tlen, mod, width, tbd->days1970);
			break;
		case 'k':
			/* we: milliseconds */
			/* 24-hour with space */
			ret = _prtnum (tstr, tlen, mod, width, _div (tbd->micro,1000LL));
			break;
		case 'K':
			/* we only: milliseconds since 1970 */
			ret = _prtnum (tstr, tlen, mod, width, _div (tbd->micro1970,1000LL));
			break;
		case 'l':
			/* we: 1 if leap year, 0 otherwise */
			/* 12-hour with space */
			ret = _prtnum (tstr, tlen, mod, width, tbd->isleap);
			break;
		case 'm':
			/* month 2 digit */
			/* O */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, tbd->mon);
			break;
		case 'M':
			/* minute 2 digit */
			/* O */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, tbd->min);
			break;
		case 'n':
			/* new line */
			ret = _xprtf (tstr, tlen, "\n");
			break;
		case 'N':
			/* we only: roman date string abbreviated */
			if (mod & MOD_LOWERCASE) {
				n = ROMKAL_F_LCASE;
			} else if (mod & MOD_UPPERCASE) {
				n = ROMKAL_F_UCASE;
			}
			ret = romandate (tstr, tlen, tbd, n | ROMKAL_F_ABBR);
			break;
		case 'o':
			/* we only: nanoseconds */
			/* O */
			ret = _prtnum (tstr, tlen, mod, width, tbd->nano);
			break;
		case 'p':
			/* am pm uppercase */
			ret = _prtstr (tstr, tlen, mod, width, ((tbd->hour >= 12) ? "PM" : "AM"));
			break;
		case 'P':
			/* we: AD/BC uppercase */
			/* am pm lowercase */
			/* + not */
			ret = _prtstr (tstr, tlen, mod, width, ((tbd->year > 0) ? "AD" : "BC"));
			break;
		case 'q':
			/* we only: fractions of a second, e.g.: 23 means 230000 microseconds  */
			if (tbd->isnano) {
				if (prec <= 0 || prec > 9) prec = 9;
				n = tbd->nano;
				if (n < 0) n *= -1;
				n %= 1000000000LL;
				n /= mypow[9-prec];
				if (prec < 9) {
					if (((tbd->nano % 1000000000LL) / mypow[8-prec]) % 10 >= 8) n++;
				}
			} else {
				if (prec <= 0 || prec > 6) prec = 6;
				n = tbd->micro;
				if (n < 0) n *= -1;
				n %= 1000000LL;
				n /= mypow[6-prec];
				if (prec < 6) {
					if (((tbd->micro % 1000000LL) / mypow[5-prec]) % 10 >= 5) n++;
				}
			}
			ret = _xprtf (tstr, tlen, "%0*d", prec, n);
			break;
		case 'Q':
			/* we only: seconds (two digit) with fractions, e.g.: 01.23 */
			if (tbd->isnano) {
				if (prec <= 0 || prec > 9) prec = 9;
			} else {
				if (prec <= 0 || prec > 6) prec = 6;
			}
			if (mod & MOD_NOPAD) {
				c = '-';
			} else if (mod & MOD_SPACEPAD) {
				c = '_';
			} else {
				c = '0';
			}
			if (width <= 0) width = 3+prec;
			width -= prec+1;
			if (width < 0) width=0;
			sprintf (xfmt, "%%%c%dS.%%.%dq", c, width, prec);
			ret = cjg_strftime (tstr, tlen, xfmt, tbd, flags);
			break;
		case 'r':
			/* we: rfc822 timeformat */
			/* %I:%M:%S %p */
			ret = cjg_strftime (tstr, tlen, "%a, %d %b %Y %H:%M:%S %z", tbd, flags);
			break;
		case 'R':
			/* we: roman date string */
			/* %H:%M */
			/* + not */
			if (mod & MOD_LOWERCASE) {
				n = ROMKAL_F_LCASE;
			} else if (mod & MOD_UPPERCASE) {
				n = ROMKAL_F_UCASE;
			}
			ret = romandate (tstr, tlen, tbd, n);
			break;
		case 's':
			/* seconds since 1970 */
			neg = 0;
			if (tbd->hasnano) {
				n = tbd->nano1970 / 1000000000LL;
				/* note: we need zero rounding here */
				if (n == 0 && tbd->nano1970 < 0) neg=1;
			} else {
				n = tbd->micro1970 / 1000000LL;
				/* note: we need zero rounding here */
				if (n == 0 && tbd->micro1970 < 0) neg=1;
			}
			ret = _prtnum (tstr, tlen, mod|(neg?MOD_NEG:0), width, n);
			break;
		case 'S':
			/* seconds 2 digits */
			/* O */
			if (!width && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, tbd->sec);
			break;
		case 't':
			/* tab */
			ret = _xprtf (tstr, tlen, "\t");
			break;
		case 'T':
			/* %H:%M:%S */
			ret = cjg_strftime (tstr, tlen, "%H:%M:%S", tbd, flags);
			break;
		case 'u':
			/* we: microseconds */
			/* day of the week as a decimal, range 1 to 7, Monday being 1 */
			/* O */
			ret = _prtnum (tstr, tlen, mod, width, tbd->micro);
			break;
		case 'U':
			/* we: microseconds since 1970 */
			/* US-weeknumber */
			/* O */
			ret = _prtnum (tstr, tlen, mod, width, tbd->micro1970);
			break;
		case 'v':
			/* we only: century - always positive */
			if (width==0 && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD | MOD_ABS, width, 
								tbd->year / 100);	/* we need zero rounding here */
			break;
		case 'V':
			/* we: full year - always positive */
			/* iso weeknumber */
			ret = _prtnum (tstr, tlen, mod | MOD_ABS, width, tbd->year);
			/* O */
			break;
		case 'w':
			/* we: day of week as decimal, 1: monday to 7: sunday */
			/* day of the week as a decimal, range 0 to 6, Sunday being 0 */
			/* O */
			ret = _prtnum (tstr, tlen, mod, width, tbd->wday+1);
			break;
		case 'W':
			/* iso weeknumber */
			/* O */
			ret = _prtnum (tstr, tlen, mod, width, tbd->weeknum);
			break;
		case 'x':
			/* standard date rep */
			/* E */
			ret = cjg_strftime (tstr, tlen, defdaterep, tbd, flags);
			break;
		case 'X':
			/* standard time rep */
			/* E */
			ret = cjg_strftime (tstr, tlen, deftimerep, tbd, flags);
			break;
		case 'y':
			/* year 2 digit */
			/* E */
			/* O */
			if (width==0 && !(mod & MOD_ROMAN)) width=2;
			ret = _prtnum (tstr, tlen, mod | MOD_ZEROPAD, width, _mod (tbd->year, 100));
			break;
		case 'Y':
			/* full year */
			/* E */
			ret = _prtnum (tstr, tlen, mod, width, tbd->year);
			break;
		case 'z':
			/* numeric timezone +hhmm or -hhmm */
			n = _div (tbd->gmtoff, 60000000LL);
			if (!(mod & MOD_ALTFUNC)) {
				ret = _xprtf (tstr, tlen, "%+03d%02d", (int) _div (n, 60), (int)_mod (n, 60));
			} else {
				ret = _xprtf (tstr, tlen, "%+03d", (int) _div (n, 15));
			}
			break;
		case 'Z':
			/* timezone string */
			ret = _prtstr (tstr, tlen, mod, width, tbd->tzname);
			break;
		default:
			if (!*fmt) fmt--;
			/* don't write anything */
			ret = 0;
		}
		if (!RERR_ISOK(ret)) return ret;
		n = (ret < tlen) ? ret : tlen;
		tlen -= n;
		tstr += n;
		wlen += ret;
	}
	if (savstr && savlen > 0) {
		n = (savlen-1) < wlen ? savlen - 1 : wlen;
		savstr[n] = 0;
	}
	return wlen;
}




static
int
_prtstr (tstr, tlen, mod, width, str)
	char			*tstr;
	const char	*str;
	int			tlen, mod, width;
{
	char	buf[64], *s;

	if (!str) str="";
	if ((mod & MOD_UPPERCASE) || (mod & MOD_LOWERCASE)) {
		strncpy (buf, str, sizeof(buf)-1);
		buf[sizeof(buf)-1]=0;
		if (mod & MOD_UPPERCASE) {
			for (s=buf; *s; s++) *s=toupper(*s);
		} else {
			for (s=buf; *s; s++) *s=tolower(*s);
		}
		str = buf;
	}
	if (width<0) width=0;
	if (mod & MOD_NOPAD) width=0;
	return _xprtf (tstr, tlen, "%*s", width, str);
}


static
int
_prtnum (tstr, tlen, mod, width, num)
	char	*tstr;
	int	tlen, mod, width;
	tmo_t	num;
{
	char			buf[128];
	const char	*fmt;
	int			ret, neg=0;

	if (num == 0 && (mod & MOD_NEG)) neg = 1;
	if ((mod & MOD_ABS) || (mod & MOD_ROMAN)) {
		if (num < 0) num *= -1LL;
	}
	if (mod & MOD_ROMAN) {
		ret = num2roman (buf, sizeof (buf), num, 
							((mod & MOD_LOWERCASE)?ROM_LCASE:0));
		if (!RERR_ISOK(ret)) return ret;
		mod &= ~(MOD_LOWERCASE|MOD_UPPERCASE);
		return _prtstr (tstr, tlen, mod, width, buf);
	}
	if (mod & MOD_NOPAD) {
		fmt = "%s%*Ld";
		width=0;
	} else if (mod & MOD_SPACEPAD) {
		fmt = "%s% *Ld";
	} else if (mod & MOD_ZEROPAD) {
		fmt = "%s%0*Ld";
	} else {
		if (width > 0) {
			fmt = "%s% *Ld";
		} else {
			fmt = "%s%*Ld";
			width=0;
		}
	}
	if (width<0) width=0;
	return _xprtf (tstr, tlen, fmt, neg?"-":"", width, (long long)num);
}


static
int
_xprtf (
	char			*tstr,
	int			tlen,
	const char	*fmt,
	...)
{
	int		num;
	va_list	ap;

	va_start (ap, fmt);
	num = vsnprintf (tstr, tlen, fmt, ap);
	va_end (ap);
	if (num < 0) return RERR_SYSTEM;
	return num;
}


static
int
loaddef ()
{
	/* to be done... */
	hasdef = 1;
	return RERR_OK;
}

static
tmo_t
_mod (a, b)
	tmo_t	a, b;
{
	tmo_t	c;

	if (a < 0) {
		c = a % b;
		if (c) c += b;
		return c;
	} else {
		return a % b;
	}
}


static
tmo_t
_div (a, b)
	tmo_t	a, b;
{
	if (a < 0 && a % b) {
		return a / b - 1;
	} else {
		return a / b;
	}
}


static
int
prt_deltatimestr (tstr, tlen, tbd, prec)
	char					*tstr;
	int					tlen, prec;
	struct cjg_bd_t	*tbd;
{
	int			ret, i, flex;
	const char	*neg="";
	tmo_t			day, year, mon, hour, min, sec, nano, days1970, micro;

	if ((!tstr && tlen>0) || tlen < 0) return RERR_PARAM;
	flex = (prec <= 0);
	if (prec<=0 || prec > 9) prec=9;
#define ABSIT(w)	if (tbd->w < 0) {\
							neg="-";\
							w = tbd->w * -1;\
						} else { \
							w = tbd->w;\
						}
	ABSIT(year);
	ABSIT(mon);
	ABSIT(day);
	ABSIT(days1970);
	ABSIT(hour);
	ABSIT(min);
	ABSIT(sec);
	ABSIT(nano);
	ABSIT(micro);
	if (tbd->hasinfo) {
		if (tbd->hasdays1970) day = days1970;
		if (!tbd->hasnano) {
			nano = micro*1000;
			if (prec > 6) prec = 6;
		}
	} else {
		if (days1970) day = days1970;
		if (!nano) {
			nano = micro*1000;
			if (prec > 6) prec=6;
		}
	}
	for (i=9; i>prec; i--) nano /= 10;
	if (flex) {
		if (nano == 0) {
			prec = 1;
		} else {
			while ((nano > 0) && ((nano % 10) == 0)) {
				nano /= 10;
				prec--;
			}
			if (prec <= 0) prec=1;
		}
	}
	if (year != 0) {
		if (mon == 0) {
			ret = snprintf (tstr, tlen, "%s%lld::%02lldd%02d:%02d:%02d.%0*lld", 
					neg, (long long)year, (long long)day, (int)hour,
					(int)min, (int)sec, prec, (long long)nano);
		} else {
			ret = snprintf (tstr, tlen, "%s%lld:%02d:%02lldd%02d:%02d:%02d.%0*lld",
					neg, (long long)year, (int)mon, (long long)day, (int)hour,
					(int)min, (int)sec, prec, (long long)nano);
		}
	} else if (mon != 0) {
		ret = snprintf (tstr, tlen, "%s%d:%02lldd%02d:%02d:%02d.%0*lld", neg,
				(int)mon, (long long)day, (int)hour, (int)min,
				(int)sec, prec, (long long)nano);
	} else if (day != 0) {
		ret = snprintf (tstr, tlen, "%s%lldd%02d:%02d:%02d.%0*lld", neg,
				(long long)day, (int)hour, (int)min, (int)sec,
				prec, (long long)nano);
	} else if (hour != 0) {
		ret = snprintf (tstr, tlen, "%s%d:%02d:%02d.%0*lld", neg,
				(int)hour, (int)min, (int)sec, prec, (long long)nano);
	} else if (min != 0) {
		ret = snprintf (tstr, tlen, "%s%d:%02d.%0*lld", neg,
				(int)min, (int)sec, prec, (long long)nano);
	} else {
		ret = snprintf (tstr, tlen, "%s%d.%0*lld", neg,
				(int)sec, prec, (long long)nano);
	}
	if (ret < 0) return RERR_SYSTEM;

	return ret;
}


/*************************************
 * strptime functions 
 *************************************/

int
cjg_strptime3 (tstamp, tstr, fmt)
	tmo_t			*tstamp;
	const char	*tstr, *fmt;
{
	return cjg_strptime2 (tstamp, tstr, fmt, 0);
}

int
cjg_strptime2 (tstamp, tstr, fmt, flags)
	tmo_t			*tstamp;
	const char	*tstr, *fmt;
	int			flags;
{
	int					ret;
	struct cjg_bd_t	tbd;

	if (!tstamp) return RERR_PARAM;
	ret = cjg_strptime (tstr, fmt, &tbd, flags);
	if (!RERR_ISOK(ret)) return ret;
	return cjg_compose2 (tstamp, &tbd, flags);
}


int
cjg_strptime (tstr, fmt, tbd, flags)
	const char			*tstr, *fmt;
	struct cjg_bd_t	*tbd;
	int					flags;
{
	int			mod, ret;
	char			*s;
	tmo_t			num;
	tmo_t			cent=0, thisyear;
	int			year=0, hascent=0;
	int			adbc=0, hassomeyear=0;
	int			hour=-1, ampm = 0;
	int			wlen=0, i;
	const char	**arr;

	if (!tstr || !fmt || !tbd) return RERR_PARAM;
	if (!(flags & CJG_F_NOZERO)) {
		bzero (tbd, sizeof (struct cjg_bd_t));
	}
	if (flags & CJG_F_ISDELTA) {
		tbd->isdelta = 1;
	}
	flags |= CJG_F_NOZERO;
	for (; *fmt; fmt++) {
		if (iswhite (*fmt)) {
			for (; iswhite (*fmt); fmt++);
			for (; iswhite (*tstr); tstr++, wlen++);
			fmt--;
			continue;
		}
		if (*fmt != '%') {
			for (s=(char*)fmt; *s && *s != '%' && !iswhite (*s); s++);
			if (strncasecmp (tstr, fmt, s-fmt) != 0) {
				return RERR_INVALID_FORMAT;
			}
			tstr += s-fmt;
			wlen += s-fmt;
			fmt = s-1;
			continue;
		}
		fmt++;
		mod = 0;
		while (1) {
			switch (*fmt) {
			case 'E':
				/* parse, but ignore */
				mod = MOD_ALTNUM;
				break;
			case 'O':
				/* parse, but ignore */
				mod = MOD_ROMAN;
				break;
			case '?':
				/* this format may miss */
				mod = MOD_OPTIONAL;
				break;
			case '~':
				/* alternative functionality */
				mod = MOD_ALTFUNC;
				break;
			default:
				goto out;
			}
			fmt++;
		}
out:
		switch (*fmt) {
		case '%':
			/* lit. % */
			if (*tstr == '%') {
				ret = 1;
			} else {
				ret = RERR_INVALID_FORMAT;
			}
			break;
		case '!':
			/* we only: match the following char */
			fmt++;
			if (islower (*tstr) == islower (*fmt)) {
				ret = 1;
			} else {
				ret = RERR_INVALID_FORMAT;
			}
			break;
		case '.':
			/* we only: match any char - no whitespace */
			if (*tstr) {
				ret = 1;
			} else {
				ret = RERR_INVALID_FORMAT;
			}
			break;
		case '*':
			/* we only: match any alphanumerical string */
			for (s=(char*)tstr; isalnum (*s); s++);
			ret = s - tstr;
			break;
		case 'a':
		case 'A':
			/* weekday (full or abbreviated) */
			arr = wdayfull;
			ret = _checkarray (tstr, arr);
			if (ret == RERR_NOT_FOUND) {
				arr = wdayabbr;
				ret = _checkarray (tstr, arr);
				if (ret == RERR_NOT_FOUND) ret = RERR_INVALID_FORMAT;
			}
			if (!RERR_ISOK(ret)) break;
			num = ret;
			ret = strlen (arr[num]);
			tbd->wday = num;
			tbd->haswday = 1;
			break;
		case 'b':
		case 'B':
			/* month name (full or abbreviated) */
			arr = monfull;
			ret = _checkarray (tstr, arr);
			if (ret == RERR_NOT_FOUND) {
				arr = monabbr;
				ret = _checkarray (tstr, arr);
				if (ret == RERR_NOT_FOUND) ret = RERR_INVALID_FORMAT;
			}
			if (!RERR_ISOK(ret)) break;
			num = ret;
			ret = strlen (arr[num]);
			tbd->mon = num + 1;
			tbd->hasmon = 1;
			break;
		case 'c':
			/* standard fmt */
			ret = cjg_strptime (tstr, defdatetimerep, tbd, flags);
			break;
		case 'C':
		case 'v':
			/* century */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			cent = num;
			hassomeyear = 1;
			hascent = 1;
			break;
		case 'd':
			/* day of month */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->day = num;
			tbd->hasday = 1;
			break;
		case 'D':
			/* we: %Y:%m:%dD%H:%M:%Q */
			/* %m/%d%Y */
			/* fall thru - all three Def can be handled equaly */
		case 'e':
			/* we: %Y%m%dT%H%M%S */
			/* day of month with space */
			/* fall thru */
		case 'f':
			/* we only: %Y%m%dT%H%M%Q */
			ret = ptstamp_parse (tbd, (char*)tstr, flags);
			break;
		case 'F':
			/* %Y-%m-%d */
			ret = cjg_strptime (tstr, "%Y-%m-%d", tbd, flags);
			break;
		case 'g':
			/* week based year without century */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			thisyear = cjg_getyear (tmo_now ());
			if ((thisyear - 60LL) % 100LL <= num) {
				tbd->wyear = ((thisyear - 60LL) / 100LL) * 100LL + num;
			} else {
				tbd->wyear = ((thisyear + 39LL) / 100LL) * 100LL + num;
			}
			tbd->haswyear = 1;
			break;
		case 'G':
			/* week based year */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->wyear = num;
			tbd->haswyear = 1;
			break;
#if 0
		case 'h':
			/* we: not */
			/* idem zu b */
			break;
#endif
		case 'H':
			/* 24-hour */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->hour = num;
			tbd->hashour = 1;
			break;
		case 'I':
			/* 12-hour */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			hour = num;
			break;
		case 'j':
			/* day of year */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->yday = num;
			tbd->hasyday = 1;
			break;
		case 'J':
			/* we only: days since 1970 */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->days1970 = num;
			tbd->hasdays1970 = 1;
			break;
		case 'k':
			/* we: milliseconds */
			/* 24-hour with space */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->micro = num * 1000LL;
			tbd->hasmicro = 1;
			break;
		case 'K':
			/* we only: milliseconds since 1970 */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->micro1970 = num * 1000LL;
			tbd->hasmicro1970 = 1;
			break;
		case 'l':
			/* leap year 1/2 is leap year, 0 not */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			if (num == 2) {
				tbd->isleap = 2;
			} else if (num) {
				tbd->isleap = 1;
			}
			break;
		case 'm':
			/* month */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->mon = num;
			tbd->hasmon = 1;
			break;
		case 'M':
			/* minute */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->min = num;
			tbd->hasmin = 1;
			break;
		case 't':
		case 'n':
			/* any whitespace */
			/* will be done automatically - so ignore */
			break;
		case 'o':
			/* we only: nanoseconds */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->nano = num;
			tbd->hasnano = 1;
			break;
		case 'p':
			/* am pm uppercase */
			sswitch ((char*)tstr) {
			sincase ("am")
				ampm = 0;
				ret = 2;
				break;
			sincase ("pm")
				ampm = 1;
				ret = 2;
				break;
			sdefault
				ret = RERR_INVALID_FORMAT;
				break;
			} esac;
			break;
		case 'P':
			/* we: AD/BC uppercase */
			/* am pm lowercase */
			sswitch ((char*)tstr) {
			sincase ("AD")
				adbc = 1;
				ret = 2;
				break;
			sincase ("BC")
				adbc = -1;
				ret = 2;
				break;
			sdefault
				ret = RERR_INVALID_FORMAT;
				break;
			} esac;
			break;
		case 'q':
			/* we only: fractions of a second, e.g.: 23 means 230000 microseconds  */
			for (s=(char*)tstr, i=0, num=0; isdigit(*s) && i<9; s++, i++) {
				num *= 10;
				num += *s - '0';
			}
			for (; i<9; i++) num *= 10;
			ret = s - tstr;
			if (ret == 0) {
				ret = RERR_INVALID_FORMAT;
				break;
			}
			tbd->nano = num;
			tbd->hasnano = 1;
			tbd->micro = num / 1000LL;
			tbd->hasmicro = 1;
			break;
		case 'Q':
			/* we only: seconds (two digit) with fractions, e.g.: 01.23 */
			/* fractions are optional */
			ret = cjg_strptime (tstr, "%S%?!.%?q", tbd, flags);
			break;
		case 'r':
			/* we: rfc822 timeformat */
			/* %I:%M:%S %p */
			ret = cjg_strptime (tstr, "%a, %d %b %Y %H:%M:%Q %z", tbd, flags);
			break;
		case 'N':
		case 'R':
			/* we: roman date string (full or abbreviated) */
			/* %H:%M */
			ret = romandateparse ((char*)tstr, tbd, ROMKAL_F_NOZERO);
			break;
		case 's':
			/* seconds since 1970 */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->micro1970 = num * 1000000LL;
			tbd->hasmicro1970 = 1;
			break;
		case 'S':
			/* seconds (0-61) */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->sec = num;
			tbd->hassec = 1;
			break;
		case 'T':
			/* %H:%M:%S */
			ret = cjg_strptime (tstr, "%H:%M:%S", tbd, flags);
			break;
		case 'u':
			/* we: microseconds */
			/* day of the week as a decimal, range 1 to 7, Monday being 1 */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->micro = num;
			tbd->hasmicro = 1;
			break;
		case 'U':
			/* we: microseconds since 1970 */
			/* US-weeknumber */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->micro1970 = num;
			tbd->hasmicro1970 = 1;
			break;
		case 'w':
			/* we: day of week as decimal, 1: monday to 7: sunday */
			/* day of the week as a decimal, range 0 to 6, Sunday being 0 */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			if (num <= 0 || num > 7) num = 7;
			num--;
			tbd->wday = num;
			tbd->haswday = 1;
			break;
		case 'W':
			/* iso weeknumber */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			tbd->weeknum = num;
			tbd->hasweeknum = 1;
			break;
		case 'x':
			/* standard date rep */
			ret = cjg_strptime (tstr, defdaterep, tbd, flags);
			break;
		case 'X':
			/* standard time rep */
			ret = cjg_strptime (tstr, deftimerep, tbd, flags);
			break;
		case 'y':
			/* year 2 digit */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			year = num;
			hassomeyear = 1;
			break;
		case 'Y':
		case 'V':
			/* full year */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			year = num % 100LL;
			cent = num / 100LL;
			hassomeyear = 1;
			hascent = 1;
			break;
		case 'z':
			/* numeric timezone +hhmm or -hhmm */
			ret = _scanum (&num, tstr, flags);
			if (!RERR_ISOK(ret)) break;
			if (mod & MOD_ALTFUNC) {
				/* 15 minutes */
				tbd->gmtoff = num * 15LL * 60LL * 1000000LL;
			} else if (num > 24 || num < -24) {
				/* yes - we do want here the wrong division */
				tbd->gmtoff = ((num / 100LL) * 60LL + (num % 100LL)) * 60LL * 1000000LL;
			} else {
				tbd->gmtoff = num * 3600LL * 1000000LL;
			}
			tbd->hasgmtoff = 1;
			break;
		case 'Z':
			/* timezone string */
			/* parse, but ignore */
			for (s=(char*)tstr; isalpha(*s); s++);
			ret = s-tstr;
			break;
		default:
			return RERR_INVALID_FORMAT;
		}
		if (!RERR_ISOK(ret)) {
			if (mod & MOD_OPTIONAL) continue;
			return ret;
		}
		tstr += ret;
		wlen += ret;
	}
	if (hassomeyear) {
		if (!hascent) {
			thisyear = cjg_getyear (tmo_now ());
			if ((thisyear - 60LL) % 100LL <= year) {
				cent = (thisyear - 60LL) / 100LL;
			} else {
				cent = (thisyear + 39LL) / 100LL;
			}
		}
		if (adbc == 0) adbc = 1;
		tbd->year = (cent * 100 + year) * adbc;
		tbd->hasyear = 1;
	}
	if (hour >= 0 && !tbd->hashour) {
		if (hour >= 12) hour = 0;
		if (ampm) hour += 12;
		tbd->hour = hour;
		tbd->hashour = 1;
	}
	return wlen;
}



static
int
_scanum (num, tstr, flags)
	tmo_t			*num;
	const char	*tstr;
	int			flags;
{
	char	*s;
	int	ret;
	int	num2;

	if (!num) return RERR_PARAM;
	if (!tstr || !*tstr) {
		*num = 0;
		return 0;
	}
	s = ((*tstr=='-') || (*tstr=='+')) ? (char*)tstr+1 : (char*)tstr;
	if (isdigit (*s)) {
		errno = 0;
		*num = strtoll (tstr, &s, 10);
		if (errno != 0) return RERR_SYSTEM;
		ret = s - tstr;
	} else if (isalpha (*s)) {
		ret = roman2num (&num2, (char*)tstr, ROM_NEG);
		if (!RERR_ISOK(ret)) return ret;
		*num = num2;
	} else {
		return RERR_INVALID_VAL;
	}
	return ret;
}


static
int
_checkarray (tstr, arr)
	const char	*tstr, **arr;
{
	char	**s;

	if (!tstr || !arr) return RERR_PARAM;
	for (s=(char**)arr; *s; s++) {
		if (!strncasecmp (*s, tstr, strlen (*s))) {
			return s-(char**)arr;
		}
	}
	return RERR_NOT_FOUND;
}




static
int
ptstamp_parse (tbd, tstr, flags)
	struct cjg_bd_t	*tbd;
	const char			*tstr;
	int					flags;
{
	const char	*dstr, *mstr, *hstr, *sstr, *ustr, *tzstr;
	const char	*ystr, *mostr, *s, *orig;
	int			len, neg=0, wlen=0, tzmin=0;
	int			isdelta=0;
	tmo_t			day;

	orig = tstr;
	if (!tbd || !tstr) return 0;
	dstr = tstr = top_skipwhite (tstr);
	if (*dstr == '-') {
		neg=1;
		dstr++;
	}

	/* check the form we have */
	for (s = dstr; isdigit (*s) || iswhite (*s) || *s == ':'; s++);
	if (*s == 'd') {
		tstr = s+1;
		isdelta = 1;
	} else if (*s == 'D') {
		tstr = s+1;
	} else if (*s == 'T') {
		return ptstamp_parsetform (tbd, tstr);
	} else {
		tstr = dstr;
		dstr = NULL;
		isdelta = 1;
	}
	if (flags & CJG_F_ISDELTA) isdelta = 1;
	if (flags & CJG_F_NODELTA) isdelta = 0;

	/* parse time string */
	hstr = top_skipwhite (tstr);
	for (s=hstr; isdigit (*s) || iswhite (*s); s++);
	if (*s==':') {
		mstr = top_skipwhite (s+1);
		for (s=mstr; isdigit (*s) || iswhite (*s); s++);
	} else {
		mstr = NULL;
	}
	if (*s==':') {
		sstr = top_skipwhite (s+1);
		for (s=sstr; isdigit (*s) || iswhite (*s); s++);
	} else {
		sstr = NULL;
	}
	if (*s=='.') {
		ustr = top_skipwhite (s+1);
		for (s=ustr; isdigit (*s); s++);
	} else {
		ustr = NULL;
	}
	if (hstr && !isdigit (*hstr)) hstr = NULL;
	if (mstr && !isdigit (*mstr)) mstr = NULL;
	if (sstr && !isdigit (*sstr)) sstr = NULL;
	if (ustr && !isdigit (*ustr)) ustr = NULL;

	/* get timezone */
	if ((*s == '+' || *s == '-') && isdigit (s[1])) {
		tzstr = s;
		for (s++; isdigit (*s); s++);
	} else {
		tzstr = NULL;
	}

	/* get end of string */
	for (s--; iswhite (*s) && s>=orig; s--) {}; s++;
	wlen = s-orig;
	if (!wlen) return RERR_INVALID_FORMAT;

	/* check what we have */
	if (!dstr || !*dstr) {
		if (!isdelta) return RERR_INVALID_FORMAT;
		dstr="0";
		while (!sstr) {
			sstr = mstr;
			mstr = hstr;
			hstr = "0";
		}
	} else {
		if (!hstr) hstr = "0";
		if (!mstr) mstr = "0";
		if (!sstr) sstr = "0";
	}
	if (!ustr) ustr = "0";

	/* parse date string */
	ystr = top_skipwhite (dstr);
	for (s=ystr; isdigit (*s) || iswhite (*s); s++);
	if (*s==':') {
		mostr = top_skipwhite (s+1);
		for (s=mostr; isdigit (*s) || iswhite (*s); s++);
	} else {
		mostr = NULL;
	}
	if (*s==':') {
		dstr = top_skipwhite (s+1);
		for (s=dstr; isdigit (*s) || iswhite (*s); s++);
	} else {
		dstr = NULL;
	}
	if (*s && *s != 'd' && *s != 'D') return RERR_INVALID_FORMAT;
	if (ystr && !isdigit (*ystr)) ystr = NULL;
	if (mostr && !isdigit (*mostr)) mostr = NULL;
	if (dstr && !isdigit (*dstr)) dstr = NULL;

	/* check what we have */
	if (isdelta) {
		while (!dstr) {
			dstr = mostr;
			mostr = ystr;
			ystr = "0";
		}
		if (!mostr) mostr = "0";
		if (!ystr) ystr = "0";
	} else {
		if (!ystr || !mostr || !ystr) return RERR_INVALID_FORMAT;
	}

	/* convert string to int and set structure */
	tbd->year = atoll (ystr);
	tbd->hasyear = 1;
	tbd->mon = atoi (mostr);
	tbd->hasmon = 1;
	tbd->day = day = atoll (dstr);
	tbd->hasday = 1;
	if (isdelta && !tbd->mon && !tbd->year) {
		tbd->days1970 = day;
		tbd->hasdays1970 = 1;
	}
	tbd->hour = atoi (hstr);
	tbd->hashour = 1;
	tbd->min = atoi (mstr);
	tbd->hasmin = 1;
	tbd->sec = atoi (sstr);
	tbd->hassec = 1;

	tbd->nano = 0;
	for (s=ustr,len=0; isdigit (*s) && len < 9; len++, s++) {
		tbd->nano *= 10;
		tbd->nano += *s-'0';
	}
	for (len = 9-len; len > 0; len--) tbd->nano *= 10;
	tbd->micro = tbd->nano/1000LL;
	tbd->hasmicro = tbd->hasnano = 1;
	if (neg) {
		tbd->year *= -1;
		if (isdelta) {
			tbd->mon *= -1;
			tbd->day *= -1;
			tbd->hour *= -1;
			tbd->min *= -1;
			tbd->sec *= -1;
			tbd->days1970 *= -1;
			tbd->micro *= -1;
			tbd->nano *= -1;
		}
	}
	tbd->isdelta = isdelta;
	if (tzstr) {
		tzmin = atoi (tzstr+1);
		tzmin = (tzmin / 100) * 60 + (tzmin % 100);
		if (*tzstr == '-') tzmin *= -1;
		tbd->gmtoff = ((tmo_t)tzmin)*60LL*1000000LL;
		tbd->hasgmtoff = 1;
	}
	return wlen;
}



static
int
ptstamp_parsetform (tbd, tstr)
	struct cjg_bd_t	*tbd;
	const char			*tstr;
{
	const char	*dstr, *s, *orig;
	int			neg=0, i;

	if (!tbd || !tstr) return RERR_PARAM;
	orig = tstr;
	if (*tstr == '-') {
		neg=1;
		tstr++;
	}
	for (s = dstr = tstr; isdigit (*s); s++);
	if (*s != 'T' && *s != 't') return RERR_INVALID_FORMAT;
	if (s-dstr != 8) return RERR_INVALID_FORMAT;
	tstr = s+1;
	for (s = tstr; isdigit (*s); s++);
	if (s-tstr != 6) return RERR_INVALID_FORMAT;
	for (tbd->year=i=0; *dstr && i<4; i++,dstr++) {
		tbd->year *= 10; tbd->year += *dstr - '0';
		if (neg) tbd->year *= -1;
		tbd->hasyear = 1;
	}
	if (neg) tbd->year*=-1;
	for (tbd->mon=i=0; *dstr && i<2; i++,dstr++) {
		tbd->mon *= 10; tbd->mon += *dstr - '0';
		tbd->hasmon = 1;
	}
	for (tbd->day=i=0; *dstr && i<2; i++,dstr++) {
		tbd->day *= 10; tbd->day += *dstr - '0';
		tbd->hasday = 1;
	}
	for (tbd->hour=i=0; *tstr && i<2; i++,tstr++) {
		tbd->hour *= 10; tbd->hour += *tstr - '0';
		tbd->hashour = 1;
	}
	for (tbd->min=i=0; *tstr && i<2; i++,tstr++) {
		tbd->min *= 10; tbd->min += *tstr - '0';
		tbd->hasmin = 1;
	}
	for (tbd->sec=i=0; *tstr && i<2; i++,tstr++) {
		tbd->sec *= 10; tbd->sec += *tstr - '0';
		tbd->hassec = 1;
	}
	while (*tstr == '.') tstr++;
	if (*tstr) {
		for (tbd->nano=i=0; *tstr && i<9; i++,tstr++) {
			tbd->nano *= 10; tbd->nano += *tstr - '0';
		}
		for (; i<9; i++) tbd->nano*=10;
		tbd->micro = tbd->nano/1000;
		tbd->hasmicro = 1;
		tbd->hasnano = 1;
	}
	while (isdigit (*tstr)) tstr++;
	return tstr-orig;
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
