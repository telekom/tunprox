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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/filter.h>
#include <dirent.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>


#include <fr/base.h>
#include <fr/netlink.h>
#include <fr/event.h>

#include "uevent2eddi.h"



#define PRT_FMT_NUL		0x00
#define PRT_FMT_NL		0x01
#define PRT_FMT_EDDI		0x02
#define PRT_FMT_MASK		0x07
#define PRT_F_WATTR		0x08
#define PRT_F_DOT			0x10
#define PRT_F_EDDISEND	0x20


#define UEVENT2EDDI_F_SIGFATHER	0x100



static int convert_nul (char *, int);
static int print_eddi (int, char*, int, int, int);
static int print_msg (int, char*, int, int, int);
static int recv_prt (int, int, int, int);
static int runloop (int, int, int, int);
static int loop_on_msg (int, int, int, int);



static const char *PROG="uevent2eddi";

int
uevent2eddi_usage ()
{
	PROG = fr_getprog ();
	printf ("%s: usage: %s [<options>]\n", PROG, PROG);
	printf ( "  options are:\n"
				"    -h               - this help screen\n"
				"    -c <config file> - alternative config file\n"
				"    -C <def config>  - alt. default config string\n"
				"    -1               - print one message only\n"
				"    -r               - print in raw-format\n"
				"    -0               - as -r but NUL-terminate lines\n"
				"    -a               - don't append device attributes to event\n"
				"    -u               - receive udev events\n"
				"    -p               - print a dot for each event to stderr\n"
				"    -d               - daemonize - eddi events are sent to eddi\n"
				"    -D               - debug (don't fork)\n"
				"    -f <file>        - use <file> as out filter\n"
				"    -F <id>          - filter <id> is loaded from config file\n"
				"    -Y               - signal father after startup (server only)\n"
				"");
	return 0;
}



int
uevent2eddi_main (argc, argv)
	int	argc;
	char	**argv;
{
	int	c, ret;
	int	one=0;
	int	sd, fmt=PRT_FMT_EDDI;
	int	flags = PRT_F_WATTR;
	int	isudev = 0;
	int	daemon=0;
	int	debug=0;
	char	*filter=NULL, *filterid=NULL;
	int	fid=0;

	PROG = fr_getprog ();
	while ((c=getopt (argc, argv, "Dc:C:h10rhaukdpf:F:Y"))!= -1) {
		switch (c) {
		case 'h':
			uevent2eddi_usage ();
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 'C':
			cf_default_cfname (optarg, 0);
			break;
		case '1':
			one=1;
			break;
		case '0':
			fmt = PRT_FMT_NUL;
			break;
		case 'r':
			fmt = PRT_FMT_NL;
			break;
		case 'a':
			flags &= ~PRT_F_WATTR;
			break;
		case 'u':
			isudev = 1;
			break;
		case 'p':
			flags |= PRT_F_DOT;
			break;
		case 'd':
			daemon = 1;
			flags |= PRT_F_EDDISEND;
			break;
		case 'D':
			debug = 1;
			break;
		case 'f':
			filter = optarg;
			break;
		case 'F':
			filterid = optarg;
			break;
		case 'Y':
			flags |= UEVENT2EDDI_F_SIGFATHER;
			break;
		}
	}
	if (daemon) fmt = PRT_FMT_EDDI;
	if ((fmt == PRT_FMT_EDDI) && (filter || filterid)) {
		/* create filter */
		fid = evf_new ();
		if (!RERR_ISOK(fid)) {
			FRLOGF (LOG_ERR2, "error creating new event filter: %s", 
									rerr_getstr3(fid));
			return fid;
		}
		if (filterid) {
			ret = evf_addfilter (fid, filterid, 0);
		} else {
			ret = evf_addfile (fid, filter, EVF_F_OUT);
		}
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error creating new event filter: %s", 
									rerr_getstr3(ret));
			return ret;
		}
		ret = eddi_addfilter (fid);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error adding filter to eddi: %s",
									rerr_getstr3(ret));
			return ret;
		}
	}
	fmt |= flags;
	ret = sd = uevent_open (-1, isudev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error opening netlink socket: %s", rerr_getstr3(ret));
	}
	if (one) {
		ret = recv_prt (sd, 1, fmt, fid);
	} else {
		signal (SIGPIPE, SIG_IGN);
		if (daemon) {
			if (!debug) frdaemonize ();
			ret = eddi_start ();
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR2, "error starting eddi: %s", rerr_getstr3(ret));
				return ret;
			}
		}
		ret = runloop (sd, fmt, fid, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR2, "error executing loop: %s", rerr_getstr3(ret));
		}
	}
	uevent_close (sd);
	return 0;
}





