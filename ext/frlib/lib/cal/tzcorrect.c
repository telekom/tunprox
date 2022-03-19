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
#include <errno.h>



#include <fr/base.h>

#include "tz.h"
#include "tzcorrect.h"

static int ctz_subtz (tmo_t*, tmo_t, struct ctz*, int);
static int ctz_subtzbyfile (tmo_t*, tmo_t, struct ctz*, int);
static int ctz_subtzbyrule (tmo_t*, tmo_t, struct ctz*, int);
static int ctz_addtz (tmo_t*, tmo_t, struct ctz*, struct ctz_info*, int);
static int ctz_addtzbyfile (tmo_t*, tmo_t, struct ctz*, struct ctz_info*, int);
static int ctz_addtzbyrule (tmo_t*, tmo_t, struct ctz*, struct ctz_info*, int);
static int ctz_subleapsec (tmo_t*, tmo_t, struct ctz_info*, struct ctz*, int);
static int ctz_addleapsec (tmo_t*, tmo_t, struct ctz*, int);



int
ctz_utc2local (otstamp, tstamp, info, tz, flags)
	tmo_t					*otstamp, tstamp;
	int					tz;
	struct ctz_info	*info;
	int					flags;
{
	struct ctz	*ctz;
	int			ret;

	if (!otstamp) return RERR_PARAM;
	if (info) bzero (info, sizeof (struct ctz_info));
	ctz = ctz_get (tz);
	if (!ctz) return RERR_INVALID_TZ;
	/* first we correct leap seconds */
	ret = ctz_subleapsec (&tstamp, tstamp, info, ctz, flags);
	if (!RERR_ISOK(ret)) return ret;
	/* now correct timezone */
	ret = ctz_addtz (&tstamp, tstamp, ctz, info, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (otstamp) *otstamp = tstamp;
	return RERR_OK;
}



int
ctz_local2utc (otstamp, tstamp, tz, flags)
	tmo_t	*otstamp, tstamp;
	int	tz;
	int	flags;
{
	struct ctz	*ctz;
	int			ret;

	if (!otstamp) return RERR_PARAM;
	ctz = ctz_get (tz);
	if (!ctz) return RERR_INVALID_TZ;
	/* first correct timezone */
	ret = ctz_subtz (&tstamp, tstamp, ctz, flags);
	if (!RERR_ISOK(ret)) return ret;
	/* now we correct leap seconds */
	ret = ctz_addleapsec (&tstamp, tstamp, ctz, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (otstamp) *otstamp = tstamp;
	return RERR_OK;
}

static
int
ctz_subleapsec (otstamp, tstamp, info, ctz, flags)
	tmo_t					*otstamp, tstamp;
	struct ctz_info	*info;
	struct ctz			*ctz;
	int					flags;
{
	int						lnum, actnum, totnum;
	int						max, min, pivot;
	struct ctz_leapinfo	*ptr;

	if (!otstamp || !ctz) return RERR_PARAM;
	if (!ctz->hasfile) return RERR_OK;
	lnum = ctz->file.leapnum;
	ptr = ctz->file.leaplist;
	if (!ptr || lnum <= 0) return RERR_OK;
	actnum = totnum = 0;
	if (tstamp < ptr[0].leapsec) goto fin;
	if (tstamp >= ptr[lnum-1].leapsec) {
		pivot = lnum-1;
		goto found;
	}
	min = 0; max = lnum-2;
	while (min <= max) {
		pivot = (max + min) / 2;
		if (tstamp >= ptr[pivot].leapsec && tstamp < ptr[pivot+1].leapsec) break;
		if (tstamp < ptr[pivot].leapsec) {
			max = pivot-1;
		} else {
			min = pivot+1;
		}
	}
found:
	while (pivot >= 0 && tstamp == ptr[pivot].leapsec) {
		actnum++;
		tstamp -= 1000000;	/* 1 sec */
		pivot--;
	}
	if (pivot >= 0) {
		totnum = ptr[pivot].leapnum;
		tstamp -= totnum * 1000000;
	}
	totnum += actnum;
fin:
	if (otstamp) *otstamp = tstamp;
	if (info) {
		info->actleapsec = actnum;
		info->totleapsec = totnum;
	}
	return RERR_OK;
}

static
int
ctz_addleapsec (otstamp, tstamp, ctz, flags)
	tmo_t			*otstamp, tstamp;
	struct ctz	*ctz;
	int			flags;
{
	int						lnum, addnum;
	int						max, min, pivot;
	struct ctz_leapinfo	*ptr;

	if (!otstamp || !ctz) return RERR_PARAM;
	if (!ctz->hasfile) return RERR_OK;
	lnum = ctz->file.leapnum;
	ptr = ctz->file.leaplist;
	if (!ptr || lnum <= 0) return RERR_OK;
	pivot = -1;
	if (tstamp < ptr[0].leapsec) goto fin;
	if (tstamp >= ptr[lnum-1].leapsec) {
		pivot = lnum-1;
		goto found;
	}
	min = 0; max = lnum-2;
	while (min <= max) {
		pivot = (max + min) / 2;
		if (tstamp >= ptr[pivot].leapsec && tstamp < ptr[pivot+1].leapsec) break;
		if (tstamp < ptr[pivot].leapsec) {
			max = pivot-1;
		} else {
			min = pivot+1;
		}
	}
found:
	if (pivot < 0) goto fin;
	addnum=0;
	do {
		tstamp += (ptr[pivot].leapnum-addnum) * 1000000;
		addnum = ptr[pivot].leapnum;
		pivot++;
	} while ((pivot < lnum ) && (tstamp >= ptr[pivot].leapsec));
fin:
	if (otstamp) *otstamp = tstamp;
	return RERR_OK;
}


static
int
ctz_addtz (otstamp, tstamp, ctz, info, flags)
	tmo_t					*otstamp, tstamp;
	struct ctz			*ctz;
	struct ctz_info	*info;
	int					flags;
{
	int			ret;

	if (!otstamp || !ctz) return RERR_PARAM;
	if (ctz->hasfile) {
		ret = ctz_addtzbyfile (otstamp, tstamp, ctz, info, flags);
		if ((ret == RERR_OUTOFRANGE) && ctz->hasrule) {
			ret = ctz_addtzbyrule (otstamp, tstamp, ctz, info, flags);
		}
		if (!RERR_ISOK(ret)) return ret;
	} else if (ctz->hasrule) {
		ret = ctz_addtzbyrule (otstamp, tstamp, ctz, info, flags);
		if (!RERR_ISOK(ret)) return ret;
	} else {
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}


static
int
ctz_subtz (otstamp, tstamp, ctz, flags)
	tmo_t			*otstamp, tstamp;
	struct ctz	*ctz;
	int			flags;
{
	int			ret;

	if (!otstamp || !ctz) return RERR_PARAM;
	if (ctz->hasfile) {
		ret = ctz_subtzbyfile (otstamp, tstamp, ctz, flags);
		if ((ret == RERR_OUTOFRANGE) && ctz->hasrule) {
			ret = ctz_subtzbyrule (otstamp, tstamp, ctz, flags);
		}
		if (!RERR_ISOK(ret)) return ret;
	} else if (ctz->hasrule) {
		ret = ctz_subtzbyrule (otstamp, tstamp, ctz, flags);
		if (!RERR_ISOK(ret)) return ret;
	} else {
		return RERR_NOT_SUPPORTED;
	}
	return RERR_OK;
}


#define CTZ_F_APPLYFUTURE	4
//#define CTZ_F_APPLYPAST		8

static
int
ctz_addtzbyfile (otstamp, tstamp, ctz, info, flags)
	tmo_t					*otstamp, tstamp;
	struct ctz			*ctz;
	struct ctz_info	*info;
	int					flags;
{
	int					i;
	struct ctz_ttinfo	*ptr;

	if (!ctz) return RERR_PARAM;
	ptr = ctz->file.ttinfolist;
	for (i=0; i<ctz->file.transnum; i++) {
		if (tstamp < ctz->file.translist[i].transtime) break;
		ptr = ctz->file.translist[i].transinfo;
	}
	if (ptr) {
		tstamp += ptr->gmtoff;
		if (info) {
			info->isdst = ptr->isdst;
			info->abbrname = ptr->abbrname;
			info->gmtoff = ptr->gmtoff;
		}
		if (otstamp) *otstamp = tstamp;
	} else {
		if (info) {
			info->isdst = 0;
			info->abbrname = NULL;
			info->gmtoff = 0;
		}
		if (otstamp) *otstamp = tstamp;
	}
	if ((i>=ctz->file.transnum) && (flags & CTZ_F_APPLYFUTURE)) {
		return RERR_OUTOFRANGE;
	}
	return RERR_OK;
}

static
int
ctz_subtzbyfile (otstamp, tstamp, ctz, flags)
	tmo_t			*otstamp, tstamp;
	struct ctz	*ctz;
	int			flags;
{
	int					i;
	struct ctz_ttinfo	*ptr;
	tmo_t					off;

	if (!ctz) return RERR_PARAM;
	ptr = ctz->file.ttinfolist;
	off = 0;
	for (i=0; i<ctz->file.transnum; i++) {
		if (tstamp - off < ctz->file.translist[i].transtime) break;
		ptr = ctz->file.translist[i].transinfo;
		off = ptr->gmtoff;
	}
	tstamp -= off;
	if (otstamp) *otstamp = tstamp;
	if ((i>=ctz->file.transnum) && (flags & CTZ_F_APPLYFUTURE)) {
		return RERR_OUTOFRANGE;
	}
	return RERR_OK;
}

static
int
ctz_addtzbyrule (otstamp, tstamp, ctz, info, flags)
	tmo_t					*otstamp, tstamp;
	struct ctz			*ctz;
	struct ctz_info	*info;
	int					flags;
{
	if (!ctz || !otstamp) return RERR_PARAM;
	if (!ctz->rule.hasdst) {
		tstamp += ((tmo_t)ctz->rule.janoff) * 1000000LL;
		*otstamp = tstamp;
		return RERR_OK;
	}

	/* to be done ... */
	return RERR_NOT_SUPPORTED;
}


static
int
ctz_subtzbyrule (otstamp, tstamp, ctz, flags)
	tmo_t			*otstamp, tstamp;
	struct ctz	*ctz;
	int			flags;
{
	if (!ctz || !otstamp) return RERR_PARAM;
	if (!ctz->rule.hasdst) {
		tstamp -= ((tmo_t)ctz->rule.janoff) * 1000000LL;
		*otstamp = tstamp;
		return RERR_OK;
	}

	/* to be done ... */
	return RERR_NOT_SUPPORTED;
}


#if 0
struct ctz_transblock {
	struct ctz_transition	*translist;
	int							transnum;
	tmo_t							begyear;
	int							begisfirst;
	tmo_t							endyear;
	int							endisfirst;
};

struct ctz_transblock	*transblock;
int							transblocknum;

static
int
ctz_searchtransition (trans, tstamp, rule, flags)
	struct ctz_ttinfo	*trans;
	tmo_t					tstamp;
	struct ctzrule		*rule;
	int					flags;
{
	int							i,j;
	struct ctz_transblock	*tb, blk;
	struct ctz_transition	*p;
	struct cjg_bd_t			tbd;
	tmo_t							xstamp, ystamp;

	if (!rule || !trans) return RERR_PARAM;
	for (i=0; i<rule->transblocknum; i++) {
		tb = rule->transblock + i;
		if (tbd->transnum > 0 && tbd->translist[i].gmtoff >= tstamp) break;
		for (j=1; j<tb->transnum; j++) {
			if (tb->translist[j].gmtoff > tstamp) {
				/* we have found a transition */
				j--;
				*trans = tb->translist[j];
				return RERR_OK;
			}
		}
	}
	/* we havn't found anything, so calculate new transitions */
	/* first of all, we break down current time using utc */
	ret = cjg_breakdown (&tbd, tstamp, CTZ_T_UTC, 0);
	if (!RERR_ISOK(ret)) return ret;
	ret = ctz_calctrans (&xstamp, tbd.year, &tbd, &(rule->first), rule->janoff);
	if (!RERR_ISOK(ret)) return ret;
#if 0
	bzero (&blk, sizeof (struct ctz_transblock));
	blk->translist = malloc (2*sizeof (struct ctz_transition));
	if (!(blk->translist
#endif
	if (tstamp >= xstamp) {
		if ((i < rule-transblocknum) && (tb->begyear == tbd.year)) {
			p = realloc (tb->translist, sizeof (struct ctz_transition) * 
								(tb->transnum + 1));
			if (!p) return RERR_NOMEM;
			rule->translist = p;
			memmove (p+1, p, szieof (struct ctz_transition) * tbd->transnum);
			bzero (p, sizeof (ctz_transition));
			
}


static
int
ctz_calctrans (otstamp, year, actbd, srule, off, flags)
	tmo_t						*otstamp, year, off;
	struct cjg_bd_t		*actbd;
	struct ctzswitchrule	*srule;
	int						flags;
{
	struct cjg_bd_t	tbd, tbd2;
	tmo_t					tstamp;
	int					flags;
	int					week;

	if (!srule) return RERR_PARAM;
	bzero (&tbd, sizeof (struct cjg_bd_t));
	tbd.year = year;
	tbd.hasyear = 1;
	if (srule->usejd) {
		tbd.yday = srule->jd;
		if (!(srule->cntleap) && srule->jd > 59) {
			if (actbd && (year == actbd->year)) {
				tbd.yday += actbd->isleap;
			} else {
				tbd.yday += cjp_isleap2 (year);
			}
		}
		tbd.hasyday = 1;
	} else {
		tbd.mon = srule->mon;
		tbd.hasmon = 1;
		/* we first calculate first of month */
		if (srule->week < 0 || srule->week == 5) {
			switch (srule->mon) {
			case 1: case 3: case 5: case 7: case 8: case 10: case 12:
				tbd.day = 31;
				break;
			case 4: case 6: case 9: case 11:
				tbd.day = 30;
				break;
			case 2:
			default:
				tbd.day = 28;
				break;
			}
		} else {
			tbd.day = 1;
		}
		tbd.hasday = 1;
	}
	tbd.hour = srule->sec / 3600;
	tbd.min = (srule->sec % 3600) / 60;
	tbd.sec = srule->sec % 60;
	tbd.hashour = 1;
	tbd.hasmin = 1;
	tbd.hassec = 1;
	ret = cjg_compose (&tstamp, &tbd, CTZ_T_UTC, CJG_F_FORCERULE);
	if (!RERR_ISOK(ret)) return ret;
	tstamp -= off;
	if (srule->usejd) {
		if (otstamp) *otstamp = tstamp;
		return RERR_OK;
	}
	ret = cjg_breakdown (&tbd, tstamp, CTZ_T_UTC, 0);
	if (!RERR_ISOK(ret)) return ret;
	bzero (&tbd2, sizeof (struct cjg_bd_t));
	tbd2.wyear = tbd.year;
	tbd2.haswyear = 1;
	/* in the following we assume, the switch does not occur in the first days,
	 *	nor in the last days of a year.
	 */
	week = srule.week;
	if (week < 0) {
		week ++;
	} else if (week == 5) {
		week = 0;
	} else {
		week--;
	}
	tbd2.weeknum = tbd.weeknum + week;
	tbd2.wday = srule->day;
	tbd2.hasweeknum = 1;
	tbd2.haswday = 1;
	tbd2.hour = srule->sec / 3600;
	tbd2.min = (srule->sec % 3600) / 60;
	tbd2.sec = srule->sec % 60;
	tbd2.hashour = 1;
	tbd2.hasmin = 1;
	tbd2.hassec = 1;
	ret = cjg_compose (&tstamp, &tbd, CTZ_T_UTC, CJG_F_FORCERULE);
	if (!RERR_ISOK(ret)) return ret;
	tstamp -= off;
	if (otstamp) *otstamp = tstamp;
	return RERR_OK;
}




#endif
























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
