/*
 * Copyright (C) 2015-2021 by Frank Reker, Deutsche Telekom AG
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
#include <arpa/inet.h>
#include <errno.h>

#include <fr/base.h>
#include <fr/xml.h>
#include <ldt/ldt.h>
#include "cmd.h"


static int print_devlist (char **, int);
static int showdev (char *, int);
static int printxml (struct xml*, int);
static int printdev (struct xml_tag*, int);
static int printtun (struct xml_tag*, int);
static int printudptun (struct xml_tag*, int);
static int showinfo (char *, const char *, int);
static int printinfo (struct xml*, const char *, int);

#define SHOWDEV_F_TIMESTAMPS	0x01

#define PROG	fr_getprog()

void
usage_newdev ()
{
	printf ("newdev: usage: %s newdev | new <options>\n"
				"         - creates a new ldt device\n"
				"  options are:\n"
				"      <name>         - is the name of the device to create. Name might contain %%d\n"
				"                       which is substituded by a unique number. If no name is given\n"
				"                       tp%%d is used\n"
				"      -h             - this help screen\n"
				"      -a             - print name of created device\n"
				"      -f <flags>     - flags are one or more of the following values, sperated by\n"
				"                       a pipe (|) or comma (,): \n"
				"           client    - we are a client device (unused yet)\n"
				"           server    - we are a server device (unused yet)\n"
				"      -T <type>      - tunnel type. Possible tunnel types are:\n"
				"           dccp      - dccp tunnel over ipv4\n"
				"           dccp6     - dccp tunnel over ipv6\n"
				"           mpdccp    - mpdccp tunnel over ipv4\n"
				"           mpdccp6   - mpdccp tunnel over ipv6\n"
				"\n", PROG);
}

static const struct top_flagmap	creat_fmap[] = {
		{ "client", LDT_CREATE_DEV_F_CLIENT },
		{ "server", LDT_CREATE_DEV_F_SERVER },
		{ NULL, 0 }};

int
cmd_newdev (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	const char	*type = NULL;
	int			c;
	char			*out_name = NULL;
	int			answer = 0;
	int			ret;
	int			flags = 0;

	while ((c=getopt (argc, argv, "haT:f:")) != -1) {
		switch (c) {
		case 'h':
			usage_newdev();
			return RERR_OK;
		case 'a':
			answer = 1;
			break;
		case 'f':
			flags = top_parseflags (optarg, creat_fmap);
			if (flags < 0) return flags;
			break;
		case 'T':
			type = optarg;
			break;
		}
	}
	if (optind < argc) {
		name = argv[optind];
	}
	ret = ldt_create_dev (name, type, answer ? &out_name : 0, flags);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error creating dev >>%s<<: %s", name,
							rerr_getstr3(ret));
		return ret;
	}
	if (answer && out_name) {
		printf ("%s\n", out_name);
		free (out_name);
	}
	return RERR_OK;
}

void
usage_rmdev()
{
	printf ("rmdev: usage: %s rmdev <options>\n"
				"         - removes given ldt device\n"
				"  options are:\n"
				"      <name>         - name of device to remove\n"
				"      -h             - this help screen\n"
				"\n", PROG);
}

int
cmd_rmdev (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	int			c;

	while ((c=getopt (argc, argv, "h")) != -1) {
		switch (c) {
		case 'h':
			usage_rmdev();
			return RERR_OK;
		}
	}
	if (optind >= argc) {
		SLOGF (LOG_ERR2, "missing device name to remove");
		return RERR_PARAM;
	}
	name = argv[optind];
	return ldt_rm_dev (name);
}


void
usage_set_mtu()
{
	printf ("setmtu: usage: %s setmtu <options>\n"
				"         - changes the mtu of net device\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"      -m <mtu>       - mtu to set\n"
				"\n", PROG);
}

int
cmd_set_mtu (argc, argv)
	int	argc;
	char	**argv;
{
	int			mtu = -1;
	const char	*name = NULL;
	int			c;

	while ((c=getopt (argc, argv, "hm:")) != -1) {
		switch (c) {
		case 'h':
			usage_set_mtu();
			return RERR_OK;
		case 'm':
			mtu = atoi (optarg);
			break;
		}
	}
	if (optind < argc) {
		name = argv[optind];
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}
	if (mtu < 0) {
		SLOGF (LOG_ERR2, "missing or invalid mtu (-m) parameter)");
		return RERR_PARAM;
	}
	return ldt_set_mtu (name, mtu);
}

void
usage_ping ()
{
	printf ("ping: usage: %s ping <options>\n"
				"        - sends a pong event (for debug purpose only)\n"
				"          NB: the ping is NOT send to the other tunnel side - it just\n"
				"          sends a pong over the event channel - this only serves for\n"
				"          event-debugging only\n"
				"  options are:\n"
				"      -h             - this help screen\n"
				"      -d <data>      - integer to be included in pong response (default 0)\n"
				"\n", PROG);
}
int
cmd_ping (argc, argv)
	int	argc;
	char	**argv;
{
	int			data = 0;
	int			c;

	while ((c=getopt (argc, argv, "hd:")) != -1) {
		switch (c) {
		case 'h':
			usage_ping();
			return RERR_OK;
		case 'd':
			data = atoi (optarg);
			break;
		}
	}
	return ldt_ping (data);
}

void
usage_getversion()
{
	printf ("getversion: usage: %s getversion | getver | version\n"
				"         - prints the version of the ldt module loaded\n"
				"  options are:\n"
				"      -h             - this help screen\n"
				"\n", PROG);
}

int
cmd_getversion (argc, argv)
	int	argc;
	char	**argv;
{
	int	ret;
	int	c;
	char	*version;

	while ((c=getopt (argc, argv, "h")) != -1) {
		switch (c) {
		case 'h':
			usage_getversion();
			return RERR_OK;
		}
	}
	ret = ldt_get_version (&version);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error receiving version information: %s", rerr_getstr3(ret));
		return ret;
	}
	printf ("%s\n", version);
	free (version);
	return RERR_OK;
}

void
usage_getdevlist()
{
	printf ("getdevlist: usage: %s getdevlist | getlist\n"
				"         - prints out a list of ldt devices in current network namespace\n"
				"  options are:\n"
				"      -h             - this help screen\n"
				"\n", PROG);
}

int
cmd_getdevlist (argc, argv)
	int	argc;
	char	**argv;
{
	int	ret;
	int	c;
	char	**devlist;

	while ((c=getopt (argc, argv, "h")) != -1) {
		switch (c) {
		case 'h':
			usage_getdevlist();
			return RERR_OK;
		}
	}
	ret = ldt_get_devlist (&devlist);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error receiving device list: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = print_devlist (devlist, ret);
	free (devlist);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error printing device list: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

void
usage_showdev()
{
	printf ("showdev: usage: %s showdev <options>\n"
				"         - prints out all settings of given ldt device\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"      -x             - if given, the output is in xml-format, otherwise in\n"
				"                       human readable format\n"
				"      -T             - print timestamps\n"
				"\n", PROG);
}

int
cmd_showdev (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name=NULL;
	char			*devinfo;
	int			ret, c;
	int			xml=0;
	int			pflags=0;

	while ((c=getopt (argc, argv, "hxT")) != -1) {
		switch (c) {
		case 'h':
			usage_showdev();
			return RERR_OK;
		case 'x':
			xml=1;
			break;
		case 'T':
			pflags |= SHOWDEV_F_TIMESTAMPS;
			break;
		}
	}
	if (optind < argc) {
		name = argv[optind];
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}
	ret = ldt_get_devinfo (name, &devinfo, NULL);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error reciving info for device >>%s<<: %s", name,
					rerr_getstr3(ret));
		return ret;
	}
	if (xml) {
		printf ("%s\n", devinfo);
		ret = RERR_OK;
	} else {
		ret = showdev (devinfo, pflags);
	}
	free (devinfo);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error printing device info for device >>%s<<: %s",
					name, rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

void
usage_showinfo()
{
	printf ("showinfo: usage: %s showinfo | info <name> <tag> [<options>]\n"
				"         - prints out all settings of given ldt device\n"
				"      <name>         - name of ldt device\n"
				"      <tag>          - information to request (e.g. atime)\n"
				"  options are:\n"
				"      -h             - this help screen\n"
				"      -e             - missing tags are treated as empty\n"
				"\n", PROG);
}

int
cmd_showinfo (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name=NULL;
	const char	*tag=NULL;
	char			*devinfo;
	int			ret, c;
	int			flags = 0;

	while ((c=getopt (argc, argv, "he")) != -1) {
		switch (c) {
		case 'h':
			usage_showinfo();
			return RERR_OK;
		case 'e':
			flags |= XML_SEARCH_NULLEMPTY;
			break;
		}
	}
	if (optind < argc) {
		name = argv[optind++];
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}
	if (optind < argc) {
		tag = argv[optind++];
	}
	if (!tag) {
		SLOGF (LOG_ERR2, "missing search tag");
		return RERR_PARAM;
	}
	ret = ldt_get_devinfo (name, &devinfo, NULL);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error reciving info for device >>%s<<: %s", name,
					rerr_getstr3(ret));
		return ret;
	}
	ret = showinfo (devinfo, tag, flags);
	free (devinfo);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error printing info >>%s<< for device >>%s<<: %s",
					tag, name, rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

void
usage_showall()
{
	printf ("showall: usage: %s showall <options>\n"
				"         - prints out all settings of all ldt devices in current network\n"
				"           namespace\n"
				"  options are:\n"
				"      -h             - this help screen\n"
				"      -x             - if given, the output is in xml-format, otherwise in\n"
				"                       human readable format\n"
				"      -T             - print timestamps\n"
				"\n", PROG);
}

int
cmd_showall (argc, argv)
	int	argc;
	char	**argv;
{
	char	*devinfo;
	int	ret, c;
	int	xml=0;
	int	pflags=0;

	while ((c=getopt (argc, argv, "hxT")) != -1) {
		switch (c) {
		case 'h':
			usage_showall();
			return RERR_OK;
		case 'x':
			xml=1;
			break;
		case 'T':
			pflags |= SHOWDEV_F_TIMESTAMPS;
			break;
		}
	}

	ret = ldt_get_alldevinfo (&devinfo, NULL);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error reciving info for device: %s", 
					rerr_getstr3(ret));
		return ret;
	}
	if (xml) {
		printf ("%s\n", devinfo);
		ret = RERR_OK;
	} else {
		ret = showdev (devinfo, pflags);
	}
	free (devinfo);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error printing device info for device: %s",
					rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


void
usage_tunbind()
{
	printf ("tunbind: usage: %s tunbind <options> <name>\n"
				"         - binds tunnel to local address and/or to device\n"
				"         - if local address (-b) is given, the tunnel is bound\n"
				"         - if dev AND gw are given the routing is bound to that\n"
				"           interface and gateway\n"
				"         - the local address can be bound only once, the gateway / device\n"
				"           information can be overwritten whenever needed\n"
				"         - both can be specified in one command\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"      -b | -l <addr> - local address to bind to\n"
				"                       the syntax is ip:port (e.g. 1.2.3.4:1234)\n"
				"                       ipv6 addresses must be enclosed in brackets:\n"
				"                       [::]:2374 (0.0.0.0 or :: means any address\n"
				"                       address can be host name to be resolved, too\n"
				"      -I <iface>     - interface to bind to\n"
				"      -4             - force address to be ipv4 (for name resolution)\n"
				"      -6             - force address to be ipv6 (for name resolution)\n"
				"\n", PROG);
}

int
cmd_tunbind (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	int			c, ret;
	const char	*sladdr=NULL, *sgw=NULL, *dev=NULL;
	frad_t		laddr, gw;
	int			flags = 0;
	int			xflags = 0;

	while ((c=getopt (argc, argv, "hl:b:I:64")) != -1) {
		switch (c) {
		case 'h':
			usage_tunbind();
			return RERR_OK;
		case 'l': case 'b':
			sladdr = optarg;
			break;
		case 'I':
			dev = optarg;
			break;
		case '6':
			flags |= FRAD_F_IPV6;
			break;
		case '4':
			flags |= FRAD_F_IPV4;
			break;
		}
	}
	if (optind < argc) {
		name = argv[optind];
		optind++;
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}
	/* parse addresses */
	if (sladdr) {
		ret = frad_getaddr (&laddr, sladdr, flags);
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR2, "error parsing local ip address: %s",
										rerr_getstr3(ret));
			return ret;
		}
	}
	if (sgw) {
		ret = frad_getaddr (&gw, sgw, flags | FRAD_F_NOPORT);
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR2, "error parsing gateway address: %s",
										rerr_getstr3(ret));
			return ret;
		}
	}
	return ldt_tunbind (name, (sladdr ? &laddr : NULL),
									dev, xflags);
}

