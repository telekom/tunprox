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
 * Portions created by the Initial Developer are Copyright (C) 2003-2019
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>


#include <fr/base.h>
#include <fr/connect.h>
#include <fr/thread.h>

#include "eddi.h"
#include "eddilisten.h"
#include "evbox.h"
#include "parseevent.h"
#include "evpool.h"
#include "evfilter.h"

#if defined Linux && !defined NOKEDDI
#	define HASKEDDI
#endif


#ifdef HASKEDDI
#	include <fr/netlink/kieddi.h>
#endif

static int read_config ();
static int config_read = 0;

#ifdef HASKEDDI
static int			c_usekeddi = 0;
static int			c_keddichan = 0;
#endif
static const char	*c_sockpath = "/var/run/eddi.sock";
static int			c_disable = 0;

static int eddifd = -1;
static int eddi_started = 0;


static int thrd_init ();
static int thrd_main ();
static int thrd_cleanup ();

static struct frthr	eddithr = {
	.name = "eddilisten",
	.init = thrd_init,
	.main = thrd_main,
	.cleanup = thrd_cleanup,
};
static int				eddithr_id = -1;

FRTHR_COND_T(int, waitforsent)	waitforsent = FRTHR_COND_INIT;

static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;
#define G_LOCK	do { pthread_mutex_lock (&mutex); } while (0)
#define G_UNLOCK do { pthread_mutex_unlock (&mutex); } while (0)

static struct scon	scon_rcv;
static struct scon	scon_snd;
static int				isconn=0;

static int prepollinit();
static void prepollclose ();
static int insertprepoll (char *);
static int dosend (char*);
static int emptyprepoll ();


int
eddilisten_start ()
{
	int	ret;

	CF_MAYREAD;
	if (eddi_started) return RERR_OK;
	if (c_disable) return RERR_OK;
	ret = frthr_start (&eddithr, 500000LL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting eddi worker thread: %s", rerr_getstr3 (ret));
		return ret;
	}
	eddithr_id = ret;
	eddi_started = 1;
	return RERR_OK;
}

int
eddilisten_finish ()
{
	int	ret;

	ret = frthr_destroy (eddithr_id);
	if (!RERR_ISOK(ret)) return ret;
	eddithr_id = -1;
	eddi_started = 0;
	return RERR_OK;
}

int
eddilisten_isstarted ()
{
	return eddi_started;
}

int
eddilisten_gettid (tid)
	pthread_t	*tid;
{
	if (!eddi_started) return RERR_NOTINIT;
	return frthr_id2tid (tid, eddithr_id);
}

int
eddiout_sendev (event)
	struct event	*event;
{
	int	ret;
	char	*evstr;

	if (!event) return RERR_PARAM;
	ret = ev_create (&evstr, event, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating event: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = eddiout_sendstr (evstr);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sendig event: %s", rerr_getstr3(ret));
		free (evstr);
		return ret;
	}
	ret = evpool_release (event);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "cannot release event: %s", rerr_getstr3(ret));
	}
	return RERR_OK;
}

int
eddiout_sendstr (evstr)
	char	*evstr;
{
	int	ret;

	if (!evstr) return RERR_PARAM;
//printf ("snd: %s\n", evstr);
	G_LOCK;
	if (!isconn) {
		ret = insertprepoll (evstr);
	} else {
		ret = dosend (evstr);
	}
	G_UNLOCK;
	return ret;
}

int
eddi_waitforsent (tout)
	tmo_t	tout;
{
#ifdef HASKEDDI
	if (c_usekeddi) {
		FRTHR_COND_WAIT (waitforsent, tout, waitforsent.waitforsent > 0);
	}
#endif
	return RERR_OK;
}

void
eddi_setwaitforsent ()
{
	FRTHR_COND_SET (waitforsent, waitforsent.waitforsent = 1);
}


int
eddiout_maysend ()
{
	G_LOCK;
	SCON_WAIT (scon_snd, 10000LL /* 10ms */);
	G_UNLOCK;
	return RERR_OK;
}


/******************* 
 * static functions 
 *******************/

static char 			**prepollbuf = NULL;
static int				prepolllen = 0;
static int				prepollfirst = 0;
static int				prepollnext = 0;
static int				prepollisinit = 0;



static
int
prepollinit ()
{
	if (prepollisinit) return RERR_OK;
	prepolllen = 32;
	prepollbuf = malloc (prepolllen * sizeof (char*));
	if (!prepollbuf) return RERR_NOMEM;
	prepollfirst = prepollnext = 0;
	prepollisinit = 1;
	return RERR_OK;
}

