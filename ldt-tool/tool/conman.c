/*
 * Copyright (C) 2015-2022 by Frank Reker, Deutsche Telekom AG
 *
 * LDT - Lightweight (MP-)DCCP Tunnel kernel module
 *
 * This is not Open Source software. 
 * This work is made available to you under a source-available license, as 
 * detailed below.
 *
 * Copyright 2022 Deutsche Telekom AG
 *
 * Permission is hereby granted, free of charge, subject to below Commons 
 * Clause, to any person obtaining a copy of this software and associated 
 * documentation files (the "Software"), to deal in the Software without 
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * “Commons Clause” License Condition v1.0
 *
 * The Software is provided to you by the Licensor under the License, as
 * defined below, subject to the following condition.
 *
 * Without limiting other conditions in the License, the grant of rights under
 * the License will not include, and the License does not grant to you, the
 * right to Sell the Software.
 *
 * For purposes of the foregoing, “Sell” means practicing any or all of the
 * rights granted to you under the License to provide to third parties, for a
 * fee or other consideration (including without limitation fees for hosting 
 * or consulting/ support services related to the Software), a product or 
 * service whose value derives, entirely or substantially, from the
 * functionality of the Software. Any license notice or attribution required
 * by the License must also include this Commons Clause License Condition
 * notice.
 *
 * Licensor: Deutsche Telekom AG
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>

#include <fr/base.h>
#include <fr/xml.h>
#include "conman.h"
#include <ldt/ldt.h>


static int wait_on_come_back (const char*, int);
static int wait_on_down (const char*);
static int addr_add (const char *, int);
static int try_connect (const char*, frad_pair_t*);
static int try_connect_all (const char*);

extern const char *PROG;

void
usage_conman ()
{
	printf ("conman: usage: %s conman | handshake | inithandshake <options>\n"
				"         - starts initial handshake (must be called after setauth)\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"      -t <id>        - tunnel id (default 0)\n"
				"      -d             - debug (don't fork to background\n"
				"      -c <addrpair>  - a pair of addresses to connect to (e.g: 10.0.0.1:1450/10.0.0.2:1450)\n"
				"                       the first is the local address, the second one the remote address.\n"
				"                       This option can be repeated several times, in which the connection\n"
				"                       manager tries in turn to establish connection\n"
				"      -4             - following addresses are IPv4 addresses\n"
				"      -6             - following addresses are IPv6 addresses\n"
				"\n", PROG);
}

static frad_pair_t	*addr_list = NULL;
static int				num_addr = 0;

int
cmd_conman (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	int			c, ret;
	int			debug=0;
	int			addrflags=0;

	while ((c=getopt (argc, argv, "hd46c:")) != -1) {
		switch (c) {
		case 'h':
			usage_conman ();
			return RERR_OK;
		case 'd':
			debug = 1;
			break;
		case '4':
			addrflags = FRAD_F_IPV4;
			break;
		case '6':
			addrflags = FRAD_F_IPV6;
			break;
		case 'c':
			ret = addr_add (optarg, addrflags);
			if (!RERR_ISOK(ret)) {
				SLOGF (LOG_ERR2, "error parsing address: %s", rerr_getstr3(ret));
				return ret;
			}
		}
	}
	if (optind < argc) {
		name = argv[optind];
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}
	ret = ldt_open ();
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR2, "error opening link to kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = ldt_event_open ();
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR2, "error opening event link: %s", rerr_getstr3(ret));
		return ret;
	}
	if (!debug) {
		frdaemonize ();
	}
	while (1) {
		ret = try_connect_all (name);
		if (!RERR_ISOK(ret)) {
			SLOGF (LOG_INFO|LOG_STDERR, "error in connect: %s", rerr_getstr3(ret));
			tmo_sleep (15000000LL);	/* 15 sec */
			continue;
		}
		ret = wait_on_down (name);
		if (!RERR_ISOK(ret)) {
			SLOGF (LOG_WARN2, "error in wait on down - try to reconnect: %s",
						rerr_getstr3(ret));
			tmo_sleep (1000000LL);	/* 1sec */
			continue;
		}
	}
	return RERR_OK;
}


static
int
try_connect_all (name)
	const char	*name;
{
	int	i, ret;

	if (num_addr <= 0) {
		return try_connect (name, NULL);
	}
	for (i=0; i<num_addr; i++) {
		ret = try_connect (name, addr_list+i);
		if (RERR_ISOK(ret)) return i;
		tmo_sleep (500000LL);	/* 0.5 sec */
	}
	return RERR_CONNECTION;
}

