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
#include <sys/types.h>
#include <signal.h>


#include <fr/base.h>
#include <fr/connect.h>
#include <fr/event.h>

#include "eddi.h"


static int read_config ();
static int config_read = 0;
static const char	*sockpath = "/var/run/eddi.sock";


static int reader_noploop (int);


static const char *PROG="eddi";



/* ********************
 * main functions
 * ********************/


int
eddi_usage ()
{
	PROG = fr_getprog ();
	printf ( "%s: usage: %s [<options>] [<eventstr>]\n", PROG, PROG);
	printf ( "  options are:\n"
				"    -h                 - this help screen\n"
				"    -c <configfile>    - alternative config file \n"
				"    -C <def conf file> - alt. default config string\n"
				"    -d                 - daemonize - become server\n"
				"    -D                 - together with -d, don't fork\n"
            "    -r, -b             - become reader\n"
				"    -w                 - send event string (default)\n"
				"    -a                 - with -r send answer, with -w wait for answer\n"
				"    -X                 - use direct reader/writer (for debugging)\n"
				"    -x                 - with -r use polling (for debugging)\n"
				"    -i <var=val>       - send integer variable\n"
				"    -f <var=val>       - send float variable\n"
				"    -s <var=val>       - send string variable\n"
				"    -v                 - be more verbose\n"
				"    -Y                 - signal father after startup (server only)\n"
				"    -N                 - don't change log name\n"
				"    -B <box>           - box to listen on (reader only)\n"
				"    -F <filterid>      - use filter id (reader only)\n"
				"");
	return 0;
}

static const char	*reader_box_name = "accept";
static const char *reader_filterid = NULL;

int
eddi_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	c, ret;
	char	*evstr = NULL;
	int	type = 0;
	char	*var = NULL;
	int	flags = EDDITOOL_F_INSTCB | EDDITOOL_F_VERBOSE | EDDITOOL_F_TRIGFD;
	int	server = 0;
	int	debug = 0;
	int	reader = 0;
	int	direct = 0;
	int	verbose = 0;
	int	noname = 0;

	PROG = fr_getprog ();
	while ((c = getopt (argc, argv, "hc:C:NdDrbwaXxi:f:s:YF:B:v")) != -1) {
		switch (c) {
		case 'h':
			eddi_usage ();
			return 0;
			break;
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'C':
			cf_default_cfname (optarg, 0);
			break;
		case 'v':
			verbose = 1;
			break;
		case 'd':
			server = 1;
			break;
		case 'D':
			debug = 1;
			break;
		case 'r':
		case 'b':
			reader = 1;
			break;
		case 'w':
			break;
		case 'a':
			flags |= EDDITOOL_F_SENDANSWER | EDDITOOL_F_WAITANSWER;
			break;
		case 'x':
			flags &= ~EDDITOOL_F_INSTCB;
			break;
		case 'X':
			direct = 1;
			break;
		case 'i':
			var = optarg;
			type = EVP_T_INT;
			break;
		case 'f':
			var = optarg;
			type = EVP_T_FLOAT;
			break;
		case 's':
			var = optarg;
			type = EVP_T_STR;
			break;
		case 'Y':
			flags |= EDDITOOL_F_SIGFATHER;
			break;
		case 'N':
			noname = 1;
			break;
		case 'B':
			reader_box_name = optarg;
			break;
		case 'F':
			reader_filterid = optarg;
			break;
		};
	}
	flags &= ~EDDITOOL_F_INSTCB;
	if (optind < argc) {
		evstr = argv[optind];
	}
	if (server && !debug) {
		frdaemonize ();
	}
	signal (SIGPIPE, SIG_IGN);
	if (server) {
		if (!noname) slog_set_prog ("eddi-daemon");
		ret = eddi_server (flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error in server loop(): %s", rerr_getstr3(ret));
			return ret;
		}
	} else if (reader) {
		if (!noname) slog_set_prog ("eddi-reader");
		if (direct) {
			ret = eddi_dirreadloop ();
		} else if (flags & EDDITOOL_F_INSTCB) {
			ret = reader_noploop (flags);
		} else {
			ret = eddi_readerpollloop (flags);
		}
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error in reader: %s", rerr_getstr3(ret));
			return ret;
		}
	} else if (var) {
		if (!noname) slog_set_prog ("eddi-sendvar");
		ret = eddi_writersendvar (var, type);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error sending var: %s", rerr_getstr3(ret));
			return ret;
		}
	} else if (!evstr) {
		FRLOGF (LOG_ERR2, "no event string specified for writer\n");
		return RERR_PARAM;
	} else {
		if (!noname) slog_set_prog ("eddi-sender");
		if (!direct) {
			eddi_setwaitforsent ();
			ret = eddi_writersend (evstr, flags);
			if (!(flags & EDDITOOL_F_SENDANSWER)) {
				//tmo_sleep (300000LL);
				eddi_waitforsent (-1);
			}
		} else {
			ret = eddi_dirsendev (evstr);
		}
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error sending event: %s", rerr_getstr3(ret));
			return ret;
		}
	}
	if (verbose) printf ("done\n");
	return RERR_OK;
}





