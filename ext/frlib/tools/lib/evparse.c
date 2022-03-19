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
#include <errno.h>
#include <stdint.h>


#include <fr/base.h>
#include <fr/event.h>

#include "evparse.h"


static const char	*PROG="evparse";


//#define ACT_NONE			0
#define ACT_NAME			1
#define ACT_TNAME			2
#define ACT_ARG			3
#define ACT_INSARG		4
#define ACT_TARG			5
#define ACT_INSTARG		6
#define ACT_ATTR			7
#define ACT_RMTARGET		8
#define ACT_RMATTR		9
#define ACT_PRTPARSED	10
#define ACT_PRTSTR		11
#define ACT_PRTNAME		12
#define ACT_PRTTNAME		13
#define ACT_HASTARGET	14
#define ACT_PRTARG		15
#define ACT_PRTTARG		16
#define ACT_PRTATTR		17
#define ACT_ADDVAR		18
#define ACT_RMVAR			19
#define ACT_PRTVAR		20

#define ACT_MASK		0xff
#define ACT_TMASK		0xff00
#define ACT_TINT		0x0100
#define ACT_TFLOAT	0x0200
#define ACT_TSTRREP	0x0400
#define ACT_TDATE		0x0800
#define ACT_TTIME		0x1000
#define ACT_TRM		0x2000

#define ACT_F_NANO	0x10000

struct action {
	int	action;
	char	*arg;
};
static struct tlst	actlst;
static int				actlist_init = 0;

static int addaction (int, char*);
static int do_ev (char*, int, int);
static int doaction (struct event*, int);
static int dooutput (struct event*, int);
static int do_stress (char*, int, int);
static int splitvar (char*, int*, int*);


int
evparse_usage ()
{
	PROG=fr_getprog ();
	printf ( "%s: usage %s [<options>] [<event>]\n", PROG, PROG);
	printf ( "  parses or creates event strings\n"
				"  <event> is a valid event string to parse\n"
				"  possible <options> are:\n"
				"    -h               - this help screen\n"
				"    -n <name>        - set name of event\n"
				"    -p <pos>:<arg>   - add argument (parameter> at position <pos>\n"
				"    -p <pos>         - remove argument at position <pos>\n"
				"    -i <pos>:<arg>   - insert argument at position <pos>\n"
				"    -t <target name> - set target name\n"
				"    -P <pos>:<arg>   - add argument to target\n"
				"    -P <pos>         - remove argument from target\n"
				"    -I <pos>:<arg>   - insert argument into target\n"
				"    -a <attr>=<val>  - add attribute/value pair\n"
				"    -v <var>=<val>   - adds a variable <var> might have a suffix "
												".<num> and an array index [num]\n"
				"    -F               - next value is interpreted as float\n"
				"    -D               - next value is interpreted as decimal int\n"
				"    -K               - next value is interpreted as calendar date\n"
				"    -G               - next value is interpreted as time difference\n"
				"    -U               - next value is interpreted as remove var\n"
				"    -S               - print string representation of next value\n"
				"    -R               - remove target\n"
				"    -r <attr>        - remove attribute\n"
				"    -d <var>         - remove variable\n"
				"    -c               - output created event string, not parsed "
																		"elements\n"
				"    -N               - output event name only\n"
				"    -T               - output target name only\n"
				"    -H               - print 1 if has a target, otherwise 0\n"
				"    -q <pos>         - output argument at position <pos>\n"
				"    -Q <pos>         - output target argument at position <pos>\n"
				"    -A <attr>        - output value for attribute <attr>\n"
				"    -V <var>         - output value for variable <var>\n"
				"    -s <num>         - stress test - parse event <num> times\n"
				"    -M               - date/time are interpreted in nano seconds\n"
				"");
	return RERR_OK;
}



