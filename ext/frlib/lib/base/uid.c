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
 * Portions created by the Initial Developer are Copyright (C) 2003-2015
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <pwd.h>
#include <unistd.h>
#ifdef SunOS
extern int errno;
#endif
#include <errno.h>
#include <ctype.h>

#include "textop.h"
#include "errors.h"
#include "uid.h"


int
fpw_getpwnam (res, buf, blen, name)
	struct passwd	**res;
	char				*buf;
	size_t			blen;
	const char		*name;
{
	char				*xbuf = NULL, *p;
	struct passwd	*myres;
	int				ret;
	const char		*s;
	uid_t				uid;

	if (!res) return RERR_PARAM;
	if (!name || !*name) return fpw_getpwuid (res, buf, blen, getuid ());
	if (!buf || blen < sizeof (struct passwd)+32) {
		blen = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (blen <= 0) blen = 1024;
		blen += sizeof (struct passwd);
		buf = xbuf = malloc (blen);
		if (!xbuf) return RERR_PARAM;
	}
	while (1) {
		ret = getpwnam_r (name, (struct passwd*)buf, buf + sizeof (struct passwd),
								blen - sizeof (struct passwd), &myres);
		if (ret != ERANGE) break;
		blen += 1024;
		p = realloc (xbuf, blen);
		if (!p) {
			if (xbuf) free (xbuf);
			return RERR_NOMEM;
		}
		buf = xbuf = p;
	}
	if (ret != 0) {
		if (xbuf) free (xbuf);
		errno = ret;
		return RERR_SYSTEM;
	}
	if (!myres) {
		for (s=name, uid=0; s && isdigit (*s); s++) uid = uid * 10 + *s - '0';
		if (s && *s) {
			if (xbuf) free (xbuf);
			return RERR_NOT_FOUND;
		}
		ret = fpw_getpwuid (res, buf, blen, uid);
		if (xbuf && (!RERR_ISOK(ret) || (char*)*res != xbuf)) free (xbuf);
		return ret;
	}
	*res = (struct passwd*) buf;
	return RERR_OK;
}


int
fpw_getpwuid (res, buf, blen, uid)
	struct passwd	**res;
	char				*buf;
	size_t			blen;
	uid_t				uid;
{
	char				*xbuf = NULL, *p;
	struct passwd	*myres;
	int				ret;

	if (!res) return RERR_PARAM;
	if (!buf || blen < sizeof (struct passwd)+32) {
		blen = sysconf(_SC_GETPW_R_SIZE_MAX);
		if (blen <= 0) blen = 1024;
		blen += sizeof (struct passwd);
		buf = xbuf = malloc (blen);
		if (!xbuf) return RERR_PARAM;
	}
	while (1) {
		ret = getpwuid_r (uid, (struct passwd*)buf, buf + sizeof (struct passwd),
								blen - sizeof (struct passwd), &myres);
		if (ret != ERANGE) break;
		blen += 1024;
		p = realloc (xbuf, blen);
		if (!p) {
			if (xbuf) free (xbuf);
			return RERR_NOMEM;
		}
		buf = xbuf = p;
	}
	if (ret != 0) {
		if (xbuf) free (xbuf);
		errno = ret;
		return RERR_SYSTEM;
	}
	if (!myres) {
		if (xbuf) free (xbuf);
		return RERR_NOT_FOUND;
	}
	*res = (struct passwd*) buf;
	return RERR_OK;
}


/* ************** *
 * users home dir *
 * ************** */

