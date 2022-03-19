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

#ifdef Linux
# define _GNU_SOURCE
#endif
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <assert.h>
#include <poll.h>
#if !defined _GNU_SOURCE && !defined POLLRDHUP
# define POLLRDHUP	0
#endif


#include "fdlist.h"
#include "tlst.h"
#include "errors.h"
#include "config.h"
#include "slog.h"
#include "connection.h"
#include "serialconn.h"
#include "textop.h"
#include <fr/event/evtrigfunc.h>
#include <fr/netlink/uevent.h>

#include "serverconn.h"


static int read_config ();
static int config_read = 0;
static int c_debugdata = 0;


static int scon_splitinsert (struct scon*, int, char*, int);
static int scon_addinsert (struct scon*, int, char*, int);
static int scon_doinsert (struct scon*, struct tlst*, int, char*, int);
static int scon_doinsert2 (struct scon*, struct tlst*, int, char*, int, int);
static int scon_dosend (struct scon*, int);
static int scon_doaccept (struct scon*, int);
static int scon_dorecv (struct scon*, int);
static int scon_sendinsert (struct scon*, int, const char*, int, int);
static int scon_sendinsert2 (struct scon*, int, const char*, int, int, int);
static int scon_docleandatalist (struct tlst*);
static int scon_rmfdfromlist (struct tlst*, int);
static int scon_autoclose (struct scon*, int);


int
scon_new (scon)
	struct scon	*scon;
{
	int	ret;

	if (!scon) return RERR_PARAM;
	bzero (scon, sizeof (struct scon));
	ret = FDLIST_NEW (scon->fdlist);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (scon->rcvmsglst, struct scondata);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (scon->rcvfraglst, struct scondata);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (scon->sendlst, struct scondata);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (scon->allfd, struct sconfd);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
scon_free (scon)
	struct scon	*scon;
{
	struct sconfd	sconfd;

