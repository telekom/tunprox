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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif


#include "connection.h"
#include "errors.h"
#include "strcase.h"
#include "parse_url.h"
#include "getdoc.h"
#include "gzip.h"
#include "config.h"
#include "slog.h"
#include "textop.h"
#include "fileop.h"



static int getdoc_via_http (char**, int, const char*, const char*, const char*, int, char**);
static int do_getdoc (char**, const char*, int, const char*, int, const char*, const char*, int, int);
static int try_getdoc (char**, int, const char*, int, const char*, const char*, const char*, int);
static int try_getdoc1 (char**, int, const char*, int, const char*, const char*, const char*, int);
static int try_getdoc2 (char**, int, const char*, int, const char*, const char*, const char*, int);
static int try_getdoc3 (char**, int, const char*, int, const char*, const char*, const char*, int);
static int getdoc2 (char**, const char*, int, const char*, const char*, int);
static int getdoc_from_file (char**, const char*, int);



int
getdoc (obuf, url, user, pass, flags)
	char			**obuf;
	const char	*url, *user, *pass;
	int			flags;
{
	int	url_is_host=0;
	int	url_is_file=0;
	int	ret;

	if (!url) {
		url = cf_getval ("getdoc_url");
		if (!url) {
			url = cf_getval ("getdoc_host");
			if (!url) {
				FRLOGF (LOG_ERR, "no url given, and none found in config file");
				return RERR_INVALID_URL;
			}
			url_is_host = 1;
		}
	}
	if (url && !strncasecmp (url, "file://", 7)) {
		url_is_file=1;
		ret = getdoc_from_file (obuf, url, flags);
	} else {
		ret = getdoc2 (obuf, url, url_is_host, user, pass, flags);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting document from %s >>%s<<: %s",
								url_is_file?"file":(url_is_host?"host":"url"), 
								url, rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
getdoc_from_file (obuf, url, flags)
	char			**obuf;
	const char	*url;
	int			flags;
{
	if (!obuf || !url) return RERR_PARAM;
	if (strncasecmp (url, "file://", 7) != 0) return RERR_INVALID_URL;
	url+=7;
	*obuf = fop_read_fn (url);
	if (!*obuf) {
		FRLOGF (LOG_ERR, "error reading file >>%s<<", url);
		return RERR_SYSTEM;
	}
	return RERR_OK;
}

static
int
getdoc2 (obuf, url, url_is_host, user, pass, flags)
	char			**obuf;
	const char	*url, *user, *pass;
	int			url_is_host, flags;
{
	int			prot=PROT_NONE, port=0;
	const char	*host = NULL, *path = NULL;
	char			*host2=NULL;
	int			ret;
	char			_buf[256], *url2 = NULL;

	if (!url || !*url) return RERR_INVALID_URL;

	if (!url_is_host) {
		url2 = top_strcpdup (_buf, sizeof (_buf), url);
		if (!url2) return RERR_NOMEM;
		ret = url_parse (url2, &prot, &host2, &port, &path, NULL, 
							PURL_FLAG_DEFAULT);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error parsing url >>%s<<: %s", url, 
									rerr_getstr3(ret));
			return ret;
		}
		if (!host2) return RERR_INVALID_URL;
		host = host2;
	} else {
		host = url;
	}
	ret = try_getdoc3 (obuf, prot, host, port, path, user, pass, flags);
	if (host2) free (host2);
	if (url2 && url2 != _buf) free (url2);
	return ret;
}


static
int
try_getdoc3 (obuf, prot, host, port, path, user, pass, flags)
	char			**obuf;
	const char	*host, *path, *user, *pass;
	int			prot, port, flags;
{
	int	ret;

	if (prot != PROT_NONE) 
		return try_getdoc2 (obuf, prot, host, port, path, user, pass, flags);
	prot = PROT_HTTPS;
	ret = try_getdoc2 (obuf, prot, host, port, path, user, pass, flags);
	if (RERR_ISOK(ret)) return ret;
	prot = PROT_HTTP;
	ret = try_getdoc2 (obuf, prot, host, port, path, user, pass, flags);
	return ret;
}


static
int
try_getdoc2 (obuf, prot, host, port, path, user, pass, flags)
	char			**obuf;
	const char	*host, *path, *user, *pass;
	int			prot, port, flags;
{
	int	ret;

	ret = try_getdoc1 (obuf, prot, host, port, path, user, pass, flags);
	if (RERR_ISOK(ret)) return ret;
	if (path && *path) return ret;
	path = cf_getval ("getdoc_path");
	if (!path || !*path) return ret;
	ret = try_getdoc1 (obuf, prot, host, port, path, user, pass, flags);
	return ret;
}

static
int
try_getdoc1 (obuf, prot, host, port, path, user, pass, flags)
	char			**obuf;
	const char	*host, *path, *user, *pass;
	int			prot, port, flags;
{
	int	ret;

	ret = try_getdoc (obuf, prot, host, port, path, user, pass, flags);
	if (ret != RERR_AUTH) return ret;
	if (user && *user) return ret;
	user = cf_getval ("getdoc_user");
	if (!user) return ret;
	ret = try_getdoc (obuf, prot, host, port, path, user, pass, flags);
	if (ret != RERR_AUTH) return ret;
	pass = cf_getval ("getdoc_pass");
	if (!pass) return ret;
	ret = try_getdoc (obuf, prot, host, port, path, user, pass, flags);
	return ret;
}


