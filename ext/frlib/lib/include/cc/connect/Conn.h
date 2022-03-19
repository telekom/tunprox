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

#ifndef _R__FRLIB_LIB_CC_CONNECT_CONNECTION_H
#define _R__FRLIB_LIB_CC_CONNECT_CONNECTION_H



#include <fr/connect/connection.h>
#include <fr/connect/fdready.h>
#include <fr/base/errors.h>
#include <fr/cc/connect/Host.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>


class Conn {
public:
	Conn ()
	{
		bzero (this, sizeof (Conn));
		fd = -1;
	};
	Conn (char *_host, int _port, int _isserver, int _flags, int _timeout, 
			int _backlog = 10)
	{
		bzero (this, sizeof (Conn));
		fd = -1;
		host = _host ? strdup (_host) : NULL;
		port = _port;
		isserver = _isserver;
		flags = _flags;
		timeout = _timeout;
		backlog = _backlog;
	};
	Conn (	Host &_host, int _port, int _isserver, int _flags, int _timeout, 
			int _backlog = 10)
	{
		bzero (this, sizeof (Conn));
		fd = -1;
		setHost (_host);
		port = _port;
		isserver = _isserver;
		flags = _flags;
		timeout = _timeout;
		backlog = _backlog;
	};
	Conn (const Conn &oconn)
	{
		bzero (this, sizeof (Conn));
		fd = -1;
		host = oconn.host ? strdup (oconn.host) : NULL;
		port = oconn.port;
		isserver = oconn.isserver;
		flags = oconn.flags;
		timeout = oconn.timeout;
	};
	~Conn ()
	{
		if (fd>=0) close();
		if (host) free (host);
		bzero (this, sizeof (Conn));
		fd = -1;
	};
	int setHost (Host &_host)
	{
		char	*__host;
		__host = (char*)_host.getName ();
		if (!__host) return RERR_INVALID_HOST;
		return setHostName (__host);
	};
	int setHostName (char *_host)
	{
		if (!_host) return RERR_PARAM;
		if (host) free (host);
		host = strdup (_host);
		return host ? RERR_OK : RERR_NOMEM;
	};
	int setPort (int _port)
	{
		port = _port;
		return RERR_OK;
	};
	int setTimeout (int _timeout)
	{
		timeout = _timeout;
		return RERR_OK;
	};
	int setBacklog (int _backlog)
	{
		backlog = _backlog;
		return RERR_OK;
	};
	int setServer (int _isserver)
	{
		isserver = _isserver;
		return RERR_OK;
	};
	int open ()
	{
		if (fd>=0) return RERR_FORBIDDEN;
		if (isserver) {
			fd = conn_open_server (host, port, flags, backlog);
		} else {
			fd = conn_open_client (host, port, flags, timeout);
		}
		if (fd<0) return (-1)*fd;
		return RERR_OK;
	};
	int close ()
	{
		int ret = conn_close (fd);
		fd = -1;
		return ret;
	};
	Conn * accept (char *welcomeMsg)
	{
		Conn	*nconn;
		if (!isserver || fd<0) return NULL;
		nconn = new Conn (*this);
		nconn->fd = conn_accept (fd, welcomeMsg, flags, timeout);
		if (nconn->fd<0) {
			delete nconn;
			return NULL;
		}
		return nconn;
	};
	Conn *accept ()
	{
		return accept (NULL);
	};
	int isReady ()
	{
		return fd_isready (fd, timeout);
	};
	static int waitReady (fd_set *rdy, int maxfd, int timeout)
	{
		return fd_waitready (rdy, maxfd, timeout);
	};
	int recvNum (char *msgbuf, int msglen)
	{
		return conn_recvnum (fd, msgbuf, msglen, timeout);
	};
	int recvLn (char **msg)
	{
		return conn_recvln (fd, msg, timeout);
	};
	int sendNum (char *msg, int msglen)
	{
		return conn_sendnum (fd, msg, msglen);
	};
	int sendLn (char *msg)
	{
		return conn_sendln (fd, msg);
	};
	int sendf (char *fmt, ...) __attribute__((format(printf, 2, 3)))
	{
		va_list	ap;
		int		ret;
		va_start (ap, fmt);
		ret = conn_vsendf (fd, fmt, ap);
		va_end (ap);
		return ret;
	};
	int vsendf (char *fmt, va_list ap)
	{
		return conn_vsendf (fd, fmt, ap);
	};
	int starttls ()
	{
		if (isserver) {
			return conn_starttls_server (fd, flags);
		} else {
			return conn_starttls_client (fd, flags);
		}
		return RERR_INTERNAL;	/* to make compiler happy */
	};

public:
	int	fd;
	char	*host;
	int	port, flags, isserver, backlog, timeout;
};
















#endif	/* _R__FRLIB_LIB_CC_CONNECT_CONNECTION_H */

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
