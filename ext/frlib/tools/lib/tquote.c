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

#include <fr/base.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include "tquote.h"

static const char	*PROG = "tquote";


int
tquote_usage ()
{
	PROG=fr_getprog ();
	printf ("%s: usage: %s [<options>]\n", PROG, PROG);
	printf ("  options are:\n"
				"    -h           this help screen\n"
				"    -f <file>    file to quote\n"
				"");
	return 0;
}





int
tquote_main (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*fname = NULL;
	char			*buf = NULL, *obuf;
	const char	*text = NULL;
	int			flen;
	int			ret, c;

	PROG=fr_getprog ();
	while ((c=getopt (argc, argv, "hf:")) != -1) {
		switch (c) {
		case 'h':
			tquote_usage ();
			return 0;
		case 'f':
			fname = optarg;
			break;
		}
	}
	if (optind < argc) {
		text = argv[optind];
	}
	if (!text && fname) {
		buf = fop_read_fn2 (fname, &flen);
		if (!buf) return RERR_NOMEM;
		text = buf;
	} else {
		if (!text) text = "";
		flen = strlen (text);
	}
	obuf = top_quotestrdup2 (text, flen, TOP_F_QUOTEHIGH|TOP_F_NOQUOTESIGN|TOP_F_QUOTE_NONL);
	if (!obuf) return RERR_NOMEM;
	flen = strlen (obuf);
	ret = write (1, obuf, flen);
	if (!RERR_ISOK(ret)) return RERR_SYSTEM;
	if (!flen || obuf[flen-1] != '\n') write (1, "\n", 1);
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
