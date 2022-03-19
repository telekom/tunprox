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
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/mman.h>

#include "tz.h"

#include <fr/base.h>



static int config_read = 0;
static const char	*gtzname = NULL;
static const char	*gfrtzfile = "/etc/frtz";
static const char	*glctzfile = "/etc/localtime";
static const char	*gfrtzdir = "/usr/share/frtz";
static const char	*glctzdir = NULL;

static int read_config ();

static int ctz_hasdefault = 0;
static int ctz_hasutc = 0;

static int ctz_parsetz (struct ctzrule*, const char*, int);
static int ctz_cmpswitchrule (struct ctzswitchrule*, struct ctzswitchrule*);
static int ctz_scanswitchrule (struct ctzswitchrule*, const char*, const char**, int);
static int ctz_scanoff (int*, const char*, const char**, int);

static int ctz_parsetzfile (struct ctz_tzfile*, const char*, int);
static int ctz_parsetzfile2 (struct ctz_tzfile*, const char*, const char*);
static int ctz_checkparsetzfile (struct ctz_tzfile*, const char*, const char*, int);
static int ctz_doparsetzfile (struct ctz_tzfile*, const char*, const char*, int);
static int ctz_parseowntzfile (struct ctz_tzfile*, const char*, const char*, int);

static int ctz_parse (struct ctz*, const char*, int);
static int ctz_doset (int, const char*, int);
static int ctz_setutc ();
static int ctz_setdefault (char*, int);

static struct ctz *ctz_find (int);
static int ctz_remove (int);
static int ctz_insert (struct ctz*, int);

static int ctz_tzfilehfree (struct ctz_tzfile*);
static int ctz_rulehfree (struct ctzrule*);
static int ctz_hfree (struct ctz*);


static int ctz_deftz = 1;




struct ctz*
ctz_get (num)
	int	num;
{
	int	ret = RERR_OK;

	if (num < CTZ_NUM_MIN) {
		switch (num) {
		case CTZ_TZ_UTC:
			if(!ctz_hasutc) {
				ret = ctz_setutc ();
			}
			break;
		case CTZ_TZ_SYSTEM:
			if (!ctz_hasdefault) {
				ret = ctz_setdefault (NULL, 0);
			}
			break;
		case CTZ_TZ_DEFAULT:
			return ctz_get (ctz_deftz);
		default:
			ret = RERR_OUTOFRANGE;
			break;
		}
		if (!RERR_ISOK(ret)) return NULL;
	}
	return ctz_find (num);
}

int
ctz_set (tzstr, flags)
	char	*tzstr;
	int	flags;
{
	if (flags & CTZ_F_DEFAULT) {
		return ctz_setdefault (tzstr, flags);
	}
	return ctz_doset (-1, tzstr, flags);
}



int
ctz_del (num)
	int	num;
{
	struct ctz	*ctz;

	ctz = ctz_find (num);
	if (!ctz) return RERR_NOT_FOUND;
	ctz_remove (num);
	ctz_hfree (ctz);
	free (ctz);
	if (num == ctz_deftz) ctz_deftz = CTZ_TZ_SYSTEM;
	return RERR_OK;
}

int
ctz_setdef (num)
	int	num;
{
	if (num < 0 || num == 2) return RERR_INVALID_TZ;
	if (num > 2 && !ctz_find (num)) return RERR_INVALID_TZ;
	ctz_deftz = num;
	return RERR_OK;
}


/*
 * static functions
 */

static
int
ctz_hfree (ctz)
	struct ctz	*ctz;
{
	if (!ctz) return RERR_PARAM;
	if (ctz->hasrule) ctz_rulehfree (&(ctz->rule));
	if (ctz->hasfile) ctz_tzfilehfree (&(ctz->file));
	bzero (ctz, sizeof (struct ctz));
	return RERR_OK;
}

