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
 * Portions created by the Initial Developer are Copyright (C) 2003-2016
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include <stdlib.h>
#include <stdio.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <net/if.h>
#include <sys/types.h>
#include <sys/utsname.h>
#include <string.h>
#include <strings.h>
#include <netdb.h>
#include <errno.h>
extern int h_errno;
#ifdef SunOS
extern int errno;
#endif
#include <arpa/inet.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <stdint.h>



#include <fr/base.h>
#include "addr.h"


void
frad_setipv4 (ad, ad4, port)
	frad_t	*ad;
	uint32_t	ad4;
	uint16_t	port;
{
	if (!ad) return;
	ad->v4 = (struct sockaddr_in) {
		.sin_family = AF_INET,
		.sin_addr.s_addr = ad4,
		.sin_port = htons (port),
	};
}

void
frad_setipv6 (ad, ad6, port)
	frad_t	*ad;
	uint8_t	ad6[16];
	uint16_t	port;
{
	if (!ad) return;
	ad->v6 = (struct sockaddr_in6) {
		.sin6_family = AF_INET6,
		.sin6_port = htons (port),
	};
	memcpy (ad->v6.sin6_addr.s6_addr, ad6, 16);
}

void
frad_setipv4only (ad, ad4)
	frad_t	*ad;
	uint32_t	ad4;
{
	if (!ad) return;
	if (FRADP_FAM(ad) != AF_INET) return;
	ad->v4.sin_addr.s_addr = ad4;
}

void
frad_setipv6only (ad, ad6)
	frad_t	*ad;
	uint8_t	ad6[16];
{
	if (!ad) return;
	if (FRADP_FAM(ad) != AF_INET6) return;
	memcpy (ad->v6.sin6_addr.s6_addr, ad6, 16);
}

void
frad_setport (ad, port)
	frad_t	*ad;
	uint16_t	port;
{
	if (!ad) return;
	if (FRADP_ISIPV6(ad)) {
		ad->v6.sin6_port = htons (port);
	} else {
		ad->v4.sin_port = htons (port);
	}
}

uint16_t
frad_getport (ad)
	frad_t *ad;
{
	if (!ad) return 0;
	if (FRADP_ISIPV6(ad)) {
		return htons (ad->v6.sin6_port);
	} else {
		return htons (ad->v4.sin_port);
	}
}

int
frad_isany (ad)
	frad_t	*ad;
{
	int	i;

	if (!ad) return 1;
	if (FRADP_ISIPV6(ad)) {
		for (i=0; i<4; i++)
         if (ad->v6.sin6_addr.s6_addr32[i]) return 0;
		return 1;
	} else {
		return !(ad->v4.sin_addr.s_addr);
	}
}



int
frad_eq (ad1, ad2)
	frad_t	*ad1;
	frad_t	*ad2;
{
	if (!ad1 || !ad2) return 0;
	if (FRADP_FAM(ad1) != FRADP_FAM(ad2)) return 0;
	if (FRADP_ISIPV6(ad1)) {
		if (ad1->v6.sin6_port != ad2->v6.sin6_port) return 0;
		if (memcmp (ad1->v6.sin6_addr.s6_addr, ad2->v6.sin6_addr.s6_addr, 16)) return 0;
		return 1;
	} else {
		if (ad1->v4.sin_port != ad2->v4.sin_port) return 0;
		if (ad1->v4.sin_addr.s_addr != ad2->v4.sin_addr.s_addr) return 0;
		return 1;
	}
}

void
frad_cp (ad1, ad2)
	frad_t	*ad1;
	frad_t	*ad2;
{
	if (!ad1 || !ad2) return;
	if (FRADP_ISIPV6(ad2)) {
		frad_setipv6 (ad1, ad2->v6.sin6_addr.s6_addr, htons(ad2->v6.sin6_port));
	} else {
		frad_setipv4 (ad1, ad2->v4.sin_addr.s_addr, htons(ad2->v4.sin_port));
	}
}

ssize_t
frad_sprint (buf, blen, addr)
	char		*buf;
	size_t	blen;
	frad_t	*addr;
{
	char	sad[INET6_ADDRSTRLEN+1];
	int	ipv6;

	if (!addr) return RERR_PARAM;
	ipv6 = FRADP_ISIPV6(addr);
	if (!inet_ntop (FRADP_FAM(addr), ipv6 ? (void*)&addr->v6.sin6_addr : 
							(void*)&addr->v4.sin_addr, sad, sizeof (sad)))
		return RERR_SYSTEM;
	if (ipv6) {
		return snprintf (buf, blen, "[%s]:%u", sad, (unsigned)frad_getport(addr));
	} else {
		return snprintf (buf, blen, "%s:%u", sad, (unsigned)frad_getport(addr));
	}
}