/* **************************
 * reader functions
 * **************************/

static int reader_box = 0;
static int reader_isinit = 0;

static int reader_init2();
static int reader_elab (int, int);
//static int reader_callback (int, void*);
static int trigfd = -1;

int
eddi_readerinit (flags)
	int	flags;
{
	int	ret;

	if (reader_isinit) return RERR_OK;
	FRLOGF (LOG_VERB, "do basic initialization");
	ret = reader_init2 ();
	if (!RERR_ISOK(ret)) return ret;
	if (flags & EDDITOOL_F_INSTCB) {
		ret = eddi_readerinstcb (flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (flags & EDDITOOL_F_TRIGFD) {
		ret = trigfd = evbox_getglobtrigfd ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error getting trigger fd: %s", rerr_getstr3 (ret));
			flags &= ~EDDITOOL_F_TRIGFD;
		}
	}
	return RERR_OK;
}

int
eddi_readerinstcb (flags)
	int	flags;
{
	int	ret;

	FRLOGF (LOG_VERB, "install call back function");
	//ret = evbox_callbackAny (&reader_callback, (void*)(size_t)flags);
	ret = RERR_OK;
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "cannot install callback function: %s",
				rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}

int
eddi_readerpollloop (flags)
	int	flags;
{
	int	ret, id;

	flags &= ~EDDITOOL_F_INSTCB;
	ret = eddi_readerinit (flags);
	if (!RERR_ISOK(ret)) return ret;
	while (1) {
		eddi_readerpoll ();
		if (trigfd > 0) {
			ret = fd_isready (trigfd, -1);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR2, "error in fd_isready(): %s", rerr_getstr3(ret));
				tmo_sleep (1000000LL);
			}
			while (read (trigfd, &id, sizeof (int)) > 0);
		} else {
			evbox_waitany (10000000LL);
		}
	}
	return RERR_INTERNAL;	/* should never reach here */
}


int
eddi_readerpoll ()
{
	int	ret;

	while ((ret = eddi_readerpollone()) == RERR_OK);
	if (ret != RERR_NOT_FOUND) return ret;
	return RERR_OK;
}

int
eddi_readerpollone ()
{	
	return reader_elab (reader_box, 0);
}


static
int
reader_noploop (flags)
	int	flags;
{
	int	ret;

	ret = eddi_readerinit (flags | EDDITOOL_F_INSTCB);
	if (!RERR_ISOK(ret)) return ret;
	while (1) {
		/* do nothing loop */
		pause (); 
	}
	return RERR_INTERNAL;	/* should never reach here */
}

