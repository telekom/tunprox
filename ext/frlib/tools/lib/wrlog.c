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
#include <unistd.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <string.h>
#include <strings.h>




#include <fr/base.h>

#include "wrlog.h"



static const char *PROG="wrlog";

int
wrlog_usage ()
{
	PROG=fr_getprog ();
	printf (	"%s: usage: %s [<options>]\n", PROG, PROG);
	printf (	"  options are:\n"
				"    -c <config file>   config file to use\n"
				"    -e                 log to stderr as well\n"
				"    -p <progname>      programme name\n"
				"    -l <level>         log level\n"
				"    -M <module>        module name\n"
				"    -m <message>       log message\n"
				"    -P <pid>           process id\n"
				"    -E <errno>         adds a colon followed by a string for errno\n"
				"\n");
	return 0;
}




int
wrlog_main (argc, argv)
	int	argc;
	char	**argv;
{
	int			c, level=LOG_NONE;
	const char	*msg="<empty log message>";
	int			log_stderr=0;
	char			*module = NULL;
	int			myerrno = -1;
	int			ret;

	PROG=fr_getprog ();

	while ((c=getopt (argc, argv, "c:hp:l:m:t:P:eM:E:"))!=-1) {
		switch (c) {
		case 'h':
			wrlog_usage ();
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'p':
			slog_set_prog (optarg);
			break;
		case 'l':
			level = slog_getlevel (optarg);
			break;
		case 'M':
			module = optarg;
			break;
		case 'm':
		case 't':
			msg= optarg;
			break;
		case 'P':
			slog_setpid (atoi (optarg));
			break;
		case 'e':
			log_stderr = 1;
			break;
		case 'E':
			myerrno=atoi (optarg);
			break;
		}
	}
	if (log_stderr) level |= LOG_STDERR;
	if (myerrno >= 0) {
		ret = smlogf (module, level, "%s: %s", msg, rerr_getstr2s (myerrno));
	} else {
		ret = smlogf (module, level, "%s", msg);
	}
	return ret;
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
