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

#ifndef _R__FRLIB_LIB_CAL_TZ_H
#define _R__FRLIB_LIB_CAL_TZ_H

#include <stdint.h>
#include <fr/base/tmo.h>


#ifdef __cplusplus
extern "C" {
#endif


#define CTZ_TZ_UTC		0
#define CTZ_TZ_SYSTEM	1	/* system settings */
#define CTZ_TZ_DEFAULT	2	/* what was set with ctz_setdef () */
#define CTZ_NUM_MIN		3	/* used internally to indicate the min. free timezone */


#define CTZ_F_NONE		0x00
#define CTZ_F_OLDFMT		0x01
#define CTZ_F_DEFAULT	0x02



/* structures for tz rule */
struct ctzswitchrule {
	struct {
		uint32_t	usejd:1,
					useweek:1,
					cntleap:1;
	};
	union {
		int		jd;
		struct {
			int	mon,
					week,
					day;
		};
	};
	int	sec;	/* second of day */
};

struct ctzrule {
	struct {
		uint32_t	usefilespec:1,
					dstisreverse:1,
					hasdst:1;
	};
	char	jantzname[16];
	char	augtzname[16];
	int	janoff;
	int	augoff;
	struct ctzswitchrule	first,
								second;
};


/* structures for tzfile */
struct ctz_ttinfo {
	tmo_t	gmtoff;
	int	isdst;
	char	*abbrname;
};
struct ctz_transition {
	tmo_t					transtime;
	struct ctz_ttinfo	*transinfo;
};
struct ctz_leapinfo {
	tmo_t	leapsec;
	int	leapnum;	/* number of leap secons till now */
};

struct ctz_tzfile {
	struct {
		uint32_t	ruleoldstyle:1;
	};
	char							*rulestr;
	char							*abbrstr;	/* buffer that holds abbriviation names */
	struct ctz_transition	*translist;
	int							transnum;
	struct ctz_ttinfo			*ttinfolist;
	int							ttinfonum;
	struct ctz_leapinfo		*leaplist;
	int							leapnum;
};


/* overall struct */
struct ctz {
	struct {
		uint32_t	hasrule:1,
					hasfile:1;
	};
	/* we might have both, so we cannot use a union */
	struct ctzrule		rule;
	struct ctz_tzfile	file;
};








/* sets default time zone */
int ctz_setdef (int tz);

/* creates new timezone, that's returned */
int ctz_set (char *tzstr, int flags);

/* converts tz to struct */
struct ctz *ctz_get (int num);

/* removes timezone from system */
int ctz_del (int num);











#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_CAL_TZ_H */
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
