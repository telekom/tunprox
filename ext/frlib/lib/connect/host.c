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

//#define SIOCGIFCONF 0x8912 /* get iface list */


#include "errors.h"
#include "config.h"
#include "slog.h"
#include "strcase.h"
#include "host.h"


struct my_host {
	char	**names;
	int	numnames;
	ip_t	*ips;
	int	numips;
};

static struct my_host	LocalHost;
static int					host_havelocalhost=0;



static int host_GetLocalHost2 ();
static int hfree_LocalHost();
static int host_LocalHostInsert (const char*);
static int host_addAltNames (const char *);
static int host_localhost_insert_ip (const char*, int, int);
static int host_GetLocalHost ();
static int host_cmphnames (const char*, const char*);
static int host_get_ifips ();

#define ishnamechar(c)	((c)&&(isalnum(c)||((c)=='.')||((c)=='-')||((c)=='_')))
#define MIN(a,b)	(((a)<(b))?(a):(b))





int
host_getip (ip, name)
	ip_t			*ip;
	const char	*name;
{
	struct hostent	* hent;
#ifdef HAVE_IP6
	int				i;
#endif
	char				*dn;

	if (!ip || !name || !*name) return RERR_PARAM;

	if (RERR_ISOK (host_str2ip (ip, name))) return RERR_OK;
	hent = gethostbyname (name);
	if (!hent && (dn=index (name, '.'))) {
		*dn = 0;
		hent = gethostbyname (name);
		*dn = '.';
	}
	if (!hent) {
		FRLOGF (LOG_ERR, "gethostbyname() failed: %s", hstrerror (h_errno));
		return RERR_INVALID_HOST;
	}
#ifdef HAVE_IP6
	if (h_addrtype == AF_INET) {
		ip->s6_addr[10] = ip->s6_addr[11] = 0xff;
		for (i=0; i<10; i++) ip->s6_addr[i] = 0;
		for (i=12; i<16; i++) ip->s6_addr[i] = hent->h_addr[i-12];
	} else if (h_addrtype == AF_INET6) {
		for (i=0; i<16; i++) ip->s6_addr[i] = hent->h_addr[i];
	}
#else
	*ip = *(uint32_t *)hent->h_addr;
#endif

	return RERR_OK;
}


int
host_getname (name, str)
	char			*name;	/* buffer, need to be at least 64 bytes long */
	const char	*str;
{
	struct hostent	*hent;
	char				*dn;

	hent = gethostbyname (str);
	if (!hent && (dn=index (str, '.'))) {
		*dn = 0;
		hent = gethostbyname (str);
		*dn = '.';
	}
	if (!hent) {
		FRLOGF (LOG_ERR, "gethostbyname() failed: %s",
								hstrerror (h_errno));
		return RERR_INVALID_HOST;
	}
	strncpy (name, hent->h_name, 63);
	name[63]=0;
	if ((dn=index (name, '.')))
		*dn=0;

	return RERR_OK;
}

int
host_getIPv4 (_ip, str)
	uint32_t	*_ip;
	const char	*str;
{
#ifdef HAVE_IP6
	ip_t	ip;
#endif

	if (!_ip || !str) return RERR_PARAM;

#ifdef HAVE_IP6
	ret = host_getip (&ip, str);
	if (!RERR_ISOK(ret)) return ret;

	_ip = *(uint32_t *) (void *) & (ip.s6_addr[12]);
	if (ip.s6_addr[10] == ip.s6_addr[11] == 0xff) {
		for (i=0; i<10; i++) if (ip.s6_addr[i] != 0) break;
		if (i==10) return RERR_OK;
	}
	return RERR_NOIPV4;
#else	/* HAVE_IP6 */
	return host_getip ((ip_t *) &_ip, str);
#endif
}



char *
host_GetLocalHostName ()
{
	int		ret;

	if (!host_havelocalhost) {
		ret = host_GetLocalHost ();
		if (!RERR_ISOK(ret)) return NULL;
	}
	if (LocalHost.numnames<1) return NULL;
	return LocalHost.names[0];
}








