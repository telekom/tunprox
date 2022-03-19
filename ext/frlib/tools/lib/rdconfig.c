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
#ifdef Linux
#include <getopt.h>
#endif

#include <fr/base/config.h>
#include <fr/base/errors.h>
#include <fr/base/prtf.h>
#include <fr/base/secureconfig.h>
#include <fr/base/frinit.h>

#include "rdconfig.h"


static const char	*PROG=NULL;
#define MYPROG	(PROG?PROG:"rdconfig")


int
rdconfig_usage ()
{
	PROG = fr_getprog ();
	fprintf (stderr, "%s: usage: %s [<options>] [<variable> [<index>]]\n"
		"\t<options> are:\n"
		"\t-p               print variable names and values\n"
		"\t-v               print only value (default)\n"
		"\t-a               print the whole array\n"
		"\t-A               print the whole config file\n"
		"\t-t               print table tab_valid_chars\n"
		"\t-s               read secure config file if present\n"
		"\t-c <config file> alternative config file, the default depends\n"
		"\t                 on first part of programme name (before "
								"underscore)\n",
		MYPROG, MYPROG);
	return 0;
}


static int print_var (char*, char*, char*, int);
static int print_var2 (char*, int, char*, int);


int
rdconfig_main (argc, argv)
	int	argc;
	char	**argv;
{
	int			c;
	const char	*s;
	char			*s2;
	int			table=0;
	const char	*var;
	int32_t		*tab=NULL;
	int			ret;
	int			array = 0;
	int			i, num;
	int			prtval=1;
	char			_r__errstr[96];
	int			printall=0;
	int			readscf=0;

	PROG = fr_getprog ();
	
	while ((c=getopt (argc, argv, "Aahc:tvps")) != -1) {
		switch (c) {
		case 'h':
			rdconfig_usage();
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 't':
			table = 1;
			break;
		case 'a':
			array=1;
			break;
		case 'A':
			printall=1;
			break;
		case 'v':
			prtval=1;
			break;
		case 'p':
			prtval=0;
			break;
		case 's':
			readscf=1;
			break;
		}
	}
	if (readscf) {
		scf_askread ();
	}
	if (table) {
		if (optind < argc) {
			var = argv[optind];
		} else {
			var = "tab_valid_chars";
		}
		s = cf_getval (var);
		if (!s) {
			printf ("%s: variable >>%s<< does not exist in %s\n", MYPROG, var,
						cf_get_cfname()?cf_get_cfname():"<NULL>");
			exit (0);
		}
		s2 = strdup (s);
		if (!s2) return RERR_NOMEM;
		ret = cf_parse_table (s2, &tab);
		if (!RERR_ISOK(ret)) {
			fprintf (stderr, "%s: cf_parse_table () returned %s\n", 
									MYPROG, rerr_getstr3 (ret));
			exit (1);
		}
		cf_print_table (&tab);
		free (s2);
	} else if (optind+2 < argc) {
		s = cf_get2arr (argv[optind], argv[optind+1], argv[optind+2]);
		print_var (argv[optind], asprtf ("%s,%s", argv[optind+1], 
					argv[optind+2]), (char*)s, prtval);
	} else if (optind+1 < argc) {
		s = cf_getarr (argv[optind], argv[optind+1]);
		print_var (argv[optind], argv[optind+1], (char*)s, prtval);
	} else if (optind < argc && array) {
		num = cf_getnumarr (argv[optind]);
		for (i=0; i<num; i++) {
			s = cf_getarrx (argv[optind], i);
			print_var2 (argv[optind], i, (char*)s, prtval);
		}
	} else if (optind < argc) {
		s = cf_getvar (argv[optind]);
		print_var (argv[optind], NULL, (char*)s, prtval);
	} else if (printall) {
		if (cf_print_config () != RERR_OK) exit (1);
	} else {
		/* do nothing */
	}
	return 0;
}



static
int
print_var2 (var, idx, val, prtval)
	char	*var, *val;
	int	idx, prtval;
{
	char	stridx[32];

	sprintf (stridx, "%d", idx);
	return print_var (var, stridx, val, prtval);
}


static
int
print_var (var, idx, val, prtval)
	char	*var, *idx, *val;
	int	prtval;
{
	if (prtval) {
		printf ("%s\n", val?val:"");
	} else if (val && (index (val, '\n') || index (val, '\'') 
										 || index (val, '"'))) {
		if (idx) {
			printf ("%s[%s] = {%s}\n", var, idx, val);
		} else {
			printf ("%s = {%s}\n", var, val);
		}
	} else {
		if (idx) {
			printf ("%s[%s]=\"%s\"\n", var, idx, val?val:"<NULL>");
		} else {
			printf ("%s=\"%s\"\n", var, val?val:"<NULL>");
		}
	}
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
