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

#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <fr/base.h>
#include <poll.h>

#include "fdready.h"


static int read_config();
static int config_read=0;

static tmo_t	gtimeout = -1;

int
fd_isready (fd, timeout)
	int	fd;
	tmo_t	timeout;
{
	struct pollfd		pfd;
	struct timespec	tspec;
	tmo_t					now, start;
	int					ret;

	if (fd < 0) return RERR_PARAM;
	CF_MAYREAD;
	if (timeout == -2) timeout = gtimeout;
	if (timeout == 0) timeout=1;
	bzero (&pfd, sizeof (struct pollfd));
	if (timeout >= 0) {
		start = tmo_now ();
		tspec.tv_sec = timeout / 1000000LL;
		tspec.tv_nsec = (timeout % 1000000LL) * 1000LL;
	}
retry:
	pfd.fd = fd;
	pfd.events = POLLIN | POLLRDHUP;
	ret = ppoll (&pfd, 1, timeout >= 0 ? &tspec : NULL, NULL);
	if (ret < 0) {
		if (errno == EINTR) {
			if (timeout >= 0) {
				now = tmo_now ();
				if (now - start > timeout) return RERR_TIMEDOUT;
				timeout -= now - start;
				if (timeout == 0) timeout=1;
				start = now;
				tspec.tv_sec = timeout / 1000000LL;
				tspec.tv_nsec = (timeout % 1000000LL) * 1000LL;
			}
			goto retry;
		} else {
			FRLOGF (LOG_ERR, "error in poll: %s", rerr_getstr3 (RERR_SYSTEM));
			return RERR_SYSTEM;
		}
	}
	if (pfd.revents & POLLIN) return RERR_OK;
	if (pfd.revents & POLLHUP || pfd.revents & POLLRDHUP) {
		FRLOGF (LOG_INFO, "connection closed by partner");
		return RERR_CONNECTION;
	}
	if (pfd.revents & POLLERR) {
		FRLOGF (LOG_NOTICE, "error condition on fd %d", fd);
	}
	if (timeout > 0) {
		now = tmo_now ();
		if (now - start > timeout) return RERR_TIMEDOUT;
	}
	goto retry;
}

#if 0
int
fd_isready2 (fd, timeout, who)
	int			fd;
	tmo_t			timeout;
	const char	*who;
{
	int	ret;

	ret = fd_isready (fd, timeout);
	if (ret == RERR_TIMEDOUT && who) {
		FRLOGF (LOG_ERR, "%s: connection timed out", who);
	}
	return ret;
}
#endif

int
fd_waitready (rdy, maxfd, timeout)
	fd_set	* rdy;
	int		maxfd;
	tmo_t		timeout;
{
	struct timeval	to;

	CF_MAY_READ;
	if (timeout==-2) timeout = gtimeout;

	to.tv_sec = timeout/1000000;
	to.tv_usec = timeout%1000000;
//#ifdef Linux
#if 1
select_restart:
	if (select(maxfd + 1, rdy, NULL, NULL, (timeout>=0)?&to:NULL) < 0) {
		if (errno == EINTR) { // 4 == EINTR
			// some signal interrupted us, so restart
			goto select_restart;
		}
#else
	if (select(maxfd + 1, rdy, 0, 0, &to) < 0) {
#endif
		FRLOGF (LOG_ERR, "error on select: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (timeout>0 && to.tv_sec == 0 && to.tv_usec == 0) return RERR_TIMEDOUT;

	return RERR_OK;
}







static
int
read_config ()
{
	static char	strtout[2];
	cf_begin_read ();
	strcpy (strtout, "5");
	gtimeout = tmo_gettimestr64 (cf_getval2 ("connection_timeout", strtout));
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
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
