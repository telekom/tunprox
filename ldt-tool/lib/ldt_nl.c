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


static int ldt_getinfo (int*, char**, uint32_t*);

static int tpfd = -1;
static int autoopen = 0;
static tmo_t timeout = 5000000LL;

void
ldt_settimeout (tout)
	tmo_t	tout;
{
	timeout = tout;
}

tmo_t
ldt_gettimeout ()
{
	return timeout;
}

int
ldt_open ()
{
	int	ret;

	if (tpfd >= 0) return tpfd;
	ret = tpfd = fnl_gopen2 (LDT_NAME, 0, timeout, /* flags= */ 0);
	if (!RERR_ISOK(ret)) {
		 SLOGFE (LOG_ERR, "error connecting to ldt kernel module: %s",
		 					rerr_getstr3(ret));
		 return ret;
	}
	return RERR_OK;
}

int
ldt_mayclose ()
{
	if (!autoopen) return RERR_OK;
	return ldt_close();
}

int
ldt_close ()
{
	int   ret;
	
	if (tpfd < 0) return RERR_OK;
	ret = fnl_close (tpfd);
	tpfd = -1;
	autoopen = 0;
	return ret;
}

int
ldt_nl_send (msg, len)
	char		*msg;
	size_t	len;
{
	int	ret;

	if (!msg) return RERR_PARAM;
	if (len == 0) return RERR_OK;

	if (tpfd < 0) {
		ret = ldt_open ();
		if (!RERR_ISOK(ret)) return ret;
		autoopen = 1;
	}
	ret = fnl_send (tpfd, msg, len, timeout, 0);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error sending messag >>%s<< to kernel: %s", msg,
						rerr_getstr3 (ret));
	}
	return ret;
}


int
ldt_nl_getret ()
{
	int	ret, rval;

	ret = ldt_getinfo (&rval, NULL, NULL);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error getting answer from kernel: %s",
									rerr_getstr3(ret));
		return ret;
	}
	if (rval != 0) {
		if (rval < 0) rval *= -1;
		SLOGF (LOG_ERR, "kernel returned: %s", strerror (rval));
		errno = rval;
		return RERR_SYSTEM;
	}
	return RERR_OK;
}


int
ldt_nl_getanswer (data, dlen)
	char		**data;
	uint32_t	*dlen;
{
	int	ret, rval;

	if (!data) return RERR_PARAM;
	*data = NULL;
	ret = ldt_getinfo (&rval, data, dlen);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error getting answer from kernel: %s",
									rerr_getstr3(ret));
		return ret;
	}
	if (rval != 0) {
		if (rval < 0) rval *= -1;
		SLOGF (LOG_ERR, "kernel returned: %s", strerror (rval));
		errno = rval;
		return RERR_SYSTEM;
	}
	if (!*data) {
		SLOGF (LOG_ERR, "no data received from kernel");
		return RERR_SERVER;
	}
	return RERR_OK;
}