static
void
prepollclose ()
{
	if (!prepollisinit) return;
	emptyprepoll ();
	free (prepollbuf);
	prepollisinit = 0;
}

static
int
emptyprepoll ()
{
	char	*evstr;
	int	ret, ret2 = RERR_OK;

	while (prepollfirst != prepollnext) {
		evstr = prepollbuf[prepollfirst];
		prepollfirst = (prepollfirst + 1) % prepolllen;
		ret = dosend (evstr);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error sending event: %s", rerr_getstr3(ret));
			ret2 = ret;
		}
	}
	return ret2;
}

static
int
insertprepoll (evstr)
	char	*evstr;
{
	int	ret;

	if (!prepollisinit) {
		ret = prepollinit ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error creating prepoll buffer, drop events: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
	FRLOGF (LOG_VERB, "inserting event >>%s<< to prepoll buffer", evstr);
	if (((prepollnext + 1) % prepolllen) == prepollnext) {
		FRLOGF (LOG_INFO, "dropping old event >>%s<<", prepollbuf[prepollfirst]);
		free (prepollbuf[prepollfirst]);
		prepollbuf[prepollfirst] = NULL;
		prepollfirst = (prepollfirst + 1) % prepolllen;
	}
	prepollbuf[prepollnext] = evstr;
	prepollnext = (prepollnext + 1) % prepolllen;
	return RERR_OK;
}

static
int
dosend (buf)
	char	*buf;
{
	int	ret;

	FRLOGF (LOG_VERB, "sending event >>%s<< to fd %d", buf, eddifd);
	ret = SCON_SENDLN (scon_snd, eddifd, buf);
	if (!RERR_ISOK(ret)) return ret;
	ret = SCON_WAIT (scon_snd, 100000LL /* 100ms */);
	free (buf);
	return ret;
}


/********************* 
 * thread functions 
 *********************/


static int run_loop ();
static int conn2server ();
static int readpollall ();
static int readpoll ();
static int run_preloop ();
static int tryconnect ();
#ifdef HASKEDDI
static ssize_t mykeddi_recv (int, void**, int, void*);
static ssize_t mykeddi_send (int, const void*, size_t, int, void*);
static int mykeddi_noclose (int, void*);
#endif


static
int
thrd_cleanup ()
{
	G_LOCK;
	isconn = 0;
	SCON_FREE (scon_rcv);
	SCON_FREE (scon_snd);
	if (eddifd>2) {
#ifdef HASKEDDI
		if (c_usekeddi) {
			kieddi_close ();
		} else
#endif
			conn_close (eddifd);
	}
	eddifd = -1;
	eddi_started = 0;
	G_UNLOCK;
	return RERR_OK;
}



static
int
thrd_main()
{
	int	ret;

	if (!isconn) {
		ret = run_preloop ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error in pre loop: %s", rerr_getstr3 (ret));
			return ret;
		}
	}
	return run_loop ();
}


static
int
thrd_init ()
{
	int	ret;

	ret = conn2server ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error connecting to server: %s", 
						rerr_getstr3(ret));
	}
	return RERR_OK;
}


static
int
conn2server ()
{
	int	ret;

	CF_MAYREAD;
	if (isconn) return RERR_OK;
#ifdef HASKEDDI
	if (c_usekeddi) {
		ret = eddifd = kieddi_connect (c_keddichan);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "cannot connect to kernel eddi: %s",
									rerr_getstr3(ret));
			return ret;
		}
	} else {
#endif
		ret = eddifd = conn_open_client (c_sockpath, 0, CONN_T_UNIX, 5);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error connecting to server "
						">>%s<<: %s", c_sockpath, rerr_getstr3 (ret));
			return ret;
		}
#ifdef HASKEDDI
	}
#endif
	ret = SCON_NEW (scon_rcv);
	if (RERR_ISOK(ret)) {
		ret = SCON_NEW (scon_snd);
		if (!RERR_ISOK(ret)) SCON_FREE (scon_rcv);
	}
	if (!RERR_ISOK(ret)) {
#ifdef HASKEDDI
		if (c_usekeddi) {
			kieddi_close ();
		} else 
#endif
			conn_close (eddifd);
		return ret;
	}