int
host_ip2str (str, ip)
	char	*str;
	ip_t	ip;
{
	struct in_addr	ip4;
	int				isip4=0;
	char				*ipstr;

#ifdef HAVE_IP6
	if (ip.s6_addr[10] == ip.s6_addr[11] == 0xff) {
		for (i=0; i<10; i++) if (ip.s6_addr[i] != 0) break;
		if (i==10) {
			isip4 = 1;
			ip4.s_addr = * (uint32_t *) (void *) &(ip.s6_addr[12]);
		}
	}
#else	/* HAVE_IP6 */
	ip4.s_addr = ip;
	isip4 = 1;
#endif	/* HAVE_IP6 */

	if (isip4) {
		ipstr = inet_ntoa (ip4);
		strcpy (str, ipstr);
#ifdef HAVE_IP6
	} else {
		inet_ntop (AF_INET6, &ip, str, INET6_ADDRSTRLEN);
#endif	/* HAVE_IP6 */
	}
	return RERR_OK;
}



int
host_str2ip (ip, str)
	ip_t			*ip;
	const char	*str;
{
	struct in_addr	ip4;
#ifndef Linux
	int				a,b,c,d,n;
	int				ok;
	uint32_t			_ip;
#endif

	if (!str || !*str || !ip) return RERR_PARAM;

	/* do we have a valid ipv4 address */
#ifdef Linux
	if (inet_aton (str, &ip4))
#else
	n=sscanf (str, "%d.%d.%d.%d", &a, &b, &c, &d);
	ok = n==4 && a<256 && a>=0 && b<256 && b>=0 && c<256 && c>=0 
		&& d<256 && d>=0;
	_ip = ((uint32_t)a << 24) | ((uint32_t)b << 16) | ((uint32_t)c << 8)
		| (uint32_t)d;
	_ip = htonl (_ip);
	ip4.s_addr = _ip;
	if (ok)
#endif
	{
#ifdef HAVE_IP6
		*(uint32_t *) (void *) &(ip->s6_addr[12]) = ip4.s_addr;
		ip->s6_addr[10] = ip->s6_addr[11] = 0xff;
		for (i=0; i<10; i++) ip->s6_addr[i] = 0;
#else
		*ip = ip4.s_addr;
#endif
#ifdef HAVE_IP6
	} else if (inet_pton (AF_INET6, str.c_str(), ip) <= 0) {
#endif
	} else {
		/* we have an invalid format */
		return RERR_INVALID_IP;
	}

	return RERR_OK;
}





/* *******************
 * host is local
 * *******************/

int
host_islocal (islocal, name)
	int			*islocal;
	const char	*name;
{
	const char	*s, *str;
	char			hostname[512];
	int			i, n, ret;
	ip_t			ip;

	if (!islocal || !name || !*name) return RERR_PARAM;
	*islocal=0;
	if (!host_havelocalhost) {
		ret = host_GetLocalHost ();
		if (!RERR_ISOK(ret)) return ret;
	}
	/* do we have an url */
	s = index (name, ':');
	if (s && *s && s[1]=='/' && s[2]=='/') {
		str=s+3;
		for (s=str; *s && ishnamechar (*s); s++);
		n=MIN((ssize_t)sizeof(hostname)-1,s-str);
		strncpy (hostname, str, n);
		hostname[n]=0;
		name=hostname;
	}
	/* do we have an ip */
	ret = host_str2ip (&ip, name);
	if (RERR_ISOK(ret)) {
		/* compare ip */
		for (i=0; i<LocalHost.numips; i++) {
			if (!memcmp (&(LocalHost.ips[i]), &ip, sizeof (ip_t))) {
				*islocal = 1;
				return RERR_OK;
			}
		}
	} else {
		/* compare names */
		for (i=0; i<LocalHost.numnames; i++) {
			if (host_cmphnames (LocalHost.names[i], name)) {
				*islocal=1;
				return RERR_OK;
			}
		}
	}
	*islocal=0;
	return RERR_OK;
}