	if (!scon) return RERR_PARAM;
	TLST_FOREACH (sconfd, scon->allfd) {
		if (sconfd.fd > 2 && !(sconfd.flags & SCON_F_NOCLOSE)) {
			close (sconfd.fd);
		}
	}
	scon_docleandatalist (&(scon->rcvmsglst));
	scon_docleandatalist (&(scon->rcvfraglst));
	scon_docleandatalist (&(scon->sendlst));
	TLST_FREE (scon->allfd);
	TLST_FREE (scon->sendlst);
	TLST_FREE (scon->rcvmsglst);
	TLST_FREE (scon->rcvfraglst);
	FDLIST_FREE (scon->fdlist);
	bzero (scon, sizeof (struct scon));
	return RERR_OK;
}

int
scon_setmaxrcv (scon, fd, maxrcv)
	struct scon	*scon;
	int			fd;
	size_t		maxrcv;
{
	int				ret;
	struct sconfd	*sconfd;

	if (!scon) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (!sconfd) return RERR_INTERNAL;
	sconfd->maxrcv = maxrcv;
	sconfd->hasmaxrcv = maxrcv > 0 ? 1 : 0;
	return RERR_OK;
}

int
scon_setmaxsnd (scon, fd, maxsnd, fullfunc, arg)
	struct scon	*scon;
	int			fd;
	size_t		maxsnd;
	int			(*fullfunc) (int, void*, int);
	void			*arg;
{
	int				ret;
	struct sconfd	*sconfd;

	if (!scon) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (!sconfd) return RERR_INTERNAL;
	sconfd->maxsnd = maxsnd;
	sconfd->hasmaxsnd = maxsnd > 0 ? 1 : 0;
	sconfd->funcs.m_full = fullfunc;
	sconfd->funcs.m_fullarg = arg;
	return RERR_OK;
}

int
scon_enablercv (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret;
	struct sconfd	*sconfd;

	if (!scon) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (!sconfd) return RERR_INTERNAL;
	sconfd->disablercv = 0;
	return RERR_OK;
}

int
scon_disablercv (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret;
	struct sconfd	*sconfd;

	if (!scon) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (!sconfd) return RERR_INTERNAL;
	sconfd->disablercv = 1;
	return RERR_OK;
}

int
scon_enablesnd (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret;
	struct sconfd	*sconfd;

	if (!scon) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (!sconfd) return RERR_INTERNAL;
	sconfd->disablesnd = 0;
	return RERR_OK;
}

int
scon_disablesnd (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret;
	struct sconfd	*sconfd;

	if (!scon) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (!sconfd) return RERR_INTERNAL;
	sconfd->disablesnd = 1;
	return RERR_OK;
}

int
scon_freemsg (scon)
	struct scon	*scon;
{
	return scon_docleandatalist (&(scon->rcvmsglst));
}


int
scon_add (
	struct scon		*scon,
	int				fd,
	int				(*fullmsg) (char*, int, int*),
	char				*welcomeMsg,
	int				flags,
	...)
{
	struct sconfd		sconfd;
	int					ret;
	scon_accept_t		acceptf;
	void					*acceptarg;
	struct sconfuncs	*funcs;
	va_list				ap;

	if (!scon || fd < 0) return RERR_PARAM;
	bzero (&sconfd, sizeof (struct sconfd));
	sconfd.fd = fd;
	sconfd.ffd = -1;
	sconfd.type = flags & SCON_F_CLIENTCONN ? FD_T_CLIENT : FD_T_SERVER;
	sconfd.fullmsg = fullmsg;
	sconfd.welcomeMsg = welcomeMsg;
	sconfd.flags = flags;
	if (flags & SCON_F_ACCEPT) {
		va_start (ap, flags);
		acceptf = va_arg (ap, scon_accept_t);
		acceptarg = va_arg (ap, void*);
		va_end (ap);
		sconfd.funcs.m_accept = acceptf;
		sconfd.funcs.m_arg = acceptarg;
	} else if (flags & SCON_F_FUNC) {
		va_start (ap, flags);
		funcs = va_arg (ap, struct sconfuncs*);
		va_end (ap);
		sconfd.funcs = *funcs;
	} else if (flags & SCON_F_SERIAL) {
		sconfd.funcs.m_send = &sercon_sendArg;
		sconfd.funcs.m_recv = &sercon_recvArg;
		sconfd.funcs.m_close = &sercon_closeArg;
		sconfd.flags |= SCON_F_CLIENTCONN;
		sconfd.type = FD_T_CLIENT;
		sconfd.isserial = 1;
	} else if (flags & SCON_F_EVTRIG) {
		sconfd.funcs.m_send = evbox_send;
		sconfd.funcs.m_recv = evbox_recv;
		sconfd.flags |= SCON_F_CLIENTCONN | SCON_F_NOCLOSE;
		sconfd.type = FD_T_CLIENT;
		sconfd.fullmsg = &evbox_trigterm;
		sconfd.welcomeMsg = NULL;
	} else if (flags & SCON_F_NETLINK) {
		return RERR_NOT_SUPPORTED;
	} else if (flags & SCON_F_UEVENT) {
		sconfd.funcs.m_send = uevent_sconsend;
		sconfd.funcs.m_recvmsg = uevent_sconrecv;
		sconfd.flags |= SCON_F_CLIENTCONN | SCON_F_NOCLOSE | SCON_F_NOPRINT;
		sconfd.type = FD_T_CLIENT;
		sconfd.fullmsg = NULL;
		sconfd.welcomeMsg = NULL;
		sconfd.nodebugprt = 1;
	}
	if (flags & SCON_F_NOPRINT) {
		sconfd.nodebugprt = 1;
	}
	ret = TLST_ADD (scon->allfd, sconfd);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
scon_getfd (sconfd, scon, fd)
	struct sconfd	**sconfd;
	struct scon		*scon;
	int				fd;
{
	int		ret;
	unsigned	idx;

	if (!sconfd || !scon || fd < 0) return RERR_PARAM;
	ret = TLST_FINDINT (idx, scon->allfd, fd);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (*sconfd, scon->allfd, idx);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


int
scon_gettype (scon, fd)
	struct scon	*scon;
	int			fd;
{
	struct sconfd	*sconfd;
	int				ret;

	if (!scon || fd < 0) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	return sconfd->type;
}

int
scon_wait (scon, timeout)
	struct scon	*scon;
	tmo_t		timeout;
{
	struct sconfd		sconfd;
	struct fdentry		fdentry;
	struct tlst			sendlst;
	struct scondata	*data;
	int					ret, event;
	unsigned				idx;

	if (!scon) return RERR_PARAM;
	//FRLOGF (LOG_VVERB, "waiting %f seconds", ((float)timeout)/1000000.);
	ret = FDLIST_RESET (scon->fdlist);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in fdlist_reset: %s", rerr_getstr3(ret));
		return ret;
	}
	TLST_FOREACH (sconfd, scon->allfd) {
		event = 0;
		if (!(sconfd.disablercv || (sconfd.hasmaxrcv && 
								sconfd.rcvsize >= sconfd.maxrcv))) {
			event = POLLIN | POLLRDHUP | FDLIST_F_ALLFD;
		}
		if (sconfd.type != FD_T_SERVER) {
			ret = TLST_HASINT (scon->sendlst, sconfd.fd);
			if (RERR_ISOK(ret)) {
				//FRLOGF (LOG_VVERB, "adding fd >>%d<< to send-wait list", sconfd.fd);
				event |= POLLOUT;
			} else if (ret != RERR_NOT_FOUND) {
				FRLOGF (LOG_ERR, "error in searching sendlist: %s",
								rerr_getstr3(ret));
				return ret;
			}
		}
		if (event != 0) {
			ret = FDLIST_ADD (scon->fdlist, sconfd.fd, event, sconfd.type, NULL);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error in adding fd to list: %s",
									rerr_getstr3(ret));
				return ret;
			}
		}
	}
	ret = FDLIST_WAITANY (scon->fdlist, timeout);
	if (!RERR_ISOK(ret)) {
		if (!(ret == RERR_SYSTEM && errno==EINTR)) {
			FRLOGF (LOG_ERR, "error in FDLIST_WAITANY(): %s",
									rerr_getstr3(ret));
			return ret;
		} else {
			return RERR_OK;
		}
	}
	FDLIST_FOREACH (fdentry, scon->fdlist) {
		if (fdentry.events & (POLLRDHUP | POLLERR | POLLHUP | POLLNVAL)) {
			/* connection closed */
			if (fdentry.events & POLLERR) {
				FRLOGF (LOG_INFO, "an error occurred while polling on %d: %s",
								fdentry.fd, rerr_getstr3(RERR_SYSTEM));
			}
			ret = scon_autoclose (scon, fdentry.fd);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "error closing %d: %s", fdentry.fd,
								rerr_getstr3(ret));
			}
			continue;
		}
		if (fdentry.events & POLLOUT) {
			ret = scon_dosend (scon, fdentry.fd);
			if ((!RERR_ISOK(ret)) && (ret != RERR_NOT_FOUND)) {
				FRLOGF (LOG_WARN, "error sending message to fd=%d: %s",
								fdentry.fd, rerr_getstr3(ret));
			}
		}
		if (fdentry.events & POLLIN) {
			if (fdentry.argi == FD_T_SERVER) {
				FRLOGF (LOG_VERB, "connection request arrived");
				ret = scon_doaccept (scon, fdentry.fd);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_WARN, "error accepting connection from "
								"%d: %s", fdentry.fd, rerr_getstr3(ret));
				}
			} else {
				FRLOGF (LOG_VERB, "data received on fd %d", fdentry.fd);
				ret = scon_dorecv (scon, fdentry.fd);
				if (!RERR_ISOK(ret)) {
					FRLOGF (LOG_WARN, "error receiving data from %d: %s",
								fdentry.fd, rerr_getstr3(ret));
				}
			}
		}
	}
	ret = TLST_NEW (sendlst, struct scondata);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating temp. sendlist: %s", rerr_getstr3(ret));
		return ret;
	}
	TLST_FOREACHPTR2 (data, scon->sendlst, idx) {
		if (data->dlen) {
			ret = TLST_ADD (sendlst, *data);
			if (!RERR_ISOK(ret)) {
				TLST_FREE (sendlst);
				FRLOGF (LOG_WARN, "error adding data to sendlist: "
								"%s", rerr_getstr3 (ret));
				return ret;
			}
		} else {
			if (data->data) free (data->data);
			bzero (data, sizeof (struct scondata));
		}
	}
	TLST_RESET (scon->sendlst);
	TLST_FOREACHPTR (data, sendlst) {
		ret = TLST_ADD (scon->sendlst, *data);
		if (!RERR_ISOK(ret)) {
			TLST_FREE (sendlst);
			FRLOGF (LOG_WARN, "error adding data to sendlist(2): "
								"%s", rerr_getstr3 (ret));
			return ret;
		}
	}
	TLST_FREE (sendlst);
	/* check again rcvfraglst */
	if (scon->dosplit) {
		TLST_FOREACHPTR2 (data, scon->rcvfraglst, idx) {
			ret = scon_splitinsert (scon, data->fd, data->data, data->dlen);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error splitting message: %s", rerr_getstr3(ret));
				return ret;
			}
			if (ret == 0) continue;
			if (ret < data->dlen) {
				data->dlen -= ret;
				memmove (data->data, data->data + ret, data->dlen);
				data->data[data->dlen]=0;
				continue;
			}
			free (data->data);
			bzero (data, sizeof (struct scondata));
			data->fd = -1;
			TLST_REMOVE (scon->rcvfraglst, idx, TLST_F_CPYLAST);
			idx--;
		}
		scon->dosplit = 0;
	}
	return RERR_OK;
}