void
usage_setpeer()
{
	printf ("setpeer: usage: %s setpeer | peer <options> <name>\n"
				"         - set peer address to connect to\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"      -r <addr> 	    - peer address to set to\n"
				"                       the syntax is ip:port (e.g. 1.2.3.4:1234)\n"
				"                       ipv6 addresses must be enclosed in brackets:\n"
				"                       [::]:2374 (0.0.0.0 or :: means any address\n"
				"                       address can be host name to be resolved, too\n"
				"      -4             - force address to be ipv4 (for name resolution)\n"
				"      -6             - force address to be ipv6 (for name resolution)\n"
				"\n", PROG);
}

int
cmd_setpeer (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	int			c, ret;
	const char	*sraddr=NULL;
	frad_t		raddr;
	int			flags = 0;

	while ((c=getopt (argc, argv, "hr:64")) != -1) {
		switch (c) {
		case 'h':
			usage_setpeer();
			return RERR_OK;
		case 'r':
			sraddr = optarg;
			break;
		case '6':
			flags |= FRAD_F_IPV6;
			break;
		case '4':
			flags |= FRAD_F_IPV4;
			break;
		}
	}
	if (optind < argc) {
		name = argv[optind];
		optind++;
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}
	/* parse addresses */
	if (!sraddr) {
		SLOGF (LOG_ERR2, "missing remote address");
		return RERR_PARAM;
	}
	ret = frad_getaddr (&raddr, sraddr, flags);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error parsing remote ip address: %s",
									rerr_getstr3(ret));
		return ret;
	}

	return ldt_setpeer (name, &raddr);
}

