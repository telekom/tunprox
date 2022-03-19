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


#include <fr/base/config.h>
#include <fr/base/frinit.h>
#include <fr/base/errors.h>
#include <fr/base/slog.h>
#include <fr/base/setenv.h>
#include <fr/base/secureconfig.h>


#include "scfmgr.h"



static const char * PROG=(char*)"scfmgr";
#define VERSION		"0.1"



#define MYPROG	(PROG?PROG:(char*)"scfmgr")

int
scfmgr_usage ()
{
	PROG=fr_getprog ();
	printf ("%s: usage: %s [<option>] <action>\n"
					"  possible actions are:\n"
					"\t-a <var=value>   add var to config file\n"
					"\t-d <var>         delete var from config file\n"
					"\t-C               create config file\n"
					"\t-l               list config file (decrypt)\n"
					"\t-e               encrypt config file\n"
					"\t-p               change passphrase\n"
					"  possible options are:\n"
					"\t-c <conifig file>        normal config file\n"
					"\t-f <secure config file>  secure config file to operate on\n"
					"\t-i <infile>              infile to operate\n"
					"\t-o <outfile>             outfile to write to\n"
					"\t\teither use -f or -i and -o\n", MYPROG, MYPROG);
	return 0;
}


int
scfmgr_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	c, ret;
	char	*var=NULL, *filename=NULL, 
			*outfile=NULL, *infile=NULL;
	char	action;

	PROG=fr_getprog ();
	while ((c=getopt (argc, argv, "hVc:f:i:o:a:d:lepC")) != -1) {
		switch (c) {
		case 'h':
			scfmgr_usage ();
			exit (0);
		case 'V':
			printf ("%s: version %s\n", PROG, VERSION);
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'f':
			filename = optarg;
			break;
		case 'i':
			infile = optarg;
			break;
		case 'o':
			outfile = optarg;
			break;
		case 'a':
			var=optarg;
			action = 'a';
			break;
		case 'd':
			var= optarg;
			action = 'd';
			break;
		case 'l':
			action = 'l';
			break;
		case 'e':
			action = 'e';
			break;
		case 'p':
			action = 'p';
			break;
		case 'C':
			action = 'c';
			break;
		}
	}


	switch (action) {
	case 'a':
		if (!infile) infile=filename;
		if (!outfile) outfile = infile;
		ret = scfmgr_addvar (infile, outfile, var);
		break;
	case 'd':
		if (!infile) infile=filename;
		if (!outfile) outfile = infile;
		ret = scfmgr_delvar (infile, outfile, var);
		break;
	case 'c':
		if (!outfile) outfile = filename;
		ret = scfmgr_createscf (outfile);
		break;
	case 'l':
		if (!infile) infile=filename;
		ret = scfmgr_listscf (infile, outfile);
		break;
	case 'e':
		if (!outfile) outfile = filename;
		ret = scfmgr_encryptscf (infile, outfile);
		break;
	case 'p':
		if (!infile) infile = filename;
		if (!outfile) outfile = infile;
		ret = scfmgr_chpasswd (infile, outfile);
		break;
	default:
		fprintf (stderr, "%s: missing action\n", PROG);
		return RERR_PARAM;
	}

	return ret;
}


int
scfmgr_addvar (infile, outfile, var)
	char	*infile, *outfile, *var;
{
	int	ret;
	char	_r__errstr[96];

	ret = scf_addvar (infile, outfile, var);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: inserting was NOT successfull: %s", 
					MYPROG, rerr_getstr3(ret));
		return ret;
	}
	fprintf (stderr, "%s: inserting was successfull\n", MYPROG);
	return ret;
}

int
scfmgr_delvar (infile, outfile, var)
	char	*infile, *outfile, *var;
{
	int	ret;
	char	_r__errstr[128];

	ret = scf_rmvar (infile, outfile, var);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: deleting var %s was NOT successfull (%s)\n",
					MYPROG, var, rerr_getstr3(ret));
		return ret;
	}
	fprintf (stderr, "%s: deleting var %s was successfull\n", MYPROG, var);
	return ret;
}

int
scfmgr_listscf (infile, outfile)
	char	*infile, *outfile;
{
	int 	ret;
	char	_r__errstr[128];

	ret = scf_list (infile, outfile);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: listing file %s was NOT successfull (%s)\n",
					MYPROG, ((infile && strcmp (infile, "-")) ? infile : 
					"<stdin>"), rerr_getstr3(ret));
		return ret;
	}
	return ret;
}

int
scfmgr_encryptscf (infile, outfile)
	char	*infile, *outfile;
{
	int 	ret;
	char	_r__errstr[128];

	ret = scf_encryptfile (infile, outfile);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: writing secure config file was NOT "
						"successfull (%s)\n", MYPROG, rerr_getstr3(ret));
		return ret;
	}
	return ret;
}


int
scfmgr_createscf (filename)
	char	*filename;
{
	int	ret;
	char	_r__errstr[128];

	ret = scf_create (filename);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: creating secure config file was NOT successfull "
							"(%s)\n", MYPROG, rerr_getstr3(ret));
		return ret;
	}
	fprintf (stderr, "%s: secure config file successfully created\n", MYPROG);
	return ret;
}


int
scfmgr_chpasswd (infile, outfile)
	char	*infile, *outfile;
{
	int	ret;
	char	_r__errstr[128];

	ret = scf_changepass (infile, outfile);
	if (!RERR_ISOK(ret)) {
		fprintf (stderr, "%s: changing passphrase NOT successfull (%s)\n",
						MYPROG, rerr_getstr3(ret));
		return ret;
	}
	fprintf (stderr, "%s: passphrase successfully changed\n", MYPROG);
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