int
scon_recv (scondata, scon)
	struct scondata	*scondata;
	struct scon			*scon;
{
	struct sconfd	*sconfd;
	int				ret;

	if (!scondata || !scon) return RERR_PARAM;
	if (TLST_GETNUM(scon->rcvmsglst) == 0) return RERR_NOT_FOUND;
	ret = TLST_GET (*scondata, scon->rcvmsglst, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF ((ret == RERR_NOT_FOUND) ? LOG_INFO : LOG_ERR, 
							"error getting data from list: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = scon_getfd (&sconfd, scon, scondata->fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_NOTICE, "fd %d not found: %s", scondata->fd,
									rerr_getstr3(ret));
		sconfd = NULL;
	}
	FRLOGF (LOG_VVERB, "%d bytes data received", scondata->dlen);
	ret = TLST_REMOVE (scon->rcvmsglst, 0, TLST_F_SHIFT);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error removing data from list: %s",
								rerr_getstr3(ret));
		return ret;
	}
	if (sconfd) {
		if ((ssize_t)scondata->dlen <= (ssize_t)sconfd->rcvsize) {
			sconfd->rcvsize -= scondata->dlen;
		} else {
			sconfd->rcvsize = 0;
		}
		if (sconfd->licnt != scondata->cnt) {
			FRLOGF (LOG_INFO, "in counter (%d) is %"PRIu32" - should be %"PRIu32, scondata->fd,
						scondata->cnt, sconfd->licnt);
			sconfd->licnt = scondata->cnt;
		}
		sconfd->licnt++;
	}
	return RERR_OK;
}