void
usage_serverstart()
{
	printf ("serverstart: usage: %s serverstart <options> <name>\n"
				"         - sets server to listen mode\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"\n", PROG);
}

int
cmd_serverstart (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	int			c;

	while ((c=getopt (argc, argv, "h")) != -1) {
		switch (c) {
		case 'h':
			usage_serverstart();
			return RERR_OK;
		}
	}
	if (optind < argc) {
		name = argv[optind];
		optind++;
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}

	return ldt_serverstart (name);
}

void
usage_setqueue()
{
	printf ("setqueue: usage: %s setqueue <options> <name>\n"
				"         - sets server to listen mode\n"
				"  options are:\n"
				"      <name>         - name of ldt device\n"
				"      -h             - this help screen\n"
				"      -T <len>       - tx queue length\n"
				"      -Q <policy>    - queueing policy, possible values:\n"
				"                       drop_oldest, drop_newest\n"
				"\n", PROG);
}

int
cmd_setqueue (argc, argv)
	int	argc;
	char	**argv;
{
	const char	*name = NULL;
	int			c;
	int			txqlen=-1, qpolicy=-1;

	while ((c=getopt (argc, argv, "hT:Q:")) != -1) {
		switch (c) {
		case 'h':
			usage_setqueue();
			return RERR_OK;
		case 'T':
			txqlen = atoi (optarg);
			if (txqlen < 0 || txqlen > 65535) {
				SLOGF (LOG_ERR2, "queue length (%d) out of range [0, 65535]", txqlen);
				return RERR_PARAM;
			}
			break;
		case 'Q':
			sswitch (optarg) {
			sicase ("drop_oldest")
			sicase ("dropoldest")
			sicase ("drop-oldest")
			sicase ("oldest")
				qpolicy = LDT_CMD_SETQUEUE_QPOLICY_DROP_OLDEST;
				break;
			sicase ("drop_newest")
			sicase ("dropnewest")
			sicase ("drop-newest")
			sicase ("newest")
				qpolicy = LDT_CMD_SETQUEUE_QPOLICY_DROP_NEWEST;
				break;
			sdefault
				SLOGF (LOG_ERR2, "invalid queueing policy %s", optarg);
				return -EINVAL;
			} esac;
		}
	}
	if (optind < argc) {
		name = argv[optind];
		optind++;
	}
	if (!name) {
		SLOGF (LOG_ERR2, "missing device name");
		return RERR_PARAM;
	}

	return ldt_setqueue (name, txqlen, qpolicy);
}



static
int
print_devlist (devlist, listlen)
	char	**devlist;
	int	listlen;
{
	int	i;

	if (!devlist || listlen < 0) return RERR_PARAM;
	for (i=0; i<listlen; i++) {
		if (devlist[i] && *devlist[i]) {
			printf ("%s\n", devlist[i]);
		}
	}
	return RERR_OK;
}


static
int
showdev (devinfo, pflags)
	char	*devinfo;
	int	pflags;
{
	struct xml	xml;
	int			ret;

	if (!devinfo) return RERR_PARAM;
	ret = xml_parse (&xml, devinfo, XMLPARSER_FLAGS_STANDARD);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error parsing xml: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = printxml (&xml, pflags);
	xml_hfree (&xml);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error printing xml: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



static
int
printxml (xml, pflags)
	struct xml	*xml;
	int			pflags;
{
	struct xml_cursor	cursor;
	struct xml_tag		*tag;
	int					ret;
	int					hasprint = 0;

	if (!xml) return RERR_PARAM;
	ret = xmlcurs_new (&cursor, xml);
	if (!RERR_ISOK(ret)) return ret;
	ret = xmlcurs_searchtag (&cursor, "dev", 0);
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR, "no >>dev<< tag found");
		return RERR_NOT_FOUND;
	}
	while (1) {
		tag = xmlcurs_tag (&cursor);
		if (!tag) return RERR_INTERNAL;
		if (hasprint) printf ("\n");
		ret = printdev (tag, pflags);
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR, "error printing device info: %s", rerr_getstr3(ret));
			return ret;
		}
		hasprint = 1;
		ret = xmlcurs_next (&cursor);
		if (ret == RERR_NOT_FOUND) break;
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


