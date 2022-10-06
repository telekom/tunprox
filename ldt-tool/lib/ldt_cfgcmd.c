/*
 * Copyright (C) 2015-2022 by Frank Reker, Deutsche Telekom AG
 *
 * LDT - Lightweight (MP-)DCCP Tunnel kernel module
 *
 * This is not Open Source software. 
 * This work is made available to you under a source-available license, as 
 * detailed below.
 *
 * Copyright 2022 Deutsche Telekom AG
 *
 * Permission is hereby granted, free of charge, subject to below Commons 
 * Clause, to any person obtaining a copy of this software and associated 
 * documentation files (the "Software"), to deal in the Software without 
 * restriction, including without limitation the rights to use, copy, modify,
 * merge, publish, distribute, sublicense, and/or sell copies of the Software,
 * and to permit persons to whom the Software is furnished to do so, subject to
 * the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING
 * FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER 
 * DEALINGS IN THE SOFTWARE.
 *
 * “Commons Clause” License Condition v1.0
 *
 * The Software is provided to you by the Licensor under the License, as
 * defined below, subject to the following condition.
 *
 * Without limiting other conditions in the License, the grant of rights under
 * the License will not include, and the License does not grant to you, the
 * right to Sell the Software.
 *
 * For purposes of the foregoing, “Sell” means practicing any or all of the
 * rights granted to you under the License to provide to third parties, for a
 * fee or other consideration (including without limitation fees for hosting 
 * or consulting/ support services related to the Software), a product or 
 * service whose value derives, entirely or substantially, from the
 * functionality of the Software. Any license notice or attribution required
 * by the License must also include this Commons Clause License Condition
 * notice.
 *
 * Licensor: Deutsche Telekom AG
 */


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <sys/types.h>
#include <errno.h>
#include <sys/poll.h>
#include <sys/stat.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/filter.h>
#include <dirent.h>
#include <assert.h>

#include <fr/base.h>
#include <fr/xml.h>
#include <fr/netlink/fnl.h>

#include <ldt/ldt.h>

static int do_tun_setpeer (const char *name, frad_t *raddr);
static int do_tun_serverstart (const char *);


int
ldt_create_dev (name, out_name, flags)
	const char	*name;
	char			**out_name;
	uint32_t		flags;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name) name = "";
	len = FNL_MSGMINLEN + strlen (name) + 2 + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,	LDT_CMD_CREATE_DEV);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_CREATE_DEV_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	ptr = fnl_putattr (ptr, LDT_CMD_CREATE_DEV_ATTR_FLAGS, &flags, 4);
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	if (!out_name) {
		ret = ldt_nl_getret ();
	} else {
		ret = ldt_nl_getanswer (out_name, NULL);
	}
	ldt_mayclose ();
	return ret;
}

int
ldt_rm_dev (name)
	const char	*name;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name) name = "";
	len = FNL_MSGMINLEN + strlen (name) + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,	LDT_CMD_RM_DEV);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_RM_DEV_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}


int
ldt_rm_tun (name)
	const char	*name;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name) name = "";
	len = FNL_MSGMINLEN + strlen (name) + 2 + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,	LDT_CMD_RM_TUN);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_RM_TUN_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}

int
ldt_newtun (name, tuntype)
	const char	*name, *tuntype;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name || !tuntype) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + strlen (tuntype) + 64;
	msg = malloc (len);
	if (!msg) return RERR_NOMEM;
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_NEWTUN);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_NEWTUN_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	ptr = fnl_putattr (	ptr, LDT_CMD_NEWTUN_ATTR_TUN_TYPE, tuntype,
								strlen(tuntype)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}

int
ldt_tunbind (name, laddr)
	const char	*name;
	frad_t		*laddr;
{
	char		*msg;
	int		ret, len;
	char		*ptr;
	uint16_t	port;

	if (!name) return RERR_PARAM;
	if (!laddr) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_BIND);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_BIND_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	if (!FRADP_ISIPV6(laddr)) {
		ptr = fnl_putattr (	ptr, LDT_CMD_BIND_ATTR_ADDR4,
									&laddr->v4.sin_addr.s_addr, 4);
	} else {
		ptr = fnl_putattr (	ptr, LDT_CMD_BIND_ATTR_ADDR6,
									laddr->v6.sin6_addr.s6_addr, 16);
	}
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	port = frad_getport (laddr);
	ptr = fnl_putattr (ptr, LDT_CMD_BIND_ATTR_PORT, &port, 2);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}

