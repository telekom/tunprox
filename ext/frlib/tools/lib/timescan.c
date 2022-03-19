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
 * Portions created by the Initial Developer are Copyright (C) 2003-2020
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>






#include <fr/base.h>
#include <fr/connect.h>
#include <fr/cal.h>

#include "timescan.h"



static const char *PROG="timescan";

int
timescan_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>] <timestring>\n", PROG, PROG);
	printf ( "   options are:\n"
				"      -h        this help screen\n"
				"      -r        reverse parsing (microseconds -> timestring)\n"
				"      -a        time is absolute time (reverse only)\n"
				"      -t        use form: YYYYMMDDTHH24MISS.MIS (reverse only)\n"
				"      -s        use seconds not microseconds\n"
				"      -m        use miliseconds not microseconds\n"
				"      -w        print weekday as string\n"
				"      -W        print weekday as number\n"
				"      -y        print year\n"
				"      -D        print day of year\n"
				"      -d        print days since 1970\n"
				"   if timestring is \"-\" or missing, read from stdin (1 per line)\n"
				"\n");
	return RERR_OK;
}


static int elab_norm (char*, int, tmo_t);
static int elab_rev (char*, int, int, tmo_t);
static int elab (char*, int, int, int, tmo_t);
static int prt_what (tmo_t tstamp, int what);


#define PRT_NORM		0
#define PRT_YEAR		1
#define PRT_YDAY		2
#define PRT_WDAY		3
#define PRT_DAYS		4
#define PRT_WDAY_STR	5



int
timescan_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	c;
	int	rev=0, tform=TMO_TIMESTRFORM_DIFF;
	tmo_t	factor = 1LL;
	char	*line;
	int	ret, i;
	int	what = 0;

	PROG = fr_getprog ();
	while ((c=getopt (argc, argv, "hratsmwydDW"))!=-1) {
		switch (c) {
		case 'h':
			timescan_usage ();
			return 0;
		case 'r':
			rev=1;
			break;
		case 'a':
			tform = TMO_TIMESTRFORM_ABS;
			break;
		case 't':
			tform = TMO_TIMESTRFORM_TFORM;
			break;
		case 's':
			factor = 1000000LL;
			break;
		case 'm':
			factor = 1000LL;
			break;
		case 'w':
			what = PRT_WDAY_STR;
			break;
		case 'W':
			what = PRT_WDAY;
			break;
		case 'y':
			what = PRT_YEAR;
			break;
		case 'D':
			what = PRT_YDAY;
			break;
		case 'd':
			what = PRT_DAYS;
			break;
		}
	}
	if (argc <= optind || ((optind+1 == argc) && !strcmp (argv[optind], "-"))) {
		while ((ret = fdcom_readln2 (0, &line, 0)) == RERR_OK) {
			ret = elab (line, rev, what, tform, factor);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR2, "error elaborating >>%s<<: %s", line, 
										rerr_getstr3(ret));
				return ret;
			}
			free (line);
		}
		if (ret != RERR_EOT) {
			FRLOGF (LOG_ERR2, "error reading line from stdin: %s", 
									rerr_getstr3(ret));
			return ret;
		}
	} else {
		for (i=optind; i<argc; i++) {
			ret = elab (argv[optind], rev, what, tform, factor);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR2, "error elaborating >>%s<<: %s", 
							argv[optind], rerr_getstr3(ret));
				return ret;
			}
		}
	}
	return RERR_OK;
}



static
int
elab (tstr, rev, what, tform, factor)
	char	*tstr;
	int	rev, tform, what;
	tmo_t	factor;
{
	int	ret;

	if (!tstr) return RERR_PARAM;
	if (!(tstr = strdup (tstr))) return RERR_NOMEM;
	if (rev) {
		ret = elab_rev (tstr, what, tform, factor);
	} else {
		ret = elab_norm (tstr, what, factor);
	}
	free (tstr);
	return ret;
}




static
int
elab_rev (tstr, what, tform, factor)
	char	*tstr;
	int	tform, what;
	tmo_t	factor;
{
	tmo_t	num;
	int	ret;
	char	buf[128];

	if (!tstr) return RERR_PARAM;
	num = atoll (tstr);
	num *= factor;
	if (what == PRT_NORM) {
		ret = tmo_prttimestr64 (buf, sizeof (buf), num, tform);
		if (!RERR_ISOK(ret)) return ret;
		printf ("%s\n", buf);
	} else {
		return prt_what (num, what);
	}
	return RERR_OK;
}



static
int
elab_norm (tstr, what, factor)
	char	*tstr;
	tmo_t	factor;
	int	what;
{
	tmo_t	num;

	if (!tstr) return RERR_PARAM;
	num = tmo_gettimestr64 (tstr);
	num /= factor;
	return prt_what (num, what);
}



static const char*	wdays[] = { "Mon", "Tue", "Wed", "Thu", "Fri", "Sat", "Sun" };
#define WDAY_STR(num)	(wdays[((num)%7+7)%7])
static
int
prt_what (tstamp, what)
	tmo_t	tstamp;
	int	what;
{
	switch (what) {
	case PRT_NORM:
		break;
	case PRT_DAYS:
		tstamp = tmo_getdaysince1970 (tstamp);
		break;
	case PRT_YDAY:
		tstamp = tmo_getdayofyear (tstamp);
		break;
	case PRT_WDAY:
	case PRT_WDAY_STR:
		tstamp = tmo_getweekday (tstamp);
		break;
	case PRT_YEAR:
		tstamp = tmo_getyear (tstamp);
		break;
	default:
		return RERR_PARAM;
	}
	if (what == PRT_WDAY_STR) {
		printf ("%s\n", WDAY_STR((int)tstamp));
	} else {
		printf ("%lld\n", (long long)tstamp);
	}
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
