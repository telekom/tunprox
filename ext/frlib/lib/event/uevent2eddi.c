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
#include <dirent.h>
#include <assert.h>


#include <fr/base.h>
#include "parseevent.h"
#include "uevent2eddi.h"


//#define UDEV_MAGIC 0xcafe1dea
struct udev_header {
	/* udev version text */
	char version[16];
	/*
 	* magic to protect against daemon <-> library message format mismatch
 	* used in the kernel from socket filter rules; needs to be stored in network order
 	*/
	unsigned int magic;
	/* properties buffer */
	unsigned short properties_off;
	unsigned short properties_len;
	/*
 	* hashes of some common device properties strings to filter with socket filters in
 	* the client used in the kernel from socket filter rules; needs to be stored in
 	* network order
 	*/
	unsigned int filter_subsystem;
	unsigned int filter_devtype;
};

static int ueventparse (struct event*, char*, int);
static int myscandir (char*, struct event*, DIR*);
static int getdir (DIR**, char**, struct event*);
static int add_sysattr (struct event*);





int
uevent2eddi (ev, uevent, ueventlen, flags)
	struct event	*ev;
	const char		*uevent;
	int				ueventlen;
	int				flags;
{
	int	ret;
	char	*buf;

	if (!uevent || !ev) return RERR_PARAM;
	buf = malloc (ueventlen+1);
	if (!buf) return RERR_NOMEM;
	memcpy (buf, uevent, ueventlen);
	buf[ueventlen] = 0;
	ret = ueventparse (ev, buf, ueventlen);
	if (!RERR_ISOK(ret)) {
		free (buf);
		FRLOGF (LOG_WARN, "error parsing uevent: %s", rerr_getstr3(ret));
		return ret;
	}
	if (flags & UEVENT2EDDI_F_ADDSYSATTR) {
		ret = add_sysattr (ev);
		if (!RERR_ISOK(ret) && ret != RERR_INVALID_FORMAT) {
			FRLOGF (LOG_WARN, "cannot add attributes to event: %s",
								rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}

static
int
ueventparse (ev, buf, buflen)
	struct event	*ev;
	char				*buf;
	int				buflen;
{
	int						ret, bufpos;
	char						*s1, *s2, *s3;
	struct udev_header	*udh;

	if (!ev || !buf) return RERR_PARAM;
	if (!strncmp(buf, "libudev", 7)) {
		udh = (struct udev_header *) buf;
		bufpos = udh->properties_off;
		ret = ev_setname (ev, "udev/uevent", 0);
	} else {
		bufpos = strlen (buf);
		ret = ev_setname (ev, "kernel/uevent", 0);
	}
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_addbuf2 (ev, buf, buflen);
	if (!RERR_ISOK(ret)) return ret;
	while (bufpos < buflen) {
		s1 = buf+bufpos;
		bufpos += strlen (s1)+1;
		s2 = index (s1, '=');
		if (!s2) continue;
		*s2 = 0;
		s1 = top_stripwhite (s1, 0);
		s2 = top_skipwhiteplus (s2+1, "=");
		if (*s2 == '"') {
			s2++;
			s3 = s2 + strlen (s2) - 1;
			if (*s3 == '"') *s3=0;
		}
		ret = ev_addattr_s (ev, s1, s2, 0);
		if (!RERR_ISOK(ret)) {
			ev_rmbuf (ev, buf);
			return ret;
		}
	}
	return RERR_OK;
}

static
int
add_sysattr (ev)
	struct event	*ev;
{
	DIR			*d;
	char			*path;
	const char	*action;
	int			ret;

	if (!ev) return RERR_PARAM;
	ret = ev_getattr_s (&action, ev, "ACTION");
	if (!RERR_ISOK(ret) && ret != RERR_NOT_FOUND) return ret;
	if (ret == RERR_NOT_FOUND || strcasecmp (action, "add") != 0) return RERR_OK;
	ret = getdir (&d, &path, ev);
	if (!RERR_ISOK(ret)) return ret;
	ret = myscandir (path, ev, d);
	closedir (d);
	free (path);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding attributes: %s", 
								rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}

static
int
myscandir (dname, ev, d)
	char				*dname;
	struct event	*ev;
	DIR				*d;
{
	struct dirent 	*dent;
	int				mode, ret, buflen;
	char				*buf, *s;
	char				fname[4096];
	char				*varname;

	if (!dname || !ev || !d) return RERR_PARAM;
	while ((dent = readdir (d))) {
		if (*dent->d_name == '.') continue;
		if (!strcmp (dent->d_name, "uevent")) continue;
		if (!strncasecmp (dent->d_name, "stat", 4)) continue;
		snprintf (fname, sizeof (fname), "%s/%s", dname, dent->d_name);
		fname[sizeof(fname)-1]=0;
		mode = fop_getfilemode (NULL, fname);
		if (!RERR_ISOK(mode)) return mode;
		if (!S_ISREG (mode)) continue;
		buf = fop_read_fn2 (fname, &buflen);
		if (!buf) continue;
		s = buf + buflen - 1;
		if (s>=buf && *s=='\n') *s=0;
		if ((ssize_t)strlen (buf) < buflen-1) {
			/* ignore binary data */
			free (buf);
			continue;
		} else if (index (buf, '\n')) {
			/* ignore multiline data */
			free (buf);
			continue;
		}
		ret = ev_addbuf2 (ev, buf, buflen);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		varname = malloc (strlen (dent->d_name) + 8);
		if (!varname) {
			free (buf);
			return RERR_NOMEM;
		}
		sprintf (varname, "ATTR/%s", dent->d_name);
		ret = ev_addbuf (ev, varname);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_addattr_s (ev, varname, buf, 0);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

static
int
getdir (dir, dname, ev)
	struct event	*ev;
	DIR				**dir;
	char				**dname;
{
	const char	*path;
	char			*fpath;
	DIR			*d;
	int			ret;

	ret = ev_getattr_s (&path, ev, "devpath");
	if (ret == RERR_NOT_FOUND) return RERR_INVALID_FORMAT;
	if (!RERR_ISOK(ret)) return ret;
	fpath = malloc (strlen (path) + 8);
	if (!fpath) return RERR_NOMEM;
	sprintf (fpath, "/sys/%s", path);
	d = opendir (fpath);
	if (!d) {
		free (fpath);
		return RERR_INVALID_FORMAT;
	}
	if (dir) {
		*dir = d;
	} else {
		closedir (d);
	}
	if (dname) {
		*dname = fpath;
	} else {
		free (fpath);
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
