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
#include <signal.h>
#include <sys/types.h>

#include <fr/base.h>
#include <fr/event.h>

#include "varmon.h"


static const char	*PROG=(char*) "varmon";



static void serverterm (int);



int
varmon_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>]\n", PROG, PROG);
	printf (	"    options are:\n"
				"\t-h                 - this help screen\n"
				"\t-c <config file>   - use an alternative config file\n"
				"\t-C <def conf file> - alt. defaultc config string\n"
				"\t-d                 - start server\n"
				"\t-D                 - debug / don't daemonize\n"
				"\t-g <grp>           - use varmon group <grp>\n"
				"\t-f <file>          - use <file> as (in->)filter\n"
				"\t-F <filterid>      - use <filterid> in config file\n"
				"\t-Y                 - signal father after startup (server only)\n"
				"\t-N                 - don't change log name\n"
				"");
	return 0;
}




int
varmon_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	server=0, debug=0;
	char	*grp=(char*)"varmon";
	int	ret;
	char	*filter=NULL, *filterid=NULL;
	int	c;
	int	sigfather=0;
	pid_t	pid;
	int	noname=0;

	PROG = fr_getprog ();
	while ((c = getopt (argc, argv, "hc:C:dDg:f:F:Y")) != -1) {
		switch (c) {
		case 'h':
			varmon_usage ();
			return 0;
			break;
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'C':
			cf_default_cfname (optarg, 0);
			break;
		case 'd':
			server = 1;
			break;
		case 'D':
			debug = 1;
			break;
		case 'g':
			grp = optarg;
			break;
		case 'f':
			filter = optarg;
			break;
		case 'F':
			filterid = optarg;
			break;
		case 'Y':
			sigfather = 1;
			break;
		case 'N':
			noname = 1;
			break;
		};
	}
	if (server && !debug) {
		frdaemonize ();
	}
	signal (SIGPIPE, SIG_IGN);
	if (server) {
		if (!noname) slog_set_prog ("varmon-daemon");
		ret = varmon_serverinit (grp, filter, filterid);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error initializing server: %s", rerr_getstr3(ret));
			return ret;
		}
		if (sigfather) {
			pid = getppid ();
			if (pid > 0) kill (pid, SIGUSR1);
		}
		varmon_serverloop ();
	} else {
		FRLOGF (LOG_ERR2, "client function not implemented yet\n");
	}
	return RERR_OK;
}


int
varmon_serverinit (grp, filter, filterid)
	const char	*grp, *filter, *filterid;
{
	int	ret, id;

	ret = eddi_start ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting eddi: %s", rerr_getstr3(ret));
		return ret;
	}
	if (filterid) {
		ret = eddi_addfilterid (filterid, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error setting filter id >>%s<<: %s",
						filterid, rerr_getstr3(ret));
		}
	} else if (filter) once {
		id = evf_new ();
		if (id < 0) {
			FRLOGF (LOG_WARN, "error creating new event filter: %s",
								rerr_getstr3(id));
			break;
		}
		ret = evf_addfile (id, filter, EVF_F_IN);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error parsing event filter >>%s<<: %s",
								filter, rerr_getstr3(ret));
			evf_release (id);
			break;
		}
		ret = eddi_addfilter (id);
		evf_release (id);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error setting event filter >>%s<<: %s",
								filter, rerr_getstr3(ret));
			break;
		}
	}
	ret = varmon_addgrp (grp, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing varmon group >>%s<<: %s",
								grp?grp:"<default>", rerr_getstr3(ret));
		return ret;
	}
	if (signal (SIGTERM, serverterm) == SIG_ERR) {
		FRLOGF (LOG_WARN, "cannot set termination handler: %s",
								rerr_getstr3(RERR_SYSTEM));
	}
	return RERR_OK;
}

int
varmon_serverfinish ()
{
	int	ret;

	ret = varmon_closeall ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error closing varmon cleanly: %s", rerr_getstr3(ret));
	}
	eddi_finish ();
	return ret;
}


#define TIMEOUT	500000LL		/* 500 ms */
void
varmon_serverloop ()
{
	int	ret;
	int	lasterr=0;

	while (1) {
		ret = varmon_pollall (TIMEOUT);
		if (!RERR_ISOK(ret) && ret != RERR_TIMEDOUT) {
			if (ret != lasterr) {
				FRLOGF (LOG_WARN, "error in poll: %s", rerr_getstr3(ret));
			}
			tmo_sleep (TIMEOUT/2);
			lasterr = ret;
		} else {
			lasterr = 0;
		}
	}
}


static
void
serverterm (sig)
	int	sig;
{
	int	ret;

	ret = varmon_serverfinish ();
	exit (ret);
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
