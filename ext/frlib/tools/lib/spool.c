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
#include <errno.h>
#ifdef Linux
#include <getopt.h>
#else
extern char *optarg;
extern int optind, opterr, optopt;
#endif
#include <time.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <sys/ioctl.h>
#include <signal.h>
#define __USE_LARGEFILE64
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <sys/wait.h>



#include <fr/base.h>
#include <fr/spool.h>
#include <fr/cal.h>

#include "spool.h"


#if 0
#ifdef SunOS
typedef int socklen_t;
#endif
#endif

static const char *PROG="spool";

/* usage:
 *		prints a help screen to stdout
 *
 *		return: always RERR_OK
 */
int
spool_usage ()
{
	int		n, n1;

	PROG=fr_getprog ();
	printf ("%s: %nusage: %s [<options>] [<action>]\n", PROG, &n1, PROG);
	printf (	"%*saction: "
				"%n-l <spool>  - list content of spool (default action)\n", 
										n1, "", &n);
	printf ( "%*s-d          - start server (daemon)\n", n, "");
	printf (	"%*s-e <num>    - erase entry <num>\n", n, "");
	printf (	"%*s-E          - erase all entries of type (use -w|b|f)\n", 
										n, "");
	printf (	"%*s-r <num>    - reinsert entry <num>\n", n, "");
	printf (	"%*s-R          - reinsert all entries of type (use -w|b|f)\n", 
										n, "");
	printf (	"%*s-C <num>    - list content of df_file of entry <num>\n", n, "");
	printf (	"%*s-i <file>   - insert <file> into spool\n", n, "");
	printf (	"%*s-I <file>   - insert <file> into spool linking it\n", n, "");
	printf (	"%*s-u          - unspool next entry\n", n, "");
	printf (	"%*soptions: "
			 	"%n-s <spool>  - spoolname\n", n1, "", &n);
	printf (	"%*s-h          - this help screen\n", n, "");
	printf (	"%*s-c <file>   - use alternative config file\n", n, "");
	printf ( "%*s-D          - debug / together with -d don't fork\n",n, "");
	printf (	"%*s-f          - list, delete or reinsert entries in state error "
										"(fault)\n", n, "");
	printf (	"%*s-b          - list, delete or reinsert entries in state \"in "
										"elaboration\"\n", n, "");
	printf (	"%*s-w          - list, delete or reinsert entries in state wait\n",
										n, "");
	printf (	"%*s-t          - list ctime (creation time) instead of mtime "
										"(modification time)\n", n, "");
	printf (	"%*s-F          - include content of first 4 lines of df_file in "
										"listing\n", n, "");
#if 0
	printf (	"%*s-M          - together with -d|-D moves the files xml and html "
										"to err-dir\n", n, "");
	printf (	"%*s-X          - together with -d|-D deletes as well the file xml "
										"and html\n", n, "");
#endif
	printf ("\n");
	return RERR_OK;
}

static int get_xml (char**, char**, char*, char*);
static int move_to (char*, char*, char*, char*);
static int spool_getbuf (char**, char*);
static int read_config ();
static int open_conn_client (int*);


static const char	*c_spoolprog = NULL;
static const char	*c_spoolsock = (char*)"/tmp/spool.sock";
static int			c_spoolstream = 1;
static int			c_direct = 1;
static const char	*c_spooltrap = NULL;
static int			c_spoolscale = 1;
static tmo_t		c_spooltimeout = -1;
static int			c_spoollisten = 0;
static tmo_t		c_spoolautoreinsert = 0;

static char			*c_curdir = (char*)"/tmp/xmlpsool/cur";
static char			*c_errdir = (char*)"/tmp/xmlspool/err";
static const char	*c_basedir = (char*)"/tmp/xmlpsool";
static int			c_curdirfree = 0;
static int			c_errdirfree = 0;

static int			config_read=0;

static char			*g_spool = (char*)"_spool";