static
int
reader_init2 ()
{
	int	ret;

	if (reader_isinit) return RERR_OK;
	ret = eddi_start ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing eddi subsystem: %s", 
					rerr_getstr3 (ret));
		return ret;
	}
	if (reader_filterid) {
		ret = eddi_addfilterid (reader_filterid, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF(LOG_WARN, "error setting filter id: %s", rerr_getstr3(ret));
		}
	}
	ret = evbox_init (EVBOX_F_GETGLOBAL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing evbox subsystem: %s", 
					rerr_getstr3 (ret));
		return ret;
	}
	reader_box = evbox_get ((char*)(void*)reader_box_name);
	if (reader_box < 0) return reader_box;
	reader_isinit = 1;
	return RERR_OK;
}

#if 0
static
int
reader_callback (id, arg)
	int	id;
	void	*arg;
{
	if (reader_box >= 2 && id != reader_box) return RERR_OK;
	return reader_elab (id, (int)(size_t)arg);
}
#endif

static
int
reader_elab (box, flags)
	int	box, flags;
{
	int				ret;
	struct event	*event;
	int64_t			reqid, shellid=0;
	int				answer = (flags & EDDITOOL_F_SENDANSWER);
	int				level;
	const char		*name, *s;

	ret = evbox_pop (&event, box);
	if (ret == RERR_NOT_FOUND) return ret;
	level = LOG_ERR | ((flags & EDDITOOL_F_VERBOSE)?LOG_STDERR:0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (level, "error popping event (box=%d): %s", box, 
								rerr_getstr3(ret));
		return ret;
	}
	ev_prtparsed (event, 1, EVP_F_PRTTYP|EVP_F_PRTTIME);
	if (answer) {
		ret = ev_getname (&name, event);
		if (RERR_ISOK(ret) && name) {
			s = rindex (name, '/');
			if (s) s = top_skipwhite (s+1);
			if (s && *s) name = s;
		} else {
			name = "config";
		}
		ret = ev_getattr_i (&reqid, event, "reqid");
		ev_getattr_i (&shellid, event, "shellid");
	}
	/* first create publish event */
	if (answer && RERR_ISOK(ret)) {
		event->hastarget = 0;
		ret = ev_setnamef (event, "varmon/%s", name);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting name to publish event: %s",
								rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = ev_setarg_s (event, "publish", 0, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_INFO, "error adding argument 0 to event: %s",
								rerr_getstr3(ret));
		}
		ret = ev_rmattr (event, "reqid");
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_INFO, "error removing reqid from event: %s",
								rerr_getstr3(ret));
		}
		ret = ev_rmattr (event, "shellid");
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_INFO, "error removing shellid from event: %s",
								rerr_getstr3(ret));
		}
		ret = eddi_sendev (event);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot send publish event: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
	} else {
		evpool_release (event);
	}
	/* second create confirm event */
	if (answer && RERR_ISOK(ret)) {
		ret = evpool_acquire (&event);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot get event from pool: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = ev_setname (event, "confirm", 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting name to event: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = ev_addattr_i (event, "reqid", reqid, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting reqid: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = ev_addattr_i (event, "shellid", shellid, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting shellid: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = ev_addattr_i (event, "ctime", tmo_now (), 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting ctime: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = ev_addattr_i (event, "scpierr", 0, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting scpierr: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
		ret = eddi_sendev (event);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot send event: %s", rerr_getstr3(ret));
			evpool_release (event);
			return ret;
		}
	}
	return RERR_OK;
}


/* **************************
 * writer function
 * **************************/

static int writer_isinit = 0;

static int writer_waitanswer ();
static int64_t writer_getreqid();
static int writer_splitvar (char *, int*, int*);
static int writer_init ();



int
eddi_writersend (ev, flags)
	char	*ev;
	int	flags;
{
	int	ret;
	int	level;

	if (!writer_isinit) {
		ret = writer_init ();
		if (!RERR_ISOK(ret)) return ret;
	}
	level = LOG_ERR | ((flags & EDDITOOL_F_VERBOSE)?LOG_STDERR:0);
	ret = eddi_sendstr (ev, EVP_F_CPY);
	if (!RERR_ISOK(ret)) {
		FRLOGF (level, "error sending event >>%s<<: %s", ev,
					rerr_getstr3(ret));
		return ret;
	}
	if (flags & EDDITOOL_F_WAITANSWER) {
		ret = writer_waitanswer ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (level, "error waiting answer: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
	return RERR_OK;
}

int
eddi_writersendvar (var, type)
	char	*var;
	int	type;
{
	char				*s;
	int				idx1, idx2;
	struct event	*ev;
	int				ret;
	char				*val;
	int64_t			ival;
	double			fval;

	if (!var) return RERR_PARAM;
	var = strdup (var);
	if (!var) return RERR_NOMEM;
	if (!writer_isinit) {
		ret = writer_init ();
		if (!RERR_ISOK(ret)) return ret;
	}
	s = top_getfield (&var, "=", 0);
	val = top_getquotedfield (&var, NULL, 0);
	if (!s || !*s) return RERR_INVALID_VAR;
	writer_splitvar (s, &idx1, &idx2);
	ret = evpool_acquire (&ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting event from pool: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = ev_setname (ev, "config", 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error setting event name: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = ev_addattr_i (ev, "reqid", writer_getreqid(), 0);
	if (!RERR_ISOK(ret)) {	
		FRLOGF (LOG_ERR, "error setting reqid: %s", rerr_getstr3(ret));
		evpool_release (ev);
		return ret;
	}
	ret = ev_addattr_i (ev, "shellid", 42, 0);
	if (!RERR_ISOK(ret)) {	
		FRLOGF (LOG_ERR, "error setting shellid: %s", rerr_getstr3(ret));
		evpool_release (ev);
		return ret;
	}
	switch (type) {
	case EVP_T_INT:
		ival = atoll (val);
		ret = ev_addvar_i (ev, s, idx1, idx2, ival, 0);
		break;
	case EVP_T_FLOAT:
		fval = atof (val);
		ret = ev_addvar_f (ev, s, idx1, idx2, fval, 0);
		break;
	default:
		ret = ev_addvar_s (ev, s, idx1, idx2, val, 0);
		break;
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error setting var: %s", rerr_getstr3(ret));
		evpool_release (ev);
		return ret;
	}
	ev_prtparsed (ev, 1, 0);
	ret = eddi_sendev (ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR|LOG_STDERR, "error sending event: %s",
					rerr_getstr3(ret));
		evpool_release (ev);
		return ret;
	}
	ret = writer_waitanswer ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR|LOG_STDERR, "error waiting answer: %s",
					rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


static
int
writer_init ()
{
	int	ret;

	if (writer_isinit) return RERR_OK;
	ret = eddi_start ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing eddi subsystem: %s", 
					rerr_getstr3 (ret));
		return ret;
	}
	writer_isinit = 1;
	return RERR_OK;
}


static
int
writer_splitvar (var, idx1, idx2)
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

static
int64_t
writer_getreqid ()
{
	int64_t	reqid;

	reqid = tmo_now () / 3;
	return reqid;
}

static
int
writer_waitanswer ()
{
	int				box;
	int				ret;
	struct event	*ev;

	box = evbox_get (NULL);
	if (box < 0) {
		FRLOGF (LOG_ERR, "error getting default box: %s", rerr_getstr3(box));
		return box;
	}
	ret = evbox_waitany (5000000LL); /* 5 sec */
	//ret = evbox_waitany (-1LL); /* inf */
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in waitany: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = evbox_pop (&ev, box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error popping event: %s", rerr_getstr3(ret));
		return ret;
	}
	ev_prtparsed (ev, 1, 0);
	evpool_release (ev);
	return RERR_OK;
}





/* **************************
 * direct writer function
 * **************************/

int
eddi_dirsendev (ev)
	const char	*ev;
{
	int	fd;
	int	ret;

	CF_MAYREAD;
	ev = top_stripwhite ((char*)ev, 0);
	if (!ev || !*ev) return RERR_PARAM;
	fd = conn_open_client (sockpath, 0, CONN_T_UNIX, 5);
	if (fd < 0) {
		FRLOGF (LOG_ERR, "error connecting to server >>%s<<: %s", 
					sockpath, rerr_getstr3 (fd));
		return fd;
	}
	ret = conn_sendln (fd, ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sending event to server: %s",
					rerr_getstr3 (ret));
		return ret;
	}
	tmo_sleep (100000LL);
	conn_close (fd);
	return RERR_OK;
}


/* **************************
 * direct reader functions
 * **************************/

static int 				dirread_isinit = 0;
static struct scon	dirread_scon;



int
eddi_dirreadloop ()
{
	int	ret;

	ret = eddi_dirreadinit ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error connecting to server: %s", 
						rerr_getstr3(ret));
		return ret;
	}
	while (1) {
		ret = SCON_WAIT (dirread_scon, 1000000);
		if (!RERR_ISOK(ret) && ret != RERR_TIMEDOUT) tmo_sleep (100000LL /* 100 ms */);
		eddi_dirreadpoll ();
	}
	return RERR_INTERNAL;	/* should never reach here */
}

int
eddi_dirreadinit ()
{
	int	fd, ret;

	if (dirread_isinit) return RERR_OK;
	CF_MAYREAD;
	fd = conn_open_client (sockpath, 0, CONN_T_UNIX, 5);
	if (fd < 0) {
		FRLOGF (LOG_ERR, "error connecting to server >>%s<<: %s", 
					sockpath, rerr_getstr3 (fd));
		return fd;
	}
	ret = SCON_NEW (dirread_scon);
	if (!RERR_ISOK(ret)) return ret;
	ret = SCON_ADD (dirread_scon, fd, scon_termnl, NULL, SCON_F_CLIENTCONN);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding fd to scon: %s", rerr_getstr3(ret));
		return ret;
	}
	dirread_isinit = 1;
	return RERR_OK;
}


int
eddi_dirreadpoll ()
{
	int	ret;

	while ((ret = eddi_dirreadpollone()) == RERR_OK);
	if (ret != RERR_NOT_FOUND) return ret;
	return RERR_OK;
}


int
eddi_dirreadpollone ()
{
	int					ret;
	struct scondata	sdata;

	ret = SCON_RECV (sdata, dirread_scon);
	if (!RERR_ISOK(ret)) return ret;
	printf ("%s\n", sdata.data ? sdata.data : "<NULL>");
	free (sdata.data);
	return RERR_OK;
}




/***************************
 * server functions
 ***************************/

static int 				server_isinit = 0;
static struct scon	serverscon;
static int				server_fd = -1;

static int server_send2all (char*, int);
static void eddi_serverterm (int);



int
eddi_server (flags)
	int	flags;
{
	int	ret;
	pid_t	pid;

	ret = eddi_serverinit ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in server init: %s", 
						rerr_getstr3(ret));
		return ret;
	}
	if (flags & EDDITOOL_F_SIGFATHER) {
		pid = getppid ();
		if (pid > 0) kill (pid, SIGUSR1);
	}
	eddi_serverloop ();
}

	
void
eddi_serverloop ()
{
	int	ret;

	FRLOGF (LOG_DEBUG, "entering server loop");
	while (1) {
		ret = SCON_WAIT (serverscon, 1000000LL);
		if (!RERR_ISOK(ret) && ret != RERR_TIMEDOUT) tmo_sleep (100000LL /* 100 ms */);
		eddi_serverpoll ();
	}
}

