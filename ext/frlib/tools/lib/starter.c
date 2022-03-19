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
#ifdef Linux
#include <getopt.h>
#endif
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <time.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>


#include <fr/base.h>

#include "starter.h"


static const char * PROG=NULL;
#define VERSION		"0.2"




int
starter_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>] [<progs to start>]\n",
						PROG, PROG);
	printf ("  options are:\n"
				"    -h                 - this help screen\n"
				"    -V                 - version information\n"
				"    -c <config file>   - alternative config file\n"
				"    -y <fd>            - file descriptor to read "
													"password from\n"
				"    -w                 - log errors during execution\n"
				"\n");
	return 0;
}

static int			config_read = 0;
static const char	*null_all_progs[] = {NULL};
static char			**all_progs = (char**)null_all_progs;
static char			**_all_progs = NULL;
static int			allow_any_prog = 0;

static int prog_is_in (char**, char*);
static int start_all_progs (char**, int);
static int start_all_progs2 (char**, int);
static int read_config();
static int parse_all_progs (char*);


int
starter_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	c;
	int	ret;
	char	**progs;
	int	flags = 0;
	int	fd = -1;

	PROG = fr_getprog ();
	while ((c=getopt (argc, argv, "Vhc:y:w")) != -1) {
		switch (c) {
		case 'h':
			starter_usage ();
			exit (0);
		case 'V':
			printf ("%s: version %s\n", PROG, VERSION);
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'y':
			fd = cf_atoi (optarg);
			break;
		case 'w':
			flags |= STPRG_F_WATCH_ERRORS;
			break;
		}
	}
	if (optind < argc) {
		progs = malloc (sizeof (char*) * (argc-optind+1));
		if (progs) {
			int i;
			for (i=0; i < argc-optind; i++) {
				progs[i] = argv[optind+i];
			}
			progs[i] = NULL;
		}
	}

	ret = scf_fdread (fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error reading secure config file: %s", 
							rerr_getstr3 (ret));
		return ret;
	}

	setup_env ();

	CF_MAYREAD;
	if (allow_any_prog) {
		ret = start_all_progs (progs, flags);
	} else {
		ret = start_all_progs2 (progs, flags);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error starting progs: %s", rerr_getstr3 (ret));
		return ret;
	}

	exit (0);
	return 0;
}



static
int
prog_is_in (progs, prog)
	char	**progs, *prog;
{
	char	**p;

	if (!progs) return 1;
	if (!prog) return 0;
	for (p=progs; *p; p++)
		if (!strcmp (*p, prog)) return 1;
	return 0;
}


static
int
start_all_progs (progs, flags)
	char	**progs;
	int	flags;
{
	char	**p;
	int	ret, ret2;

	CF_MAYREAD;
	if (!progs) progs = (char **) all_progs;
	ret = RERR_OK;
	for (p=progs; *p; p++) {
		ret2 = startprog (*p, flags);
		if (ret2 != RERR_OK) ret = ret2;
	}
	return ret;
}



static
int
start_all_progs2 (progs, flags)
	char	**progs;
	int	flags;
{
	char	**p;
	int	ret, ret2;

	CF_MAYREAD;
	ret = RERR_OK;
	for (p= (char **)all_progs; *p; p++) {
		if (prog_is_in (progs, *p)) {
			ret2 = startprog (*p, flags);
			if (ret2 != RERR_OK) ret = ret2;
		}
	}
	return ret;
}




static
int
read_config()
{
	char	* str;

	cf_begin_read ();
	str = strdup (cf_getval2 ("all_progs", ""));
	parse_all_progs (str);
	allow_any_prog = cf_isyes (cf_getval ("allow_any_prog"));
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
}


static
int
parse_all_progs (progs)
	char	*progs;
{
	char	*str;
	int	num = 0;

	if (!progs) progs=(char*)"";
	if (_all_progs) free (_all_progs);
	all_progs = (char**) null_all_progs;
	_all_progs = malloc (sizeof (char*)*2);
	if (!_all_progs) return RERR_NOMEM;
	_all_progs[0] = progs;
	str = strtok (progs, ";/, \t");
	while (str) {
		num++;
		_all_progs = realloc (_all_progs, (2+num)*sizeof (char*));
		if (!_all_progs) return RERR_NOMEM;
		_all_progs[num] = str;
		str = strtok (NULL, ";/, \t");
	}
	_all_progs[num+1] = NULL;
	all_progs = _all_progs+1;

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