int
spool_main (argc, argv)
	int	argc;
	char	**argv;
{
	int			c;
	int			ret;
	int			prt_all=1, flags=0;
	int			debug = 0;
	int			daemon = 0;
	int			num=0;
	int			action=0;
	char			*options = NULL;
	char			*file=NULL;
	char			*buf=NULL;
	int			fd = -1;
	const char	*s;

	PROG=fr_getprog ();
	while ((c=getopt (argc, argv, "hc:dDl::s:fwbFtMXue:Er:RC:i:I:o:y:"))!=-1) {
		switch (c) {
		case 'h':
			spool_usage ();
			exit (0);
			break;
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'd':
			daemon = 1;
			break;
		case 'D':
			debug = 1;
			break;
		case 'l':
			if (optarg) g_spool=optarg;
			action = ACTION_LIST;
			break;
		case 's':
			g_spool=optarg;
			break;
		case 'f':
			flags |= SPOOL_F_PRT_ERRALL;
			prt_all=0;
			break;
		case 'w':
			flags |= SPOOL_F_PRT_WAIT;
			prt_all=0;
			break;
		case 'b':
			flags |= SPOOL_F_PRT_ELAB;
			prt_all=0;
			break;
		case 'F':
			flags |= SPOOL_F_SHOW_DFFILE;
			break;
		case 't':
			flags |= SPOOL_F_SHOW_CTIME;
			break;
		case 'M':
			flags |= SPOOL_F_MOVEXML;
			break;
		case 'X':
			flags |= SPOOL_F_RMXML;
			break;
		case 'u':
			action = ACTION_UNSPOOL;
			break;
		case 'e':
			action = ACTION_DELETE;
			num=cf_atoi (optarg);
			break;
		case 'E':
			action = ACTION_DELETE_ALL;
			break;
		case 'r':
			action = ACTION_REINSERT;
			num=cf_atoi (optarg);
			break;
		case 'R':
			action = ACTION_REINSERT_ALL;
			break;
		case 'C':
			action = ACTION_CAT_DFFILE;
			num=cf_atoi (optarg);
			break;
		case 'i':
			action = ACTION_INSERT;
			file = optarg;
			break;
		case 'I':
			action = ACTION_INSERT;
			file = optarg;
			flags |= SPOOL_F_LINK;
			break;
		case 'o':
			options = optarg;
			break;
		case 'y':
			fd = atoi (optarg);
			break;
		}
	}
	if (daemon) action=0;
	if (!action) {
		s = (s = index (PROG, '_')) ? s+1 : PROG;
		sswitch (s) {
		sicase ("spoold")
			daemon = 1;
			break;
		sicase ("spoolw")
			action = ACTION_INSERT;
			if (optind < argc) {
				file = argv[optind];
			}
			break;
		sdefault
			action = ACTION_LIST;
			break;
		} esac;
	}
	if (prt_all && action == ACTION_LIST) {
		flags |= SPOOL_FLAG_PRT_ALL;
	}
	ret = scf_fdread (fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error reading config file: %s\n", 
									rerr_getstr3(ret));
		return ret;
	}
	CF_MAYREAD;
	if (action == ACTION_INSERT) {
		if (c_direct) flags |= SPOOL_F_DIRECT;
		if (!c_spoolstream && !(flags & SPOOL_F_LINK)) {
			ret = spool_getbuf (&buf, file);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR2, "error getting file to spool: %s", 
										rerr_getstr3(ret));
				return ret;
			}
		}
	}
	set_user();
	set_environment();
	if (action != ACTION_INSERT) set_pwd ();
	if (daemon && !debug) frdaemonize ();
	if (daemon) {
		if (c_spoollisten) flags |= SPOOL_F_LISTEN;
		if (!c_spoolstream) flags |= SPOOL_F_NOSTREAM;
		ret = spool_server (g_spool, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error starting spool server: %s", 
									rerr_getstr3(ret));
			return ret;
		}
	} else if (action == ACTION_INSERT) {
		if (buf) {
			ret = spool_insertbuf (g_spool, buf, options, flags);
			free (buf);
		} else {
			ret = spool_insertfile (g_spool, file, options, flags);
		}
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error inserting file >>%s<< into spool: %s",
									(file?file:"-"), rerr_getstr3(ret));
			return ret;
		}
	} else {
		ret = spool_action (g_spool, action, options, num, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}



int
spool_action (spool, action, options, num, flags)
	int	action, flags, num;
	char	*options, *spool;
{
	char	*cf_file, *buf;
	int	buflen;
	int	ret;

	switch (action) {
	case ACTION_INSERT:
		break;
	case ACTION_UNSPOOL:
		ret = spool_outdel (spool, &buf, &buflen);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error unspooling from >>%s<<: %s\n",
										spool?spool:"", rerr_getstr3(ret));
			return ret;
		}
		fwrite (buf, 1, buflen, stdout);
		free (buf);
		break;
	case ACTION_CAT_DFFILE:
		ret = spool_num2cf (&cf_file, spool, num);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error getting file from num %d: %s\n",
									num, rerr_getstr3(ret));
			return ret;
		}
		ret = spool_read_df (&buf, &buflen, spool, cf_file);
		free (cf_file);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error reading df-file for num %d: %s\n",
									num, rerr_getstr3(ret));
			return ret;
		}
		fwrite (buf, 1, buflen, stdout);
		free (buf);
		break;
	case ACTION_LIST:
		ret = spool_print (spool, flags|SPOOL_F_SORT_NUM|SPOOL_F_SORT_STATE);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error listing spool >>%s<<: %s\n",
									spool?spool:"", rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACTION_DELETE:
		ret = spool_num2cf (&cf_file, spool, num);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error getting file from num %d: %s\n",
									num, rerr_getstr3(ret));
			return ret;
		}
		ret = spool_delwithxml (spool, cf_file, flags);
		if (!RERR_ISOK(ret)) return ret;
		break;
	case ACTION_DELETE_ALL:
		ret = spool_deleteall (spool, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error deleting all: %s\n", rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACTION_REINSERT:
		ret = spool_num2cf (&cf_file, spool, num);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error getting file from num %d: %s\n",
									num, rerr_getstr3(ret));
			return ret;
		}
		ret = spool_chstate2 (spool, cf_file, SPOOL_STATE_WAIT, 0, options);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error changing state for %d: %s\n",
									num, rerr_getstr3(ret));
			return ret;
		}
		break;
	case ACTION_REINSERT_ALL:
		ret = spool_reinsertall (spool, options, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error reinserting all: %s\n", rerr_getstr3(ret));
			return ret;
		}
		break;
	}
	return RERR_OK;
}



