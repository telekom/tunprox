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
#include <stdint.h>
#include <string.h>
#include <strings.h>
#ifdef Linux
#include <getopt.h>
#endif
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <sys/types.h>


#include <fr/base.h>
#include <fr/connect.h>

#include "garp.h"


static const char	*PROG=NULL;
#define MYPROG	(PROG?PROG:"garp")


int
garp_usage ()
{
	PROG = fr_getprog ();
	fprintf (stderr, "%s: usage: %s -i <iface> [<options>]\n"
		"  sends a gratious arp request and/or reply\n"
		"    <options> are:\n"
		"    -i <iface>   interface the arp shall be sent to (e.g. eth0)\n"
		"    -m <mac>     senders mac address - if not given, it is retreived\n"
		"                 automatically from interface\n"
		"    -I <ip>      senders ip address - if not gien, it is retreived\n"
		"                 automatically from interface\n"
		"    -r           sends an ARP REQUEST (default is a REPLY)\n"
		"    -R           sends an ARP REQUEST followed by an ARP REPLY\n"
		"    -c <file>    alternative config file, the default depends\n"
		"                 on first part of programme name (before underscore)\n",
		MYPROG, MYPROG);
	return 0;
}



int
garp_main (argc, argv)
	int	argc;
	char	**argv;
{
	int				c;
	struct in_addr	in_addr;
	uint32_t			ip = 0;
	const char		*iface = NULL;
	u_char			mac[MAC_ADDR_LEN];
	const char		*s;
	u_char			x, *m=NULL;
	int				hx=0;
	int				optype = OP_ARP_REPLY;
	int				ret;

	PROG = fr_getprog ();
	
	while ((c=getopt (argc, argv, "hi:I:m:rR")) != -1) {
		switch (c) {
		case 'h':
			garp_usage();
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
			exit (0);
		case 'i':
			iface = optarg;
			break;
		case 'I':
			inet_aton (optarg, &in_addr);
			ip = in_addr.s_addr;
			break;
		case 'm':
			for (s = optarg, m=mac; *s && m-mac < MAC_ADDR_LEN; s++) {
				if (!ishex (*s)) continue;
				x = x<<4 | HEX2NUM (*s);
				if (hx) {
					*m = x;
					m++;
					hx=0;
				} else hx=1;
			}
			break;
		case 'r':
			optype = OP_ARP_REQUEST;
			break;
		case 'R':
			optype = 0;
			break;
		}
	}
	ret = if_sendarp2 (iface, ip, mac, optype);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR2, "error sending arp: %s", rerr_getstr3 (ret));
		return ret;
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