/* ************************
 * static functions
 * ************************/

static
int
runloop (sd, fmt, fid, flags)
	int	sd, fmt, fid, flags;
{
	pid_t	pid;

	if (flags & UEVENT2EDDI_F_SIGFATHER) {
		pid = getppid ();
		if (pid > 0) kill (pid, SIGUSR1);
	}
	loop_on_msg (sd, 1, fmt, fid);
	return RERR_OK;
}

static
int
loop_on_msg (sd, fd, fmt, fid)
	int	sd, fd, fmt, fid;
{
	if (sd < 3 || fd < 0) return RERR_PARAM;
	while (1) {
		recv_prt (sd, fd, fmt, fid);
	}
	return RERR_INTERNAL;
}


static
int
recv_prt (sd, fd, fmt, fid)
	int	sd, fd, fmt, fid;
{
	int	ret, blen;
	char	*buf;

	ret = blen = uevent_recv (sd, &buf, -1);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error receiving uevent: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = print_msg (fd, buf, blen, fmt, fid);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error printing uevent: %s", rerr_getstr3(ret));
		free (buf);
		return ret;
	}
	return RERR_OK;
}


static
int
print_msg (fd, buf, buflen, fmt, fid)
	int	fd, buflen, fmt, fid;
	char	*buf;
{
	int	ret, obuflen;
	char	*obuf = NULL;

	switch (fmt & PRT_FMT_MASK) {
	case PRT_FMT_NL:
		ret = convert_nul (buf, buflen);
		break;
	case PRT_FMT_EDDI:
		ret = print_eddi (fd, buf, buflen, fmt, fid);
		if (!RERR_ISOK(ret)) return ret;
		goto afterprt;
		break;
	case PRT_FMT_NUL:
		ret = RERR_OK;
		break;
	default:
		ret = RERR_INVALID_FORMAT;
		break;
	}
	if (!RERR_ISOK(ret)) return ret;
	obuflen = obuf ? (ssize_t)strlen (obuf) : buflen + 1;
	ret = write (fd, obuf ? obuf : buf, obuflen);
	if (obuf) free (obuf);
	if (ret < 0) {
		FRLOGF (LOG_ERR2, "error writing message: %s", 
					rerr_getstr3 (RERR_SYSTEM));
		return RERR_SYSTEM;
	} else if (ret < obuflen) {
		FRLOGF (LOG_ERR2, "write truncated");
		return RERR_SYSTEM;
	}
afterprt:
	free (buf);
	if (fmt & PRT_F_DOT) write (2, ".", 1);
	return RERR_OK;
}

static
int
print_eddi (fd, buf, buflen, fmt, fid)
	int	fd, buflen, fmt, fid;
	char	*buf;
{
	int				ret, ret2=RERR_OK;
	struct event	*ev;
	struct evlist	evlist, *ptr;
	char				*obuf;

	ret = evpool_acquire (&ev);
	if (!RERR_ISOK(ret)) return ret;
	ret = uevent2eddi (ev, buf, buflen, (fmt & PRT_F_WATTR) ? UEVENT2EDDI_F_ADDSYSATTR : 0);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev);
		return ret;
	}
	if (fmt & PRT_F_EDDISEND) {
		ret = eddi_sendev (ev);
		if (!RERR_ISOK(ret)) {
			evpool_release (ev);
			return ret;
		}
		return RERR_OK;
	}
	ret = evf_apply (&evlist, fid, ev, EVF_F_OUT|EVF_F_OUTTARGET);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev);
		return ret;
	}
	for (ptr=&evlist; ptr; ptr=ptr->next) {
		if (ptr->target != EVF_T_ACCEPT) continue;
		ret = ev_create (&obuf, ptr->event, 0);
		if (!RERR_ISOK(ret)) {
			ret2 = ret;
			continue;
		}
		fdprtf (fd, "%s\n", obuf);
		free (obuf);
	}
	evf_evlistfree (&evlist, 0);
	if (!RERR_ISOK(ret2)) {
		evpool_release (ev);
		return ret2;
	}
	//evpool_release (ev);
	return RERR_OK;
}


static
int
convert_nul (buf, buflen)
	char	*buf;
	int	buflen;
{
	int	i;

	if (!buf) return RERR_PARAM;
	for (i=0; i<buflen+2; i++) {
		if (buf[i]==0) buf[i]='\n';
	}
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