#ifdef HASKEDDI
	if (c_usekeddi) {
		struct sconfuncs funcs = {
			.m_recvmsg = mykeddi_recv,
			.m_send = mykeddi_send,
			.m_close = mykeddi_noclose,
		};
		ret = SCON_ADD3 (scon_rcv, eddifd, funcs, SCON_F_CLIENTCONN|\
								SCON_F_ERRNOCLOSE|SCON_F_NOCLOSE);
		if (RERR_ISOK(ret)) {
			ret = SCON_ADD3 (scon_snd, eddifd, funcs, SCON_F_CLIENTCONN|\
								SCON_F_ERRNOCLOSE|SCON_F_NOCLOSE);
		}
	} else
#endif
	{
		ret = SCON_ADD (scon_rcv, eddifd, scon_termnl, NULL, SCON_F_CLIENTCONN);
		if (RERR_ISOK(ret)) {
			ret = SCON_ADD (scon_snd, eddifd, scon_termnl, NULL, SCON_F_CLIENTCONN);
		}
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding fd to scon: %s",
					rerr_getstr3(ret));
		SCON_FREE (scon_rcv);
		SCON_FREE (scon_snd);
#ifdef HASKEDDI
		if (c_usekeddi) {
			kieddi_close ();
		} else 
#endif
			conn_close (eddifd);
		return ret;
	}
	SCON_DISABLESND (scon_rcv, eddifd);
	SCON_DISABLERCV (scon_snd, eddifd);
	isconn = 1;
	return RERR_OK;
}

#ifdef HASKEDDI
static
ssize_t
mykeddi_recv (fd, buf, flags, arg)
	int	fd, flags;
	void	**buf;
	void	*arg;
{
	return kieddi_recv ((char**)(void*)buf);
}

static
ssize_t
mykeddi_send (fd, buf, blen, flags, arg)
	int			fd, flags;
	const void	*buf;
	size_t		blen;
	void			*arg;
{
	int	ret;

	ret = kieddi_send ((const char*)buf);
	FRTHR_COND_SET (waitforsent, waitforsent.waitforsent = 0);
	FRTHR_COND_SIGNAL (waitforsent);
	if (!RERR_ISOK(ret)) return ret;
	return blen;
}

static
int
mykeddi_noclose (fd, arg)
	int	fd;
	void	*arg;
{
	return 0;
}
#endif	/* HASKEDDI */

static
int
run_preloop ()
{
	int	ret;

	while (1) {
		tmo_sleep (200000LL);	/* 200ms */
		ret = tryconnect ();
		if (RERR_ISOK(ret)) return RERR_OK;
	}
	return RERR_INTERNAL;
}

static
int
tryconnect ()
{
	int	ret;

	ret = conn2server ();
	if (!RERR_ISOK(ret)) return ret;
	G_LOCK;
	prepollclose ();
	G_UNLOCK;
	return RERR_OK;
}

static
int
run_loop ()
{
	while (1) {
		SCON_WAIT (scon_rcv, 2000000LL);	/* 1s */
		readpollall ();
	}
	return RERR_INTERNAL;	/* should never reach here */
}



static
int
readpollall ()
{
	int	ret;

	while ((ret = readpoll()) == RERR_OK);
	if (ret != RERR_NOT_FOUND) return ret;
	return RERR_OK;
}


static
int
readpoll ()
{
	int					ret;
	struct scondata	sdata;
	pthread_t			tid;

	ret = SCON_RECV (sdata, scon_rcv);
	if (!RERR_ISOK(ret)) return ret;
//printf ("rcv: %s\n", sdata.data);
	eddi_gettid (&tid);
	ret = evbox_insertstr (sdata.data, 0, "in", tid);
	if (!RERR_ISOK(ret)) {
		FRLOGF ((ret == RERR_FULL) ? LOG_INFO : LOG_ERR, "error inserting "
						"event into event box: %s", rerr_getstr3 (ret));
		FRLOGF (LOG_INFO, "event >>%s<< will be dropped", sdata.data);
		free (sdata.data);
		return ret;
	}
	free (sdata.data);
	return RERR_OK;
}



static
int
read_config ()
{
	cf_begin_read ();
	c_sockpath = cf_getarr2 ("sock", "eddi", "/var/run/eddi.sock");
#ifdef HASKEDDI
	c_usekeddi = cf_isyes (cf_getval2 ("UseKeddi", "no"));
	c_keddichan = cf_atoi (cf_getval2 ("KeddiChan", "0"));
#endif
	c_disable = cf_isyes (cf_getval2 ("DisableEddiListener", "no"));
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