static int fpw_gethomebyname1024 (char**, char*, size_t, const char*);
int
fpw_gethomebyname (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_gethomebyname1024 (res, buf, blen, name);
	ret = fpw_getpwnam (&pwd, buf, blen, name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	/* in case no dir is given - take / as home dir */
	if (!pwd->pw_dir || !*pwd->pw_dir) {
		strcpy (*res, "/");
		len = 2;
	} else {
		len = strlen (pwd->pw_dir) + 1;
		memmove (*res, pwd->pw_dir, len);
	}
	if (*res != buf) {
		s = realloc (*res, len);
		if (s) *res = s;
	}
	return RERR_OK;
}

static
int
fpw_gethomebyname1024 (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	const char		*hd;
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwnam (&pwd, xbuf, sizeof(xbuf), name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	/* in case no dir is given - take / as home dir */
	if (!pwd->pw_dir || !*pwd->pw_dir) {
		hd = "/";
	} else {
		hd = pwd->pw_dir;
	}
	*res = top_strcpdup (buf, blen, hd);
	if (!*res) return RERR_NOMEM;
	return RERR_OK;
}


static int fpw_gethomebyuid1024 (char**, char*, size_t, uid_t);
int
fpw_gethomebyuid (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_gethomebyuid1024 (res, buf, blen, uid);
	ret = fpw_getpwuid (&pwd, buf, blen, uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	/* in case no dir is given - take / as home dir */
	if (!pwd->pw_dir || !*pwd->pw_dir) {
		strcpy (*res, "/");
		len = 2;
	} else {
		len = strlen (pwd->pw_dir) + 1;
		memmove (*res, pwd->pw_dir, len);
	}
	if (*res != buf) {
		s = realloc (*res, len);
		if (s) *res = s;
	}
	return RERR_OK;
}

static
int
fpw_gethomebyuid1024 (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	const char		*hd;
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwuid (&pwd, xbuf, sizeof(xbuf), uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	/* in case no dir is given - take / as home dir */
	if (!pwd->pw_dir || !*pwd->pw_dir) {
		hd = "/";
	} else {
		hd = pwd->pw_dir;
	}
	*res = top_strcpdup (buf, blen, hd);
	if (!*res) return RERR_NOMEM;
	return RERR_OK;
}


int
fpw_getmyhome (res, buf, blen)
	char		**res, *buf;
	size_t	blen;
{
	char		*home;
	size_t	len;

	if (!res) return RERR_PARAM;
	home = getenv ("HOME");
	if (!home || !*home) return fpw_gethomebyuid (res, buf, blen, getuid ());
	len = strlen (home) + 1;
	if (len > blen || !buf) {
		buf = strdup (home);
		if (!buf) return RERR_NOMEM;
	} else {
		strcpy (buf, home);
	}
	*res = buf;
	return RERR_OK;
}


int
fpw_gethometilde (res, buf, blen, fn)
	char			**res, *buf;
	size_t		blen;
	const char	*fn;
{
	const char	*s;
	char			*nbuf, _nbuf[256];
	size_t		len;
	int			ret;

	if (!res || !fn) return RERR_PARAM;
	if (*fn == '~') fn++;
	for (s=fn; *s && *s != '/'; s++);
	len = s-fn;
	if (!len) return fpw_getmyhome (res, buf, blen);
	if (!*s) {
		nbuf = (char*)fn;	/* is ok here */
	} else {
		nbuf = top_strcpdup2 (_nbuf, sizeof (_nbuf), fn, len);
		if (!nbuf) return RERR_NOMEM;
	}
	ret = fpw_gethomebyname (res, buf, blen, nbuf);
	if (nbuf != fn && nbuf != _nbuf) free (nbuf);
	return ret;
}



/* *********** *
 * users shell *
 * *********** */


static int fpw_getshellbyname1024 (char**, char*, size_t, const char*);
int
fpw_getshellbyname (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_getshellbyname1024 (res, buf, blen, name);
	ret = fpw_getpwnam (&pwd, buf, blen, name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	if (!pwd->pw_shell || !*pwd->pw_shell) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		len = strlen (pwd->pw_shell) + 1;
		memmove (*res, pwd->pw_shell, len);
		if (*res != buf) {
			s = realloc (*res, len);
			if (s) *res = s;
		}
	}
	return RERR_OK;
}

static
int
fpw_getshellbyname1024 (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwnam (&pwd, xbuf, sizeof(xbuf), name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	if (!pwd->pw_shell || !*pwd->pw_shell) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		*res = top_strcpdup (buf, blen, pwd->pw_shell);
		if (!*res) return RERR_NOMEM;
	}
	return RERR_OK;
}


static int fpw_getshellbyuid1024 (char**, char*, size_t, uid_t);
int
fpw_getshellbyuid (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_getshellbyuid1024 (res, buf, blen, uid);
	ret = fpw_getpwuid (&pwd, buf, blen, uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	if (!pwd->pw_shell || !*pwd->pw_shell) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		len = strlen (pwd->pw_shell) + 1;
		memmove (*res, pwd->pw_shell, len);
		if (*res != buf) {
			s = realloc (*res, len);
			if (s) *res = s;
		}
	}
	return RERR_OK;
}

static
int
fpw_getshellbyuid1024 (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwuid (&pwd, xbuf, sizeof(xbuf), uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	if (!pwd->pw_shell || !*pwd->pw_shell) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		*res = top_strcpdup (buf, blen, pwd->pw_shell);
		if (!*res) return RERR_NOMEM;
	}
	return RERR_OK;
}


int
fpw_getmyshell (res, buf, blen)
	char		**res, *buf;
	size_t	blen;
{
	char		*shell;
	size_t	len;
	int		ret;

	if (!res) return RERR_PARAM;
	shell = getenv ("SHELL");
	if (!shell || !*shell) {
		ret = fpw_getshellbyuid (res, buf, blen, getuid ());
		if (RERR_ISOK(ret) && !res) {
			shell = (char*)"/bin/sh";
		} else {
			return ret;
		}
	}
	len = strlen (shell) + 1;
	if (len > blen || !buf) {
		buf = strdup (shell);
		if (!buf) return RERR_NOMEM;
	} else {
		strcpy (buf, shell);
	}
	*res = buf;
	return RERR_OK;
}


/* **************** *
 * user information *
 * **************** */


static int fpw_getuinfobyname1024 (char**, char*, size_t, const char*);
int
fpw_getuinfobyname (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_getuinfobyname1024 (res, buf, blen, name);
	ret = fpw_getpwnam (&pwd, buf, blen, name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	if (!pwd->pw_gecos || !*pwd->pw_gecos) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		len = strlen (pwd->pw_gecos) + 1;
		memmove (*res, pwd->pw_gecos, len);
		if (*res != buf) {
			s = realloc (*res, len);
			if (s) *res = s;
		}
	}
	return RERR_OK;
}

static
int
fpw_getuinfobyname1024 (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwnam (&pwd, xbuf, sizeof(xbuf), name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	if (!pwd->pw_gecos || !*pwd->pw_gecos) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		*res = top_strcpdup (buf, blen, pwd->pw_gecos);
		if (!*res) return RERR_NOMEM;
	}
	return RERR_OK;
}


static int fpw_getuinfobyuid1024 (char**, char*, size_t, uid_t);
int
fpw_getuinfobyuid (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_getuinfobyuid1024 (res, buf, blen, uid);
	ret = fpw_getpwuid (&pwd, buf, blen, uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	if (!pwd->pw_gecos || !*pwd->pw_gecos) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		len = strlen (pwd->pw_gecos) + 1;
		memmove (*res, pwd->pw_gecos, len);
		if (*res != buf) {
			s = realloc (*res, len);
			if (s) *res = s;
		}
	}
	return RERR_OK;
}

static
int
fpw_getuinfobyuid1024 (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwuid (&pwd, xbuf, sizeof(xbuf), uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	if (!pwd->pw_gecos || !*pwd->pw_gecos) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		*res = top_strcpdup (buf, blen, pwd->pw_gecos);
		if (!*res) return RERR_NOMEM;
	}
	return RERR_OK;
}


int
fpw_getrealnamebyname (res, buf, blen, name)
	char			**res, *buf;
	size_t		blen;
	const char	*name;
{
	char		*s;
	int		ret;
	size_t	len;

	if (!res) return RERR_PARAM;
	ret = fpw_getuinfobyname (res, buf, blen, name);
	if (!RERR_ISOK(ret)) return ret;
	if (!*res) return RERR_OK;
	s = index (*res, ',');
	if (!s) return RERR_OK;
	*s = 0;
	if (*res != buf) {
		len = s - *res + 1;
		s = realloc (*res, len);
		if (s) *res = s;
	}
	return RERR_OK;
}


int
fpw_getrealnamebyuid (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	char		*s;
	int		ret;
	size_t	len;

	if (!res) return RERR_PARAM;
	ret = fpw_getuinfobyuid (res, buf, blen, uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!*res) return RERR_OK;
	s = index (*res, ',');
	if (!s) return RERR_OK;
	*s = 0;
	if (*res != buf) {
		len = s - *res + 1;
		s = realloc (*res, len);
		if (s) *res = s;
	}
	return RERR_OK;
}


/* ************************ *
 * user name <-> id mapping *
 * ************************ */


static int fpw_getnamebyuid1024 (char**, char*, size_t, uid_t);
int
fpw_getnamebyuid (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	int				ret;
	size_t			len;
	char				*s;

	if (!res) return RERR_PARAM;
	if (!buf || blen < 1024) return fpw_getnamebyuid1024 (res, buf, blen, uid);
	ret = fpw_getpwuid (&pwd, buf, blen, uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = (char*)pwd;
	if (!pwd->pw_name || !*pwd->pw_name) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		len = strlen (pwd->pw_name) + 1;
		memmove (*res, pwd->pw_name, len);
		if (*res != buf) {
			s = realloc (*res, len);
			if (s) *res = s;
		}
	}
	return RERR_OK;
}

static
int
fpw_getnamebyuid1024 (res, buf, blen, uid)
	char		**res, *buf;
	size_t	blen;
	uid_t		uid;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwuid (&pwd, xbuf, sizeof(xbuf), uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	if (!pwd->pw_name || !*pwd->pw_name) {
		*res = NULL;
		if ((char*)pwd != buf) free (pwd);
	} else {
		*res = top_strcpdup (buf, blen, pwd->pw_name);
		if (!*res) return RERR_NOMEM;
	}
	return RERR_OK;
}


int
fpw_getuidbyname (res, name)
	uid_t			*res;
	const char	*name;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwnam (&pwd, xbuf, sizeof(xbuf), name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = pwd->pw_uid;
	return RERR_OK;
}

int
fpw_getuidbyname2 (name)
	const char	*name;
{
	uid_t	uid;
	int	ret;

	ret = fpw_getuidbyname (&uid, name);
	if (!RERR_ISOK(ret)) return ret;
	return (int) uid;
}



/* ******** *
 * group id *
 * ******** */


int
fpw_getgidbyname (res, name)
	gid_t			*res;
	const char	*name;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwnam (&pwd, xbuf, sizeof(xbuf), name);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = pwd->pw_gid;
	return RERR_OK;
}


int
fpw_getgidbyname2 (name)
	const char	*name;
{
	gid_t	gid;
	int	ret;

	ret = fpw_getgidbyname (&gid, name);
	if (!RERR_ISOK(ret)) return ret;
	return (int) gid;
}

int
fpw_getgidbyuid (res, uid)
	gid_t	*res;
	uid_t	uid;
{
	struct passwd	*pwd;
	char				xbuf[1024];
	int				ret;

	if (!res) return RERR_PARAM;
	ret = fpw_getpwuid (&pwd, xbuf, sizeof(xbuf), uid);
	if (!RERR_ISOK(ret)) return ret;
	if (!pwd) return RERR_INTERNAL;
	*res = pwd->pw_gid;
	return RERR_OK;
}


int
fpw_getgidbyuid2 (uid)
	uid_t	uid;
{
	gid_t	gid;
	int	ret;

	ret = fpw_getgidbyuid (&gid, uid);
	if (!RERR_ISOK(ret)) return ret;
	return (int) gid;
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