int
scon_send (scon, fd, data, dlen)
	struct scon	*scon;
	int			fd;
	const char	*data;
	int			dlen;
{
	return scon_sendinsert (scon, fd, data, dlen, 0);
}

int
scon_sendln (scon, fd, data, dlen)
	struct scon	*scon;
	int			fd;
	const char	*data;
	int			dlen;
{
	return scon_sendinsert (scon, fd, data, dlen, 1);
}

int
scon_sendoob (scon, fd, data)
	struct scon		*scon;
	int				fd;
	unsigned char	data;
{
	int	ret;

	/* try to send it directly */
	ret = conn_send_oob (fd, data);
	if (!RERR_ISOK(ret)) {
		/* if fail - try to insert it for later send */
		ret = scon_sendinsert2 (scon, fd, (char*)&data, 1, 0, SCON_F_OOB);
	}
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


int
scon_close (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret, doclose;
	struct sconfd	*sconfd;

	FRLOGF (LOG_VVERB, "closing fd %d", fd);
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error getting file descriptor: %s", 
								rerr_getstr3(ret));
		doclose = 1;
	} else {
		doclose = !(sconfd->flags & SCON_F_NOCLOSE);
	}
	ret = scon_rmfd (scon, fd);
	if ((fd > 2) && doclose) {
		if (sconfd->funcs.m_close) {
			ret = sconfd->funcs.m_close (fd, sconfd->funcs.m_arg);
		} else {
			ret = close (fd);
		}
		if (ret < 0) {
			FRLOGF (LOG_WARN, "error closing fd %d: %s", fd,
					strerror (errno));
		}
	}
	return ret;
}

int
scon_rmfd (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret, ret2 = RERR_OK;
	struct sconfd	*ptr;
	unsigned			idx;

	if (!scon || fd < 0) return RERR_PARAM;
	ret = scon_rmfdfromlist (&(scon->rcvmsglst), fd);
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = scon_rmfdfromlist (&(scon->rcvfraglst), fd);
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = scon_rmfdfromlist (&(scon->sendlst), fd);
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = TLST_FINDINT (idx, scon->allfd, fd);
	if (ret == RERR_NOT_FOUND) {
		/* do nothing */
	} else if (!RERR_ISOK(ret)) {
		ret2 = ret;
	} else {
		ret = TLST_GETPTR (ptr, scon->allfd, idx);
		if (!RERR_ISOK(ret)) ret2 = ret;
		bzero (ptr, sizeof (struct sconfd));
		ptr->fd = -1;
		ret = TLST_REMOVE (scon->allfd, idx, TLST_F_CPYLAST);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	if (!RERR_ISOK(ret2)) {
		FRLOGF (LOG_WARN, "removing internal structures for fd %d "
								"was not successufull: %s", fd, rerr_getstr3(ret2));
		return ret2;
	}
	return RERR_OK;
}


int
scon_setfullmsg (scon, fd, fullmsg)
	struct scon	*scon;
	int			fd;
	int			(*fullmsg) (char*, int, int*);
{
	int				ret;
	unsigned			idx;
	struct sconfd	*ptr;

	if (!scon || fd < 0) return RERR_PARAM;
	ret = TLST_FINDINT (idx, scon->allfd, fd);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (ptr, scon->allfd, idx);
	if (!RERR_ISOK(ret)) return ret;
	ptr->fullmsg = fullmsg;
	scon->dosplit = 1;
	return RERR_OK;
}

static
int
scon_autoclose (scon, fd)
	struct scon	*scon;
	int			fd;
{
	/* int				type; */
	int				flags;
	struct sconfd	*sconfd;
	int				ret;

	/* need to be safed before close, afterwards sconfd is invalid */
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	/* type = sconfd->type; */
	flags = sconfd->flags;

	/* close the connection */
	ret = scon_close (scon, fd);
	if (!RERR_ISOK(ret)) return ret;

	/* insert close message into receive queue - need to be done after scon_close,
	 * otherwise the message will be discarded by scon_close ()
	 */
	if (flags & SCON_F_RECVONCLOSE) {
		ret = scon_doinsert2 (scon, &(scon->rcvmsglst), fd, NULL, 0, SCON_DF_CLOSE);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "cannot insert close message for fd (%d): %s", fd,
						rerr_getstr3 (ret));
			return ret;
		}
	}
	return RERR_OK;
}