/* spool_deleteall:
 *		deletes all entries in spool indicated by flags
 *
 *		spool:	spool-name
 *		flags:	see spool_list
 *
 *		return: error code
 */
int
spool_deleteall (spool, flags)
	char	*spool;
	int	flags;
{
	struct spoollist	list;
	int					ret, i;

	ret = spool_list (spool, &list, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error in getting list of files: %s",
								rerr_getstr3(ret));
		return ret;
	}
	for (i=0; i<list.listnum; i++) {
		spool_delwithxml (spool, list.list[i].cf_name, flags);
	}
	hfree_spoollist (&list);
	return RERR_OK;
}


/* spool_delwithxml
 *		deletes one entry from spool and might move/del xml/html file
 *
 *		spool:	spool name
 *		cf_file:	cf file to remove
 *		flags:	see spool_list
 *
 *		return: error code
 */
int
spool_delwithxml (spool, cf_file, flags)
	char	*spool, *cf_file;
	int	flags;
{
	int	ret;

	if (flags & SPOOL_F_MOVEXML) {
		ret = spool_xmlmove2err (spool, cf_file);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error moving xml/html file (cf_file=>>%s<<): %s\n",
									cf_file, rerr_getstr3(ret));
			return ret;
		}
	} else if (flags & SPOOL_F_RMXML) {
		ret = spool_xmldel (spool, cf_file);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error deleting xml/html file (cf_file=>>%s<<): "
									"%s\n", cf_file, rerr_getstr3(ret));
			return ret;
		}
	}
	ret = spool_delentry (spool, cf_file);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error deleting >>%s<<: %s\n", cf_file,
									rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


/* reinsert_all:
 *		changes the state of all entries in spool indicated by flags
 *		to wait
 *
 *		spool:	spool-name
 *		flags:	see spool_list
 *		options: options to add
 *
 *		return: errore code
 */
int
spool_reinsertall (spool, options, flags)
	char	*spool;
	int	flags;
	char	*options;
{
	struct spoollist	list;
	int					ret, i;

	flags &= ~SPOOL_FLAG_PRT_WAIT;
	ret = spool_list (spool, &list, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error in getting list of files: %s\n",
									rerr_getstr3(ret));
		return ret;
	}
	for (i=0; i<list.listnum; i++) {
		ret = spool_chstate2 (spool, list.list[i].cf_name, 
								SPOOL_STATE_WAIT, 0, options);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error reinserting >>%s<<: %s\n",
							list.list[i].cf_name, rerr_getstr3(ret));
		}
	}
	hfree_spoollist (&list);
	return RERR_OK;
}



/* spool_xmlmove2err:
 *		extracts xml- and html-file from spool and moves them to err-dir
 *
 *		spool:		name of spool
 *		cf_name:	name of cf-file in spool
 *
 *		return: error code
 */
int
spool_xmlmove2err (spool, cf_name)
	char	*cf_name;
	char	*spool;
{
	char	*xml, *html;
	int	ret;

	if (!cf_name) return RERR_PARAM;
	ret = get_xml (&xml, &html, spool, cf_name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error in get_xml(): %s", rerr_getstr3(ret));
		return ret;
	}
	ret = move_to (xml, html, (char*) c_curdir, (char*)c_errdir);
	if (xml) free (xml);
	if (html) free (html);
	return ret;
}


/* spool_xmldel
 *		extracts xml- and html-file from spool and deletes them
 *
 *		spool:	name of spool
 *		cf_name:	name of cf-file in spool
 *
 *		return: error code
 */
