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
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <sys/stat.h>
#include <dirent.h>
#include <signal.h>


#include "errors.h"
#include "textop.h"
#include "fileop.h"
#include "slog.h"
#include "procop.h"


int
pop_parentof (ppid, pid)
	pid_t	*ppid, pid;
{
	char	fname[32];
	char	*buf, *s;
	char	c;
	int	num;

	if (!ppid) return RERR_PARAM;
	snprintf (fname, sizeof(fname)-1, "/proc/%d/stat", (int)pid);
	fname[sizeof(fname)-1]=0;
	buf = fop_read_fn (fname);
	if (!buf) return (errno==EACCES)?RERR_FORBIDDEN:RERR_SYSTEM;
	s = index (buf, ')');
	if (!s) return RERR_NOT_FOUND;
	s = top_skipwhite (s+1);
	num = sscanf (s, "%c %d", &c, ppid);
	if (num != 2) return RERR_NOT_FOUND;
	return RERR_OK;
}


int
pop_numchild (numchild, pid)
	int	*numchild;
	pid_t	pid;
{
	DIR				*dd;
	struct dirent	*dirent;
	pid_t				ppid;
	int				ret;

	if (!numchild) return RERR_PARAM;
	*numchild=0;
	dd = opendir ("/proc/");
	if (!dd) {
		FRLOGF (LOG_WARN, "error opening dir %s: %s", "/proc",
						strerror (errno));
		return RERR_SYSTEM;
	}
	while ((dirent=readdir(dd))) {
		if (!strcmp (dirent->d_name, ".") || !strcmp (dirent->d_name, "..")) 
			continue;
		if (!top_isnum (dirent->d_name)) continue;
		ret = pop_parentof (&ppid, atoi (dirent->d_name));
		if (!RERR_ISOK (ret)) {
			if (ret != RERR_FORBIDDEN) break;
			continue;
		}
		if (ppid == pid) (*numchild)++;
	}
	closedir (dd);
	if (ret == RERR_FORBIDDEN) ret = RERR_OK;
	return ret;
}





int
pop_getcmdname (name, size, pid)
	char	*name;
	int	size;
	pid_t	pid;
{
	char			proc[64], *buf;
	int			ret, len, saverr;
	struct stat	sbuf;

	if (!name || size <=0) return RERR_PARAM;
	sprintf (proc, "/proc/%d/cmdline", (int)pid);
	buf = fop_read_fn2 (proc, &len);
	if (!buf) {
		saverr = errno;
		sprintf (proc, "/proc/%d", (int)pid);
		ret = stat (proc, &sbuf);
		if (ret < 0 && errno == ENOENT) {
			ret = stat ("/proc", &sbuf);
			if (ret < 0 || S_ISDIR (sbuf.st_mode)) {
				errno = saverr;
				return RERR_SYSTEM;
			} else {
				return RERR_NOT_FOUND;
			}
		} else {
			errno = saverr;
			return RERR_SYSTEM;
		}
	}
	strncpy (name, buf, size-1);
	name[size-1]=0;
	return RERR_OK;
}



int
pop_killall (sig, prog)
	int			sig;
	const char	*prog;
{
	return pop_killall2 (sig, prog, 0);
}


int
pop_killall2 (sig, prog, flags)
	int			sig, flags;
	const char	*prog;
{
	DIR				*dd;
	struct dirent	*dirent;
	int				ret=RERR_OK, ret2=RERR_OK;
	char				fname[32];
	char				*buf, *s;
	pid_t				pid;
	int				abs;

	if (!prog) return RERR_PARAM;
	abs = index (prog, '/') ? 1 : 0;
	dd = opendir ("/proc/");
	if (!dd) {
		FRLOGF (LOG_WARN, "error opening dir %s: %s", "/proc",
						strerror (errno));
		return RERR_SYSTEM;
	}
	while ((dirent=readdir(dd))) {
		if (!strcmp (dirent->d_name, ".") || !strcmp (dirent->d_name, "..")) 
			continue;
		if (!top_isnum (dirent->d_name)) continue;
		sprintf (fname, "/proc/%s/cmdline", dirent->d_name);
		buf = fop_read_fn (fname);
		if (!buf) {
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "cannot read >>%s<<: %s", fname, rerr_getstr3 (ret));
			ret2 = ret;
			continue;
		}
		if (abs) {
			s = buf;
		} else {
			s = (s = rindex (buf, '/')) ? (s+1) : buf;
		}
		if (flags & POP_KILLALL_F_BEGIN) {
			if (flags & POP_KILLALL_F_ICASE) {
				ret = strncasecmp (s, prog, strlen (prog));
			} else {
				ret = strncmp (s, prog, strlen (prog));
			}
		} else {
			if (flags & POP_KILLALL_F_ICASE) {
				ret = strcasecmp (s, prog);
			} else {
				ret = strcmp (s, prog);
			}
		}
		if (ret != 0) {
			free (buf);
			continue;
		}
		pid = atoi (dirent->d_name);
		ret = kill (pid, sig);
		if (ret < 0) {
			ret = RERR_SYSTEM;
			FRLOGF (LOG_ERR, "cannot kill (%d): %s", (int)pid, rerr_getstr3 (ret));
			ret2 = ret;
			continue;
		}
	}
	closedir (dd);
	if (ret2 < 0) return ret2;
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