static
int
ctz_setdefault (tzstr, flags)
	char	*tzstr;
	int	flags;
{
	int	ret;

	if (ctz_hasdefault) {
		ctz_del (1);
		ctz_hasdefault = 0;
	}
	ret = ctz_doset (1, tzstr, flags);
	if (!RERR_ISOK(ret)) return ret;
	ctz_hasdefault = 1;
	return RERR_OK;
}

static
int
ctz_setutc ()
{
	int	ret;

	if (ctz_hasutc) return RERR_OK;
	ret = ctz_doset (0, "UTC+0000", 0);
	if (!RERR_ISOK(ret)) return ret;
	ctz_hasutc = 1;
	return RERR_OK;
}


static
int
ctz_doset (num, tzstr, flags)
	const char	*tzstr;
	int			flags, num;
{
	struct ctz	*ctz;
	int			ret;

	ctz = malloc (sizeof (struct ctz));
	if (!ctz) return RERR_NOMEM;
	bzero (ctz, sizeof (struct ctz));
	ret = ctz_parse (ctz, tzstr, flags);
	if (!RERR_ISOK(ret)) {
		ctz_hfree (ctz);
		free (ctz);
		return ret;
	}
	ret = ctz_insert (ctz, num);
	if (!RERR_ISOK(ret)) {
		ctz_hfree (ctz);
		free (ctz);
		return ret;
	}
	return RERR_OK;
}


static
int
ctz_parse (ctz, tzstr, flags)
	struct ctz	*ctz;
	const char	*tzstr;
	int			flags;
{
	int	ret;

	if (!ctz) return RERR_PARAM;
	bzero (ctz, sizeof (struct ctz));
	if (!tzstr) {
		tzstr = getenv ("FRTZ");
		if (tzstr) {
			flags &= ~CTZ_F_OLDFMT;
		} else {
			tzstr = getenv ("TZ");
			flags |= CTZ_F_OLDFMT;
		}
	}
	if (tzstr) {
		ret = ctz_parsetz (&(ctz->rule), tzstr, flags);
		if (!RERR_ISOK(ret)) return ret;
		if (ctz->rule.usefilespec) {
			tzstr = top_skipwhiteplus (tzstr, ":");
		} else {
			ctz->hasrule = 1;
			return RERR_OK;
		}
	}
	ret = ctz_parsetzfile (&(ctz->file), tzstr, flags);
	if (!RERR_ISOK(ret)) return ret;
	ctz->hasfile = 1;
	if (ctz->file.rulestr) {
		if (ctz->file.ruleoldstyle) {
			flags |= CTZ_F_OLDFMT;
		} else {
			flags &= ~CTZ_F_OLDFMT;
		}
		ret = ctz_parsetz (&(ctz->rule), ctz->file.rulestr, flags);
		if (RERR_ISOK(ret)) {
			ctz->hasrule = 1;
		}
	}
	if (!ctz->hasrule && !ctz->hasfile) return RERR_INTERNAL;
	return RERR_OK;
}


/* 
 * calculate tz - rule as specified in TZ environment variable 
 */

static
int
ctz_rulehfree (rule)
	struct ctzrule	*rule;
{
	if (!rule) return RERR_PARAM;
	bzero (rule, sizeof (struct ctzrule));
	return RERR_OK;
}