int
spool_xmldel (spool, cf_name)
	char	*cf_name;
	char	*spool;
{
	char	*xml, *html;
	int	ret;

	if (!cf_name) return RERR_PARAM;
	ret = get_xml (&xml, &html, spool, cf_name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error in get_xml(): %s", rerr_getstr3(ret));
		return ret;
	}
	if (xml) unlink (xml);
	if (html) unlink (html);
	if (xml) free (xml);
	if (html) free (html);
	return RERR_OK;
}


/* get_xml:
 *		extracts xml- and html-file from spool
 *
 *		xml:		name of xml-file (output)
 *		html:		name of html-file (output)
 *		spool:	name of spool
 *		cf_name:	name of cf-file in spool
 *
 *		return: error code
 */
static
int
get_xml (xml, html, spool, cf_name)
	char	**xml, **html, *cf_name;
	char	*spool;
{
	char	*buf, *buf2;
	char	*line, *field, *value;
	int	buflen, ret;

	if (!xml || !html || !cf_name) return RERR_PARAM;
	if (!config_read) read_config();
	*xml = *html = NULL;
	ret = spool_read_df (&buf, &buflen, spool, cf_name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error getting df file (%s): %s\n", cf_name,
								rerr_getstr3(ret));
		return ret;
	}
	buf2=buf;
	while ((line=top_getline (&buf2, 0))) {
		field = top_getfield (&line, ":", 0);
		value = top_getfield (&line, NULL, TOP_F_DQUOTE|TOP_F_SQUOTE);
		sswitch (field) {
		sicase ("xmlfile") 
		sicase ("xml")
			if (!value) break;
			if (*value == '/') {
				*xml = strdup (value);
				if (!*xml) {
					if (*html) free (*html);
					free (buf);
					return RERR_NOMEM;
				}
			} else {
				*xml = (char*) malloc (strlen (c_curdir) + strlen(value) + 2);
				if (!*xml) {
					if (*html) free (*html);
					free (buf);
					return RERR_NOMEM;
				}
				sprintf (*xml, "%s/%s", c_curdir, value);
			}
			break;
		sicase ("htmlfile")
		sicase ("html")
			if (!value) break;
			if (*value == '/') {
				*html = strdup (value);
				if (!*html) {
					if (*xml) free (*xml);
					free (buf);
					return RERR_NOMEM;
				}
			} else {
				*html = (char*) malloc (strlen (c_curdir) + strlen(value) + 2);
				if (!*html) {
					if (*xml) free (*xml);
					free (buf);
					return RERR_NOMEM;
				}
				sprintf (*html, "%s/%s", c_curdir, value);
			}
			break;
		} esac;
	}
	free (buf);
	if (!*xml) {
		if (*html) free (*html);
		FRLOGF (LOG_ERR2, "got index file without xmlfile");
		return RERR_NO_XML;
	}
	return RERR_OK;
}





/* move_to:
 *		moves html- and xml-file from fromdir to todir
 *
 *		xml:		name of xml-file (relative to c_curdir)
 *		html:		name of html-file (relative to c_curdir)
 *		fromdir:	name of origin dir
 *		todir:		name of destination dir
 *
 *		return: error code
 */
static
int
move_to (xml, html, fromdir, todir)
	char	*xml, *html, *fromdir, *todir; 
{
	char	*from1, *from2=NULL, *to1, *to2=NULL;
	int		ret;

	if (!xml || !fromdir || !todir) return RERR_PARAM;
	ret = RERR_NOMEM;
	from1 = (char*) malloc (strlen(xml)+strlen (fromdir)+2);
	to1 = (char*) malloc (strlen (xml)+strlen (todir)+2);
	if (!from1 || !to1) goto err_out;
	if (*xml == '/') xml=rindex (xml, '/');
	if (!xml) goto err_out;
	if (html && *html == '/') html=rindex (html, '/');
	sprintf (from1, "%s/%s", fromdir, xml);
	sprintf (to1, "%s/%s", todir, xml);
	if (html) {
		from2= (char*) malloc (strlen (html)+strlen (fromdir)+2);
		to2 = (char*) malloc (strlen (html)+strlen (todir)+2);
		if (!from2 || !to2) goto err_out;
		sprintf (from2, "%s/%s", fromdir, html);
		sprintf (to2, "%s/%s", todir, html);
	}
	ret = RERR_OK;
	if (link (from1, to1) != 0) {
		slogf (LOG_ERR2, "move_to(): cannot move xml file (%s) to (%s): %s",
						xml, todir, strerror (errno));
		ret = RERR_SYSTEM;
		goto err_out;
	}
	if (html) {
		if (link (from2, to2) != 0) {
			slogf (LOG_ERR2, "move_to(): cannot move html file (%s) to (%s): %s",
							html, todir, strerror (errno));
			unlink (to1);
			ret = RERR_SYSTEM;
			goto err_out;
		}
		unlink (from2);
	}
	unlink (from1);
	ret = RERR_OK;
err_out:
	if (from1) free ((void*)from1);
	if (to1) free ((void*)to1);
	if (from2) free ((void*)from2);
	if (to2) free ((void*)to2);
	return ret;
}