static
int
ldt_getinfo (rval, data, dlen)
	int		*rval;
	char		**data;
	uint32_t	*dlen;
{
	ssize_t	len, mlen;
	char		*buf=NULL, *ptr, *ptr2, *xptr, *s;
	int		ret, kret;
	int		hasret=0, hasdata=0;

	if (tpfd < 0) return RERR_NOT_AVAILABLE;

	if (data) *data = NULL;
	if (dlen) *dlen = 0;
retry:
	ret = mlen = fnl_recv (tpfd, &buf, timeout, FNL_F_NOHDR);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error receiving data: %s", rerr_getstr3 (ret));
		return ret;
	}
	if (mlen == 0) goto retry;
	if (!buf) return RERR_INTERNAL;
	ret = fnl_getmsgtype (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	if (ret == NLMSG_ERROR) {
		if (fnl_getcmd (buf) < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
			SLOGF (LOG_ERR, "truncated error message");
			free (buf);
			return RERR_SYSTEM;
		} else {
			errno = -((struct nlmsgerr*)NLMSG_DATA(buf))->error;
			if (rval) *rval = errno;
		}
		free (buf);
		return RERR_OK;
	}
	ret = fnl_getcmd (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	if (ret == LDT_CMD_SEND_EVENT) {
		/* ignore for now - should be handeld !!! - TBD */
		free (buf);
		goto retry;
	}
	if (ret != LDT_CMD_SEND_INFO) {
		free (buf);
		return RERR_INVALID_CMD;
	}
	ptr = fnl_getmsgdata (buf, 0);
	while (ptr && ptr-buf < mlen) {
		ret = fnl_getattrid (ptr);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		switch (ret) {
		case LDT_CMD_SEND_INFO_ATTR_RET:
			hasret = 1;
			ptr2 = fnl_getattrdata (ptr);
			if (!ptr2) {
				free (buf);
				return RERR_INTERNAL;
			}
			kret = (int)*(uint32_t*)ptr2;
			if (rval) *rval = kret;
			break;
		case LDT_CMD_SEND_INFO_ATTR_INFO:
			hasdata = 1;
			break;
		}
		ptr = fnl_getnextattr (ptr);
	}
	if (hasdata && data) {
		xptr = buf;
		ptr = fnl_getmsgdata (buf, 0);
		while (ptr && ptr-buf < mlen) {
			ret = fnl_getattrid (ptr);
			if (!RERR_ISOK(ret)) {
				free (buf);
				return ret;
			}
			len = fnl_getattrlen (ptr);
			s = fnl_getattrdata (ptr);
			ptr = fnl_getnextattr (ptr);
			switch (ret) {
			case LDT_CMD_SEND_INFO_ATTR_INFO:
				memmove (xptr, s, len);
				xptr += len;
				break;
			}
		}
		*xptr = 0;
		len = xptr - buf;
		*data = realloc (buf, len+1);
		if (!*data) *data = buf;
		if (dlen) *dlen = len;
	} else {
		free (buf);
	}
	if (!hasret) {
		if (hasdata && rval) *rval = 0;
	}
	return RERR_OK;
}


int
ldt_event_recv (evtype, iarg, sarg, tout)
	int		*evtype;
	uint32_t	*iarg;
	char		**sarg;
	tmo_t		tout;
{
	ssize_t	len, slen=-1;
	char		*buf=NULL, *ptr, *ptr2;
	int		ret;
	tmo_t		start, now;

	if (tpfd < 0) return RERR_NOT_AVAILABLE;

	/* set default values */
	if (tout == -2) tout = timeout;
	if (evtype) *evtype = 0;
	if (iarg) *iarg = 0;
	if (sarg) *sarg = NULL;

	TMO_START(start,tout);
startover:
	/* receive data */
	do {
		ret = len = fnl_recv (tpfd, &buf, TMO_GETTIMEOUT(start,tout,now), FNL_F_NOHDR);
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR, "error receiving data: %s", rerr_getstr3 (ret));
			return ret;
		}
		if (len > 0) break;
	} while (TMO_CHECK(start,tout));
	if (!buf) return RERR_INTERNAL;

	/* do some error checking */
	ret = fnl_getmsgtype (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	if (ret == NLMSG_ERROR) {
		if (fnl_getcmd (buf) < NLMSG_LENGTH(sizeof(struct nlmsgerr))) {
			SLOGF (LOG_ERR, "truncated error message");
		} else {
			errno = -((struct nlmsgerr*)NLMSG_DATA(buf))->error;
		}
		free (buf);
		return RERR_SYSTEM;
	} else if (ret < NLMSG_MIN_TYPE) {
		SLOGF (LOG_NOTICE, "received unhandled controll messag of type %d", ret);
		free (buf);
		goto startover;
	}
	ret = fnl_getcmd (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	if (ret !=  LDT_CMD_SEND_EVENT) {
		SLOGF (LOG_NOTICE, "receive invalid event command %d", ret);
		free (buf);
		goto startover;
	}

	/* extract data */
	ptr = fnl_getmsgdata (buf, 0);
	while (ptr && ptr-buf < len) {
		ret = fnl_getattrid (ptr);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		switch (ret) {
		case LDT_CMD_SEND_EVENT_ATTR_EVTYPE:
			if (!evtype) break;
			ptr2 = fnl_getattrdata (ptr);
			if (!ptr2) {
				free (buf);
				return RERR_INTERNAL;
			}
			*evtype = (int)*(uint32_t*)ptr2;
			break;
		case LDT_CMD_SEND_EVENT_ATTR_IARG:
			if (!iarg) break;
			ptr2 = fnl_getattrdata (ptr);
			if (!ptr2) {
				free (buf);
				return RERR_INTERNAL;
			}
			*iarg = (int)*(uint32_t*)ptr2;
			break;
		case LDT_CMD_SEND_EVENT_ATTR_SARG:
			if (!sarg) break;
			slen = fnl_getattrlen (ptr);
			ptr2 = fnl_getattrdata (ptr);
			if (!ptr2 || slen < 0) {
				free (buf);
				return RERR_INTERNAL;
			}
			*sarg = ptr2;
			break;
		}
		ptr = fnl_getnextattr (ptr);
	}
	if (sarg && slen >= 0) {
		if (slen > 0) memmove (buf, *sarg, slen);
		buf[slen]=0;
		*sarg = realloc (buf, slen+1);
		if (!*sarg) *sarg = buf;
	} else {
		free (buf);
	}

	/* done */
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