#ifndef MIN
# define MIN(a,b)	(((a)<(b))?(a):(b))
#endif
static
int
ctz_parsetz (rule, tzstr, flags)
	struct ctzrule	*rule;
	const char		*tzstr;
	int				flags;
{
	const char				*s;
	int						ret;
	struct ctzswitchrule	dummyswitch;
	char						dummybuf[16];
	int						dummyoff;

	if (!rule || !tzstr) return RERR_PARAM;
	bzero (rule, sizeof (struct ctzrule));
	tzstr = top_skipwhite (tzstr);
	for (s=tzstr; isalpha (*s) || *s == '/'; s++);
	if (!*s || *tzstr == ':') {
		rule->usefilespec = 1;
		return RERR_OK;
	}
	for (s=tzstr; isalpha (*s); s++);
	strncpy (rule->jantzname, tzstr, MIN(16-1,s-tzstr));
	rule->jantzname[MIN(16-1,s-tzstr)-1]=0;
	ret = ctz_scanoff (&(rule->janoff), s, &s, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (s) s = top_skipwhite (s);
	if (!s || !*s) return RERR_OK;

	rule->hasdst = 1;
	for (tzstr=s /* this is correct */; isalpha (*s); s++);
	strncpy (rule->augtzname, tzstr, MIN(16-1,s-tzstr));
	rule->augtzname[MIN(16-1,s-tzstr)]=0;
	s = top_skipwhite (s);
	if (*s == ',') {
		rule->augoff = rule->janoff+1;
	} else {
		ret = ctz_scanoff (&(rule->augoff), s, &s, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	tzstr = top_skipwhiteplus (s, ",");
	ret = ctz_scanswitchrule (&(rule->first), tzstr, &s, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (rule->first.sec < 0) rule->first.sec = 2*3600;
	tzstr = top_skipwhiteplus (s, ",");
	ret = ctz_scanswitchrule (&(rule->first), tzstr, &s, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (rule->second.sec < 0) rule->second.sec = 3*3600;
	if (ctz_cmpswitchrule (&(rule->first), &(rule->second)) > 0) {
		dummyswitch = rule->first;
		rule->first = rule->second;
		rule->second = dummyswitch;
		dummyoff = rule->janoff;
		rule->janoff = rule->augoff;
		rule->augoff = dummyoff;
		strcpy (dummybuf, rule->jantzname);
		strcpy (rule->jantzname, rule->augtzname);
		strcpy (rule->augtzname, dummybuf);
		rule->dstisreverse = 1;
	}
	return RERR_OK;
}


static const int monslen[] = { 0, 0, 31, 59, 90, 120, 151, 181, 212, 243, 
										273, 304, 334, 365 };

/* the following algorithm might fail if both dates are too close 
	(less then 5 days of difference) 
	but in praxis this is irrelavent
	(an exact algorithm needs information about the explicit year to which it 
	applies)
 */
static
int
ctz_cmpswitchrule (rule1, rule2)
	struct ctzswitchrule	*rule1, *rule2;
{
	int	day, week;

	if (!rule1 && !rule2) return 0;
	if (!rule1) return -1;
	if (!rule2) return 1;
	if (rule1->usejd) {
		if (!(rule2->usejd)) return (-1)*ctz_cmpswitchrule (rule2, rule1);
		if (rule1->jd == rule2->jd) return 0;
		if (rule1->jd < rule2->jd) return -1;
		return 1;
	}
	if (!(rule2->usejd)) {
		if (rule1->mon < rule2->mon) return -1;
		if (rule1->mon > rule2->mon) return 1;
		if (rule1->week < rule2->week) return -1;
		if (rule1->week > rule2->week) return 1;
		if (rule1->day < rule2->day) return -1;
		if (rule1->day > rule2->day) return 1;
		return 0;
	}
	day = monslen [rule1->mon] + rule1->day - 1;
	week = rule1->week;
	if (week < 0) {
		week = 5-week;
	}
	day += (week-1)*7 + 3;	/* fuzzy, but ok for us */
	if (day > rule2->jd) return 1;
	if (day < rule2->jd) return -1;
	return 0;
}


static
int
ctz_scanswitchrule (rule, str, estr, flags)
	struct ctzswitchrule	*rule;
	const char				*str, **estr;
	int						flags;
{
	int	day, mon, week, rev, hour, min,sec;
	char	*str2;

	if (!rule || !str || !estr) return RERR_PARAM;
	bzero (rule, sizeof (struct ctzswitchrule));
	str = top_skipwhite (str);
	*estr = str;
	switch (*str) {
	case 'J':
		rule->usejd = 1;
		str = top_skipwhite (++str);
		rule->jd = strtol (str, &str2, 10);
		str = str2;
		break;
	case '0': case '1': case '2': case '3': case '4':
	case '5': case '6': case '7': case '8': case '9':
		rule->usejd = 1;
		rule->cntleap = 1;
		rule->jd = strtol (str, &str2, 10);
		str = str2;
		break;
	case 'D':
		str = top_skipwhite (++str);
		mon = strtol (str, &str2, 10);
		str = str2;
		str = top_skipwhite (str);
		if (*str == '.') rev=1;
		str = top_skipwhiteplus (str, ".-");
		day = strtol (str, &str2, 10);
		str = str2;
		if (day == 0) return RERR_INVALID_FORMAT;
		if (rev) {
			rev = mon;
			mon = day;
			day = rev;
		}
		if (mon < 0 || mon > 12) return RERR_INVALID_DATE;
		if (day < 1) return RERR_INVALID_DATE;
		day += monslen[mon] - 1;
		rule->jd = day;
		rule->usejd = 1;
		break;
	case 'M': case 'W':
		mon = strtol (str, &str2, 10);
		str = str2;
		str = top_skipwhiteplus (str, ".");
		week = strtol (str, &str2, 10);
		str = str2;
		str = top_skipwhiteplus (str, ".");
		if (isdigit (*str)) {
			day = strtol (str, &str2, 10);
			str = str2;
		} else {
			day = 7;	/* sunday */
		}
		if (mon < 1 || mon > 12 || day < 0) return RERR_INVALID_DATE;
		if (week >= 5) week = -1;
		if (week <= -5) week = 1;
		if (week == 0) week = 1;
		day %= 7;
		if (day == 0) day = 7;
		rule->mon = mon;
		rule->week = week;
		rule->day = day;
		rule->useweek = 1;
		break;
	default:
		return RERR_INVALID_FORMAT;
	}
	str = top_skipwhiteplus (str, "/");
	hour = min = sec = 0;
	if (isdigit (*str)) {
		sscanf (str, "%d:%d:%d", &hour, &min, &sec);
		sec += min * 60 + hour * 3600;
		if (sec < 0) sec = 0;
		if (sec > 86399) sec = 86399;
		str = top_skipwhiteplus (str, "0123456789:");
	} else {
		sec = -1;	/* default to be set in caller function */
	}

	rule->sec = sec;
	str = top_skipwhite (str);
	if (estr) *estr = str;
	return RERR_OK;
}



static
int
ctz_scanoff (off, str, estr, flags)
	int			*off, flags;
	const char	*str, **estr;
{
	char	*s;
	int	hour, min, sec;

	if (!off || !str) return RERR_PARAM;
	if (estr) *estr = str;
	str = top_skipwhite (str);
	hour = strtol (str, &s, 10);
	min = sec = 0;
	if (!s) return RERR_INTERNAL;
	once {
		if (s-str > 3) {
			if (hour < 0) {
				min = hour % 60 + 60;
				hour = hour / 60 - 1;
			} else {
				min = hour % 60;
				hour /= 60;
			}
			break;
		}
		str = top_skipwhite (s);
		if (!(*str == ':' || *str == '.')) break;
		str = top_skipwhite (str);
		min = strtol (str, &s, 10);
		if (!s) return RERR_INTERNAL;
		str = top_skipwhite (s);
		if (!(*str == ':' || *str == '.')) break;
		str = top_skipwhite (str);
		sec = strtol (str, &s, 10);
		if (!s) return RERR_INTERNAL;
	}
	sec += 60 * min;
	if (hour < 0) sec *= -1;
	sec += 3600 * hour;
	if (flags & CTZ_F_OLDFMT) sec *= -1;
	*off = sec;
	str = top_skipwhite (s);
	if (estr) *estr = str;
	return RERR_OK;
}



/*
 * read tz file 
 */

static
int
ctz_tzfilehfree (tzfile)
	struct ctz_tzfile	*tzfile;
{
	if (!tzfile) return RERR_PARAM;
	if (tzfile->rulestr) free (tzfile->rulestr);
	if (tzfile->abbrstr) free (tzfile->abbrstr);
	if (tzfile->translist) free (tzfile->translist);
	if (tzfile->ttinfolist) free (tzfile->ttinfolist);
	if (tzfile->leaplist) free (tzfile->leaplist);
	bzero (tzfile, sizeof (struct ctz_tzfile));
	return RERR_OK;
}


static
int
ctz_parsetzfile (tzfile, tzname, flags)
	struct ctz_tzfile	*tzfile;
	const char			*tzname;	
	int					flags;
{
	int	usedef;
	int	ret;
	char	buf[4096];
	char	*tbuf;

	CF_MAYREAD;
	if (!tzfile) return RERR_PARAM;
	usedef = tzname ? 0 : 1;
	if (!tzname && !gtzname) {
		tbuf = fop_read_fn ("/etc/timezone");
		if (!tbuf) return RERR_NOT_FOUND;
		gtzname = tzname = top_stripwhite (tbuf, 0);
	} else if (!tzname) {
		tzname = gtzname;
	}
	if (!tzname || !*tzname) return RERR_NOT_FOUND;
	if (*tzname == '/') {
		if (!fop_exist (tzname)) return RERR_NOT_FOUND;
		return ctz_parsetzfile2 (tzfile, tzname, NULL);
	}
	ret = RERR_NOT_FOUND;
	if (usedef) {
		if (fop_exist (gfrtzfile)) {
			ret = ctz_parsetzfile2 (tzfile, gfrtzfile, tzname);
			if (RERR_ISOK(ret)) return RERR_OK;
		}
		if (fop_exist (glctzfile)) {
			ret = ctz_parsetzfile2 (tzfile, glctzfile, tzname);
			if (RERR_ISOK(ret)) return RERR_OK;
		}
	}
	buf[sizeof(buf)-1]=0;
	snprintf (buf, sizeof(buf)-1, "%s/%s", gfrtzdir, tzname);
	if (fop_exist (buf)) {
		ret = ctz_parsetzfile2 (tzfile, buf, tzname);
		if (RERR_ISOK(ret)) return RERR_OK;
	}
	if (glctzdir) {
		snprintf (buf, sizeof(buf)-1, "%s/%s", glctzdir, tzname);
		if (fop_exist (buf)) {
			ret = ctz_parsetzfile2 (tzfile, buf, tzname);
			if (RERR_ISOK(ret)) return RERR_OK;
		}
	}
	snprintf (buf, sizeof(buf)-1, "/usr/share/zoneinfo/%s", tzname);
	if (fop_exist (buf)) {
		ret = ctz_parsetzfile2 (tzfile, buf, tzname);
		if (RERR_ISOK(ret)) return RERR_OK;
	}
	snprintf (buf, sizeof(buf)-1, "/usr/lib/zoneinfo/%s", tzname);
	if (fop_exist (buf)) {
		ret = ctz_parsetzfile2 (tzfile, buf, tzname);
		if (RERR_ISOK(ret)) return RERR_OK;
	}
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}




static
int
ctz_parsetzfile2 (tzfile, filename, tzname)
	struct ctz_tzfile	*tzfile;
	const char			*filename;
	const char			*tzname;
{
	int			fd;
	struct stat	buf;
	void			*ptr;
	int			ret;

	if (!tzfile) return RERR_PARAM;
	fd = open (filename, O_RDONLY);
	if (fd < 0) return RERR_SYSTEM;
	if (fstat (fd, &buf) < 0) {
		close (fd);
		return RERR_SYSTEM;
	}
	ptr = mmap (NULL, buf.st_size, PROT_READ, MAP_SHARED, fd, 0);
	close (fd);
	if (ptr == MAP_FAILED) {
		return RERR_SYSTEM;
	}
	ret = ctz_checkparsetzfile (tzfile, tzname, ptr, buf.st_size);
	munmap (ptr, buf.st_size);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
ctz_checkparsetzfile (tzfile, tzname, ptr, len)
	struct ctz_tzfile	*tzfile;
	const char			*tzname;
	const char			*ptr;
	int					len;
{
	int	ret, ret2;

	if (!tzfile || !ptr || len <= 0) return RERR_PARAM;
	if (len < 8) return RERR_INVALID_FORMAT;
	ret = ctz_parseowntzfile (tzfile, tzname, ptr, len);
	if (!RERR_ISOK(ret)) {
		ret2 = ret;
		ret = ctz_doparsetzfile (tzfile, tzname, ptr, len);
		if (!RERR_ISOK(ret)) return ret2;
	}
	return RERR_OK;
}

/*
 * read tz file (own version)
 */


static
int
ctz_parseowntzfile (tzfile, tzname, ptr, len)
	struct ctz_tzfile	*tzfile;
	const char			*tzname;
	const char			*ptr;
	int					len;
{
	if (!tzfile || !ptr || len <= 0) return RERR_PARAM;
	return RERR_NOT_SUPPORTED;
}



/*
 * read tz file (libc version)
 */

static
int
ctz_doparsetzfile (tzfile, tzname, ptr, len)
	struct ctz_tzfile	*tzfile;
	const char			*tzname;
	const char			*ptr;
	int					len;
{
	uint32_t			tzh_ttisgmtcnt,
			  			tzh_ttisstdcnt,
			  			tzh_leapcnt,
			  			tzh_timecnt,
			  			tzh_typecnt,
			  			tzh_charcnt,
			  			*iptr;
	uint32_t			tzh_ttisgmtcnt64,
			  			tzh_ttisstdcnt64,
			  			tzh_leapcnt64,
			  			tzh_timecnt64,
			  			tzh_typecnt64,
			  			tzh_charcnt64;
	int	  			use64bit = 0;
	int	  			sz32, sz64, i, j;
	uint32_t			*transptr;
	uint64_t			*transptr64;
	unsigned char	*localidx, *ttinfoptr, *abbrname;
	unsigned char	*ruleptr, /* *isgmtptr, *isstdptr, */ *leapptr;
	int				rulelen;
	uint32_t			dummy32;
	uint64_t			dummy64;

	if (!tzfile) return RERR_PARAM;
	if (!ptr || ptr == MAP_FAILED || len < 0) return RERR_PARAM;
	if (len < 4) return RERR_INVALID_FORMAT;
	if (strncmp (ptr, "TZif", 4) != 0) return RERR_INVALID_FORMAT;
	if (len < 44) return RERR_INVALID_FORMAT;

	/* read header */
	if (ptr[4] == '2') use64bit=1;
	iptr = (uint32_t*)(void*)(ptr+20);
	bo_cv32 (&tzh_ttisgmtcnt, iptr[0], BO_FROM_BE);
	bo_cv32 (&tzh_ttisstdcnt, iptr[1], BO_FROM_BE);
	bo_cv32 (&tzh_leapcnt, iptr[2], BO_FROM_BE);
	bo_cv32 (&tzh_timecnt, iptr[3], BO_FROM_BE);
	bo_cv32 (&tzh_typecnt, iptr[4], BO_FROM_BE);
	bo_cv32 (&tzh_charcnt, iptr[5], BO_FROM_BE);
	sz32 = 44;
	sz32 += tzh_timecnt * 5;
	sz32 += tzh_typecnt * 6;
	sz32 += tzh_charcnt;
	sz32 += tzh_leapcnt * 4;
	sz32 += tzh_ttisstdcnt + tzh_ttisgmtcnt;
	if (len < sz32) return RERR_INVALID_FORMAT;
	if (len < sz32 + 44) use64bit = 0;
	if (use64bit) {
		iptr = (uint32_t*)(void*)(ptr+20+sz32);
		bo_cv32 (&tzh_ttisgmtcnt64, iptr[0], BO_FROM_BE);
		bo_cv32 (&tzh_ttisstdcnt64, iptr[1], BO_FROM_BE);
		bo_cv32 (&tzh_leapcnt64, iptr[2], BO_FROM_BE);
		bo_cv32 (&tzh_timecnt64, iptr[3], BO_FROM_BE);
		bo_cv32 (&tzh_typecnt64, iptr[4], BO_FROM_BE);
		bo_cv32 (&tzh_charcnt64, iptr[5], BO_FROM_BE);
		sz64 = 44;
		sz64 += tzh_timecnt64 * 9;
		sz64 += tzh_typecnt64 * 6;
		sz64 += tzh_charcnt64;
		sz64 += tzh_leapcnt64 * 8;
		sz64 += tzh_ttisstdcnt64 + tzh_ttisgmtcnt64;
		if (len < (sz32 + sz64)) use64bit = 0;
	}
	if (use64bit) {
		ptr += sz32;
		tzh_ttisgmtcnt = tzh_ttisgmtcnt64;
		tzh_ttisstdcnt = tzh_ttisstdcnt64;
		tzh_leapcnt = tzh_leapcnt64;
		tzh_timecnt = tzh_timecnt64;
		tzh_typecnt = tzh_typecnt64;
		tzh_charcnt = tzh_charcnt64;
	}

	/* set pointer */
	ptr += 44;
	if (use64bit) {
		transptr64 = (uint64_t*)ptr;
		ptr += tzh_timecnt * 8;
	} else {
		transptr = (uint32_t*)ptr;
		ptr += tzh_timecnt * 4;
	}
	localidx = (unsigned char *)ptr;
	ptr += tzh_timecnt;
	ttinfoptr = (unsigned char *)ptr;
	ptr += tzh_typecnt * 6;
	abbrname = (unsigned char *)ptr;
	ptr += tzh_charcnt;
	leapptr = (unsigned char *)ptr;
	if (use64bit) {
		ptr += tzh_leapcnt * 12;
	} else {
		ptr += tzh_leapcnt * 8;
	}
	/* isstdptr = (unsigned char *)ptr; */
	ptr += tzh_ttisstdcnt;
	/* isgmtptr = (unsigned char *)ptr; */
	ptr += tzh_ttisgmtcnt;
	if (len > (sz32 + sz64)) {
		ruleptr = (unsigned char*)ptr;
		rulelen = len - (sz32 + sz64);
	} else {
		ruleptr = NULL;
	}

	/* copy and convert data */
	bzero (tzfile, sizeof (struct ctz_tzfile));

	/* copy strings */
	if (ruleptr) {
		tzfile->rulestr = malloc (rulelen+1);
		if (!tzfile->rulestr) return RERR_NOMEM;
		strncpy (tzfile->rulestr, (char*)ruleptr, rulelen);
		tzfile->rulestr[rulelen] = 0;
		tzfile->ruleoldstyle = 1;
	}
	tzfile->abbrstr = malloc (tzh_charcnt+1);
	if (!tzfile->abbrstr) return RERR_NOMEM;
	memcpy (tzfile->abbrstr, abbrname, tzh_charcnt);
	tzfile->abbrstr[tzh_charcnt] = 0;

	/* copy ttinfo structure */
	if (tzh_typecnt < 1) return RERR_INVALID_FORMAT;
	tzfile->ttinfolist = malloc (sizeof (struct ctz_ttinfo) * tzh_typecnt);
	if (!tzfile->ttinfolist) return RERR_NOMEM;
	bzero (tzfile->ttinfolist, sizeof (struct ctz_ttinfo)*tzh_typecnt);
	tzfile->ttinfonum = tzh_typecnt;
	for (i=0; i<(ssize_t)tzh_typecnt; i++) {
		bo_cv32 (&dummy32, *(uint32_t*)ttinfoptr, BO_FROM_BE);
		tzfile->ttinfolist[i].gmtoff = ((int64_t)(int32_t)dummy32) * 1000000LL;
		tzfile->ttinfolist[i].isdst = ttinfoptr[4];
		tzfile->ttinfolist[i].abbrname = tzfile->abbrstr + ttinfoptr[5];
		ttinfoptr += 6;
	}

	/* copy transition list */
	tzfile->translist = malloc ((tzh_timecnt+1) * sizeof (struct ctz_transition));
	if (!tzfile->translist) return RERR_NOMEM;
	tzfile->transnum = tzh_timecnt+1;
	/* dummy transition, set smallest number possible */
	tzfile->translist[0].transtime = (tmo_t)1 << (sizeof (tmo_t) * 8 - 1);
	tzfile->translist[0].transinfo = tzfile->ttinfolist;
	for (i=0; i<(ssize_t)tzh_timecnt; i++) {
		if (use64bit) {
			bo_cv64 (&dummy64, transptr64[i], BO_FROM_BE);
		} else {
			bo_cv32 (&dummy32, transptr[i], BO_FROM_BE);
			dummy64 = dummy32;
		}
		dummy64 *= 1000000LL;
		tzfile->translist[i+1].transtime = dummy64;
		j = localidx[i];
		tzfile->translist[i+1].transinfo = tzfile->ttinfolist + j;
		/* consider isstd and isgmt */
#if 0	/* do we really need that ?? */
		if ((j<tzh_ttisgmtcnt) && isgmtptr[j]) {
			/* do nothing - we already have gmt */
		} else if (tzfile->ttinfolist[j].isdst && !((j<tzh_ttisstdcnt) 
																		&& isstdptr[j])) {
			/* add dstoffset minus old dstoffset */
		} else {
			/* add stdoffset minus old stdoffset */
		}
#endif
	}

	/* copy leap seconds */
	if (tzh_leapcnt > 0) {
		tzfile->leaplist = malloc (tzh_leapcnt * sizeof (struct ctz_leapinfo));
		tzfile->leapnum = tzh_leapcnt;
		for (i=0; i<(ssize_t)tzh_leapcnt; i++) {
			if (use64bit) {
				bo_cv64 (&dummy64, *(uint64_t*)leapptr, BO_FROM_BE);
				leapptr += 8;
			}else {
				bo_cv32 (&dummy32, *(uint32_t*)leapptr, BO_FROM_BE);
				dummy64 = dummy32;
				leapptr += 4;
			}
			dummy64 *= 1000000LL;
			tzfile->leaplist[i].leapsec = dummy64;
			bo_cv32 (&dummy32, *(uint32_t*)leapptr, BO_FROM_BE);
			tzfile->leaplist[i].leapnum = dummy32;
			leapptr += 4;
		}
	}

	/* we are done */
	return RERR_OK;
}





/*
 * number handling functions
 */

static struct ctz	**g_tzlist = NULL;
static int			g_tzlistlen = 0;


static
int
ctz_insert (ctz, num)
	struct ctz	*ctz;
	int			num;
{
	struct ctz	**ptr;

	if (!ctz) return RERR_PARAM;
	if (num < 0) {
		for (num=CTZ_NUM_MIN; num<g_tzlistlen && g_tzlist[num]; num++);
	}
	if (num >= g_tzlistlen) {
		ptr = realloc (g_tzlist, (num+16)*sizeof (void*));
		if (!ptr) return RERR_NOMEM;
		bzero (ptr+g_tzlistlen, (num+16-g_tzlistlen) * sizeof (void*));
		g_tzlist = ptr;
		g_tzlistlen = num+16;
	}
	g_tzlist[num] = ctz;
	return num;
}

static
int
ctz_remove (num)
	int	num;
{
	if (num < 0 || num >= g_tzlistlen) return RERR_OUTOFRANGE;
	g_tzlist[num] = NULL;
	return RERR_OK;
}

static
struct ctz*
ctz_find (num)
	int	num;
{
	if (num < 0 || num >= g_tzlistlen) return NULL;
	return g_tzlist[num];
}


/*
 * config reader
 */


static
int
read_config ()
{
	cf_begin_read ();
	gtzname = cf_getval ("timezone");
	gfrtzfile = cf_getval2 ("tzfile", "/etc/frtz");
	glctzfile = cf_getval2 ("tzfile", "/etc/localtime");
	gfrtzdir = cf_getval2 ("tzdir", "/usr/share/frtz");
	glctzdir = cf_getval ("lctzdir");

	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
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