static
int
scon_rmfdfromlist (list, fd)
	struct tlst	*list;
	int			fd;
{
	int					ret;
	struct scondata	*ptr;
	unsigned				idx;

	if (!list || fd < 0) return RERR_PARAM;
	while ((ret = TLST_FINDINT (idx, *list, fd)) != RERR_NOT_FOUND) {
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_GETPTR (ptr, *list, idx);
		if (!RERR_ISOK(ret)) return ret;
		if (ptr->data) free (ptr->data);
		bzero (ptr, sizeof (struct scondata));
		ptr->fd = -1;
		ret = TLST_REMOVE (*list, idx, TLST_F_CPYLAST);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

static
int
scon_docleandatalist (list)
	struct tlst	*list;
{
	struct scondata	*scondata;

	if (!list) return RERR_PARAM;
	TLST_FOREACHPTR (scondata, *list) {
		if (scondata->data) free (scondata->data);
		bzero (scondata, sizeof (struct scondata));
		scondata->fd = -1;
	}
	TLST_RESET (*list);
	return RERR_OK;
}

static
int
scon_sendinsert (scon, fd, data, dlen, addln)
	struct scon	*scon;
	int			fd;
	const char	*data;
	int			dlen;
	int			addln;
{
	return scon_sendinsert2 (scon, fd, data, dlen, addln, 0);
}

static
int
scon_sendinsert2 (scon, fd, data, dlen, addln, flags)
	struct scon	*scon;
	int			fd;
	const char	*data;
	int			dlen;
	int			addln, flags;
{
	struct scondata	scondata;
	struct sconfd		*sconfd;
	int					ret;
	int					trg;

	if (!scon || !data || fd < 0 || dlen < 0) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	bzero (&scondata, sizeof (struct scondata));
	scondata.fd = fd;
	scondata.data = malloc (dlen + 2);
	if (!scondata.data) return RERR_NOMEM;
	memcpy (scondata.data, data, dlen);
	if (addln) {
		if (dlen==0 || scondata.data[dlen-1] != '\n') {
			scondata.data[dlen]='\n';
			dlen++;
		}
	}
	scondata.data[dlen]=0;	/* makes debugging easier */
	scondata.dlen = dlen;
	scondata.dflags = flags;
	scondata.cnt = sconfd->ocnt;
	FRLOGF (LOG_VVERB, "sendinsert >>%s<< to fd %d", scondata.data, scondata.fd);
	ret = TLST_ADD (scon->sendlst, scondata);
	if (!RERR_ISOK(ret)) {
		free (scondata.data);
		FRLOGF (LOG_ERR, "error inserting data into scon: %s", rerr_getstr3(ret));
		return ret;
	}
	sconfd->ocnt++;
	trg = (sconfd->hasmaxsnd && (sconfd->sndsize <= sconfd->maxsnd) && 
						(sconfd->sndsize + dlen > sconfd->maxsnd));
	sconfd->sndsize += dlen;
	if (trg && (sconfd->funcs.m_full)) {
		sconfd->funcs.m_full (fd, sconfd->funcs.m_fullarg, 1);
	}
	return RERR_OK;
}


static
int
scon_dorecv (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				ret, dlen, ret2, num, i;
	char				data[4096], *pdata=NULL;
	struct sconfd	*sconfd;
	unsigned char	ch;
	int				haveoob=0, level;
	tmo_t				now;

	if (!scon || fd < 0) return RERR_PARAM;
	CF_MAYREAD;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting file descriptor: %s", 
								rerr_getstr3(ret));
		return ret;
	}
	/* test for oob */
	if ((sconfd->flags & SCON_F_OOB) && !sconfd->isserial) {
		ret = conn_recv_oob (fd, &ch);
		if (RERR_ISOK(ret)) {
			ret = scon_doinsert2 (scon, &(scon->rcvmsglst), fd, (char*)&ch, 1, SCON_DF_OOB);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error inserting oob message (%u): %s", (unsigned)ch, 
										rerr_getstr3(ret));
				return ret;
			}
			haveoob=1;
		} else if (ret == RERR_FAIL) {
			ret = RERR_OK;
		}
	} else {
		ret = RERR_OK;
	}
	if (RERR_ISOK(ret)) {
		if (sconfd && sconfd->funcs.m_recvmsg) {
			ret = sconfd->funcs.m_recvmsg (fd, (void**)&pdata, MSG_DONTWAIT,
													sconfd->funcs.m_arg);
			if (RERR_ISOK(ret) && !pdata) ret=RERR_SERVER;
		} else if (sconfd && sconfd->funcs.m_recv) {
			ret = sconfd->funcs.m_recv (fd, data, sizeof(data)-1, MSG_DONTWAIT,
													sconfd->funcs.m_arg);
		} else {
			ret = conn_recvdirect (fd, data, sizeof(data)-1, MSG_DONTWAIT);
			if (ret < 0) ret = RERR_SYSTEM;
		}
	}
	if (RERR_ISOK(ret) && (sconfd->isserial || sconfd->funcs.m_checkBreak)) {
		if (sconfd->isserial) {
			num = sercon_getbrk (fd);
		} else {
			num = sconfd->funcs.m_checkBreak (fd, sconfd->funcs.m_arg);
		}
		if (num > 0) {
			FRLOGF (LOG_DEBUG, "break received (num=%d)", num);
			for (i=0; i<num; i++) {
				now = tmo_now ();
				ret2 = scon_doinsert2 (scon, &(scon->rcvmsglst), fd, (char*)&now,
												sizeof (now), SCON_DF_BREAK);
				if (!RERR_ISOK(ret2)) {
					FRLOGF (LOG_ERR, "error inserting break message: %s",
								rerr_getstr3(ret));
					return ret;
				}
			}
			if (ret == 0) return RERR_OK;
		}
	} 
	if (RERR_ISOK(ret) && c_debugdata && !(sconfd && sconfd->nodebugprt)) {
		char	_buf[128], *buf;
		buf = top_quotestrcpdup2 (_buf, sizeof (_buf), data, ret, 0);
		FRLOGF (LOG_DEBUG, "received: >>%s<<", buf);
		if (buf && buf != _buf) free (buf);
	}

	if (!RERR_ISOK(ret) || (ret == 0 && !haveoob)) {
		if (ret == 0) {
			errno = ECONNRESET;	/* normally that what happened */
			ret = RERR_CONNECTION;
			level = LOG_VERB;
		} else {
			level = LOG_INFO;
		}
		if (errno == ECONNRESET && sconfd->type != FD_T_CLIENT) {
			ret2 = ret;
		} else {
			ret2 = RERR_SYSTEM;
		}
		FRLOGF (level, "error receiving data: %s", rerr_getstr3(ret));
		if (errno == EAGAIN) return RERR_OK;

		if (!(sconfd->flags & SCON_F_ERRNOCLOSE)) {
			ret = scon_autoclose (scon, fd);
			if (!RERR_ISOK(ret)) ret2 = ret;
		}
		return ret2;
	}
	if (ret == 0) {
		FRLOGF (LOG_DEBUG, "zero data returned");
		return RERR_OK;
	}
	dlen = ret;
	if (!pdata) {
		data[dlen] = 0;	/* for string operations */
	}
	if (pdata) {
		ret = scon_doinsert (scon, &(scon->rcvmsglst), fd, pdata, dlen);
		free (pdata);
	} else if (!sconfd->fullmsg) {
		ret = scon_doinsert (scon, &(scon->rcvmsglst), fd, data, dlen);
	} else {
		ret = scon_addinsert (scon, fd, data, dlen);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting data into list: %s",
								rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


static
int
scon_doaccept (scon, fd)
	struct scon	*scon;
	int			fd;
{
	int				nfd, ret;
	struct sconfd	sconfd;
	struct sconfd	*serverfd;

	if (!scon || fd < 0) return RERR_PARAM;
	ret = scon_getfd (&serverfd, scon, fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error finding server fd %d: %s", fd,
								rerr_getstr3(ret));
		return ret;
	}
	if (serverfd->type != FD_T_SERVER) return RERR_PARAM;
	if (serverfd->funcs.m_accept) {
		nfd = serverfd->funcs.m_accept (fd, serverfd->flags, serverfd->funcs.m_arg);
	} else {
		nfd = conn_accept64 (fd, NULL, serverfd->flags | CONN_F_NORDYCHK, 10000LL);
	}
	if (nfd < 0) {
		ret = nfd;
		FRLOGF (LOG_ERR, "error accepting connection: %s",
										rerr_getstr3(ret));
		return ret;
	}
	FRLOGF (LOG_DEBUG, "connection accepted (new fd=%d)", nfd);
	bzero (&sconfd, sizeof (struct sconfd));
	sconfd.fd = nfd;
	sconfd.ffd = fd;
	sconfd.type = FD_T_CONN;
	sconfd.fullmsg = serverfd->fullmsg;
	sconfd.flags = serverfd->flags;
	sconfd.funcs = serverfd->funcs;
	ret = TLST_ADD (scon->allfd, sconfd);
	if (!RERR_ISOK(ret)) {
		close (nfd);
		return ret;
	}
	if (serverfd->welcomeMsg) {
		scon_sendln (scon, fd, serverfd->welcomeMsg, strlen (serverfd->welcomeMsg));
	}
	return RERR_OK;
}


