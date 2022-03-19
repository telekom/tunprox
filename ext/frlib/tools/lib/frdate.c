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
#include <fr/cal.h>

#include "frdate.h"

static const char	*PROG="frdate";


int
frdate_usage ()
{
	PROG=fr_getprog ();
	printf ("%s: usage: %s [<options>]\n", PROG, PROG);
	printf ("  options are:\n"
				"    -f <fmt>     output format\n"
				"    -i <fmt>     input format\n"
				"    -d <date>    date to parse\n"
				"    -n           use nano seconds, not microseconds as base\n"
				"    -t           use (relative) time not date\n"
				"");
	return 0;
}




int
frdate_main (argc, argv)
	int	argc;
	char	**argv;
{
	int			c;
	const char	*fmt=NULL;
	tmo_t			now;
	char			*tstr = NULL;
	int			ret;
	char			_r__errstr[96];
	char			*ifmt=NULL, *dstr = NULL;
	int			flags = 0;

	PROG=fr_getprog ();
	while ((c=getopt (argc, argv, "hf:i:d:nt")) != -1) {
		switch (c) {
		case 'h':
			frdate_usage ();
			return 0;
		case 'f':
			fmt = optarg;
			break;
		case 'i':
			ifmt = optarg;
			break;
		case 'd':
			dstr=optarg;
			break;
		case 't':
			flags |= CJG_F_ISDELTA;
			break;
		case 'n':
			flags |= CJG_F_NANO;
			break;
		default:
			fprintf (stderr, "%s: invalid option (%c)\n", PROG, c);
			return RERR_PARAM;
		}
	}
	if (!fmt && (argc > optind)) {
		fmt = argv[optind];
		if (*fmt == '+') fmt++;
	}
	if (!fmt) fmt="%c";
	if (dstr && ifmt) {
		ret = cjg_strptime2 (&now, dstr, ifmt, flags);
		if (!RERR_ISOK(ret)) {
			fprintf (stderr, "%s: error in strptime: %s\n", PROG, rerr_getstr3(ret));
			return ret;
		}
	} else {
		if (flags & CJG_F_NANO) {
			now = tmo_nowns ();
		} else {
			now = tmo_now ();
		}
	}
	ret = cjg_astrftime2 (&tstr, fmt, now, flags);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: error in strftime: %s\n", PROG, rerr_getstr3(ret));
		return ret;
	}
	printf ("%s\n", tstr);
	free (tstr);
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