int
spool_insertbuf (spool, buf, options, flags)
	char	*buf, *options, *spool;
	int	flags;
{
	int	sd;
	int	ret;

	if (!(flags & SPOOL_F_DIRECT) && !options && open_conn_client(&sd) == RERR_OK) {
		write (sd, buf, strlen (buf));
		sleep (1);
		/* read (sd, buf2, 2); */
		close (sd);
	} else {
		ret = spool_in (spool, buf, strlen (buf), options);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_CRIT2, "cannot spool to >>%s<<: %s", spool,
										rerr_getstr3 (ret));
			return ret;
		}
	}
	return RERR_OK;
}


static
int
spool_getbuf (obuf, infile)
	char	**obuf, *infile;
{
	FILE	*f;

	if (!obuf) return RERR_PARAM;
	if (!strcmp (infile, "-")) infile=NULL;
	if (infile) {
		f = fopen (infile, "r");
		if (!f) {
			FRLOGF (LOG_ERR2, "cannot open file >>%s<< for reading: %s",
							infile, rerr_getstr3(RERR_SYSTEM));
			return RERR_SYSTEM;
		}
	} else {
		f = stdin;
	}
	*obuf = fop_read_file (f);
	if (f!=stdin) fclose (f);
	if (!*obuf) {
		FRLOGF (LOG_ERR2, "error read input\n");
		return RERR_NOMEM;
	}
	return RERR_OK;
}

int
spool_insertfile (spool, infile, options, flags)
	char	*infile, *options, *spool;
	int	flags;
{
	int	fd, sd, num, ret;
	char	buf2[1024];

	if (!strcmp (infile, "-")) infile=NULL;
	if (flags & SPOOL_F_LINK) flags |= SPOOL_F_DIRECT;

	if (!(flags & SPOOL_F_DIRECT) && !options
						&& open_conn_client(&sd) == RERR_OK) {
		if (infile) {
			fd = open (infile, O_RDONLY|O_LARGEFILE);
			if (fd<0) {
				FRLOGF (LOG_ERR, "error opening infile >>%s<<: %s", infile,
										rerr_getstr3(RERR_SYSTEM));
				return RERR_SYSTEM;
			}
		} else {
			fd = 0;
		}
		while ((num=read (fd, buf2, sizeof(buf2))) > 0) {
			write (sd, buf2, num);
		}
		if (fd>0) close (fd);
	} else {
		if (infile) {
			if (flags & SPOOL_F_LINK) {
				ret = spool_infile (spool, infile, 1, options);
				if (!RERR_ISOK(ret)) {
					ret = spool_infile (spool, infile, 0, options);
					unlink (infile);
				}
			} else {
				ret = spool_infile (spool, infile, 0, options);
			}
		} else {
			ret = spool_infd (spool, 0 /* stdin */, options);
		}
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_CRIT2, "cannot spool to >>%s<<: %s", spool,
										rerr_getstr3 (ret));
			return ret;
		}
	}

	return 0;
}