static
int
scon_dosend (scon, fd)
	struct scon	*scon;
	int			fd;
{
	struct scondata	*ptr;
	struct sconfd		*sconfd;
	int					ret, type, trg;
	unsigned				idx;
	int					hassent=0;

	if (!scon || fd < 0) return RERR_PARAM;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) sconfd = NULL;
retry:
	FRLOGF (LOG_VVERB, "scon_dosend() - start");
	ret = TLST_FINDINT (idx, scon->sendlst, fd);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (ptr, scon->sendlst, idx);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr->data || ptr->dlen <= 0) {
		if (ptr->data) free (ptr->data);
		bzero (ptr, sizeof (struct scondata));
		ptr->fd = -1;
		goto retry;
	}
	FRLOGF (LOG_VVERB, "sending data >>%.*s<<", ptr->dlen, (char*) ptr->data);
	if (sconfd) {
		if (sconfd->locnt != ptr->cnt) {
			FRLOGF (LOG_INFO, "out counter is %"PRIu32" - should be %"PRIu32, ptr->cnt,
						sconfd->locnt);
			sconfd->locnt = ptr->cnt;
		}
	}
	if (ptr->dflags & SCON_F_OOB) {
		ret = conn_send_oob (fd, *(ptr->data));
		if (RERR_ISOK(ret)) ret = 1;
	} else if (sconfd && sconfd->funcs.m_send) {
		ret = sconfd->funcs.m_send (	fd, ptr->data, ptr->dlen, MSG_DONTWAIT,
												sconfd->funcs.m_arg);
	} else {
		ret = conn_senddirect (fd, ptr->data, ptr->dlen, MSG_DONTWAIT);
	}
	if (ret <= 0) {
		if (ret == 0) {
			if (hassent) return RERR_OK;
			errno = ECONNRESET;	/* normally that what happened */
		}
		if (errno == EINTR) goto retry;
		if (errno == EAGAIN) return RERR_OK;
		type = scon_gettype (scon, fd);
		ret = scon_close (scon, fd);
		if (errno == ECONNRESET && type != FD_T_CLIENT) return ret;
		return RERR_SYSTEM;
	}
	if (sconfd) {
		trg = sconfd->hasmaxsnd && (sconfd->sndsize > sconfd->maxsnd);
		if ((ssize_t)sconfd->sndsize >= ret) {
			sconfd->sndsize -= ret;
		} else {
			sconfd->sndsize = 0;
		}
		trg = trg && sconfd->sndsize <= sconfd->maxsnd;
		if (trg && sconfd->funcs.m_full) {
			sconfd->funcs.m_full (fd, sconfd->funcs.m_fullarg, 0);
		}
	}
	if (ret >= ptr->dlen) {
		if (sconfd) sconfd->locnt++;
		free (ptr->data);		/* is garanteed to exist */
		bzero (ptr, sizeof (struct scondata));
		ptr->fd = -1;
		hassent=1;
		goto retry;
		return RERR_OK;
	}
	ptr->dlen -= ret;
	memmove (ptr->data, ptr->data + ret, ptr->dlen);
	ptr->data[ptr->dlen] = 0;
	return RERR_OK;
}