static
int
try_connect (name, addr)
	const char	*name;
	frad_pair_t	*addr;
{
	char	buf[128];
	int	ret;

	if (!addr) {
		ret = ldt_tun_setpeer (name, NULL, 5000000LL);
		if (!RERR_ISOK(ret)) {
			SLOGF (LOG_WARN2, "error reconnecting: %s", rerr_getstr3(ret));
			return ret;
		}
	} else {
		ret = ldt_tunbind (name, &addr->local);
		if (!RERR_ISOK(ret)) {
			frad_sprint (buf, sizeof (buf), &addr->local);
			SLOGF (LOG_WARN2, "error binding to address %s: %s\n", buf,
					rerr_getstr3(ret));
			return ret;
		}
		ret = ldt_tun_setpeer (name, &addr->remote, 5000000LL);
		if (!RERR_ISOK(ret)) {
			frad_sprint (buf, sizeof (buf), &addr->remote);
			SLOGF (LOG_WARN2, "error connecting to %s: %s\n", buf,
					rerr_getstr3(ret));
			return ret;
		}
	}
	return RERR_OK;
}



static
int
addr_add (str, flags)
	const char	*str;
	int			flags;
{
	frad_pair_t	addr, *p;
	int			ret;

	if (!str) return RERR_PARAM;
	ret = frad_getaddrpair (&addr, str, flags);
	if (!RERR_ISOK(ret)) return ret;
	p = realloc (addr_list, sizeof (frad_pair_t) * (num_addr+1));
	if (!p) return RERR_NOMEM;
	addr_list = p;
	frad_cppair (&addr_list[num_addr], &addr);
	num_addr++;
	return RERR_OK;
}


static
int
wait_on_down (name)
	const char	*name;
{
	int							ret;
	struct ldt_evinfo	evinfo;

	if (!name) return RERR_PARAM;
	ret = ldt_waitifaceev (&evinfo, name, (1<<LDT_EVTYPE_DOWN) |
						(1<<LDT_EVTYPE_TUNDOWN) | (1<<LDT_EVTYPE_IFDOWN) |
						(1<<LDT_EVTYPE_TPDOWN), -1, 0);
	if (RERR_ISOK(ret)) {
		if (ret != RERR_TIMEDOUT) {
			SLOGFE (LOG_ERR2, "error waiting on down event: %s",
									rerr_getstr3(ret));
		}
		return ret;
	}
	if (evinfo.evtype != LDT_EVTYPE_DOWN) {
		wait_on_come_back (name, evinfo.evtype);
		return RERR_OK;
	}
	return RERR_OK;
	
}


static
int
wait_on_come_back (name, evtype)
	const char	*name;
	int			evtype;
{
	int							ret;
	int							tundown=0, ifdown=0;
	struct ldt_evinfo	evinfo;

	switch (evtype) {
	case LDT_EVTYPE_TUNDOWN:
		tundown=1;
		break;
	case LDT_EVTYPE_IFDOWN:
		ifdown=1;
		break;
	case LDT_EVTYPE_TPDOWN:
		exit (0);
	}
	while (1) {
		ret = ldt_waitifaceev (&evinfo, name, (1<<LDT_EVTYPE_TUNUP) |
						(1<<LDT_EVTYPE_TUNDOWN) | (1<<LDT_EVTYPE_IFDOWN) |
						(1<<LDT_EVTYPE_IFUP) | (1<<LDT_EVTYPE_TPDOWN), -1, 0);
		if (!RERR_ISOK(ret)) {
			SLOGF (LOG_WARN2, "error waiting on tunnel to come back: %s",
							rerr_getstr3(ret));
			return ret;
		}
		switch (evinfo.evtype) {
		case LDT_EVTYPE_TUNDOWN:
			tundown=1;
			break;
		case LDT_EVTYPE_IFDOWN:
			ifdown=1;
			break;
		case LDT_EVTYPE_TUNUP:
			tundown=0;
			if (!ifdown) return RERR_OK;
			break;
		case LDT_EVTYPE_IFUP:
			ifdown=0;
			if (!tundown) return RERR_OK;
			break;
		case LDT_EVTYPE_TPDOWN:
			exit (0);
		}
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