static
int
open_conn_client (fd)
	int	*fd;
{
	int						sd;
	struct sockaddr_un	saddr;
	const char				*usock;

	if (!fd) return RERR_PARAM;
	CF_MAYREAD;
	usock = c_spoolsock ? c_spoolsock : "/tmp/spoold.sock";
	sd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (sd < 0) {
		FRLOGF (LOG_WARN, "cannot create socket: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	saddr.sun_family = AF_UNIX;
	strcpy (saddr.sun_path, usock);
	if (connect (sd, (struct sockaddr *) &saddr, sizeof (saddr)) != 0) {
		FRLOGF (LOG_WARN, "cannot connect to server %s",
									rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	*fd = sd;
	return RERR_OK;
}






/* read_config:
 *		reads neccessary variables from config file
 *
 *		return: error code
 */
static
int
read_config ()
{
	cf_begin_read ();
	if (!g_spool) g_spool=(char*)"_spool";
	c_spoolprog = cf_getarr2 ("prog", g_spool, g_spool);
	c_spoolsock = cf_getarr ("spool_sock", g_spool);
	if (!c_spoolsock) c_spoolsock = cf_getarr ("sock", g_spool);
	if (!c_spoolsock) {
		c_spoolsock = asprtf ("/tmp/%s.sock", g_spool);
		if (!c_spoolsock) c_spoolsock = "/tmp/spool.sock";
	}
	c_spoolstream = cf_isyes (cf_getarr2 ("spool_stream", g_spool, "yes"));
	c_direct = cf_isyes (cf_getarr2 ("spool_direct", g_spool, "yes"));
	c_spoollisten = cf_isyes (cf_getarr2 ("spool_listen", g_spool, "yes"));
	c_spoolscale = cf_atoi (cf_getarr2 ("spoold_scale", g_spool, "1"));
	c_spooltrap = cf_getval ("spoold_trap");
	c_spooltimeout = tmo_gettimestr64 (cf_getarr2 ("spoold_timeout", g_spool, "-1"));
	c_spoolautoreinsert = cf_atotm (cf_getarr2 ("spool_autoreinsert", g_spool, "0"));
	/* read xml stuff */
	if (c_curdirfree) free (c_curdir);
	if (c_errdirfree) free (c_errdir);
	c_curdirfree = 0;
	c_errdirfree = 0;
	c_basedir=cf_getval2 ("xmlspool_base", "/tmp/xmlspool");
	c_curdir=(char*)cf_getval ("xmlspool_cur");
	c_errdir=(char*)cf_getval ("xmlspool_err");
	if (!c_curdir || !*c_curdir) {
		c_curdir = (char*)malloc (strlen(c_basedir)+8);
		if (!c_curdir) {
			c_curdir=(char*)"/tmp/xmlspool/cur";
		} else {
			sprintf (c_curdir, "%s/cur", c_basedir);
			c_curdirfree = 1;
		}
	}
	if (!c_errdir || !*c_errdir) {
		c_errdir = (char*)malloc (strlen(c_basedir)+8);
		if (!c_errdir) {
			c_errdir=(char*)"/tmp/xmlspool/err";
		} else {
			sprintf (c_errdir, "%s/err", c_basedir);
			c_errdirfree = 1;
		}
	}
	config_read=1;
	cf_end_read_cb (&read_config);
	return RERR_OK;
}



/* *********************
 * server functions 
 * *********************/




static int open_conn_server (int*);
static int server_callprog2 (char*, char*, char*);
static int server_callprog (char*, char*, char*, char*, char*);
static int server_checkchild ();
static int server_mainloop (char*, int);
static int server_sendtrap ();
static int server_calltrap ();
static int server_listenchild (char*, int);
static int server_autoreinsert (char*, int);
static int server_reinsertall (char*, int);

static int	server_numchilds=0;



int
spool_server (spool, flags)
	char	*spool;
	int	flags;
{
	pid_t	pid;
	int	ret;

	CF_MAYREAD;

	if (flags & SPOOL_F_LISTEN) {
		pid = fork ();
		if (pid < 0) {
			FRLOGF (LOG_WARN, "cannot fork child, no socket available: %s",
									rerr_getstr3(RERR_SYSTEM));
		} else if (pid == 0) {
			ret = server_listenchild (spool, flags);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "cannot run listen child: %s", rerr_getstr3(ret));
				exit (ret);
			}
			exit (0);
		}
	}
	if (c_spoolautoreinsert > 0) {
		if (pid < 0) {
			FRLOGF (LOG_WARN, "cannot fork child, no socket available: %s",
									rerr_getstr3(RERR_SYSTEM));
		} else if (pid == 0) {
			ret = server_autoreinsert (spool, flags);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "cannot run listen child: %s", rerr_getstr3(ret));
				exit (ret);
			}
			exit (0);
		}
	}
	return server_mainloop (spool, flags);
}

static
int
server_autoreinsert (spool, flags)
	char	*spool;
	int	flags;
{
	tmo_t	ival = c_spoolautoreinsert / 10;
	int	ret;

	if (c_spoolautoreinsert <= 0) return RERR_OK;
	if (ival < 1000000LL) ival = 1000000LL;
	FRLOGF (LOG_DEBUG, "starting auto reinsert server");
	while (1) {
		tmo_sleep (ival);
		ret = server_reinsertall (spool, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error in reinserting: %s", rerr_getstr3(ret));
			tmo_msleep (10000);
		}
	}
	return RERR_OK;
}

static
int
server_reinsertall (spool, flags)
	char	*spool;
	int	flags;
{
	struct spoollist	list;
	int					ret, i;
	tmo_t					now;
	int					ret2 = RERR_OK;

	flags &= ~SPOOL_FLAG_PRT_WAIT;
	ret = spool_list (spool, &list, SPOOL_F_PRT_ERRALL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error in getting list of files: %s\n",
									rerr_getstr3(ret));
		return ret;
	}
	now = tmo_now ();
	for (i=0; i<list.listnum; i++) {
		if (now - list.list[i].mtime < c_spoolautoreinsert) continue;
		ret = spool_chstate (spool, list.list[i].cf_name, 
								SPOOL_STATE_WAIT, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error reinserting >>%s<<: %s\n",
							list.list[i].cf_name, rerr_getstr3(ret));
			ret2 = ret;
		}
	}
	hfree_spoollist (&list);
	return ret2;
}