static
int
printdev (tag, pflags)
	struct xml_tag	*tag;
	int				pflags;
{
	const char	*name;
	char			*s;
	int			ret, ret2=RERR_OK;
	int			neednl=0;
	char			buf[64];
	tmo_t			ts;

	if (!tag) return RERR_PARAM;
	name = xmltag_getattr (tag, "name");
	if (!name) name="<noname>";
	if (strlen (name) > 9) {
		printf ("%s\n%10c", name, ' ');
	} else {
		printf ("%-9s ", name);
	}
	ret = xmltag_search (&s, tag, "mtu", 0);
	if (RERR_ISOK(ret)) {
		printf ("mtu: %s  ", s);
		neednl=1;
	}
	if (neednl) printf ("\n");
	neednl=0;
	if (pflags & SHOWDEV_F_TIMESTAMPS) {
		xmltag_search (&s, tag, "ctime", 0);
		ts = atoll (s);
		cjg_strftime3 (buf, sizeof(buf), "%D", ts*1000000LL);
		printf ("          ctime: %s\n", buf);
		xmltag_search (&s, tag, "mtime", 0);
		ts = atoll (s);
		cjg_strftime3 (buf, sizeof(buf), "%D", ts*1000000LL);
		printf ("          mtime: %s\n", buf);
	}
	ret = printtun (tag, pflags);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error printing tunnel list: %s", rerr_getstr3(ret));
		ret2 = ret;
	}
	return ret2;
}



