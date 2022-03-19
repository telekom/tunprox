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
#include <string.h>
#include <strings.h>

#include "errors.h"
#include "config.h"
#include "slog.h"
#include "strcase.h"
#include "parse_url.h"




int 
url_parse (url, prot, host, port, path, use_ssl, flags)
	char			*url, **host;
	const char	**path;
	int			*prot, *port, flags, *use_ssl;
{
	const char	*s, *shost;
	int			_prot, _port;
	int			p, ret;
	int			default_port=80;

	if (!url || !*url) return RERR_INVALID_URL;

	/* initialize */
	if (prot) *prot=PROT_NONE;
	if (host) *host=NULL;
	if (port) *port=0;
	if (path) *path=NULL;
	_prot=PROT_NONE;
	_port=0;

	/* parse protocoll part if present */
	for (s=url; isalpha (*s); s++);
	if (*s==':' && s[1]=='/' && s[2]=='/') {
		ret = url_map_prot (url, s-url, &_prot, &default_port, use_ssl);
		if (ret==RERR_INVALID_URL) 
			FRLOGF (LOG_ERR, "invalid protocoll: >>%s<<", url);
		if (!RERR_ISOK(ret)) return ret;
		for (s++; *s=='/'; s++);
	} else {
		s=url;
	}

	/* parse host part */
	shost=s;
	for (; *s && *s!='/' && *s!=':' && *s!='?'; s++);
	if (s==shost) return RERR_INVALID_URL;
	if (host) {
		*host = malloc (s-shost+1);
		if (!*host) return RERR_NOMEM;
		strncpy (*host, shost, s-shost);
		(*host)[s-shost]=0;
	}

	/* parse port - if present */
	if (*s==':') {
		for (p=0, s++; isdigit (*s); s++) {
			p*=10;
			p+=*s-'0';
		}
		_port=p;
	}
	if (*s && path) *path=s;

	if (flags & PURL_FLAG_DEFAULT) {
		/* if no protocoll is given use default one */
		if (_prot == PROT_NONE) {
			s = cf_getval ("url_default_prot");
			if (!s) s = cf_getval2 ("url_default_protocoll", "http");
			ret = url_map_prot (s, -1, &_prot, &default_port, use_ssl);
			if (ret == RERR_INVALID_PROT) {
				FRLOGF (LOG_WARN, "invalid protocoll in config "
											"file: >>%s<< - assume http", s);
				_prot = PROT_HTTP;
			}
		}

		/* if no port was given use the default one */
		if (_port == 0) {
			s = cf_getval ("url_default_port");
			if (s) {
				_port = cf_atoi (s);
			} else {
				_port = default_port;
			}
		}
	}

	if (prot) *prot=_prot;
	if (port) *port=_port;

	/* everything's ok */
	return RERR_OK;
}


int
url_make (url, prot, host, port, path)
	char			**url;
	const char	*host, *path;
	int			prot, port;
{
	const char	*sprot, *sport, *stuff;
	char			_sport[32];
	int			len;

	if (!url) return RERR_PARAM;
	if (!host || !*host) return RERR_INVALID_URL;
	switch (prot) {
	case PROT_HTTP:
		sprot="http://";
		break;
	case PROT_HTTPS:
		sprot="https://";
		break;
	case PROT_NONE:
		sprot="";
		break;
	default:
		return RERR_INVALID_PROT;
		break;
	}
	if (port>0) {
		sprintf (_sport, ":%d", port);
		sport=_sport;
	} else {
		sport="";
	}
	if (!path) path="";
	if (*path!='/' && *path!='?') {
		stuff="/";
	} else {
		stuff="";
	}
	len = strlen (sprot) + strlen (host) + strlen (sport) + strlen (path) + 
			strlen (stuff);
	*url = malloc (len+1);
	if (!*url) return RERR_NOMEM;
	sprintf (*url, "%s%s%s%s%s", sprot, host, sport, stuff, path);
	return RERR_OK;
}




struct prot_map {
	const char	*sprot;
	int			prot;
	int			ssl, port;
};

static struct prot_map prot_map[] = {
	{ "http", PROT_HTTP, 0, 80 },
	{ "https", PROT_HTTPS, 1, 443 },
	{ "smtp", PROT_SMTP, 0, 25 },
	{ "smtps", PROT_SMTPS, 1, 465 },
	{ "pop3", PROT_POP3, 0, 110},
	{ "pop", PROT_POP3, 0, 110},
	{ "pop3s", PROT_POP3S, 1, 995},
	{ "pops", PROT_POP3S, 1, 995},
	{ "imap", PROT_IMAP, 0, 143},
	{ "imap4", PROT_IMAP, 0, 143},
	{ "imaps", PROT_IMAP, 1, 993},
	{ "imap4s", PROT_IMAP, 1, 993},
	{ "ldap", PROT_LDAP, 0, 389},
	{ "ldaps", PROT_LDAPS, 1, 636},
	{ "syncml", PROT_SYNCML, 0, 8443},
	{ "syncmls", PROT_SYNCMLS, 1, 8443},
	{ "ftp", PROT_FTP, 0, 21 },
	{ "tftp", PROT_TFTP, 0, 69 },
	{ "sftp", PROT_SFTP, 0, 115 },
	{ "gopher", PROT_GOPHER, 0, 70 },
	{ "rsync", PROT_RSYNC, 0, 873 },
	{ "finger", PROT_FINGER, 0, 79 },
	{ NULL, PROT_NONE, 0, 0 }};


int
url_map_prot (sprot, len, prot, default_port, use_ssl)
	const char	*sprot;
	int			*prot, *default_port, *use_ssl;
	int			len;
{
	struct prot_map	*pm;

	if (!sprot) return RERR_INVALID_PROT;
	if (len<=0) len=strlen (sprot);
	for (pm=prot_map; pm->sprot; pm++) {
		if ((ssize_t)strlen (pm->sprot)==len && !strncasecmp (pm->sprot, sprot, len)) 
			break;
	}
	if (!pm->sprot) {
		return RERR_INVALID_PROT;
	}
	if (prot) *prot=pm->prot;
	if (use_ssl) *use_ssl=pm->ssl;
	if (default_port) *default_port = pm->port;
	return RERR_OK;
}


const char*
url_prot_to_str (prot)
	int		prot;
{
	struct prot_map	*pm;

	for (pm=prot_map; pm->sprot; pm++) {
		if (pm->prot == prot) return pm->sprot;
	}
	return "unknown";
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