int
evparse_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	i, c, num;
	int	hasprt = 0;
	char	_buf[128], *buf=NULL;
	int	needfree=0;
	int	ret;
	int	typ = 0;
	int	prtstr=0;
	int	stress=0;
	int	flags=0;

	PROG=fr_getprog ();
	while ((c=getopt (argc, argv, "n:hct:p:P:a:Rr:TNHA:q:Q:i:I:DFSs:V:v:d:KGMU")) != -1) {
		switch (c) {
		case 'h':
			evparse_usage ();
			return 0;
		case 'F':
			typ = ACT_TFLOAT;
			break;
		case 'D':
			typ = ACT_TINT;
			break;
		case 'U':
			typ = ACT_TRM;
			break;
		case 'S':
			typ = ACT_TSTRREP;
			prtstr=1;
			break;
		case 'K':
			typ = ACT_TDATE;
			break;
		case 'G':
			typ = ACT_TTIME;
			break;
		case 'c':
			addaction (ACT_PRTSTR, NULL);
			hasprt=1;
			break;
		case 'n':
			addaction (ACT_NAME, optarg);
			break;
		case 'p':
			addaction (typ|ACT_ARG, optarg);
			typ=0;
			break;
		case 'i':
			addaction (typ|ACT_INSARG, optarg);
			typ=0;
			break;
		case 't':
			addaction (ACT_TNAME, optarg);
			break;
		case 'P':
			addaction (typ|ACT_TARG, optarg);
			typ=0;
			break;
		case 'I':
			addaction (typ|ACT_INSTARG, optarg);
			typ=0;
			break;
		case 'a':
			addaction (typ|ACT_ATTR, optarg);
			typ=0;
			break;
		case 'v':
			addaction (typ|ACT_ADDVAR, optarg);
			typ=0;
			break;
		case 'R':
			addaction (ACT_RMTARGET, NULL);
			break;
		case 'r':
			addaction (ACT_RMATTR, optarg);
			break;
		case 'd':
			addaction (ACT_RMVAR, optarg);
			break;
		case 'T':
			addaction (ACT_PRTTNAME, NULL);
			hasprt=1;
			break;
		case 'N':
			addaction (ACT_PRTNAME, NULL);
			hasprt=1;
			break;
		case 'H':
			addaction (ACT_HASTARGET, NULL);
			hasprt=1;
			break;
		case 'A':
			addaction (typ|ACT_PRTATTR, optarg);
			typ=0;
			hasprt=1;
			break;
		case 'V':
			addaction (typ|ACT_PRTVAR, optarg);
			typ=0;
			hasprt=1;
			break;
		case 'q':
			addaction (typ|ACT_PRTARG, optarg);
			typ=0;
			hasprt=1;
			break;
		case 'Q':
			addaction (typ|ACT_PRTTARG, optarg);
			typ=0;
			hasprt=1;
			break;
		case 's':
			stress = atoi (optarg);
			break;
		case 'M':
			flags |= ACT_F_NANO;
			break;
		}
	}
	if (optind + 1 < argc) {
		for (i=optind, num=0; i<argc; i++) {
			num += strlen (argv[i]) + 1;
		}
		if (num <= (ssize_t)sizeof (_buf)) {
			buf = _buf;
		} else {
			buf = malloc (num);
			if (!buf) {
				printf ("out of memory");
				return RERR_NOMEM;
			}
			needfree = 1;
		}
		for (i=optind, *buf=0; i<argc; i++) {
			strcat (buf, argv[i]);
			strcat (buf, " ");
		}
	} else if (optind < argc) {
		buf = argv[optind];
	}
	if (!hasprt) {
		if (prtstr) {
			typ=ACT_TSTRREP;
		} else {
			typ=0;
		}
		if (buf) {
			addaction (typ|ACT_PRTPARSED, NULL);
		} else {
			addaction (ACT_PRTSTR, NULL);
		}
	}
	if (stress >0 && buf) {
		ret = do_stress (buf, stress, flags);
	} else {
		ret = do_ev (buf, 0, flags);
	}
	if (buf && needfree) free (buf);
	TLST_FREE (actlst);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error elaborating event: %s\n", rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}


static
int
do_stress (evstr, num, flags)
	char	*evstr;
	int	num, flags;
{
	struct event	ev;
	int				i, ret;

	if (!evstr) return RERR_PARAM;
	ret = ev_new (&ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating event: %s\n", rerr_getstr3 (ret));
		return ret;
	}
	for (i=0; i<num; i++) {
		ret = ev_parse (&ev, evstr, 0);
		if (!RERR_ISOK(ret)) {
			ev_free (&ev);
			FRLOGF (LOG_ERR, "error parsing >>%s<<: %s", evstr,
									rerr_getstr3 (ret));
			return ret;
		}
		ev_clear (&ev);
	}
	ev_free (&ev);
	return RERR_OK;
}