static
void
eddi_serverterm (sig)
	int	sig;
{
	if (server_fd > 0) close (server_fd);
	server_fd = -1;
	FRLOGF (LOG_INFO, "I was killed - bye");
	SCON_FREE (serverscon);
	server_isinit = 0;
	if (sig == SIGTERM) exit (0);
}

int
eddi_serverinit ()
{
	int	ret, fd;

	CF_MAYREAD;
	if (server_isinit) return RERR_OK;
	fd = conn_open_server (sockpath, 0, CONN_T_UNIX, 10);
	if (fd < 0) {
		FRLOGF (LOG_ERR, "error openning socket >>%s<<: %s", 
					sockpath, rerr_getstr3 (fd));
		return fd;
	}
	ret = SCON_NEW (serverscon);
	if (!RERR_ISOK(ret)) {
		conn_close (fd);
		return ret;
	}
	ret = SCON_ADD (serverscon, fd, scon_termnl, NULL, 0);
	if (!RERR_ISOK(ret)) {
		conn_close (fd);
		FRLOGF (LOG_ERR, "error adding fd to scon: %s", rerr_getstr3 (ret));
		return ret;
	}
	server_fd = fd;
	signal (SIGTERM, eddi_serverterm);
	signal (SIGPIPE, SIG_IGN);
	server_isinit = 1;
	FRLOGF (LOG_DEBUG, "I am started");
	return RERR_OK;
}


