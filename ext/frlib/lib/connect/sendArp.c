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
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include "sendArp.h"

#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <ctype.h>
#include <errno.h>
#include <netdb.h>
#include <netinet/in.h>
#include <net/if.h>
#include <netinet/if_ether.h>
#include <arpa/inet.h>
#include <sys/socket.h>
#include <linux/sockios.h>
#include <sys/ioctl.h>

#include "errors.h"
#include "slog.h"



#define IP_ADDR_LEN		4
#define ARP_FRAME_TYPE	0x0806
#define ETHER_HW_TYPE	1
#define IP_PROTO_TYPE	0x0800

struct arp_packet {
	u_char	targ_hw_addr[MAC_ADDR_LEN];
	u_char	src_hw_addr[MAC_ADDR_LEN];
	u_short	frame_type;
	u_short	hw_type;
	u_short	prot_type;
	u_char	hw_addr_size;
	u_char	prot_addr_size;
	u_short	op;
	u_char	sndr_hw_addr[MAC_ADDR_LEN];
	u_char	sndr_ip_addr[IP_ADDR_LEN];
	u_char	rcpt_hw_addr[MAC_ADDR_LEN];
	u_char	rcpt_ip_addr[IP_ADDR_LEN];
	u_char	padding[18];
};


int
if_sendarp (iface)
	const char	*iface;
{
	return if_sendarp2 (iface, 0, NULL, 0);
}

int
if_sendarp2 (iface, ip, mac, optype)
	const char		*iface;
	uint32_t			ip;
	const u_char	*mac;
	int				optype;
{
#ifdef Linux
	struct in_addr		in_addr;
	struct arp_packet	pkt;
	struct sockaddr	sa;
	int					sock;
	u_char				hwaddr[MAC_ADDR_LEN];
	int					i, ret;
	char					*s;

	if (!iface || !*iface) return RERR_PARAM;

	if (!optype) {
		ret = if_sendarp2 (iface, ip, mac, OP_ARP_REQUEST);
		if (!RERR_ISOK(ret)) return ret;
		return if_sendarp2 (iface, ip, mac, OP_ARP_REPLY);
	}

	if (!mac) {
		ret = if_gethwaddr (hwaddr, iface);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error getting hardware address for iface >>%s<<: %s",
									iface, rerr_getstr3 (ret));
			return ret;
		}
		mac = hwaddr;
	}
	if (!ip) {
		ret = if_getipaddr (&in_addr, iface);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error getting ip address for iface >>%s<<: %s",
									iface, rerr_getstr3 (ret));
			return ret;
		}
	} else {
		in_addr.s_addr = htonl (ip);
	}

	sock = socket (AF_INET, SOCK_PACKET, htons(ETH_P_RARP));
	if (sock < 0) {
		FRLOGF (LOG_ERR, "unable to create socket: %s",
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}

	pkt.frame_type = htons(ARP_FRAME_TYPE);
	pkt.hw_type = htons(ETHER_HW_TYPE);
	pkt.prot_type = htons(IP_PROTO_TYPE);
	pkt.hw_addr_size = MAC_ADDR_LEN;
	pkt.prot_addr_size = IP_ADDR_LEN;
	pkt.op = htons (optype);
	for (i = 0; i < MAC_ADDR_LEN; i++) pkt.targ_hw_addr[i] = 0xff;
	for (i = 0; i < MAC_ADDR_LEN; i++) pkt.rcpt_hw_addr[i] = 0xff;
	for (i = 0; i < MAC_ADDR_LEN; i++) pkt.src_hw_addr[i] = hwaddr[i];
	for (i = 0; i < MAC_ADDR_LEN; i++) pkt.sndr_hw_addr[i] = hwaddr[i];
	memcpy (pkt.sndr_ip_addr,&in_addr,IP_ADDR_LEN);
	memcpy (pkt.rcpt_ip_addr,&in_addr,IP_ADDR_LEN);
	memset (pkt.padding,0, 18);

	strncpy (sa.sa_data, iface, sizeof (sa.sa_data));
	sa.sa_data[sizeof(sa.sa_data)-1]=0;
	s = index (sa.sa_data, ':');
	if (s) *s = 0;

	ret = sendto (sock, (const void *)&pkt, sizeof(pkt), 0, &sa,sizeof(sa));
	close(sock);
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error ending arp(): %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}

	return RERR_OK;
#else /* Linux */
	/* to be done */
	return RERR_OK;
#endif
}


int
if_getipaddr (in_addr, iface)
	struct in_addr	*in_addr;
	const char		*iface;
{
	struct ifreq			ifr;
	struct sockaddr_in	sin;
	int						ioctl_sock, ret;

	if (!in_addr || !iface) return RERR_PARAM;
	ret = ioctl_sock = socket(AF_INET, SOCK_PACKET, htons(ETH_P_RARP));
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "unable to create socket: %s", rerr_getstr3(ret));
		return ret;
	}
	strncpy(ifr.ifr_name, iface, sizeof (ifr.ifr_name));
	ifr.ifr_name[sizeof (ifr.ifr_name)-1] = 0;
	ifr.ifr_addr.sa_family = AF_INET;
	ret = ioctl (ioctl_sock, SIOCGIFADDR, &ifr);
	close (ioctl_sock);
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error in ioctl: %s", rerr_getstr3(ret));
		return ret;
	}
	memcpy(&sin, &ifr.ifr_addr, sizeof(struct sockaddr_in));
	in_addr->s_addr = sin.sin_addr.s_addr;
	return RERR_OK;
}


int
if_gethwaddr (obuf, iface)
	u_char		*obuf;
	const char	*iface;
{
	int				ioctl_sock, ret;
	struct ifreq	ifr;
	char				*s;

	if (!obuf || !iface) return RERR_PARAM;
	ret = ioctl_sock = socket(AF_INET, SOCK_PACKET, htons(ETH_P_RARP));
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "unable to create socket: %s", rerr_getstr3(ret));
		return ret;
	}
	strncpy(ifr.ifr_name, iface, sizeof (ifr.ifr_name));
	ifr.ifr_name[sizeof (ifr.ifr_name)-1] = 0;
	s = index (ifr.ifr_name, ':');
	if (s) *s = 0;

	ret = ioctl(ioctl_sock, SIOCGIFHWADDR, &ifr);
	close (ioctl_sock);
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error in ioctl: %s", rerr_getstr3 (ret));
		return ret;
	}
	memcpy(obuf, ifr.ifr_hwaddr.sa_data, MAC_ADDR_LEN);
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