static
int
scon_doinsert (scon, list, fd, data, dlen)
	struct scon	*scon;
	struct tlst	*list;
	char			*data;
	int			fd, dlen;
{
	return scon_doinsert2 (scon, list, fd, data, dlen, 0);
}


static
int
scon_doinsert2 (scon, list, fd, data, dlen, dflags)
	struct scon	*scon;
	struct tlst	*list;
	char			*data;
	int			fd, dlen, dflags;
{
	struct scondata	scondata;
	struct sconfd		*sconfd;
	int					ret;
	int					noinc;

	if (!list || (!data && dlen > 0) || dlen <0) return RERR_PARAM;
	noinc = !scon || (list == &(scon->rcvfraglst));
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) sconfd = NULL;
	if (data) {
		scondata.data = malloc (dlen+1);
		if (!scondata.data) return RERR_NOMEM;
		memcpy (scondata.data, data, dlen);
		scondata.data[dlen]=0;
	} else {
		scondata.data = NULL;
	}
	scondata.dlen = dlen;
	scondata.fd =fd;
	scondata.dflags = dflags;
	if (sconfd && !noinc) {
		scondata.cnt = sconfd->icnt;
	} else {
		scondata.cnt = -1;
	}
	ret = TLST_ADD (*list, scondata);
	if (!RERR_ISOK(ret)) {
		free (scondata.data);
		return ret;
	}
	if (sconfd) sconfd->rcvsize += dlen;
	if (sconfd && !noinc) sconfd->icnt++;
	return RERR_OK;
}


