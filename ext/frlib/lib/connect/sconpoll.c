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
#include <unistd.h>


#include "sconpoll.h"
#include "serverconn.h"

#include <fr/base/errors.h>
#include <fr/base/slog.h>
#include <fr/connect/connection.h>
#include <fr/base/tmo.h>


static struct scon	scon;
static int				isinit = 0;




int
sconpoll_init (host, port, flags)
	char	*host;
	int	port, flags;
{
	int	fd, ret;

	if (isinit) return 1;
	ret = SCON_NEW (scon);
	if (!RERR_ISOK(ret)) return ret;
	fd = conn_open_server (host, port, flags | CONN_F_NOBLK, 50);
	if (fd < 0) {
		ret = fd;
		FRLOGF (LOG_ERR, "error opening server >>%s<<: %s",
								host?host:"", rerr_getstr3(ret));
		return ret;
	}
	FRLOGF (LOG_DEBUG, "connection >>%s<< opened at %d", host?host:"", fd);
	ret = SCON_ADD (scon, fd, scon_termnl, NULL, FD_T_SERVER);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding connection to list: %s",
								rerr_getstr3 (ret));
		conn_close (fd);
		return ret;
	}
	isinit = 1;
	return RERR_OK;
}


int
sconpoll (func, flags, timeout)
	tmo_t	timeout;
	int	(*func) (char**, char*, int);
	int	flags;
{
	int					ret;
	struct scondata	scondata;
	char					*answer;
	const char			*answer2;
	int					needfree;
	static tmo_t		toexit=0;
	tmo_t					now;

	if (!isinit) return RERR_NOTINIT;
	if (timeout < 0) return RERR_PARAM;
	FRLOGF (LOG_VVERB, "do communication poll (timeout=%" PRtmo ")", timeout);
	if (toexit > 0) {
		now = tmo_now ();
		if (now + timeout > toexit) timeout = toexit - now;
		if (timeout < 0) timeout = 10;
	}
	ret = SCON_WAIT (scon, timeout);
	if (!RERR_ISOK(ret)) return ret;
	if (toexit > 0) {
		now = tmo_now();
		if (now >= toexit) return RERR_EOT;
	}
	while (RERR_ISOK(ret = SCON_RECV (scondata, scon))) {
		answer = NULL;
		needfree = 0;
		FRLOGF (LOG_DEBUG, "received command >>%s<<", 
										scondata.data?scondata.data:"<NULL>");
		ret = func (&answer, scondata.data, flags);
		if (RERR_ISOK(ret)) {
			if (!answer) {
				answer2="OK\n";
			} else {
				answer2 = answer;
				needfree = 1;
			}
		} else if (ret == RERR_EOT) {
			toexit = tmo_now() + 1300000;
			if (!answer) {
				answer2 = "OK\n";
			} else {
				answer2 = answer;
				needfree = 1;
			}
		} else {
			answer2 = "ERR -1\n";
		}
		ret = SCON_SEND (scon, scondata.fd, answer2, strlen (answer2));
		if (needfree) free (answer);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error sending answer: %s", rerr_getstr3 (ret));
		}
		if (toexit > 0) {
			SCON_WAIT (scon, 10);
		}
	}
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


int
sconpoll_free ()
{
	if (!isinit) return RERR_OK;
	return SCON_FREE (scon);
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