static
int
host_cmphnames (hn1, hn2)
	const char	*hn1, *hn2;
{
	char	*dn1, *dn2;
	int	ok = 0;

	if (!hn1 || !hn2) return 0;
	if (!strcmp (hn1, hn2)) return 1;
	dn1 = index (hn1, '.');
	dn2 = index (hn2, '.');
	if ((dn1 && dn2) || (!dn1 && !dn2)) return 0;
	if (dn1) {
		*dn1 = 0;
		ok = !strcmp (hn1, hn2);
		*dn1 = '.';
	} else {
		*dn2 = 0;
		ok = !strcmp (hn1, hn2);
		*dn2 = '.';
	}
	return ok;
}


/* *****************
 * get local host 
 * *****************/




static
int
host_GetLocalHost ()
{
	int	ret;

	if (host_havelocalhost) 
		return RERR_OK;
	ret = host_GetLocalHost2();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error: %s", rerr_getstr3(ret));
		hfree_LocalHost();
		return ret;
	}
	return RERR_OK;
}


static
int
hfree_LocalHost ()
{
	if (LocalHost.names) free (LocalHost.names);
	if (LocalHost.ips) free (LocalHost.ips);
	bzero (&LocalHost, sizeof (struct my_host));
	host_havelocalhost = 0;
	return 1;
}


static
int
host_LocalHostInsert (name)
	const char	*name;
{
	int	i;

	if (!name) return RERR_PARAM;
	if (!*name) return RERR_OK;
	for (i=0; i<LocalHost.numnames; i++) {
		if (!strcasecmp (LocalHost.names[i], name)) {
			/* it's already inserted */
			return RERR_OK;
		}
	}
	i=LocalHost.numnames++;
	LocalHost.names = realloc (LocalHost.names, sizeof(struct my_host)*
								LocalHost.numnames);
	if (!LocalHost.names) {
		LocalHost.numnames=0;
		return RERR_NOMEM;
	}
	LocalHost.names[i] = strdup (name);
	if (!LocalHost.names[i]) {
		LocalHost.numnames--;
		return RERR_NOMEM;
	}
	return RERR_OK;
}

static
int
host_GetLocalHost2 ()
{
	struct utsname	buf;
	char				*str, *_str;
	char				hostname[512];
	int				ret, i, num;
#ifdef Linux
	int				len;
	char				*s;
#endif

	if (host_havelocalhost) {
		hfree_LocalHost ();
	}

	bzero (&LocalHost, sizeof (struct my_host));

	ret = host_LocalHostInsert ("localhost");
	if (!RERR_ISOK(ret)) {
		hfree_LocalHost ();
		return ret;
	}
	once {
		hostname[sizeof(hostname)-1] = 0;
		if (gethostname (hostname, sizeof(hostname)-1) == -1) break;
		if (!*hostname) break;
#ifdef Linux
		len=strlen (hostname);
		s=hostname+len+1;
		if (!index (hostname, '.') &&
				getdomainname (s, sizeof(hostname)-len-2) != -1 && *s) 
			hostname[len]='.';
#endif
		ret = host_LocalHostInsert (hostname);
		if (!RERR_ISOK(ret)) {
			hfree_LocalHost ();
			return ret;
		}
	}

	once {
		if (uname (&buf) != 0) break;

		str = buf.nodename;
		if (!str || !*str) break;
#if defined _GNU_SOURCE
		if (!index (str, '.') && buf.domainname) {
			_str = malloc (sizeof (str) + sizeof (buf.domainname)+2);
			if (!_str) {
				hfree_LocalHost ();
				return RERR_NOMEM;
			}
			sprintf (_str, "%s.%s", str, buf.domainname);
		} else {
#endif
			_str = strdup (str);
			if (!_str) {
				hfree_LocalHost ();
				return RERR_NOMEM;
			}
#if defined _GNU_SOURCE
		}
#endif
		ret = host_LocalHostInsert (_str);
		free (_str);
		if (!RERR_ISOK(ret)) {
			hfree_LocalHost ();
			return ret;
		}
	}

	num = LocalHost.numnames;
	for (i=0; i<num; i++) {
		ret = host_addAltNames (LocalHost.names[i]);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error in host_addAltNames(): %s",
									rerr_getstr3(ret));
			hfree_LocalHost ();
			return ret;
		}
	}
	host_havelocalhost = 1;
	ret = host_get_ifips ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error in host_get_ifips(): %s",
								rerr_getstr3(ret));
	}

	return RERR_OK;
}