static
int
printtun (tag, pflags)
	struct xml_tag	*tag;
	int				pflags;
{
	char	*s;
	int	ret;
	int	(*func)(struct xml_tag*, int);
	char	buf[64];
	tmo_t	ts;

	if (!tag) return RERR_PARAM;
	printf ("          tunnel ");
	ret = xmltag_search (&s, tag, "type", 0);
	if (!RERR_ISOK(ret)) {
		printf ("type: <unknown>\n");
		return RERR_OK;
	}
	sswitch (s) {
	sincase ("dccp")
	sincase ("dccp4")
	sincase ("dccp6")
	sincase ("mpdccp")
	sincase ("mpdccp4")
	sincase ("mpdccp6")
		func = &printudptun;
		break;
	sdefault
		printf ("type: %s (unknown)\n", s);
		return RERR_OK;
	} esac;
	printf ("type: %s\n", s);
	if (pflags & SHOWDEV_F_TIMESTAMPS) {
		xmltag_search (&s, tag, "ctime", 0);
		ts = atoll (s);
		cjg_strftime3 (buf, sizeof(buf), "%D", ts*1000000LL);
		printf ("          ctime:  %s\n", buf);
		xmltag_search (&s, tag, "mtime", 0);
		ts = atoll (s);
		cjg_strftime3 (buf, sizeof(buf), "%D", ts*1000000LL);
		printf ("          mtime:  %s\n", buf);
		xmltag_search (&s, tag, "atime", 0);
		ts = atoll (s);
		cjg_strftime3 (buf, sizeof(buf), "%D", ts*1000000LL);
		printf ("          atime:  %s\n", buf);
		xmltag_search (&s, tag, "lstime", 0);
		ts = atoll (s);
		cjg_strftime3 (buf, sizeof(buf), "%D", ts*1000000LL);
		printf ("          lstime: %s\n", buf);
	}
	return func (tag, pflags);
}


