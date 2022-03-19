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
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <netdb.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <signal.h>
#include <fcntl.h>
#include <sys/un.h>
#include <sys/stat.h>
#include <arpa/inet.h>
#include <time.h>
#include <sys/time.h>
#include <setjmp.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include "dlssl.h"
#include <stdarg.h>
#ifdef LINUX
# include <linux/un.h>
#else
# define UNIX_PATH_MAX 108
#endif
#include <pthread.h>

extern int h_errno;


#include "connection.h"
#include "host.h"
#include "fdready.h"
#include <fr/base/slog.h>
#include <fr/base/config.h>
#include <fr/base/errors.h>
#include <fr/base/fileop.h>
#include <fr/base/prtf.h>
#include <fr/base/frinit.h>
#include <fr/base/tmo.h>
#include <fr/base/dlcrypto.h>
#include <fr/cal/tmo.h>


#ifdef SunOS
//typedef int	socklen_t;
#endif


static int connStatus=0;
static int asyncFD=-1;
static tmo_t c_timeout = 5000000LL;	/* 5 seconds */
static const char	*c_cafile = NULL;
static const char	*c_capath = NULL;
static const char	*c_keyfile = NULL;
static const char	*c_certfile = NULL;
static int config_read = 0;

static int read_config ();
static int setNonblocking (int);
static int setBlocking(int);
static void noConnect1 (int);
static int setupAsync (int, tmo_t);
static int resetAsync();
static int conn_dosend (int, const char*, int, int);
static int lostConnection (int, int, const char*);
static int my_recv (int, char*, int, int);
static int my_send (int, const char*, int, int);
static int conn_accept_ip (int, int, tmo_t);
static int conn_accept_unix (int, int, tmo_t);
static int conn_open_server_serial (const char*, int);
static int conn_open_server_ip (const char*, int, int, int);
static int conn_open_server_unix (const char*, int, int);
static int conn_open_client_serial (const char*, int);
static int conn_open_client_ip (const char*, int, int, tmo_t);
static int conn_open_client_unix (const char*, int, tmo_t);
static int conn_dostarttls (int, int, int);
static void conn_ssl_log (int);
static int conn_isready64 (int fd, tmo_t timeout, const char *who);

#define RCV_BUF_SIZE	1024

struct connect {
	char	rcvbuf[RCV_BUF_SIZE];
	char	*rcvptr;
	int	bufsz;
	int	lost;
	void	*ssl;
};
struct conn_list {
	struct connect	**list;
	int				listsz;
};
#define CONN_LIST_NULL	((struct conn_list){NULL,0})
static struct conn_list	conn_list=CONN_LIST_NULL;

static int ssl_is_initialized = 0;
static void	*conn_ssl_ctx_client = NULL;
static void	*conn_ssl_ctx_server = NULL;
static int conn_ssl_client_init ();
static int conn_ssl_server_init ();
static int conn_ssl_global_init ();

static int get_conn_list (struct connect**, int);



int
conn_open_client (host, port, flags, timeout)
	const char	*host;
	int			port, flags;
	int			timeout;
{
	return conn_open_client64 (host, port, flags, ((tmo_t)timeout)*1000000LL);
}

int
conn_open_client64 (host, port, flags, timeout)
	const char	*host;
	int			port, flags;
	tmo_t			timeout;
{
	int			sock;
	int			ret;
	char			*msg;
	int			throwWelcomeMsg;
	int			use_ssl;
	int			noblock;
	const char	*funcstr = "";


	if (!host) return RERR_PARAM;
	throwWelcomeMsg = flags & CONN_FLAG_THROW_WELCOME_MSG;
	use_ssl = flags & CONN_FLAG_SSL;
	noblock = flags & CONN_FLAG_NOBLOCK;
	if (noblock) {
		if (throwWelcomeMsg) {
			throwWelcomeMsg=0;
			FRLOGF (LOG_NOTICE, "noblock and throwWelcomeMsg are mutual "
										"exclusive - ignore throwWelcomeMsg");
		}
		if (use_ssl) {
			use_ssl = 0;
			FRLOGF (LOG_NOTICE, "noblock and CONN_FLAG_SSL are mutual "
										"exclusive - ignore CONN_FLAG_SSL");
		}
	}
	switch (flags & CONN_MASK_SOCKTYPE) {
	case CONN_SOCKTYPE_UDP:
		if (use_ssl) {
			FRLOGF (LOG_WARN, "ssl doesn't work on udp connections - "
								"disabling ssl");
			use_ssl=0;
		}
		/* fall thru */
	case CONN_SOCKTYPE_TCP:
		sock = conn_open_client_ip (host, port, flags, timeout);
		funcstr="conn_open_client_ip";
		break;
	case CONN_SOCKTYPE_UNIX:
		sock = conn_open_client_unix (host, flags, timeout);
		funcstr="conn_open_client_unix";
		break;
	case CONN_SOCKTYPE_SERIAL:
		sock = conn_open_client_serial (host, flags);
		funcstr="conn_open_client_serial";
		break;
	}
	if (sock < 0) {
		ret = sock;
		FRLOGF (LOG_ERR, "error in %s(): %s", funcstr, rerr_getstr3(ret));
		return ret;
	}

	if (use_ssl) {
		ret = conn_starttls_client (sock, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error starting ssl: %s", rerr_getstr3(ret));
			close (sock);
			return ret;
		}
	}
	if (throwWelcomeMsg) {
		ret = conn_recvln64 (sock, &msg, timeout);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error receiving welcome message: %s",
								rerr_getstr3(ret));
			conn_close (sock);
			return ret;
		} else {
			FRLOGF (LOG_VERB, "received server welcome message: %s", msg);
			free (msg);
		}
	}
	return sock;
}


