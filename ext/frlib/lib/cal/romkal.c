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
//#define _XOPEN_SOURCE	/* glibc2 needs this */
#include <time.h>

#include <string.h>
#include <strings.h>
#include <errno.h>
#include <ctype.h>

#include "errors.h"
#include "romkal.h"
#include "roman.h"
#include "cjg.h"
#include "textop.h"


struct idnontable {
	int 	nonis,
			idibus,
			maxday;
};

static const struct idnontable idnontable[] = {
				{5, 13, 31},
				{5, 13, 28},
				{7, 15, 31},
				{5, 13, 30},
				{7, 15, 31},
				{5, 13, 30},
				{7, 15, 31},
				{5, 13, 31},
				{5, 13, 30},
				{7, 15, 31},
				{5, 13, 30},
				{5, 13, 31}};

struct nomentable {
	const char	*full1,
					*full2,
					*abbr;
};

static const struct nomentable nomentable[] = {
	{"ante diem", "", "a.d."},
	{"pridie", "", "pr."},
	{"Kalendis", "Kalendas", "Kal."},
	{"Nonis", "Nonas", "Non."},
	{"Idibus", "Idus", "Id."},
	{"ab urbe condita", "", "a.u.c."},
	{"prae urbe condita", "", "p.u.c."},
	{"bis", "", "bis"},
	{"Ianuariis", "Ianuarias", "Ian"},
	{"Februariis", "Februarias", "Feb"},
	{"Martiis", "Martias", "Mar"},
	{"Apriliis", "Aprilias", "Apr"},
	{"Maiis", "Maias", "Mai"},
	{"Iuniis", "Iunias", "Iun"},
	{"Iuliis", "Iulias", "Iul"},
	{"Augustis", "Augustas", "Aug"},
	{"Septembris", "Septembras", "Sep"},
	{"Octobris", "Octobras", "Oct"},
	{"Novembris", "Novembras", "Nov"},
	{"Decembris", "Decembras", "Dec"},
	};

#define AD		0
#define PR		1
#define KAL		2
#define NON		3
#define ID		4
#define AUC		5
#define PUC		6
#define BIS		7
#define MONTH	8
#define MAXNOM	19

#define NOMEN2(what,num,abbrev)  \
				((abbrev) ? nomentable[(what)].abbr : (((num)==1) ? \
				nomentable[(what)].full1 : nomentable[(what)].full2))
#define ADJRANGE(a,min,max) (((a)<(min))?(min):(((a)>(max))?(max):(a)))
#define NOMEN(what,num,abbr) NOMEN2(ADJRANGE((what),0,MAXNOM),(num),(abbr))
#define MONTHNAME(mon,num,abbr) \
				NOMEN(((mon)-1+MONTH),(num),(abbr))

static int _xprtf (char*, int, const char*, ...);
static int _checkentry (int, char*, int*);
static int _checkentry2 (struct nomentable*, char*, int*);
static int _mystrcmp (const char*, const char*, int);