static
int
try_getdoc (obuf, prot, host, port, path, user, pass, flags)
	char			**obuf;
	const char	*host, *path, *user, *pass;
	int			prot, port, flags;
{
	char			*buf, *url;
	const char	*s;
	int			ret, myport;

	ret = url_make (&url, prot, host, port, path);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in url_make (prot=%d, host=>>%s<<, port=%d, "
								"path=>>%s<<): %s", prot, host, port, path,
								rerr_getstr3(ret));
		return ret;
	}
	if (port<=0) {
		s = cf_getval ("getdoc_port");
		if (s) {
			myport = cf_atoi (s);
		} else {
			switch (prot) {
			case PROT_HTTP:
				myport = 80;
				break;
			case PROT_HTTPS:
				myport = 443;
				break;
			}
		}
	} else {
		myport=port;
	}
	buf=NULL;
	ret = do_getdoc (&buf, url, prot, host, myport, user, pass, flags, 0);
	free (url);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_INFO, "could not get document with incredentials: "
								"url=>>%s<<, prot=%d, host=>>%s<<, myport=%d -> %s",
								url, prot, host, port, rerr_getstr3(ret));
		return ret;
	}

	if (obuf) {
		*obuf = buf;
	} else {
		free (buf);
	}
	return RERR_OK;
}



static
int
do_getdoc (obuf, url, prot, host, port, user, pass, flags, num_redirect)
	char			**obuf;
	const char	*url, *host, *user, *pass;
	int			prot, port, flags, num_redirect;
{
	int	cflags=0;
	int	fd;
	int	timeout, ret;
	char	*redirect = NULL, *host2 = NULL;

	if (!obuf || !url || !host || prot<=0 || port<=0) return RERR_PARAM;
	timeout = cf_atoi (cf_getval2 ("getdoc_timeout", "90"));
	switch (prot) {
	case PROT_HTTPS:
		cflags |= CONN_FLAG_SSL;
		break;
	case PROT_HTTP:
		cflags |= CONN_FLAG_TWM;
		break;
	default:
		return RERR_INVALID_PROT;
	}
	fd = conn_open_client (host, port, cflags, timeout);
	if (fd<0) {
		ret = fd;
		FRLOGF (LOG_ERR, "error connecting to %s:%d -> %s", 
									host, (int)port, rerr_getstr3(ret));
		return ret;
	}
	switch (prot) {
	case PROT_HTTP:
	case PROT_HTTPS:
		ret = getdoc_via_http (obuf, fd, url, user, pass, flags, &redirect);
		break;
	default:
		ret = RERR_INVALID_PROT;
	}
	conn_close (fd);

	if (ret == RERR_REDIRECT) {
		if (!redirect) return RERR_INTERNAL;
		if (++num_redirect > 5) {
			FRLOGF (LOG_ERR, "to many redirects (>5)");
			free (redirect);
			return RERR_LOOP;
		}
		ret = url_parse (redirect, &prot, &host2, &port, NULL, NULL,
							PURL_FLAG_DEFAULT);
		if (!RERR_ISOK(ret)) {
			free (redirect);
			FRLOGF (LOG_ERR, "error parsing redirect url "
									">>%s<<: %s", redirect, rerr_getstr3(ret));
			return ret;
		}
		switch (prot) {
		case PROT_HTTP:
			if (port<=0) port=80;
			break;
		case PROT_HTTPS:
			if (port<=0) port=443;
			break;
		default:
			free (host2);
			free (redirect);
			return RERR_INVALID_PROT;
		}
		ret = do_getdoc (obuf, redirect, prot, host2, port, user, pass, 
							flags, num_redirect);
		free (host2);
		free (redirect);
	}

	return ret;
}