static
int
conn_open_client_unix (path, flags, timeout)
	const char	*path;
	int			flags;
	tmo_t			timeout;
{
	int						sd;
	struct sockaddr_un	saddr;
	int						noblock;

	noblock = flags & CONN_FLAG_NOBLOCK;
	if (!path || !*path) return RERR_PARAM;
	if (strlen(path)>=UNIX_PATH_MAX) {
		FRLOGF (LOG_ERR, "pathname >>%s<< too long", path);
		return RERR_PARAM;
	}
	sd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		FRLOGF (LOG_ERR, "cannot create socket: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_CONNECTION;
	}
	saddr.sun_family = AF_UNIX;
	strcpy (saddr.sun_path, path);

	if (noblock) {
		setNonblocking (sd);
	} else if (timeout>0) {
		setupAsync (sd, timeout);
	}
	if (connect (sd, (struct sockaddr *) &saddr, sizeof (saddr)) != 0) {
		FRLOGF (LOG_ERR, "cannot connect to server %s",
									rerr_getstr3(RERR_SYSTEM));
		close (sd);
		return RERR_CONNECTION;
	}
	if (!noblock && timeout>0) {
		if (!resetAsync ()) {
			close (sd);
			FRLOGF (LOG_ERR, "connection to >>%s<< timed out", path);
			return RERR_TIMEDOUT;
		}
	}
	return sd;
}