static
int
do_ev (evstr, onlyone, flags)
	char	*evstr;
	int	onlyone, flags;
{
	struct event	ev;
	int				ret;

	ret = ev_new (&ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating event: %s\n", rerr_getstr3 (ret));
		return ret;
	}
	if (evstr) {
		ret = ev_parse (&ev, evstr, 0);
		if (!RERR_ISOK(ret)) {
			ev_free (&ev);
			FRLOGF (LOG_ERR, "error parsing >>%s<<: %s", evstr,
								rerr_getstr3 (ret));
			return ret;
		}
	}
	ret = doaction (&ev, flags);
	if (!RERR_ISOK(ret)) {
		ev_free (&ev);
		FRLOGF (LOG_ERR, "error executing actions: %s", rerr_getstr3 (ret));
		return ret;
	}
	ret = dooutput (&ev, onlyone);
	if (!RERR_ISOK(ret)) {
		ev_free (&ev);
		FRLOGF (LOG_ERR, "error printing output: %s", rerr_getstr3 (ret));
		return ret;
	}
	ev_free (&ev);
	return RERR_OK;
}



static
int
addaction (action, arg)
	int	action;
	char	*arg;
{
	struct action	act;
	int				ret;

	if (!actlist_init) {
		ret = TLST_NEW (actlst, struct action);
		if (!RERR_ISOK(ret)) return ret;
		actlist_init = 1;
	}
	act.action = action;
	act.arg = arg;
	return TLST_ADD (actlst, act);
}