int
romandate (tstr, tlen, tbd, flags)
	char					*tstr;
	int					tlen;
	struct cjg_bd_t	*tbd;
	int					flags;
{
	int 					mon;
	int					day;
	int					year;
	char					*s;
	int 					isnomen1;
	int					isleap;
	struct idnontable	*idnon;
	const char			*idnonstr;
	const char			*adstr;
	const char			*bis="";
	int					neednum=0;
	const char			*monthstr;
	const char			*aucstr;
	char					daystr[32];
	char					yearstr[64];
	int					abbr, ret, wlen;

	if (!tbd) return RERR_PARAM;
	abbr = (flags & ROMKAL_F_ABBR) ? 1 : 0;
	day = tbd->day;
	mon = tbd->mon;
	year = tbd->year;
	isleap = tbd->isleap ? 1 : 0;		/* ignore the special case sweden */
	idnon = (struct idnontable*) &idnontable[mon-1];
	isnomen1 = (day == 1) || (day == idnon->nonis) || (day == idnon->idibus);
	if (day == 1 || day > idnon->idibus) {
		idnonstr = NOMEN (KAL, isnomen1, abbr);
	} else if (day <= idnon->nonis) {
		idnonstr = NOMEN (NON, isnomen1, abbr);
	} else {
		idnonstr = NOMEN (ID, isnomen1, abbr);
	}
	if (isleap && mon == 2 && day > 24) {
		if (day == 25) {
			bis = NOMEN (BIS, 1, abbr);
		}
		day--;
	}
	if (!isnomen1) {
		if ((day == idnon->nonis-1) || (day == idnon->idibus-1) || 
							(day == idnon->maxday)) {
			adstr = NOMEN (PR, 1, abbr);
			if (day == idnon->maxday) {
				mon ++;
				if (mon > 12) {
					mon = 1;
					year ++;
				}
			}
		} else {
			adstr = NOMEN (AD, 1, abbr);
			neednum = 1;
			if (day < idnon->nonis) {
				day = idnon->nonis + 1 - day;
			} else if (day < idnon->idibus) {
				day = idnon->idibus + 1 - day;
			} else {
				day = idnon->maxday + 2 - day;
				mon ++;
				if (mon > 12) {
					mon = 1;
					year ++;
				}
			}
		}
	}

	if (neednum) {
		ret = num2roman (daystr, sizeof (daystr), day, 0);
		if (!RERR_ISOK(ret)) return ret;
	}

	monthstr = MONTHNAME (mon, isnomen1, abbr);
	if (year < 0) year += 1;
	year += 753;
	if (year <= 0) {
		year = (year - 1) * (-1);
		aucstr = NOMEN (PUC, 1, abbr);
	} else {
		aucstr = NOMEN (AUC, 1, abbr);
	}

	ret = num2roman (yearstr, sizeof (yearstr), year, 0);
	if (!RERR_ISOK(ret)) return ret;

	if (isnomen1) {
		ret = _xprtf (tstr, tlen, "%s %s %s %s", idnonstr, monthstr, 
								yearstr, aucstr);
	} else if (!neednum) {
		ret = _xprtf (tstr, tlen, "%s %s %s %s %s", adstr, idnonstr, monthstr,
								yearstr, aucstr);
	} else {
		ret = _xprtf (tstr, tlen, "%s %s%s %s %s %s %s", adstr, daystr, bis,
								idnonstr, monthstr, yearstr, aucstr);
	}
	if (!RERR_ISOK(ret)) return ret;
	wlen = ret;
	
	if (flags & ROMKAL_F_LCASE) {
		for (s=tstr; s && *s; s++) {
			*s = tolower (*s);
		}
	} else if (flags & ROMKAL_F_UCASE) {
		for (s=tstr; s && *s; s++) {
			if (*s == 'u') {
				*s = 'V';
			} else {
				*s = toupper (*s);
			}
		}
	}
	return wlen;
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



int
romandateparse (tstr, tbd, flags)
	char					*tstr;
	struct cjg_bd_t	*tbd;
	int					flags;
{
	char	*s;
	int	i, len, ret, bis=0, day;
	int	days, mon, year, neg=0;
	int	isid=0, isnon=0, iskal=0;

	if (!tstr || !tbd) return RERR_PARAM;
	if (!(flags & ROMKAL_F_NOZERO)) {
		bzero (tbd, sizeof (struct cjg_bd_t));
	}
	s = top_skipwhite (tstr);
	if (_checkentry (AD, s, &len)) {
		s = top_skipwhiteplus (s+len, ".");
		if (_checkentry (BIS, s, &len)) {
			s = top_skipwhiteplus (s+len, ".");
			bis=1;
		}
		ret = roman2num (&days, s, 0);
		if (!RERR_ISOK(ret)) return ret;
		s = top_skipwhiteplus (s+ret, ".");
	} else if (_checkentry (PR, s, &len)) {
		s = top_skipwhiteplus (s+len, ".");
		days = 2;
	} else {
		days = 1;
	}
	days--;
	if (_checkentry (ID, s, &len)) {
		isid=1;
		s = top_skipwhiteplus (s+len, ".");
	} else if (_checkentry (NON, s, &len)) {
		isnon=1;
		s = top_skipwhiteplus (s+len, ".");
	} else if (_checkentry (KAL, s, &len)) {
		iskal=1;
		s = top_skipwhiteplus (s+len, ".");
	} else {
		return RERR_INVALID_FORMAT;
	}
	for (i=0; i<12; i++) {
		if (_checkentry (MONTH+i, s, &len)) break;
	}
	if (i>=12) return RERR_PARAM;
	s = top_skipwhiteplus (s+len, ".");
	mon = i+1;
	ret = roman2num (&year, s, 0);
	if (!RERR_ISOK(ret)) return ret;
	s = top_skipwhiteplus (s+len, ".");
	if (_checkentry (AUC, s, &len)) {
		s+=len;
	} else if (_checkentry (PUC, s, &len)) {
		s+=len;
		neg=1;
	}
	len = s-tstr;
	/* now calculate day */
	if (neg) year = (-1)*year + 1;
	if (isid) {
		day = idnontable[mon-1].idibus - days;
	} else if (isnon) {
		day = idnontable[mon-1].nonis - days;
	} else if (iskal) {
		mon--;
		if (mon==0) {
			mon=12;
			year--;
		}
		if (cjg_isleap (year, tbd->rul) && mon==2 && days>=6 && !bis) days++;
		day = idnontable[mon-1].maxday + 1 - days;
	}
	year -= 753;
	if (year <= 0) year--;
	tbd->year = year;
	tbd->mon = mon;
	tbd->day = day;
	tbd->hasyear = 1;
	tbd->hasmon = 1;
	tbd->hasday = 1;
	return len;
}

static
int
_checkentry (num, tstr, olen)
	int	num, *olen;
	char	*tstr;
{
	if (num < 0 || num > MAXNOM) return 0;
	return _checkentry2 ((struct nomentable*)&(nomentable[num]), tstr, olen);
}

static
int
_checkentry2 (entry, tstr, olen)
	struct nomentable	*entry;
	char					*tstr;
	int					*olen;
{
	int	len;

	if (!entry || !tstr) return 0;
	len = strlen (entry->full1);
	if (!_mystrcmp (entry->full1, tstr, len)) {
		if (olen) *olen = len;
		return 1;
	}
	len = strlen (entry->full2);
	if (len && !_mystrcmp (entry->full2, tstr, len)) {
		if (olen) *olen = len;
		return 1;
	}
	len = strlen (entry->abbr);
	if (!_mystrcmp (entry->abbr, tstr, len)) {
		if (olen) *olen = len;
		return 1;
	}
	return 0;
}


#define MYISWHITE(c)	(iswhite(c) || (c)=='.')
static
int
_mystrcmp (str1, str2, len)
	const char	*str1, *str2;
	int			len;
{
	if (!str1 && !str2) return 0;
	if (len <= 0) return 0;
	if (!str1) return -1;
	if (!str2) return 1;
	for (; len && *str1 && *str2; len--, str1++, str2++) {
		if (MYISWHITE(*str1)) {
			if (!MYISWHITE(*str2)) return -1;
			for (; len && MYISWHITE(*str1); str1++, len--);
			len++; str1--;
			for (; MYISWHITE(*str2); str2++);
			str2--;
		} else {
			if (islower(*str1) < islower(*str2)) return -1;
			if (islower(*str1) > islower(*str2)) return 1;
		}
	}
	if (!len) return 0;
	if (!*str1 && *str2) return -1;
	if (*str1 && !*str2) return 1;
	return 0;
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