static
int
conn_open_client_serial (dev, flags)
	const char	*dev;
	int			flags;
{
	int	fd, noblock;

	if (!dev || !*dev) return RERR_PARAM;
	noblock = flags & CONN_FLAG_NOBLOCK;
	/* TODO: we probably should set serial parameters first */
	fd = open (dev, O_RDWR|(noblock?O_NONBLOCK:0));
	if (fd < 0) {
		FRLOGF (LOG_ERR, "error opening device >>%s<<: %s", dev, 
							rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return fd;
}


static
int
conn_open_client_ip (host, port, flags, timeout)
	const char	*host;
	int			port, flags;
	tmo_t			timeout;
{
	int						ret;
	int						sock;
	int						noblock;
	ip_t						ip;
#ifdef HAVE_IP6
	struct sockaddr_in6	saddr;
#else
	struct sockaddr_in	saddr;
#endif
	int						socktype;

	noblock = flags & CONN_FLAG_NOBLOCK;
	ret = host_getip (&ip, host);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting ip from host (%s): %s", host,
							rerr_getstr3(ret));
		return ret;
	}
	socktype=SOCK_STREAM;
	switch (flags & CONN_MASK_SOCKTYPE) {
	case CONN_SOCKTYPE_TCP:
		socktype=SOCK_STREAM;
		break;
	case CONN_SOCKTYPE_UDP:
		socktype=SOCK_DGRAM;
		break;
	}

#ifdef HAVE_IP6
	sock = socket (PF_INET6, socktype, 0);
#else
	sock = socket (PF_INET, socktype, 0);
#endif
	if (sock == -1) {
		FRLOGF (LOG_ERR, "cannot create socket: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
#ifdef HAVE_IP6
	saddr.sin6_family = AF_INET6;
	saddr.sin6_port = htons ((short)port);
	saddr.sin6_flowinfo = 0;
	saddr.sin6_scope_id = 0;
	saddr.sin6_addr = ip;
#else
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons ((short)port);
	saddr.sin_addr.s_addr = ip;
#endif
	if (noblock) {
		setNonblocking (sock);
	} else if (timeout>0) {
		setupAsync (sock, timeout);
	}
	if (connect (sock, (struct sockaddr *)&saddr, sizeof (saddr)) == -1) {
		close (sock);
		FRLOGF (LOG_ERR, "cannot connect to %s:%d: %s", host, port,
						rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (!noblock && timeout>0) {
		if (!resetAsync ()) {
			close (sock);
			FRLOGF (LOG_ERR, "connection to >>%s<< timed out", host);
			return RERR_TIMEDOUT;
		}
	}
	return sock;
}





int
conn_open_server (host, port, flags, backlog)
	const char	*host;
	int			port, flags, backlog;
{
	int			sock;
	int			ret;
	int			use_ssl;
	const char	*funcstr="";


	use_ssl = flags & CONN_FLAG_SSL;
	switch (flags & CONN_MASK_SOCKTYPE) {
	case CONN_SOCKTYPE_UDP:
		if (use_ssl) {
			FRLOGF (LOG_WARN, "ssl doesn't work on udp connections - "
										"disabling ssl");
			use_ssl=0;
		}
		/* fall thru */
	case CONN_SOCKTYPE_TCP:
		sock = conn_open_server_ip (host, port, flags, backlog);
		funcstr="conn_open_server_ip";
		break;
	case CONN_SOCKTYPE_UNIX:
		sock = conn_open_server_unix (host, backlog, flags);
		funcstr="conn_open_server_unix";
		break;
	case CONN_SOCKTYPE_SERIAL:
		sock = conn_open_server_serial (host, flags);
		funcstr="conn_open_server_serial";
		break;
	}
	if (sock < 0) {
		ret = sock;
		FRLOGF (LOG_ERR, "error in %s(): %s", funcstr, rerr_getstr3(ret));
		return ret;
	}

	return sock;
}

static
int
conn_open_server_unix (path, backlog, flags)
	const char	*path;
	int			backlog, flags;
{
	int						sd;
	struct sockaddr_un	saddr;
	struct stat				statbuf;
	char						dpath[UNIX_PATH_MAX];
	char						*s;

	if (!path || !*path) return RERR_PARAM;
	if (strlen(path)>=UNIX_PATH_MAX) {
		FRLOGF (LOG_ERR, "pathname >>%s<< too long", path);
		return RERR_PARAM;
	}
	s = rindex (path, '/');
	if (s && s>path) {
		strncpy (dpath, path, (s-path));
		dpath[s-path]=0;
		fop_mkdir_rec (dpath);
	}

	if (!lstat (path, &statbuf) && S_ISSOCK (statbuf.st_mode)) {
		unlink (path);
	}
	sd = socket (PF_UNIX, SOCK_STREAM, 0);
	if (sd == -1) {
		FRLOGF (LOG_ERR, "cannot create socket (path=%s): %s", path, 
					rerr_getstr3 (RERR_SYSTEM));
		return RERR_CONNECTION;
	}
	saddr.sun_family = AF_UNIX;
	strcpy (saddr.sun_path, path );
	if (bind (sd, (struct sockaddr *)&saddr, sizeof (saddr)) == -1) {
		FRLOGF (LOG_ERR, "cannot bind to socket (path=%s): %s", path,
						rerr_getstr3(RERR_SYSTEM));
		close (sd);
		return RERR_CONNECTION;
	}

	if (listen (sd, backlog) != 0) {
		FRLOGF (LOG_ERR,"cannot listen to socket (path=%s): %s", path,
						rerr_getstr3 (RERR_SYSTEM));
		close (sd);
		return RERR_CONNECTION;
	}

	return sd;
}


static
int
conn_open_server_ip (host, port, flags, backlog)
	const char	*host;
	int			port, backlog, flags;
{
	int						sock, socktype;
#ifdef HAVE_IP6
	struct sockaddr_in6	saddr;
#else
	struct sockaddr_in	saddr;
#endif
	int						reuse_addr, ret;
	int						any, islocal;
	ip_t						ip;

	if (!host || !*host || !strcasecmp (host, "any")) {
		any=1;
	} else {
		any=0;
		ret = host_islocal (&islocal, host);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error in host_islocal() (host=>>%s<<): %s", host,
								rerr_getstr3(ret));
			return ret;
		}
		if (!islocal) {
			FRLOGF (LOG_ERR, "cannot bind to remote host >>%s<<", host);
			return RERR_PARAM;
		}
		ret = host_getip (&ip, host);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error getting ip from host (%s): %s", host,
								rerr_getstr3(ret));
			return ret;
		}
	}
	socktype=SOCK_STREAM;
	switch (flags & CONN_MASK_SOCKTYPE) {
	case CONN_SOCKTYPE_TCP:
		socktype=SOCK_STREAM;
		break;
	case CONN_SOCKTYPE_UDP:
		socktype=SOCK_DGRAM;
		break;
	}
#ifdef HAVE_IP6
	sock = socket (PF_INET6, socktype, 0);
#else
	sock = socket (PF_INET, socktype, 0);
#endif
	if (sock == -1) {
		FRLOGF (LOG_ERR, "cannot create socket: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_CONNECTION;
	}
	/* So that we can re-bind to it without TIME_WAIT problems */
	reuse_addr = 1;
	setsockopt (sock, SOL_SOCKET, SO_REUSEADDR, 
				(const char*) &reuse_addr, sizeof(reuse_addr));
#ifdef HAVE_IP6
	saddr.sin6_family = AF_INET6;
	saddr.sin6_port = htons (port);
	saddr.sin6_flowinfo = 0;
	saddr.sin6_scope_id = 0;
	if (any) {
		saddr.sin6_addr = in6addr_any;
	} else {
		saddr.sin6_addr = ip;
	}
#else
	saddr.sin_family = AF_INET;
	saddr.sin_port = htons (port);
	if (any) {
		saddr.sin_addr.s_addr = INADDR_ANY;
	} else {
		saddr.sin_addr.s_addr = ip;
	}
#endif
	if (0 != bind (sock, (struct sockaddr *)&saddr, sizeof (saddr))) {
		close (sock);
		FRLOGF (LOG_ERR, "cannot bind name: %s", rerr_getstr3 (RERR_SYSTEM));
		return RERR_CONNECTION;
	}
	socktype = SOCK_STREAM;
	if (socktype == SOCK_STREAM || socktype == SOCK_SEQPACKET) {
		if (listen (sock, backlog) != 0) {
			FRLOGF (LOG_ERR, "cannot listen: %s", rerr_getstr3(RERR_SYSTEM));
			return RERR_CONNECTION;
		}
	}

	return sock;
}


static
int
conn_open_server_serial (dev, flags)
	const char	*dev;
	int			flags;
{
	int		fd, noblock;

	if (!dev || !*dev) return RERR_PARAM;
	noblock = flags & CONN_FLAG_NOBLOCK;
	/* TODO: we probably should set serial parameters first */
	fd = open (dev, O_RDWR|(noblock?O_NONBLOCK:0));
	if (fd < 0) {
		FRLOGF (LOG_ERR, "error opening device >>%s<<: %s", dev,
							rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return fd;
}



int
conn_accept (fd, welcomeMsg, flags, timeout)
	const char	*welcomeMsg;
	int			timeout;
	int			fd, flags;
{
	return conn_accept64 (fd, welcomeMsg, flags, ((tmo_t)timeout)*1000000LL);
}

int
conn_accept64 (fd, welcomeMsg, flags, timeout)
	const char	*welcomeMsg;
	tmo_t			timeout;
	int			fd, flags;
{
	int			use_ssl;
	int			ret;

	if (fd<0) return RERR_PARAM;
	use_ssl = flags & CONN_FLAG_SSL;
	switch (flags & CONN_MASK_SOCKTYPE) {
	case CONN_SOCKTYPE_UDP:
		if (use_ssl) {
			FRLOGF (LOG_WARN, "ssl doesn't work on udp connections - "
									"disabling ssl");
			use_ssl=0;
		}
		/* we don't do anything here */
		break;
	case CONN_SOCKTYPE_TCP:
		fd = conn_accept_ip (fd, flags, timeout);
		break;
	case CONN_SOCKTYPE_UNIX:
		fd = conn_accept_unix (fd, flags, timeout);
		break;
	case CONN_SOCKTYPE_SERIAL:
		/* we don't do anything here */
		break;
	}
	if (!RERR_ISOK(fd)) return fd;

	if (use_ssl) {
		ret = conn_starttls_server (fd, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error starting ssl: %s", rerr_getstr3(ret));
			close (fd);
			return ret;
		}
	}
	/* send welcome message */
	if (welcomeMsg && *welcomeMsg) {
		conn_sendln (fd, welcomeMsg);
	}
	return fd;
}


static
int
conn_accept_ip (fd, flags, timeout)
	int	flags,fd;
	tmo_t	timeout;
{
#ifdef HAVE_IP6
	struct sockaddr_in6	saddr;
#else
	struct sockaddr_in	saddr;
#endif
	int						ret, newfd;
	socklen_t				slen;

	if (fd<0) return RERR_PARAM;

#ifdef HAVE_IP6
	slen = sizeof (struct sockaddr_in6);
#else
	slen = sizeof (struct sockaddr_in);
#endif
	if (!(flags & CONN_F_NORDYCHK)) {
		ret = conn_isready64 (fd, timeout, "conn_accept_ip");
		if (!RERR_ISOK(ret)) return ret;
	}
	if ((newfd = accept (fd, (struct sockaddr *)&saddr, &slen)) < 0) {
		FRLOGF (LOG_ERR, "error accepting new connection: %s",
							rerr_getstr3(RERR_SYSTEM));
		return RERR_CONNECTION;
	}

	return newfd;
}	


static
int
conn_accept_unix (fd, flags, timeout)
	int	flags,fd;
	tmo_t	timeout;
{
	struct sockaddr_un	saddr;
	int						ret, newfd;
	socklen_t				slen;

	if (fd<0) return RERR_PARAM;

	slen = sizeof (struct sockaddr_un);
	if (!(flags & CONN_F_NORDYCHK)) {
		ret = conn_isready64 (fd, timeout, "conn_accept_ip");
		if (!RERR_ISOK(ret)) return ret;
	}
	if ((newfd = accept (fd, (struct sockaddr *)&saddr, &slen)) < 0) {
		FRLOGF (LOG_ERR, "error accepting new connection: %s", 
					rerr_getstr3 (RERR_SYSTEM));
		return RERR_CONNECTION;
	}

	return newfd;
}	


int
conn_free (fd)
	int		fd;
{
	if (fd<0) return RERR_PARAM;
	if ((fd < conn_list.listsz) && conn_list.list[fd]) {
		if (conn_list.list[fd]->ssl) {
			EXTCALL(SSL_free) (conn_list.list[fd]->ssl);
		}
		bzero (conn_list.list[fd], sizeof (struct connect));
		free (conn_list.list[fd]);
		conn_list.list[fd] = NULL;
	}
	return close (fd) == 0 ? RERR_OK : RERR_SYSTEM;
}


int
conn_close (fd)
	int		fd;
{
	if (fd<0) return RERR_PARAM;
	if ((fd < conn_list.listsz) && conn_list.list[fd]) {
		if (conn_list.list[fd]->ssl) {
			EXTCALL(SSL_shutdown) (conn_list.list[fd]->ssl);
			EXTCALL(SSL_free) (conn_list.list[fd]->ssl);
		}
		bzero (conn_list.list[fd], sizeof (struct connect));
		free (conn_list.list[fd]);
		conn_list.list[fd] = NULL;
	}
	return close (fd) == 0 ? RERR_OK : RERR_SYSTEM;
}

int
conn_starttls_client (fd, flags)
	int		fd, flags;
{
	return conn_dostarttls (fd, flags, 0);
}

int
conn_starttls_server (fd, flags)
	int		fd, flags;
{
	return conn_dostarttls (fd, flags, 1);
}


static
int
conn_dostarttls (fd, flags, isserver)
	int		fd, flags, isserver;
{
	void				*ssl_ctx;
	void				*ssl;
	struct connect	*conn;
	int				ret;

	if (isserver) {
		ret = conn_ssl_server_init ();
		ssl_ctx = conn_ssl_ctx_server;
	} else {
		ret = conn_ssl_client_init ();
		ssl_ctx = conn_ssl_ctx_client;
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing ssl context: %s", rerr_getstr3(ret));
		return ret;
	}

	ret = get_conn_list (&conn, fd);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in get_conn_list(): %s", rerr_getstr3(ret));
		return ret;
	}
	ssl = EXTCALL(SSL_new) (ssl_ctx);
	if (!ssl) {
		conn_ssl_log (LOG_ERR);
		FRLOGF (LOG_ERR, "error creating ssl struct");
		return RERR_SSL_CONNECTION;
	}
	if (!EXTCALL(SSL_set_fd) (ssl, fd)) {
		conn_ssl_log (LOG_ERR);
		FRLOGF (LOG_ERR, "error setting fd to SSL");
		return RERR_SSL_CONNECTION;
	}
	if (!isserver) {
		if (EXTCALL(SSL_connect) (ssl) != 1) {
			conn_ssl_log (LOG_ERR);
			FRLOGF (LOG_ERR, "error establishing ssl connection");
			EXTCALL(SSL_free) (ssl);
			return RERR_SSL_CONNECTION;
		}
	} else {
		if (EXTCALL(SSL_accept) (ssl) != 1) {
			conn_ssl_log (LOG_ERR);
			FRLOGF (LOG_ERR, "error establishing ssl connection");
			EXTCALL(SSL_free) (ssl);
			return RERR_SSL_CONNECTION;
		}
	}
	conn->ssl = ssl;
	return RERR_OK;
}

static
void
conn_ssl_log (level)
	int	level;
{
	int	e;
	char	buf[256];

	while ((e=EXTCALL(ERR_get_error) ())) {
		EXTCALL(ERR_error_string_n) (e, buf, sizeof (buf));
		FRLOGF (level, "SSL %s", buf);
	}
}

static
int
conn_ssl_global_init ()
{
	if (ssl_is_initialized) return RERR_OK;
	EXTCALL(SSL_load_error_strings)();	/* readable error messages */
	EXTCALL(SSL_library_init)();			/* initialize library */
	/* actions_to_seed_PRNG(); */
	ssl_is_initialized = 1;
	return RERR_OK;
}

static
int
conn_ssl_client_init ()
{
	if (conn_ssl_ctx_client) return RERR_OK;
	if (!ssl_is_initialized) {
		int	ret = conn_ssl_global_init ();
		if (!RERR_ISOK(ret)) return ret;
	}

	CF_MAYREAD;
	/* create context */
	conn_ssl_ctx_client = EXTCALL(SSL_CTX_new) (EXTCALL(SSLv23_client_method)());
	if (!conn_ssl_ctx_client) {
		FRLOGF (LOG_ERR, "error creating client ssl context");
		return RERR_SSL_CONNECTION;
	}
	if (c_capath || c_cafile) {
		if (!EXTCALL(SSL_CTX_load_verify_locations) (conn_ssl_ctx_client, c_cafile, c_capath)) {
			FRLOGF (LOG_WARN, "error setting CAfile/CApath");
		}
	} else {
		FRLOGF (LOG_DEBUG, "no certificate store configured - use system wide");
	}
	return RERR_OK;
}

static
int
conn_ssl_server_init ()
{
	if (conn_ssl_ctx_server) return RERR_OK;
	if (!ssl_is_initialized) {
		int	ret = conn_ssl_global_init ();
		if (!RERR_ISOK(ret)) return ret;
	}

	CF_MAYREAD;
	if (!c_keyfile || !c_certfile) {
		FRLOGF (LOG_ERR, "no key file (ssl_key) configured");
		return RERR_CONFIG;
	}
	/* create context */
	conn_ssl_ctx_server = EXTCALL(SSL_CTX_new) (EXTCALL(SSLv23_server_method)());
	if (!conn_ssl_ctx_server) {
		conn_ssl_log (LOG_ERR);
		FRLOGF (LOG_ERR, "error creating server ssl context");
		return RERR_SSL_CONNECTION;
	}
	if (c_capath || c_cafile) {
		if (!EXTCALL(SSL_CTX_load_verify_locations) (conn_ssl_ctx_server, c_cafile, c_capath)) {
			conn_ssl_log (LOG_WARN);
			FRLOGF (LOG_WARN, "error setting CAfile/CApath");
		}
	} else {
		FRLOGF (LOG_DEBUG, "no certificate store configured - use system wide");
	}
	if (!EXTCALL(SSL_CTX_use_PrivateKey_file) (conn_ssl_ctx_server, c_keyfile, SSL_FILETYPE_PEM)) {
		conn_ssl_log (LOG_ERR);
		FRLOGF (LOG_ERR, "private key file couldn't be loaded");
		return RERR_SSL_CONNECTION;
	}
	if (!EXTCALL(SSL_CTX_use_certificate_file) (conn_ssl_ctx_server, c_certfile, SSL_FILETYPE_PEM)) {
		conn_ssl_log (LOG_ERR);
		FRLOGF (LOG_ERR, "server certificate (%s) couldn't be loaded", c_certfile);
		return RERR_SSL_CONNECTION;
	}
	if (!EXTCALL(SSL_CTX_check_private_key) (conn_ssl_ctx_server)) {
		conn_ssl_log (LOG_ERR);
		FRLOGF (LOG_ERR, "no certificate for private key found in store");
		return RERR_SSL_CONNECTION;
	}
	return RERR_OK;
}
	

static
int
conn_dosend (fd, msg, msgsize, doln)
	int			fd, msgsize, doln;
	const char	*msg;
{
	int		num;
	char		*str, *s;
	int		hasadded=0;

	if (!msg || msgsize<0 || fd<0) return RERR_PARAM;
	if (msgsize == 0) return RERR_OK;
	s = (char *) msg;
	if (doln && s[msgsize-1] != '\n') {
		str = s = strdup (msg);
		s[msgsize] = '\n';
		hasadded = 1;
		msgsize++;
	}
	while (msgsize>0) {
		num = my_send (fd, s, msgsize, 0);
		if (num <= 0) {
			if (num < 0)
				lostConnection (fd, 0, "conn_send");
			if (hasadded) free (str);
			return RERR_CONNECTION;
		}
		msgsize-=num;
		s+=num;
	}
	if (hasadded) free (str);

	return RERR_OK;
}

int
conn_sendln (fd, msg)
	int			fd;
	const char	*msg;
{
	if (!msg) return RERR_PARAM;
	return conn_dosend (fd, msg, strlen(msg), 1);
}

int
conn_senddirect (fd, msg, msglen, flags)
	int			fd, msglen, flags;
	const char	*msg;
{
	return my_send (fd, msg, msglen, flags);
}

int
conn_vsendf (fd, fmt, ap)
	int			fd;
	const char	*fmt;
	va_list		ap;
{
	char	*s;
	char	buf[1024];
	int	ret;

	if (!fmt) return RERR_PARAM;
	s = va2sprtf (buf, sizeof(buf), fmt, ap);
	if (!s) return RERR_SYSTEM;
	ret = conn_sendln (fd, s);
	if (s && (s!=buf)) free (s);
	return ret;
}

int
conn_sendf (
	int			fd,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!fmt) return RERR_PARAM;
	va_start (ap, fmt);
	ret = conn_vsendf (fd, (char*)fmt, ap);
	va_end (ap);
	return ret;
}

int
conn_sendnum (fd, msg, msglen)
	int			fd, msglen;
	const char	*msg;
{
	return conn_dosend (fd, msg, msglen, 0);
}

static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;

static
int
get_conn_list (_conn, fd)
	struct connect	**_conn;
	int				fd;
{
	int				num, i;
	struct connect	*conn;

	if (fd<0) return RERR_PARAM;
	if (_conn) *_conn=NULL;
	pthread_mutex_lock (&mutex);
	num=conn_list.listsz;
	if (fd>=num) {
		conn_list.listsz=fd+1;
		conn_list.list = realloc (conn_list.list, conn_list.listsz*
												sizeof(void*));
		if (!conn_list.list) {
			FRLOGF (LOG_ERR, "cannot alloc conn_list (fd=%d)", fd);
			conn_list.listsz=0;
			pthread_mutex_unlock (&mutex);
			return RERR_NOMEM;
		}
		for (i=num; i<conn_list.listsz; i++)
			conn_list.list[i]=NULL;
	}
	if (!conn_list.list[fd]) {
		conn = conn_list.list[fd] = malloc (sizeof (struct connect));
		if (!conn) {
			FRLOGF (LOG_ERR, "cannot alloc struct connect");
			pthread_mutex_unlock (&mutex);
			return RERR_NOMEM;
		}
		bzero (conn, sizeof (struct connect));
	} else {
		conn = conn_list.list[fd];
	}
	if (_conn) *_conn = conn;
	pthread_mutex_unlock (&mutex);
	return RERR_OK;
}

int
conn_recvln (fd, msg, timeout)
	char	**msg;
	int	fd;
	int	timeout;
{
	return conn_recvln64 (fd, msg, ((tmo_t)timeout)*1000000LL);
}

int
conn_recvln64 (fd, msg, timeout)
	char	**msg;
	int	fd;
	tmo_t	timeout;
{
	return conn_recvln64f (fd, msg, timeout, 0);
}

int
conn_recvln64f (fd, msg, timeout, flags)
	char	**msg;
	int	fd, flags;
	tmo_t	timeout;
{
	int				num;
	char				*s,*end,*_msg;
	struct connect	*conn;
	int				ret,rcvnum;
#if 0
	int				msglen;
	char				*s2;
#endif
	size_t			bsz;

	if (fd<0 || !msg) return RERR_PARAM;
	ret = get_conn_list (&conn, fd);
	if (!RERR_ISOK(ret)) return ret;
	if (conn->lost && (!conn->rcvptr || !*(conn->rcvptr))) return RERR_CONNECTION;
	for (s=conn->rcvptr,end=conn->rcvptr+conn->bufsz; s&&s<end&&*s!='\n'; s++);
	if (s) {
		if (s<end) s++;
		num=s-conn->rcvptr;
		_msg = malloc (num+1);
		if (!_msg) {
			FRLOGF (LOG_ERR, "cannot alloc _msg (size=%d)", num+1);
			return RERR_NOMEM;
		}
		strncpy (_msg,conn->rcvptr,num);
		_msg[num]=0;
		if (s<end) {
			conn->rcvptr+=num;
			conn->bufsz-=num;
		} else {
			conn->bufsz=0;
			conn->rcvptr=NULL;
		}
		rcvnum=num;
		if (rcvnum>0&&_msg[rcvnum-1]=='\n') {
			rcvnum--;
			_msg[rcvnum]=0;
			if (rcvnum>0&&_msg[rcvnum-1]=='\r') {
				rcvnum--;
				_msg[rcvnum]=0;
			}
			goto finish;
		}
	} else {
		rcvnum=0;
		_msg=NULL;
	}

	if (flags & CONN_F_BYTEWISE) {
		bsz = 1;
	} else {
		bsz = RCV_BUF_SIZE;
	}
	while (1) {
		ret = conn_isready64 (fd, timeout, "conn_recvln64()");
		if (!RERR_ISOK(ret)) {
			if (_msg) free (_msg);
			return ret;
		}
		num = my_recv (fd, conn->rcvbuf, bsz, 0);
		if (num == -1) {
			lostConnection (fd, 0, "conn_recvln64()");
			if (_msg && *_msg) goto finish;
			return RERR_CONNECTION;
		} else if (num == 0) {
			/* we still have a lost connection, but it is
				not recognized - why ??? */
			/* work around */
			errno = EPIPE;
			lostConnection (fd, 0, "conn_recvln64()");
			if (_msg && *_msg) goto finish;
			return RERR_CONNECTION;
		}
		conn->bufsz=num;
		conn->rcvptr=conn->rcvbuf;
		for (s=conn->rcvbuf,end=conn->rcvbuf+num; s<end&&*s!='\n'; s++);
		if (s<end) s++;
		num=s-conn->rcvbuf;
		_msg = realloc (_msg, rcvnum+num+1);
		if (!_msg) {
			FRLOGF (LOG_ERR, "cannot realloc _msg1 (size=%d)", rcvnum+num+1);
			return RERR_NOMEM;
		}
		strncpy (_msg+rcvnum,conn->rcvbuf,num);
		rcvnum+=num;
		_msg[rcvnum]=0;
		if (s<end) {
			conn->bufsz-=num;
			conn->rcvptr=conn->rcvbuf+num;
		} else {
			conn->bufsz=0;
			conn->rcvptr=NULL;
		}
		if (rcvnum>0&&_msg[rcvnum-1]=='\n') {
			rcvnum--;
			_msg[rcvnum]=0;
			if (rcvnum>0&&_msg[rcvnum-1]=='\r') {
				rcvnum--;
				_msg[rcvnum]=0;
			}
			break;
		}
	}
finish:
	if (!_msg) 
		return RERR_CONNECTION;
#if 0
	s = top_skipwhite (_msg);
	if (!s||!*s) {
		free (_msg);
		return conn_recvln64 (fd, msg, timeout);
	}
	if (s>_msg) {
		for (s2=_msg; *s; s++,s2++) *s2=*s;
		for (s2--; s2>=_msg&&iswhite (*s2); s2--); s2++;
		*s2=0;
	} else {
		for (s2=_msg+strlen(_msg)-1; s2>=_msg&&iswhite(*s2); s2--); s2++;
		*s2=0;
	}
	if (!*_msg) {
		free (_msg);
		return conn_recvln64 (fd, msg, timeout);
	}
	msglen = s2-_msg+1;
	_msg = realloc (_msg, msglen);
	if (!_msg) {
		FRLOGF (LOG_ERR, "cannot realloc _msg2 (size=%d)", msglen);
		return RERR_NOMEM;
	}
#endif
	*msg = _msg;

	return RERR_OK;
}

int
conn_recvdirect (fd, msg, msglen, flags)
	int	fd, msglen, flags;
	char	*msg;
{
	return my_recv (fd, msg, msglen, flags);
}


int
conn_recvnum (fd, msg, msglen, timeout)
	char	*msg;
	int	msglen, fd;
	int	timeout;
{
	return conn_recvnum64 (fd, msg, msglen, ((tmo_t)timeout)*1000000LL);
}

#define MIN(a,b)	(((a)<=(b))?(a):(b))
int
conn_recvnum64 (fd, msg, msglen, timeout)
	char	*msg;
	int	msglen, fd;
	tmo_t	timeout;
{
	struct connect	*conn;
	char				*mptr;
	int				num, ret;

	if (msglen<0 || !msg || fd<0) return RERR_PARAM;
	if (msglen==0) {
		*msg=0;
		return RERR_OK;
	}
	mptr=msg;
	if (conn_list.listsz > fd && (conn=conn_list.list[fd]) && (conn->rcvptr) && 
				(conn->bufsz>0)) {
		num=MIN(msglen, conn->bufsz);
		strncpy (msg, conn->rcvptr, num);
		mptr+=num;
		msglen-=num;
		conn->bufsz-=num;
		if (!conn->bufsz) {
			conn->rcvptr=NULL;
		} else {
			conn->rcvptr+=num;
		}
	}
	while (msglen>0) {
		ret = conn_isready64 (fd, timeout, "conn_recvnum64 ()");
		if (!RERR_ISOK(ret)) return ret;
		num = my_recv (fd, mptr, msglen, 0);
		if (num == -1) {
			lostConnection (fd, 0, "conn_recvnum64()");
			return RERR_CONNECTION;
		} else if (num == 0) {
			/* we still have a lost connection, but it is
				not recognized - why ??? */
			/* work around */
			errno = EPIPE;
			lostConnection (fd, 0, "conn_recvnum64()");
			return RERR_CONNECTION;
		}
		mptr+=num;
		msglen-=num;
	}
	*mptr=0;
	return RERR_OK;
}

static
int
my_send (fd, buf, len, dummy)
	int			fd, len, dummy;
	const char	*buf;
{
	struct connect	*conn=NULL;
	int				ret;

	if (fd<conn_list.listsz) conn=conn_list.list[fd];
	if (conn && conn->ssl) {
		ret = EXTCALL(SSL_write) (conn->ssl, buf, len);
		if (ret <= 0) ret = 0;
		return ret;
	} else {
		return send (fd, buf, len, dummy);
	}
	return 0;
}


static
int
my_recv (fd, buf, len, dummy)
	int	fd, len, dummy;
	char	*buf;
{
	struct connect	*conn=NULL;
	int				ret;

	if (fd<conn_list.listsz) conn=conn_list.list[fd];
	if (conn && conn->ssl) {
		ret = EXTCALL(SSL_read) (conn->ssl, buf, len);
		if (ret <= 0) ret = 0;
		return ret;
	} else {
		return recv (fd, buf, len, dummy);
	}
	return 0;
}

int
conn_recv_oob (fd, c)
	int				fd;
	unsigned char	*c;
{
#if 0	/* does not work - why ?? */
	int	ret;

	ret = recv (fd, c, 1, MSG_OOB);
	if (ret < 0) return RERR_SYSTEM;
	if (ret == 1) return RERR_OK;
#endif
	return RERR_FAIL;
}

int
conn_send_oob (fd, c)
	int				fd;
	unsigned char	c;
{
	int	ret;

	ret = send (fd, &c, 1, MSG_OOB);
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}


static
int
conn_isready64 (fd, timeout, who)
	int			fd;
	tmo_t			timeout;
	const char	*who;
{
	int	ret;

	ret = fd_isready (fd, timeout);
	if (!RERR_ISOK(ret) && ret != RERR_TIMEDOUT) {
		FRLOGF (LOG_ERR, "%s: error waiting for data: %s",
						who?who:"<>", rerr_getstr3 (ret));
	}
	return ret;
}



static
int
lostConnection (fd, bySignal, action)
	int			bySignal, fd;
	const char	*action;
{
	int 	myerrno = errno;

	if (!action) action="lostConnection";
	if (!bySignal) {
		FRLOGF (LOG_ERR, "%s(): error (fd=%d): %s", action, fd,
					rerr_getstr3(RERR_SYSTEM));
		switch (myerrno) {
		case EPIPE:
		case ENETDOWN:			/* Network down */
		case ENETUNREACH:		/* Network unreachable */
		case ENETRESET:		/* Network dropped connection */
		case ECONNABORTED:	/* Software caused connection abort */
		case ECONNRESET:		/* Connection reset by peer */
		case ENOTCONN:			/* Socket is not connected */
		case ESHUTDOWN:		/* Can't send after socket shutdown */
		case EHOSTDOWN:		/* Host is down */
			break;
		default:
			return RERR_OK;
		}
	}
	FRLOGF (LOG_ERR, "lost connection to %d", fd);
	if (conn_list.listsz>fd) {
		conn_list.list[fd]->lost = 1;
	}
	return RERR_CONNECTION;
}













/*----------------------------------------------------------------------
 Portable function to set a socket into nonblocking mode.
 Calling this on a socket causes all future read() and write() calls on
 that socket to do only as much as they can immediately, and return
 without waiting.
 If no data can be read or written, they return -1 and set errno
 to EAGAIN (or EWOULDBLOCK).
 Thanks to Bjorn Reese for this code.
----------------------------------------------------------------------*/


int
conn_setNonBlocking (fd)
	int	fd;
{
	return setNonblocking (fd);
}

int
conn_setBlocking (fd)
	int	fd;
{
	return setBlocking (fd);
}


static
int
setNonblocking (int fd)
{
	int		flags;

	/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
	return fcntl(fd, F_SETFL, flags | O_NONBLOCK);
#else
	/* Otherwise, use the old way of doing it */
	flags = 1;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

static
int
setBlocking (int fd)
{
	int		flags;

	/* If they have O_NONBLOCK, use the Posix way to do it */
#if defined(O_NONBLOCK)
	/* Fixme: O_NONBLOCK is defined but broken on SunOS 4.1.x and AIX 3.2.5. */
	if (-1 == (flags = fcntl(fd, F_GETFL, 0))) flags = 0;
	flags &= ~O_NONBLOCK;
	return fcntl(fd, F_SETFL, flags);
#else
	/* Otherwise, use the old way of doing it */
	flags = 0;
	return ioctl(fd, FIOBIO, &flags);
#endif
}

static
void 
noConnect1 (int sig)
{
	connStatus=0;

	alarm (0);
	signal (SIGALRM, SIG_IGN);
	setNonblocking (asyncFD);
}

static
int
setupAsync (fd, timeout)
	int	fd;
	tmo_t	timeout;
{
	struct itimerval	itval;

	CF_MAY_READ;
	connStatus = 1;
	if (timeout == -2) timeout = c_timeout;
	if (timeout < 0) return 1;
	if (timeout==0) timeout=1;	/* 0 timeout might lead to undesired effects */
	asyncFD = fd;

	bzero (&itval, sizeof (struct itimerval));
	itval.it_value.tv_sec = timeout / 1000000LL;
	itval.it_value.tv_usec = timeout % 1000000LL;
	signal (SIGALRM, noConnect1);
	setitimer (ITIMER_REAL, &itval, NULL);

	return 1;
}


static
int
resetAsync ()
{
	struct itimerval	itval;

	if (!connStatus) setBlocking (asyncFD);
	asyncFD = -1;
	bzero (&itval, sizeof (struct itimerval));
	setitimer (ITIMER_REAL, &itval, NULL);
	signal (SIGALRM, SIG_IGN);

	return connStatus;
}



static
int
read_config ()
{
	cf_begin_read ();
	c_timeout = cf_atotm (cf_getval2 ("connection_timeout", "5"));
	c_cafile = cf_getarr ("CAfile", fr_getprog ());
	c_capath = cf_getarr ("CApath", fr_getprog ());
	c_keyfile = cf_getarr ("SSL_key", fr_getprog ());
	c_certfile = cf_getarr ("SSL_cert", fr_getprog ());
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