static
int
printudptun (tag, pflags)
	struct xml_tag	*tag;
	int				pflags;
{
	char				*s, *s2;
	char				*locaddr, *locport;
	int				ret, ipv6, pos, pos2, pos3;
	int				hasactloc=0;

	if (!tag) return RERR_PARAM;
	ret = xmltag_search (&s, tag, "ipv6", 0);
	ipv6 = (ret == RERR_NOT_FOUND) ? 0 : 1;		/* errors are treated as ok */
	ret = xmltag_search (&s, tag, "locaddr", 0);
	if (!RERR_ISOK(ret)) s = (char*)((ipv6) ? "??::??" : "?.?.?.?");
	locaddr=s;
	ret = xmltag_search (&s2, tag, "locport", 0);
	if (!RERR_ISOK(ret)) s2 = NULL;
	locport=s2;
	if (s2) {
		if (ipv6) {
			printf ("          %nlocal:  [%s]:%s%n", &pos3, s, s2, &pos);
		} else {
			printf ("          %nlocal:  %s:%s%n", &pos3, s, s2, &pos);
		}
	} else {
		if (ipv6) {
			printf ("          %nlocal:  %s%n", &pos3, s, &pos);
		} else {
			printf ("          %nlocal:  %s%n", &pos3, s, &pos);
		}
	}
	ret = xmltag_search (&s, tag, "remaddr", 0);
	if (!RERR_ISOK(ret)) s = (char*)((ipv6) ? "??::??" : "?.?.?.?");
	ret = xmltag_search (&s2, tag, "remport", 0);
	if (!RERR_ISOK(ret)) s2 = "??";
	if (ipv6) {
		printf (" --> %nremote: [%s]:%s\n", &pos2, s, s2);
	} else {
		printf (" --> %nremote: %s:%s\n", &pos2, s, s2);
	}
	pos+=pos2;
	ret = xmltag_search (&s, tag, "actlocaddr", 0);
	if (RERR_ISOK(ret)) {
		hasactloc = 1;
	} else {
		s = locaddr;
	}
	ret = xmltag_search (&s2, tag, "actlocport", 0);
	if (RERR_ISOK(ret)) {
		hasactloc = 1;
	} else {
		s2 = locport;
	}
	if (hasactloc) {
		if (ipv6) {
			printf ("%*cactual: [%s]:%s %n", pos3, ' ', s, s2, &pos2);
		} else {
			printf ("%*cactual: %s:%s %n", pos3, ' ', s, s2, &pos2);
		}
		pos -= pos2;
		if (pos <= 0) pos = 1;
	}
	ret = xmltag_search (&s, tag, "actremaddr", 0);
	if (RERR_ISOK(ret)) {
		ret = xmltag_search (&s2, tag, "actremport", 0);
	}
	if (RERR_ISOK(ret)) {
		if (ipv6) {
			printf ("%*cactual: [%s]:%s\n", pos, ' ', s, s2);
		} else {
			printf ("%*cactual: %s:%s\n", pos, ' ', s, s2);
		}
	} else if (hasactloc) {
		printf ("\n");
	}
	ret = xmltag_search (&s, tag, "status", 0);
	if (RERR_ISOK(ret)) {
		printf ("          status: %s\n", s);
	}

	return RERR_OK;
}