int
eddi_serverpoll ()
{
	int	ret;

	while ((ret = eddi_serverpollone()) == RERR_OK);
	if (ret != RERR_NOT_FOUND) return ret;
	return RERR_OK;
}

int
eddi_serverpollone ()
{
	int					ret;
	struct scondata	sdata;
#ifdef SEND_ACCEPT
	char					_buf[4096];
	char					*buf;
#endif
	char					*s;

	if (!server_isinit) return RERR_NOTINIT;
	ret = SCON_RECV (sdata, serverscon);
	if (!RERR_ISOK(ret)) return ret;
	FRLOGF (LOG_VVERB, "received >>%s<< from %d", sdata.data, sdata.fd);
	s = top_skipwhiteplus (sdata.data, "*");
	if (!sdata.data || sdata.data[0] != '*' || !s || !*s) {
		FRLOGF (LOG_DEBUG, "received invalid event >>%s<<",
									(sdata.data ? sdata.data : "<NULL>"));
		if (sdata.data) free (sdata.data);
		return RERR_OK;
	}
#ifdef SEND_ACCEPT
	if (sdata.dlen + 12 < sizeof (_buf)) {
		buf = _buf;
	} else {
		buf = malloc (sdata.dlen + 12);
		if (!buf) {
			free (sdata.data);
			return RERR_NOMEM;
		}
	}
	sprintf (buf, "# ACCEPT::%s", s);
	free (sdata.data);
	ret = server_send2all (buf, sdata.fd);
	if (buf != _buf) free (buf);
#else
	ret = server_send2all (sdata.data, sdata.fd);
	free (sdata.data);
#endif
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error distributing event: %s",
					rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



static
int
server_send2all (buf, fromfd)
	char	*buf;
	int	fromfd;
{
	struct sconfd	sconfd;
	int				ret;

	if (!buf) return RERR_PARAM;
	FRLOGF (LOG_VERB, "distributing ev >>%s<<", buf);
	TLST_FOREACH (sconfd, serverscon.allfd) {
		if (sconfd.type != FD_T_CONN) continue;
		if (sconfd.fd == fromfd) continue;
		ret = SCON_SENDLN (serverscon, sconfd.fd, buf);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error sending event to %d: %s",
								sconfd.fd, rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}







/* ****************************
 * general static functions
 * ****************************/


static
int
read_config ()
{
	cf_begin_read ();
	sockpath = cf_getarr2 ("sock", "eddi", "/var/run/eddi.sock");
	config_read = 1;
	cf_end_read_cb (&read_config);
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