static
int
doaction (ev, uflags)
	struct event	*ev;
	int				uflags;
{
	struct action	act;
	char				*s, *ptr, *val;
	int				pos, ret=RERR_OK;
	int				flags;
	int64_t			ival;
	tmo_t				tval;
	double			fval;
	int				idx1, idx2;

	if (!ev) return RERR_PARAM;
	if (!actlist_init) return RERR_INTERNAL;
	TLST_FOREACH (act, actlst) {
		flags = 0;
		switch (act.action & ACT_MASK) {
		case ACT_NAME:
			ret = ev_setname (ev, act.arg, 0);
			break;
		case ACT_TNAME:
			ret = ev_settname (ev, act.arg, 0);
			break;
		case ACT_INSARG:
			flags = EVP_F_INSERT;
			/* fall thru */
		case ACT_ARG:
			if (!act.arg) return RERR_PARAM;
			ptr = act.arg;
			s = top_getfield (&ptr, ":", 0);
			val = top_getquotedfield (&ptr, NULL, 0);
			if (!s || !*s) break;
			pos = atoi (s);
			if (!val || !*val) {
				ret = ev_rmarg (ev, pos);
			} else {
				switch (act.action & ACT_TMASK) {
				case ACT_TINT:
					ival = atoll (val);
					ret = ev_setarg_i (ev, ival, pos, flags);
					break;
				case ACT_TDATE:
					if (uflags & ACT_F_NANO) {
						ret = cjg_gettimestr2ns (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_setarg_d (ev, tval, pos, flags | EVP_F_NANO);
					} else {
						ret = cjg_gettimestr2 (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_setarg_d (ev, tval, pos, flags);
					}
					break;
				case ACT_TTIME:
					if (uflags & ACT_F_NANO) {
						ret = cjg_gettimestr2ns (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_setarg_t (ev, tval, pos, flags | EVP_F_NANO);
					} else {
						ret = cjg_gettimestr2 (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_setarg_t (ev, tval, pos, flags);
					}
					break;
				case ACT_TFLOAT:
					fval = atof (val);
					ret = ev_setarg_f (ev, fval, pos, flags);
					break;
				default:
					ret = ev_setarg_s (ev, val, pos, flags);
					break;
				}
			}
			break;
		case ACT_INSTARG:
			flags = EVP_F_INSERT;
			/* fall thru */
		case ACT_TARG:
			if (!act.arg) return RERR_PARAM;
			ptr = act.arg;
			s = top_getfield (&ptr, ":", 0);
			val = top_getquotedfield (&ptr, NULL, 0);
			if (!s || !*s) break;
			pos = atoi (s);
			if (!val || !*val) {
				ret = ev_rmtarg (ev, pos);
			} else {
				switch (act.action & ACT_TMASK) {
				case ACT_TINT:
					ival = atoll (val);
					ret = ev_settarg_i (ev, ival, pos, flags);
					break;
				case ACT_TDATE:
					if (uflags & ACT_F_NANO) {
						ret = cjg_gettimestr2ns (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_settarg_d (ev, tval, pos, flags | EVP_F_NANO);
					} else {
						ret = cjg_gettimestr2 (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_settarg_d (ev, tval, pos, flags);
					}
					break;
				case ACT_TTIME:
					if (uflags & ACT_F_NANO) {
						ret = cjg_gettimestr2ns (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_settarg_t (ev, tval, pos, flags | EVP_F_NANO);
					} else {
						ret = cjg_gettimestr2 (&tval, val);
						if (!RERR_ISOK(ret)) return ret;
						ret = ev_settarg_t (ev, tval, pos, flags);
					}
					break;
				case ACT_TFLOAT:
					fval = atof (val);
					ret = ev_settarg_f (ev, fval, pos, flags);
					break;
				default:
					ret = ev_settarg_s (ev, val, pos, flags);
					break;
				}
			}
			break;
		case ACT_ATTR:
			if (!act.arg) return RERR_PARAM;
			ptr = act.arg;
			s = top_getfield (&ptr, "=", 0);
			val = top_getquotedfield (&ptr, NULL, 0);
			if (!s || !*s) break;
			if (!val || !*val) {
				ret = ev_rmattr (ev, s);
				break;
			}
			switch (act.action & ACT_TMASK) {
			case ACT_TINT:
				ival = atoll (val);
				ret = ev_addattr_i (ev, s, ival, 0);
				break;
			case ACT_TFLOAT:
				fval = atof (val);
				ret = ev_addattr_f (ev, s, fval, 0);
				break;
			case ACT_TDATE:
				if (uflags & ACT_F_NANO) {
					ret = cjg_gettimestr2ns (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addattr_d (ev, s, tval, EVP_F_NANO);
				} else {
					ret = cjg_gettimestr2 (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addattr_d (ev, s, tval, 0);
				}
				break;
			case ACT_TTIME:
				if (uflags & ACT_F_NANO) {
					ret = cjg_gettimestr2ns (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addattr_t (ev, s, tval, EVP_F_NANO);
				} else {
					ret = cjg_gettimestr2 (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addattr_t (ev, s, tval, 0);
				}
				break;
			default:
				ret = ev_addattr_s (ev, s, val, 0);
				break;
			}
			break;
		case ACT_ADDVAR:
			if (!act.arg) return RERR_PARAM;
			ptr = act.arg;
			s = top_getfield (&ptr, "=", 0);
			val = top_getquotedfield (&ptr, NULL, 0);
			if (!s || !*s) break;
			splitvar (s, &idx1, &idx2);
			if (!val || !*val) {
				ret = ev_rmvar (ev, s, idx1, idx2);
				break;
			}
			switch (act.action & ACT_TMASK) {
			case ACT_TINT:
				ival = atoll (val);
				ret = ev_addvar_i (ev, s, idx1, idx2, ival, 0);
				break;
			case ACT_TFLOAT:
				fval = atof (val);
				ret = ev_addvar_f (ev, s, idx1, idx2, fval, 0);
				break;
			case ACT_TDATE:
				if (uflags & ACT_F_NANO) {
					ret = cjg_gettimestr2ns (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addvar_d (ev, s, idx1, idx2, tval, EVP_F_NANO);
				} else {
					ret = cjg_gettimestr2 (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addvar_d (ev, s, idx1, idx2, tval, 0);
				}
				break;
			case ACT_TTIME:
				if (uflags & ACT_F_NANO) {
					ret = cjg_gettimestr2ns (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addvar_t (ev, s, idx1, idx2, tval, EVP_F_NANO);
				} else {
					ret = cjg_gettimestr2 (&tval, val);
					if (!RERR_ISOK(ret)) return ret;
					ret = ev_addvar_t (ev, s, idx1, idx2, tval, 0);
				}
				break;
			case ACT_TRM:
				ret = ev_addvar_rm (ev, s, idx1, idx2, 0);
				break;
			default:
				ret = ev_addvar_s (ev, s, idx1, idx2, val, 0);
				break;
			}
			break;
		case ACT_RMTARGET:
			if (!act.arg) return RERR_PARAM;
			ev->hastarget = 0;
			break;
		case ACT_RMATTR:
			ret = ev_rmattr (ev, act.arg);
			break;
		case ACT_RMVAR:
			splitvar (act.arg, &idx1, &idx2);
			ret = ev_rmvar (ev, act.arg, idx1, idx2);
			break;
		}
	}
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
dooutput (ev, onlyone)
	struct event	*ev;
	int				onlyone;
{
	struct action	act;
	int				ret = RERR_OK;
	char				buf[128];
	const char		*c_str;
	char				*str;
	int64_t			ival;
	tmo_t				tval;
	double			fval;
	int				needfree;
	int				idx1, idx2;

	if (!ev) return RERR_PARAM;
	if (!actlist_init) return RERR_INTERNAL;
	TLST_FOREACH (act, actlst) {
		needfree = 0;
		switch (act.action & ACT_MASK) {
		case ACT_PRTPARSED:
			if ((act.action & ACT_TMASK) == ACT_TSTRREP) {
				ret = ev_prtparsed (ev, 1, EVP_F_PRTSTR);
			} else {
				ret = ev_prtparsed (ev, 1, EVP_F_PRTTYP);
			}
			break;
		case ACT_PRTSTR:
			str = NULL;
			ret = ev_create (&str, ev, 0);
			if (!RERR_ISOK(ret)) break;
			printf ("%s\n", str?str:"<NULL>");
			if (str) free (str);
			break;
		case ACT_PRTNAME:
			printf ("%s\n", ev->name ? ev->name : "<null>");
			break;
		case ACT_PRTTNAME:
			printf ("%s\n", ev->targetname ? ev->targetname : "<null>");
			break;
		case ACT_HASTARGET:
			printf ("%d\n", ev->hastarget);
			break;
		case ACT_PRTARG:
			if ((act.action & ACT_TMASK) == ACT_TSTRREP) {
				ret = ev_getarg_sr (&str, ev, atoi (act.arg));
				c_str = str;
				needfree=1;
			} else {
				switch (ev_ntypeofarg (ev, atoi (act.arg))) {
				case EVP_T_INT:
					ret = ev_getarg_i (&ival, ev, atoi (act.arg));
					sprintf (buf, "%lld", (long long) ival);
					c_str = buf;
					break;
				case EVP_T_FLOAT:
					ret = ev_getarg_f (&fval, ev, atoi (act.arg));
					sprintf (buf, "%lg", fval);
					c_str = buf;
					break;
				case EVP_T_STR:
					ret = ev_getarg_s (&c_str, ev, atoi (act.arg));
					break;
				case EVP_T_DATE:
					ret = ev_getarg_d (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_NDATE:
					ret = ev_getarg_d (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_TIME:
					ret = ev_getarg_t (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				case EVP_T_NTIME:
					ret = ev_getarg_t (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				default:
					c_str = "invalid type";
					break;
				}
			}
			if (!RERR_ISOK(ret)) break;
			printf ("%s\n", c_str?c_str:"<NULL>");
			if (needfree && str) free (str);
			break;
		case ACT_PRTTARG:
			if ((act.action & ACT_TMASK) == ACT_TSTRREP) {
				ret = ev_gettarg_sr (&str, ev, atoi (act.arg));
				c_str = str;
				needfree=1;
			} else {
				switch (ev_ntypeoftarg (ev, atoi (act.arg))) {
				case EVP_T_INT:
					ret = ev_gettarg_i (&ival, ev, atoi (act.arg));
					sprintf (buf, "%lld", (long long) ival);
					c_str = buf;
					break;
				case EVP_T_FLOAT:
					ret = ev_gettarg_f (&fval, ev, atoi (act.arg));
					sprintf (buf, "%lg", fval);
					c_str = buf;
					break;
				case EVP_T_STR:
					ret = ev_gettarg_s (&c_str, ev, atoi (act.arg));
					break;
				case EVP_T_DATE:
					ret = ev_gettarg_d (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_NDATE:
					ret = ev_gettarg_d (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_TIME:
					ret = ev_gettarg_t (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				case EVP_T_NTIME:
					ret = ev_gettarg_t (&tval, ev, atoi (act.arg));
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				default:
					c_str = "invalid type";
					break;
				}
			}
			if (!RERR_ISOK(ret)) break;
			printf ("%s\n", c_str?c_str:"<NULL>");
			if (needfree && str) free (str);
			break;
		case ACT_PRTATTR:
			if ((act.action & ACT_TMASK) == ACT_TSTRREP) {
				ret = ev_getattr_sr (&str, ev, act.arg);
				c_str = str;
				needfree=1;
			} else {
				switch (ev_ntypeofattr (ev, act.arg)) {
				case EVP_T_INT:
					ret = ev_getattr_i (&ival, ev, act.arg);
					sprintf (buf, "%lld", (long long) ival);
					c_str = buf;
					break;
				case EVP_T_FLOAT:
					ret = ev_getattr_f (&fval, ev, act.arg);
					sprintf (buf, "%lg", fval);
					c_str = buf;
					break;
				case EVP_T_STR:
					ret = ev_getattr_s (&c_str, ev, act.arg);
					break;
				case EVP_T_DATE:
					ret = ev_getattr_d (&tval, ev, act.arg);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_NDATE:
					ret = ev_getattr_d (&tval, ev, act.arg);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_TIME:
					ret = ev_getattr_t (&tval, ev, act.arg);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				case EVP_T_NTIME:
					ret = ev_getattr_t (&tval, ev, act.arg);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				default:
					c_str = "invalid type";
					break;
				}
			}
			if (!RERR_ISOK(ret)) break;
			printf ("%s\n", c_str?c_str:"<NULL>");
			if (needfree && str) free (str);
			break;
		case ACT_PRTVAR:
			splitvar (act.arg, &idx1, &idx2);
			if ((act.action & ACT_TMASK) == ACT_TSTRREP) {
				ret = ev_getvar_sr (&str, ev, act.arg, idx1, idx2);
				c_str = str;
				needfree=1;
			} else {
				switch (ev_ntypeofvar (ev, act.arg, idx1, idx2)) {
				case EVP_T_INT:
					ret = ev_getvar_i (&ival, ev, act.arg, idx1, idx2);
					sprintf (buf, "%lld", (long long) ival);
					c_str = buf;
					break;
				case EVP_T_FLOAT:
					ret = ev_getvar_f (&fval, ev, act.arg, idx1, idx2);
					sprintf (buf, "%lg", fval);
					c_str = buf;
					break;
				case EVP_T_STR:
					ret = ev_getvar_s (&c_str, ev, act.arg, idx1, idx2);
					break;
				case EVP_T_DATE:
					ret = ev_getvar_d (&tval, ev, act.arg, idx1, idx2);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_NDATE:
					ret = ev_getvar_d (&tval, ev, act.arg, idx1, idx2);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_D);
						c_str = buf;
					}
					break;
				case EVP_T_TIME:
					ret = ev_getvar_t (&tval, ev, act.arg, idx1, idx2);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestr (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				case EVP_T_NTIME:
					ret = ev_getvar_t (&tval, ev, act.arg, idx1, idx2);
					if (RERR_ISOK(ret)) {
						ret = cjg_prttimestrns (buf, sizeof(buf), tval, CJG_TSTR_T_DDELTA);
						c_str = buf;
					}
					break;
				default:
					c_str = "invalid type";
					break;
				}
			}
			if (!RERR_ISOK(ret)) break;
			printf ("%s\n", c_str?c_str:"<NULL>");
			if (needfree && str) free (str);
			break;
		default:
			continue;
		}
		if (onlyone) break;
	}
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}



static
int
splitvar (var, idx1, idx2)
	char	*var;
	int	*idx1, *idx2;
{
	char	*s, *s2;

	if (!var) return RERR_PARAM;
	s = var;
	top_getfield (&s, ".", 0);
	s2 = s?s:var;
	top_getfield (&s2, "[", 0);
	if (s) s = top_stripwhite (s, 0);
	if (s2) s2 = top_getfield (&s2, "]", 0);
	if (!s) s=(char*)"-1";
	if (!s2) s2=(char*)"-1";
	if (idx1) *idx1 = atoi (s);
	if (idx2) *idx2 = atoi (s2);
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