int
ldt_tunbind2dev (name, dev)
	const char	*name, *dev;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name) return RERR_PARAM;
	if (!dev) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + (dev ? strlen (dev) : 0) + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_BIND2DEV);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_BIND_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	ptr = fnl_putattr (	ptr, LDT_CMD_BIND2DEV_ATTR_DEV, dev,
								strlen(dev)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}

int
ldt_tun_setpeer (name, raddr, tout)
	const char	*name;
	frad_t		*raddr;
	tmo_t			tout;
{
	int							ret;
	struct ldt_evinfo	evinfo;

	ldt_event_open ();
	ret = do_tun_setpeer (name, raddr);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error setting peer: %s", rerr_getstr3(ret));
		ldt_mayclose ();
		return ret;
	}
	if (tout == 0) return RERR_OK;

	bzero (&evinfo, sizeof (evinfo));
	ret = ldt_waitifaceev (&evinfo, name, 
						(1<<LDT_EVTYPE_TUNDOWN) | (1<<LDT_EVTYPE_IFDOWN) |
						(1<<LDT_EVTYPE_TPDOWN) | (1<<LDT_EVTYPE_REBIND) |
						(1<<LDT_EVTYPE_CONN_ESTAB_FAIL) |
						(1<<LDT_EVTYPE_CONN_ESTAB), tout, 0);
	if (!RERR_ISOK(ret)) {
		if (ret != RERR_TIMEDOUT) {
			SLOGFE (LOG_ERR, "error waiting on connection establishment: %s",
									rerr_getstr3(ret));
			ldt_event_send (	name, LDT_EVTYPE_CONN_ESTAB_FAIL,
										errno);
		}
		ldt_mayclose ();
		return ret;
	}
	if (evinfo.evtype != LDT_EVTYPE_CONN_ESTAB) {
		switch (evinfo.evtype) {
		case LDT_EVTYPE_TUNDOWN:
		case LDT_EVTYPE_IFDOWN:
		case LDT_EVTYPE_TPDOWN:
			ldt_event_send (	name, LDT_EVTYPE_CONN_ESTAB_FAIL,
										ENOTCONN);
			break;
		case LDT_EVTYPE_REBIND:
			ldt_event_send (	name, LDT_EVTYPE_CONN_ESTAB_FAIL,
										ENOTCONN);
			break;
		case LDT_EVTYPE_CONN_ESTAB_FAIL:
			SLOGF (LOG_ERR, "connection failed with error: %s", strerror (evinfo.reason));
			ldt_mayclose ();
			errno = evinfo.reason;
			return RERR_SYSTEM;
		default:
			break;
		}
		ldt_mayclose ();
		return RERR_FAIL;
	}
	ldt_mayclose ();
	return ret;
}

static
int
do_tun_setpeer (name, raddr)
	const char	*name;
	frad_t		*raddr;
{
	char		*msg;
	int		ret, len;
	char		*ptr;
	uint16_t	port;

	if (!name) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_PEER);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_PEER_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	if (raddr) {
		if (!FRADP_ISIPV6(raddr)) {
			ptr = fnl_putattr (	ptr, LDT_CMD_PEER_ATTR_ADDR4,
										&raddr->v4.sin_addr.s_addr, 4);
		} else {
			ptr = fnl_putattr (	ptr, LDT_CMD_PEER_ATTR_ADDR6,
										raddr->v6.sin6_addr.s6_addr, 16);
		}
		if (!ptr) {
			free (msg);
			return RERR_INTERNAL;
		}
		port = frad_getport (raddr);
		ptr = fnl_putattr (ptr, LDT_CMD_PEER_ATTR_PORT, &port, 2);
		if (!ptr) {
			free (msg);
			return RERR_INTERNAL;
		}
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}