static
int
host_get_ifips ()
{
	int 				numreqs = 50, sd, i;
	struct ifconf	ifc;
	struct ifreq	*ifr;
	struct in_addr	*ia;
	int				ret;

	sd=socket (AF_INET, SOCK_STREAM, 0);
	ifc.ifc_len = sizeof(struct ifreq) * numreqs;
	ifc.ifc_buf = malloc(ifc.ifc_len);
	if (ioctl(sd, SIOCGIFCONF, &ifc) < 0) {
		FRLOGF (LOG_ERR, "error in ioctl (\"SIOCGIFCONF\"): %s",
								rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	ifr = ifc.ifc_req;
	for (i=0; i < ifc.ifc_len; i += sizeof(struct ifreq)) {
		ia= (struct in_addr *) ((ifr->ifr_ifru.ifru_addr.sa_data)+2);

		ret = host_localhost_insert_ip ((char*)(void*)&(ia->s_addr), AF_INET, 4);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error in host_localhost_insert_ip(): %s", 
									rerr_getstr3(ret));
			free (ifc.ifc_buf);
			close (sd);
			return ret;
		}
		ifr++;
	}

	free (ifc.ifc_buf);
	close (sd);
	return RERR_OK;
}



static
int
host_addAltNames (name)
	const char	*name;
{
	struct hostent	* hent;
	int				i, ret;
	char				*dn;

	hent = gethostbyname (name);
	if (!hent && (dn=index (name, '.'))) {
		*dn = 0;
		hent = gethostbyname (name);
		*dn = '.';
	}
	if (!hent) return RERR_OK;
	ret = host_LocalHostInsert (hent->h_name);
	if (!RERR_ISOK(ret)) return ret;

	for (i=0; hent->h_aliases && hent->h_aliases[i]; i++) {
		ret = host_LocalHostInsert (hent->h_aliases[i]);
		if (!RERR_ISOK(ret)) return ret;
	}
	for (i=0; hent->h_addr_list && hent->h_addr_list[i]; i++) {
		ret = host_localhost_insert_ip (hent->h_addr_list[i], 
							hent->h_addrtype, hent->h_length);
		if (!RERR_ISOK(ret)) return ret;
	}

	return RERR_OK;
}



static
int
host_localhost_insert_ip (addr, addrtype, addrlen)
	const char	*addr;
	int			addrtype, addrlen;
{
	int	i;
	ip_t	ip;

	if (!addr) return RERR_PARAM;
#ifdef HAVE_IP6
	if (addrtype == AF_INET) {
		ip.s6_addr[10] = ip.s6_addr[11] = 0xff;
		for (i=0; i<10; i++) ip.s6_addr[i] = 0;
		for (i=12; i<16; i++) ip.s6_addr[i] = addr[i-12];
	} else if (h_addrtype == AF_INET6) {
		for (i=0; i<16; i++) ip.s6_addr[i] = addr[i];
	}
#else
	ip = *(uint32_t *)addr;
#endif
	/* insert ip */
	for (i=0; i<LocalHost.numips; i++) {
		if (!memcmp (&(LocalHost.ips[i]), &ip, sizeof (ip_t))) break;
	}
	if (i>=LocalHost.numips) {
		i=LocalHost.numips++;
		LocalHost.ips = realloc (LocalHost.ips, sizeof (ip_t)*LocalHost.numips);
		if (!LocalHost.ips) {
			LocalHost.numips=0;
			return RERR_NOMEM;
		}
		LocalHost.ips[i] = ip;
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