static
int
server_listenchild (spool, flags)
	char	*spool;
	int	flags;
{
	int	ret, sd, nsd;
	char	*options=NULL;
	char	*buf;

	ret = open_conn_server (&sd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "cannot open socket to listen on: %s",
									rerr_getstr3(ret));
		return ret;
	}
	while (1) {
		nsd = accept(sd, (struct sockaddr *)0, (socklen_t*)0);
		if (nsd == -1) {
			FRLOGF (LOG_ERR, "error accepting connection: %s", 
									rerr_getstr3 (RERR_SYSTEM));
			continue;
		}
		if (!(flags & SPOOL_F_NOSTREAM)) {
			ret = spool_infd (spool, nsd, options);
		} else {
			buf = fop_read_fd (nsd);
			write (nsd, "ok", 2);
			close (nsd);
			ret = spool_in (spool, buf, strlen (buf), options);
			free (buf);
		}
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error spooling to >>%s<< forking child directly",
									spool);
			server_callprog2 (buf, NULL, options);
		}
		if (!(flags & SPOOL_F_NOSTREAM)) {
			write (nsd, "ok", 2);
			close (nsd);
		}
	}

	return RERR_OK;
}

static
int
server_mainloop (spool, flags)
	char	*spool;
	int	flags;
{
	int	ret;
	char	*cf_name, *buf;
	int	buflen;
	char	*df_name;
	char	*options=NULL;

	while (1) {
		if (!server_checkchild ()) continue;
		df_name=NULL;
		buf=NULL;
		FRLOGF (LOG_VVERB, "spool out file");
		if (!(flags & SPOOL_F_NOSTREAM)) {
			ret = spool_outfile (spool, &cf_name, &df_name, &options);
		} else {
			ret = spool_out (spool, &buf, &buflen, &cf_name, NULL, &options);
		}
		if (!RERR_ISOK(ret)) {
			if (ret == RERR_NOT_FOUND) {
				FRLOGF (LOG_VVERB, "nothing found to spool out");
			} else {
				FRLOGF (LOG_ERR, "error spooling out: %s", 
										rerr_getstr3(ret));
			}
			sleep (1);
			continue;
		}
		FRLOGF (LOG_VERB, "found file >>%s<<, process it...", cf_name);
		ret = server_callprog (spool, buf, cf_name, df_name, options);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error processing file >>%s<<: %s",
									cf_name, rerr_getstr3(ret));
		} else {
			FRLOGF (LOG_VERB, "file >>%s<< processed with success", cf_name);
		}
		free (cf_name);
		if (buf) free (buf);
		if (df_name) free (df_name);
		if (options) free (options);
	}
	return RERR_INTERNAL;	/* should never reach here */
}


static
int
server_callprog2 (buf, df_name, options)
	char	*buf, *df_name, *options;
{
	int			ret;
	const char	*pass;
	const char	*config_file = NULL;

	CF_MAYREAD;
	config_file = cf_get_cfname();
	if (buf) df_name=NULL;
	if (scf_havefile ()) {
		pass = scf_getpass ();
		ret = iopipef64 (buf, buf?strlen(buf):0, NULL, NULL, c_spooltimeout,
								IOPIPE_F_RET_EXIT_CODE, "%s %s%s -y %y%s%s %s",
								pass, c_spoolprog, config_file?"-c ":"",
								config_file?config_file:"",
								df_name?" -f ":"", df_name?df_name:"",
								options?options:"");
	} else {
		ret = iopipef64 (buf, buf?strlen(buf):0, NULL, NULL, c_spooltimeout,
								IOPIPE_F_RET_EXIT_CODE, "%s %s%s %s%s %s",
								c_spoolprog, config_file?"-c ":"",
								config_file?config_file:"",
								df_name?" -f ":"", df_name?df_name:"",
								options?options:"");
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error executing program (%s): %d", 
								c_spoolprog, ret);
	}
	return ret;
}


static
int
server_callprog (spool, buf, cf_name, df_name, options)
	char	*spool, *buf, *cf_name, *df_name, *options;
{
	int	ret, ret2;
	pid_t	pid=-1;

	server_numchilds++;
	if (c_spoolscale > 1 || c_spoolscale < 0) {
		pid = fork();
		if (pid < 0) {
			FRLOGF (LOG_WARN, "cannot fork into background: %s",
										rerr_getstr3(RERR_SYSTEM));
		} else if (pid > 0) {
			return RERR_OK;
		}
	}
	ret = server_callprog2 (buf, df_name, options);
	if (RERR_ISOK(ret)) {
		FRLOGF (LOG_VERB, "spool entry >>%s<< processed with success - deleting "
								"spool entry", cf_name);
		ret = spool_delentry (spool, cf_name);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "cannot delete spool entry %s: %s", cf_name,
									rerr_getstr3(ret));
			spool_chstate (spool, cf_name, SPOOL_STATE_ERROR, ret);
		}
	} else {
		FRLOGF (LOG_ERR, "error executing spool entry >>%s<<: %s",
							cf_name, rerr_getstr3(ret));
		ret2 = spool_chstate (spool, cf_name, SPOOL_STATE_ERROR, ret);
		if (ret2 < 0) {
			FRLOGF (LOG_WARN, "cannot change status of spool entry %s: %s",
									cf_name, rerr_getstr3(ret2));
		}
		server_sendtrap (cf_name, ret);
	}
	server_numchilds--;
	if (pid == 0) exit (ret);
	return ret;
}