static
int
scon_addinsert (scon, fd, data, dlen)
	struct scon	*scon;
	char			*data;
	int			fd, dlen;
{
	int					ret;
	unsigned				idx;
	struct scondata	*ptr;
	char					*dptr;

	if (!scon || !data || dlen <0) return RERR_PARAM;
	if (dlen == 0) return RERR_OK;
	ret = TLST_FINDINT (idx, scon->rcvfraglst, fd);
	if (ret == RERR_NOT_FOUND) {
		ret = scon_splitinsert (scon, fd, data, dlen);
		if (!RERR_ISOK(ret)) return ret;
		if (ret >= dlen) return RERR_OK;
		return scon_doinsert (scon, &(scon->rcvfraglst), fd, data+ret, dlen-ret);
	} 
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (ptr, scon->rcvfraglst, idx);
	if (!RERR_ISOK(ret)) return ret;
	dptr = realloc (ptr->data, ptr->dlen + dlen + 1);
	if (!dptr) return RERR_NOMEM;
	ptr->data = dptr;
	dptr += ptr->dlen;
	ptr->dlen += dlen;
	memcpy (dptr, data, dlen);
	dptr[dlen]=0;
	data = ptr->data;
	dlen = ptr->dlen;
	ret = scon_splitinsert (scon, fd, data, dlen);
	if (!RERR_ISOK(ret)) return ret;
	if (ret == 0) return RERR_OK;
	if (ret < dlen) {
		dlen -= ret;
		memmove (data, data + ret, dlen);
		data[dlen]=0;
		ptr->dlen = dlen;
		return RERR_OK;
	}
	free (ptr->data);
	bzero (ptr, sizeof (struct scondata));
	ptr->fd = -1;
	TLST_REMOVE (scon->rcvfraglst, idx, TLST_F_CPYLAST);
	return RERR_OK;
}



static
int
scon_splitinsert (scon, fd, data, dlen)
	struct scon	*scon;
	char			*data;
	int			fd, dlen;
{
	int				ret, mlen, wlen=0, tlen;
	char				*ptr;
	struct sconfd	*sconfd;

	if (!scon || !data || dlen <0) return RERR_PARAM;
	if (dlen == 0) return 0;
	ptr = data;
	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	while (dlen > 0) {
		if (sconfd->stepping && !sconfd->step) break;
		mlen = sconfd->fullmsg (ptr, dlen, &tlen);
		if (mlen < 0) return mlen;
		wlen += mlen + tlen;
		if (mlen + tlen == 0) break;
		if (mlen > 0) {
			ret = scon_doinsert (scon, &(scon->rcvmsglst), fd, ptr, mlen);
			if (!RERR_ISOK(ret)) return ret;
		}
		mlen += tlen;
		dlen -= mlen;
		ptr += mlen;
		if (sconfd->stepping) sconfd->step = 0;
	}
	return wlen;
}


int
scon_setstepping (scon, fd)
	struct scon	*scon;
	int			fd;
{
	struct sconfd	*sconfd;
	int				ret;

	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	sconfd->stepping = 1;
	sconfd->step = 1;
	return RERR_OK;
}


int
scon_unsetstepping (scon, fd)
	struct scon	*scon;
	int			fd;
{
	struct sconfd	*sconfd;
	int				ret;

	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	sconfd->stepping = 0;
	sconfd->step = 0;
	scon->dosplit = 1;
	return RERR_OK;
}


int
scon_step (scon, fd)
	struct scon	*scon;
	int			fd;
{
	struct sconfd	*sconfd;
	int				ret;

	ret = scon_getfd (&sconfd, scon, fd);
	if (!RERR_ISOK(ret)) return ret;
	sconfd->step = 1;
	return RERR_OK;
}


int
scon_termnl (data, dlen, tlen)
	char	*data;
	int	dlen, *tlen;
{
	char	*s;

	if (!data || !dlen) return RERR_PARAM;
	s = index (data, '\n');
	if (s) {
		if ((s>data) && (s[-1] == '\r')) {
			s--;
			if (tlen) *tlen = 2;
		} else {
			if (tlen) *tlen = 1;
		}
		return (s-data);
	} else {
		if (tlen) *tlen = 0;
	}
	return 0;
}






static
int
read_config ()
{
	cf_begin_read ();
	c_debugdata = cf_isyes (cf_getval2 ("SconDebugData", "no"));
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