static
int
getdoc_via_http (obuf, fd, url, user, pass, flags, redirect)
	char			**obuf, **redirect;
	const char	*url, *user, *pass; 
	int			fd, flags;
{
	int	len, clen, err;
	char	*cmd;
	char	*upass;
	/* char	*ctype; */
	char	*line, *cenc, *s, *buf;
	int	ret;

	if (!obuf || fd<0 || !url | !*url) return RERR_PARAM;
	if (!pass) pass="";
	if (user && !*user) user=NULL;

	/* create command to send */
	len = 64;
	len += strlen (url);
	if (user) len += (strlen (user) + strlen (pass)) * 2 + 32;
	cmd = malloc (len);
	if (!cmd) return RERR_NOMEM;
	sprintf (cmd, "GET %s HTTP/1.0\r\n", url);
	if (user) {
		upass = malloc (strlen (user) + strlen(pass) + 4);
		if (!upass) {
			free (cmd);
			return RERR_NOMEM;
		}
		sprintf (upass, "%s:%s", user, pass);
		strcat (cmd, "Authorization: basic ");
		top_base64encode_short (upass, strlen (upass), cmd+strlen (cmd));
		free (upass);
		strcat (cmd, "\r\n");
	}
	strcat (cmd, "\r\n");

	ret = conn_sendnum (fd, cmd, strlen (cmd));
	free (cmd);
	if (!RERR_ISOK(ret)) return ret;

	/* receive answer - first line */
	ret = conn_recvln (fd, &line, 30);
	if (!RERR_ISOK(ret)) return ret;
	if (strncasecmp (line, "http/", 5)) {
		/* http/0.9 - receive till end of transfer */
		*obuf=line;
		while (RERR_ISOK (ret=conn_recvln (fd, &line, 30))) {
			*obuf = realloc (*obuf, strlen (*obuf) + strlen (line) + 1);
			if (!*obuf) {
				free (line);
				return RERR_NOMEM;
			}
			strcat (*obuf, line);
			free (line);
		}
		return RERR_OK;
	}

	/* check error code */
	for (s=line; *s && !isspace (*s); s++);
	for (; *s && isspace (*s); s++);
	for (err=0; *s && isdigit (*s); s++) {
		err*=10; err+=*s-'0';
	}
	if (err/100 != 2) {
		FRLOGF (LOG_INFO, "received >>%s<<", line);
	}
	free (line);
	if (err==400) return RERR_INVALID_URL;
	if (err==401) return RERR_AUTH;
	if (err==403) return RERR_FORBIDDEN;
	if (err==404 || err==204) return RERR_NOT_FOUND;
	if (err/100==3) {
		/* redirected */
		while (RERR_ISOK(ret=conn_recvln (fd, &line, 30))) {
			if (!strncasecmp (line, "Location:", 9)) {
				for (s=line+9; *s && isspace (*s); s++);
				if (redirect) {
					*redirect = strdup (s);
					if (!*redirect) {
						free (line); return RERR_NOMEM;
					}
				}
				free (line);
				return RERR_REDIRECT;
			}
			free (line);
		}
		return RERR_SERVER;
	}
	if (err/100 != 2) return RERR_SERVER;

	/* receive header */
	cenc=NULL;
	clen=-1;
	while (RERR_ISOK(ret = conn_recvln (fd, &line, 30))) {
		if (!*line || (line[0]=='\r' && line[1]=='\n') || *line=='\n') {
			free (line);
			break;
		}
		sswitch (line) {
		sincase ("Content-Type:")
			/* ctype=top_skipwhite (line+13); */
			break;
		sincase ("Content-Length:")
			s = top_skipwhite (line+15);
			clen = cf_atoi (s);
			break;
		sincase ("Content-Encoding:")
			cenc = top_skipwhite (line+17);
			if (cenc && *cenc) {
				cenc = strdup (cenc);
				if (!cenc) {
					free (line);
					return RERR_NOMEM;
				}
			}
			break;
		} sesac;
		free (line);
	}
	if (!RERR_ISOK(ret)) {
		if (cenc) free (cenc);
		return ret;
	}
	if (clen==0) {
		if (cenc) free (cenc);
		if (flags & GETDOC_FLAG_ACCEPT_EMPTY_BODY) {
			*obuf=strdup ("");
			if (!*obuf) return RERR_NOMEM;
			return RERR_OK;
		} else {
			return RERR_EMPTY_BODY;
		}
	}
	if (clen>0) {
		buf = malloc (clen);
		if (!buf) {
			if (cenc) free (cenc);
			return RERR_NOMEM;
		}
		ret = conn_recvnum (fd, buf, clen, 30);
		if (!RERR_ISOK(ret)) {
			if (cenc) free (cenc);
			return ret;
		}
	} else {
		/* receive till end of transfer */
		buf=NULL;
		len=0;
		buf = strdup ("");
		if (!buf) {
			if (cenc) free (cenc);
			return RERR_NOMEM;
		}
		while (RERR_ISOK(ret=conn_recvln (fd, &line, 30))) {
			len+=strlen (line)+1;
			buf = realloc (buf, len + 1);
			if (!buf) {
				if (cenc) free (cenc);
				free (line);
				return RERR_NOMEM;
			}
			strcat (buf, line);
			strcat (buf, "\n");
			free (line);
		}
	}

	/* decode body */
	if (!cenc) {
		*obuf = buf;
		return RERR_OK;
	}
	sswitch (cenc) {
	sincase ("x-gzip")
	sincase ("gzip")
		ret = gunzip (buf, clen, obuf, &len);
		if (!RERR_ISOK(ret)) {
			free (cenc);
			return ret;
		}
		break;
	sincase ("x-compress")
	sincase ("compress")
		ret = uncompress (buf, clen, obuf, &len);
		if (!RERR_ISOK(ret)) {
			free (cenc);
			return ret;
		}
		break;
	sincase ("base64")
		ret = top_base64decode (buf, obuf, &len);
		if (!RERR_ISOK(ret)) {
			free (cenc);
			return ret;
		}
		break;
	sdefault
		*obuf=buf;
		ret = RERR_OK;
		break;
	} sesac;
	free (cenc);
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