int
frad_getaddr (addr, str, flags)
	frad_t		*addr;
	const char	*str;
	int			flags;
{
	int					ipv6, a, b, c, d, n, port;
	uint32_t				ip4;
	char					*s, *s2;
	struct in6_addr	ad6;
	struct addrinfo	hints, *res;
	int					ret;

	if (!str || !addr) return RERR_PARAM;
	if (flags & FRAD_F_IPV4) {
		ipv6 = 0;
	} else if (flags & FRAD_F_IPV6) {
		ipv6 = 1;
	} else {
		/* it is ipv6 if it has at least 2 : */
		ipv6 = ((s=index(str, ':')) && index (s+1, ':')) ? 2 : 0;
	}
	once {
		if (!ipv6) {
			n = sscanf (str, "%d.%d.%d.%d:%d", &a, &b, &c, &d, &port);
			if (n < 5) port = 0;
			if (n < 4) break;
			ip4 = (uint32_t)a << 24 | (uint32_t)b << 16 | (uint32_t)c << 8 | (uint32_t)d;
			ip4 = htonl (ip4);
			if (flags & FRAD_F_NOPORT) {
				port = frad_getport (addr);
			}
			frad_setipv4 (addr, ip4, port);
			return RERR_OK;
		}
		if (ipv6 != 2) ipv6 = ((s=index(str, ':')) && index (s+1, ':')) ? 2 : 1;
		if (ipv6 != 2) break;
		if ((s = index (str, '['))) {
			s2 = strdup (s+1);
			s = index (s2, ']');
			if (s) {
				*s = 0;
				s++;
			}
			if (flags & FRAD_F_NOPORT) {
				port = frad_getport (addr);
			} else {
				if (s) s=index (s, ':');
				if (s) s = top_skipwhite (s+1);
				if (!s || !isdigit (*s)) {
					port = 0;
				} else {
					port = atoi (s);
				}
			}
		} else {
			s2 = (char*)str;
			if (flags & FRAD_F_NOPORT) {
				port = frad_getport (addr);
			} else {
				port = 0;
			}
		}
		ret = inet_pton (AF_INET6, s2, &ad6);
		if (s2 != str) free (s2);
		if (ret != 1) return RERR_INVALID_FORMAT;
		frad_setipv6 (addr, ad6.s6_addr, port);
		return RERR_OK;
	}
	/* need dns */
	if (flags & FRAD_F_NODNS) return RERR_INVALID_FORMAT;
	ipv6 = (flags & FRAD_F_DNSV6 || flags & FRAD_F_IPV6) ? 1 : 0;
	bzero (&hints, sizeof (hints));
	if (flags & FRAD_F_DNSV4 || flags & FRAD_F_IPV4) {
		hints.ai_family = AF_INET;
	} else if (flags & FRAD_F_DNSV6 || flags & FRAD_F_IPV6) {
		hints.ai_family = AF_INET6;
		hints.ai_flags = AI_V4MAPPED;
	} else {
		hints.ai_family = AF_UNSPEC;
		hints.ai_flags = AI_ADDRCONFIG;
	}
	/* get port first */
	s2 = (char*)str;
	if (flags & FRAD_F_NOPORT) {
		port = frad_getport (addr);
	} else {
		s = rindex (str, ':');
		if (!s) {
			port = 0;
		} else {
			s2 = strdup (str);
			if (!s2) return RERR_NOMEM;
			s = rindex (s2, ':');
			if (!s) { free (s2); return RERR_INTERNAL; }
			*s = 0;
			s = top_skipwhite (s+1);
			port = atoi (s);
		}
	}
	ret = getaddrinfo (s2, NULL, &hints, &res);
	if (s2 != str) free (s2);
	if (ret == EAI_SYSTEM) return RERR_SYSTEM;
	if (ret != 0) return RERR_NOT_FOUND;
	if (!res) return RERR_INTERNAL;
	frad_cp (addr, (frad_t*)res->ai_addr);
	frad_setport (addr, port);
	freeaddrinfo (res);
	return RERR_OK;
}


int
frad_getaddrpair (addr, str, flags)
	frad_pair_t	*addr;
	const char	*str;
	int			flags;
{
	const char	*s;
	int			ret;

	if (!addr || !str) return RERR_PARAM;
	ret = frad_getaddr (&addr->local, str, flags);
	if (!RERR_ISOK(ret)) return ret;
	s = index (str, '/');
	if (!s) s = index (str, '-');
	if (!s) return RERR_INVALID_FORMAT;
	s++;
	ret = frad_getaddr (&addr->remote, s, flags);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


void
frad_cppair (ad1, ad2)
	frad_pair_t	*ad1;
	frad_pair_t	*ad2;
{
	if (!ad1 || !ad2) return;
	frad_cp (&ad1->local, &ad2->local);
	frad_cp (&ad1->remote, &ad2->remote);
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
