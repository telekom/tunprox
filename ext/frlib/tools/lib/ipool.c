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
#include <stdint.h>

#include <fr/base.h>

#include "ipool.h"

static const char	*PROG="pool";

int
ipool_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>] [<list of ranges>]\n"
				"  options are:\n"
				"    -h          this help screen\n"
				"    -c <file>   alternative config file\n"
				"    -I          print ip not int\n"
				"    -x          print number in hex\n"
				"    -a <range>  range to add (as without -a)\n"
				"    -A <file>   add range from file\n"
				"    -s <range>  subtract range\n"
				"    -S <file>   subtract range from file (can be >>-<< for stdin)\n"
				"    -C <num>    choose num numbers\n"
				"    -n <num>    each value choosen with n-1 following reserved\n"
				"", PROG, PROG);
	return RERR_OK;
}

int
ipool_main (argc, argv)
	int	argc;
	char	**argv;
{
	int				i, c, ret;
	struct ipool	pool = IPOOL_NULL;
	struct ipool	subpool = IPOOL_NULL;
	int				prtflags=0, seq=1, num=1, choose=0, has=0;
	uint64_t			val;

	while ((c=getopt (argc, argv, "hc:Ia:A:s:S:C:n:x")) != -1) {
		switch (c) {
		case 'h':
			ipool_usage ();
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'I':
			prtflags |= IPOOL_F_PRT_IP;
			break;
		case 'x':
			prtflags |= IPOOL_F_PRT_HEX;
			break;
		case 'a':
			ret = ipool_add (&pool, optarg, 0);
			if (!RERR_ISOK(ret)) {
				SLOGFE (LOG_ERR2, "cannot add range (%s): %s", optarg,
							rerr_getstr3(ret));
				return ret;
			}
			break;
		case 'A':
			ret = ipool_addfile (&pool, optarg, 0);
			if (!RERR_ISOK(ret)) {
				SLOGFE (LOG_ERR2, "cannot add range file (%s): %s", optarg,
							rerr_getstr3(ret));
				return ret;
			}
			break;
		case 's':
			ret = ipool_add (&subpool, optarg, 0);
			if (!RERR_ISOK(ret)) {
				SLOGFE (LOG_ERR2, "cannot add range (%s): %s", optarg,
							rerr_getstr3(ret));
				return ret;
			}
			break;
		case 'S':
			ret = ipool_addfile (&subpool, optarg, 0);
			if (!RERR_ISOK(ret)) {
				SLOGFE (LOG_ERR2, "cannot add range file (%s): %s", optarg,
							rerr_getstr3(ret));
				return ret;
			}
			break;
		case 'C':
			num = atoi (optarg);
			choose = 1;
			break;
		case 'n':
			seq = atoi (optarg);
			choose = 1;
			break;
		}
	}
	for (i=optind; i<argc; i++) {
		ret = ipool_add (&pool, argv[i], 0);
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR2, "cannot add range (%s): %s", argv[i],
						rerr_getstr3(ret));
			return ret;
		}
	}
	ret = ipool_subtract_pool (&pool, &subpool);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error subtracting pools: %s", rerr_getstr3(ret));
		return ret;
	}
	if (choose) {
		for (i=0; i<num; i++) {
			ret = ipool_choose (&val, &pool, seq);
			if (!RERR_ISOK(ret)) {
				SLOGFE (LOG_ERR2, "error choosing sequence %d: %s", seq,
							rerr_getstr3(ret));
				return ret;
			}
			if (has) printf (", ");
			ipool_print_num (val, prtflags);
			has = 1;
		}
		if (has) printf ("\n");
	} else {
		ipool_print (&pool,prtflags);
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