static
int
showinfo (devinfo, stag, flags)
	char			*devinfo;
	const char	*stag;
	int			flags;
{
	struct xml	xml;
	int			ret;

	if (!devinfo) return RERR_PARAM;
	ret = xml_parse (&xml, devinfo, XMLPARSER_FLAGS_STANDARD);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error parsing xml: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = printinfo (&xml, stag, flags);
	xml_hfree (&xml);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error printing xml: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



static
int
printinfo (xml, stag, flags)
	struct xml	*xml;
	const char	*stag;
	int			flags;
{
	struct xml_cursor	cursor;
	int					ret;
	char					*text;

	if (!xml) return RERR_PARAM;
	ret = xmlcurs_new (&cursor, xml);
	if (!RERR_ISOK(ret)) return ret;
	ret = xmlcurs_searchtag (&cursor, "dev", 0);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "no >>dev<< tag found: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = xmlcurs_search (&text, &cursor, (char*)stag, flags);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "querry >>%s<< not found: %s", stag, rerr_getstr3(ret));
		return ret;
	}
	printf ("%s\n", text);
	return RERR_OK;
}




/*
 * event communication 
 */


void
usage_prtev()
{
	printf ("printev: usage: %s printev | prtev <options>\n"
				"        - waits for event and prints it to stdout\n"
				"  options are:\n"
				"      -h             - this help screen\n"
				"      -o             - exits after the first event received\n"
				"      -e             - exit on receive error (implied by -o)\n"
				"      -T <timeout>   - timeout on wait\n"
				"\n", PROG);
}

int
cmd_prtev (argc, argv)
	int	argc;
	char	**argv;
{
	int			c, ret;
	int			one=0;
	tmo_t			timeout=-1;
	int			evtype;
	uint32_t		iarg;
	char			*sarg;
	const char	*evst,*evname;
	int			brkerr = 0;

	while ((c=getopt (argc, argv, "hoT:t:e")) != -1) {
		switch (c) {
		case 'h':
			usage_prtev();
			return RERR_OK;
		case 'o':
			one = 1;
			break;
		case 'e':
			brkerr = 1;
			break;
		case 'T':
		case 't':
			timeout = cf_atotm (optarg);
			break;
		}
	}
	ret = ldt_event_open ();
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR2, "error opening netlink to kernel: %s", rerr_getstr3(ret));
		return ret;
	}
	do {
		sarg = NULL;
		ret = ldt_event_recv (&evtype, &iarg, &sarg, timeout);
		if (!RERR_ISOK(ret)) {
			SLOGF (LOG_ERR2, "error receiving event: %s", rerr_getstr3(ret));
			if (one || brkerr) return ret;
			continue;
		}
		evst = ldt_evgetdesc (evtype);
		if (!evst) evst = "unknown event";
		evname = ldt_evgetname (evtype);
		if (!evname) evname = "unknown";
		printf ("%s (%d, %s): %lu\n", evst, evtype, evname, (unsigned long) iarg);
		if (sarg) {
			printf (":%s\n", sarg);
			free (sarg);
		}
		printf ("\n");
	} while (!one);
	ldt_event_close();

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