static
int
server_checkchild ()
{
	pid_t				pid;
	int				status;
	int				num, ret;
	static tmo_t	lastnumcheck=0;
	tmo_t				now;

	now = tmo_now ();
	if (lastnumcheck > now) lastnumcheck = 0;
	if (now - lastnumcheck >= 600000000LL /* 10 min */) {
		FRLOGF (LOG_VERB, "check number of childs");
		ret = pop_numchild (&num, getpid ());
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error in pop_numchild(): %s",
										rerr_getstr3(ret));
			lastnumcheck = now - 600000000LL /* 10 min */ + 5000000LL /* 5 sec */;
		} else {
			if (num != server_numchilds) {
				FRLOGF (LOG_NOTICE, "number of processes %d != %d (as should be - "
										"correct it)", num, server_numchilds);
				server_numchilds = num;
			}
			lastnumcheck = now;
		}
	}
	if (server_numchilds < 0) server_numchilds = 0;
	/* cleanup */
	num = server_numchilds + 4;
	while (num--) {
		pid = waitpid (-1, &status, WNOHANG);
		if (pid <= 0) break;
		if (server_numchilds) server_numchilds--;
	}
	if (c_spoolscale <= 1) return 1;
	if (c_spoolscale > server_numchilds) return 1;
	tmo_sleep (100000LL /* 100 ms */);
	return 0;
}






static
int
open_conn_server (fd)
	int	*fd;
{
	int						sd;
	struct sockaddr_un	saddr;
	struct stat				statbuf;
	const char				*usock;

	if (!fd) return RERR_PARAM;
	if (!config_read) read_config();
	usock = c_spoolsock ? c_spoolsock : "/var/spool/spoold.sock";
	if (!lstat (usock, &statbuf) && S_ISSOCK (statbuf.st_mode)) {
		unlink (usock);
	}
	sd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		FRLOGF (LOG_ERR, "cannot create socket: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	saddr.sun_family = AF_UNIX;
	strcpy (saddr.sun_path, usock);
	if (bind (sd, (struct sockaddr *)&saddr, sizeof (saddr)) == -1) {
		FRLOGF (LOG_ERR, "cannot bind to socket: %s", rerr_getstr3(RERR_SYSTEM));
		close (sd);
		return RERR_SYSTEM;
	}
	if (listen (sd, 0) != 0) {
		FRLOGF (LOG_ERR, "cannot listen to socket: %s", 
									rerr_getstr3(RERR_SYSTEM));
		close (sd);
		return RERR_SYSTEM;
	}
	chmod (usock, 0622);
	*fd = sd;
	return RERR_OK;
}



static
int
server_sendtrap (name, code_err)
	char	*name;
	int	code_err;
{
	pid_t	pid;
	int	status, ret;

	if (!c_spooltrap) return RERR_OK;
	pid = fork ();
	if (pid < 0) {
		FRLOGF (LOG_WARN, "cannot fork: %s", rerr_getstr3(RERR_SYSTEM));
		return server_calltrap (name, code_err);
	} else if (pid > 0) {
		waitpid (pid, &status, 0);
		return NEGWEXITSTATUS (status);
	}
	pid = fork ();
	if (pid < 0) {
		ret = server_calltrap (name, code_err);
		exit (ret);
	} else if (pid > 0) {
		/* father */
		exit (0);
	}
	freopen ("/dev/null", "r+", stdin);
	freopen ("/dev/null", "r+", stdout);
	freopen ("/dev/null", "r+", stderr);
	setsid ();
	if (fork () > 0) exit (0);
	ret = server_calltrap (name, code_err);
	exit (ret);
	return ret;
}


static
int
server_calltrap (name, code_err)
	char	*name;
	int	code_err;
{
	char	cmd[1024];
	int	ret;

	if (!c_spooltrap) return RERR_OK;
	if (!name) return RERR_PARAM;
	snprintf (cmd, sizeof (cmd)-1, "%s %s %d", c_spooltrap, name, code_err);
	cmd[sizeof(cmd)-1]=0;
	ret = iopipe (cmd, NULL, 0, NULL, NULL, -1, 0);
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
