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

#include <fr/base.h>

#include "myprog.h"

static const char *PROG="myprog";


static
int
usage ()
{
	printf ("%s: usage: %s [<options>] [<file>...]\n", PROG, PROG);
	printf ("  options are:\n"
				"    -h                 this help screen\n"
				"    -c <config file>   non standard config file\n"
				"    -d                 don't fork to background (for debugging)\n"
				"    -y <fd>            read password from file descriptor\n"
				"\n");
	return 0;
}


int
main (argc, argv)
	int	argc;
	char	**argv;
{
	int	c, fd=-1, debug=0;
	int	ret;

	/* initialize frlib */
#if 0	/* neccessary only if to be initialized with a flag != 0 */
	frinit (argc, argv, 0);
#endif
	/* get program name for usage function */
	PROG=fr_getprog ();

	/* parse command line */
	while ((c=getopt (argc, argv, "hdc:y:")) != -1) {
		switch (c) {
		case 'h':
			usage ();
			exit (0);
		case 'd':
			debug=1;
			break;
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'y':
			fd = cf_atoi (optarg);
			break;
		}
	}

	/* read secure config file */
	ret = scf_fdread (fd);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_WARN2, "error reading secure config file (%s), continuing "
								"anyway...", rerr_getstr3 (ret));
	}

	/* change user to what is set in config file */
	ret = set_user ();
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_WARN2, "error changing user (%s), continuing anyway...",
								rerr_getstr3 (ret));
	}
	/* set environment variables according to config file */
	ret = set_environment ();
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_WARN2, "error setting environment variables (%s), "
								"continuing anyway...", rerr_getstr3 (ret));
	}

	if (!debug) {
		/* fork into background and daemonize my prog */
		ret = frdaemonize ();
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR2, "error daemonizing: %s", rerr_getstr3(ret));
			exit (ret);
		}
	}

#if 0		/* uncomment if wanted */
	/* set callback function to reread config file on SIGHUP signal */
	ret = cf_hup_reread ();
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_WARN2, "error starting hup reread for config file: %s",
									rerr_getstr3(ret));
	}
#endif

	/* do what to be done ... */
	ret = myprog (...);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error in execution(): %s", rerr_getstr3(ret));
		exit (ret);
	}

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