int
ldt_tun_setqueue (name, txqlen, qpolicy)
	const char	*name;
	int			txqlen, qpolicy;
{
	char		*msg;
	int		ret, len;
	char		*ptr;
	uint16_t	val;

	if (!name) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + + 24 + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_SET_QUEUE);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_SETQUEUE_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	if (txqlen > 0) {
		val = (uint16_t)txqlen;
		ptr = fnl_putattr (ptr, LDT_CMD_SETQUEUE_ATTR_TXQLEN, &val, 2);
		if (!ptr) {
			free (msg);
			return RERR_INTERNAL;
		}
	}
	if (qpolicy >= 0) {
		val = (uint16_t)qpolicy;
		ptr = fnl_putattr (ptr, LDT_CMD_SETQUEUE_ATTR_QPOLICY, &val, 2);
		if (!ptr) {
			free (msg);
			return RERR_INTERNAL;
		}
	}

	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}

int
ldt_tun_serverstart (name, tout)
	const char	*name;
	tmo_t			tout;
{
	int							ret;
	struct ldt_evinfo	evinfo;

	ldt_event_open ();
	ret = do_tun_serverstart (name);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error starting server: %s", rerr_getstr3(ret));
		ldt_mayclose ();
		return ret;
	}
	if (tout == 0) return RERR_OK;

	bzero (&evinfo, sizeof (evinfo));
	ret = ldt_waitifaceev (&evinfo, name, 
						(1<<LDT_EVTYPE_TUNDOWN) | (1<<LDT_EVTYPE_IFDOWN) |
						(1<<LDT_EVTYPE_TPDOWN) | (1<<LDT_EVTYPE_REBIND) |
						(1LL<<LDT_EVTYPE_CONN_LISTEN_FAIL) |
						(1LL<<LDT_EVTYPE_CONN_LISTEN), tout, 0);
	if (!RERR_ISOK(ret)) {
		if (ret != RERR_TIMEDOUT) {
			SLOGFE (LOG_ERR, "error waiting on connection establishment: %s",
									rerr_getstr3(ret));
			ldt_event_send (	name, LDT_EVTYPE_CONN_LISTEN_FAIL,
										errno);
		}
		ldt_mayclose ();
		return ret;
	}
	if (evinfo.evtype != LDT_EVTYPE_CONN_LISTEN) {
		switch (evinfo.evtype) {
		case LDT_EVTYPE_TUNDOWN:
		case LDT_EVTYPE_IFDOWN:
		case LDT_EVTYPE_TPDOWN:
			ldt_event_send (	name, LDT_EVTYPE_CONN_LISTEN_FAIL,
										ENOTCONN);
			break;
		case LDT_EVTYPE_REBIND:
			ldt_event_send (	name, LDT_EVTYPE_CONN_LISTEN_FAIL,
										ENOTCONN);
			break;
		case LDT_EVTYPE_CONN_LISTEN_FAIL:
			SLOGF (LOG_ERR, "connection failed with error: %s", strerror (evinfo.reason));
			ldt_mayclose ();
			errno = evinfo.reason;
			return RERR_SYSTEM;
		default:
			break;
		}
		ldt_mayclose ();
		return RERR_FAIL;
	}
	ldt_mayclose ();
	return ret;
}

static
int
do_tun_serverstart (name)
	const char	*name;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_SERVERSTART);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_SERVERSTART_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}


int
ldt_set_mtu (name, mtu)
	const char	*name;
	uint32_t		mtu;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name || (int)mtu < 0) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + 14 + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,LDT_CMD_SET_MTU);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_SET_MTU_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	ptr = fnl_putattr (ptr, LDT_CMD_SET_MTU_ATTR_MTU, &mtu, 4);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
}


int
ldt_event_send (name, evtype, reason)
	const char	*name;
	int			evtype;
	int			reason;
{
	char		*msg;
	int		ret, len;
	char		*ptr;
	uint32_t	reason32 = (uint32_t)reason;
	uint32_t	evtype32 = (uint32_t)evtype;

	if (!name) return RERR_PARAM;
	len = FNL_MSGMINLEN + strlen (name) + 18 + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,LDT_CMD_EVSEND);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_EVSEND_ATTR_NAME, name,
								strlen(name)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	ptr = fnl_putattr (ptr, LDT_CMD_EVSEND_ATTR_EVTYPE, &evtype32, 4);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	ptr = fnl_putattr (ptr, LDT_CMD_EVSEND_ATTR_REASON, &reason32, 4);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	ldt_mayclose ();
	return ret;
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
