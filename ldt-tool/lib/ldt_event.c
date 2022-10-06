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


#define TP_PARSE_NEED_IFC	1
static int ldt_ifaceev_parse (char*, struct ldt_evinfo*, int);
static int ldt_ifaceev_parse2 (struct xml*, struct ldt_evinfo*, int);
static int tp_getparseflags (int);

int
ldt_event_open ()
{
	return ldt_subscribe();
}

int
ldt_event_close ()
{
	return ldt_close ();
}

int
ldt_subscribe ()
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	ret = ldt_open ();
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR, "error opening connection to ldt module: %s",
						rerr_getstr3(ret));
		return ret;
	}
	len = FNL_MSGMINLEN + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_SUBSCRIBE);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	len = ptr - msg;
	ret = ldt_nl_send (msg, len);
	free (msg);
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR, "error sending request to ldt kernel module: %s",
					rerr_getstr3(ret));
		return ret;
	}
	SLOGF (LOG_VVERB, "sent %d bytes", ret);
	ret = ldt_nl_getret ();
	return ret;
}


int
ldt_waitev (evstr, evtype, timeout, flags)
	char	**evstr;
	int	evtype, flags;
	tmo_t	timeout;
{
	int	evt, ret;
	tmo_t	start, now;

	if ((unsigned)evtype < sizeof(evtype)*4) {
		return ldt_waitev2 (evstr, NULL, 1<<evtype, timeout, flags);
	}
	TMO_START(start,timeout);
	do {
		ret= ldt_waitev2 (evstr, &evt, 0, TMO_GETTIMEOUT(start,timeout,now), flags);
		if (!RERR_ISOK(ret)) return ret;
		if (evt == evtype) return RERR_OK;
	} while (TMO_CHECK(start,timeout));
	return RERR_TIMEDOUT;
}

int
ldt_waitev2 (evstr, evtype, listen_evtypes, timeout, flags)
	char	**evstr;
	int	*evtype, flags, listen_evtypes;
	tmo_t	timeout;
{
	int			ret;
	int			revtype;
	char			*ev;
	int			errcnt=5;
	tmo_t			lasterr = 0, tout, start, now;

	start = tmo_now ();
	tout = timeout;
	while (1) {	
		ev = NULL;
		ret = ldt_event_recv (&revtype, NULL, &ev, tout);
		if (ret == RERR_TIMEDOUT) return ret;
		now = tmo_now ();
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR2, "error receiving event: %s", rerr_getstr3(ret));
			if (flags & LDT_F_CONTONERR) {
				if (--errcnt <= 0 && now-lasterr < 2000000LL) {
					tout = 5000000LL; 	/* 5 s */
					errcnt=5;
				} else {
					tout = 100000LL;		/* 100 ms */
				}
				if (timeout > 0 && now - start + tout > timeout) tout = timeout - (now - start);
				if (timeout == 0 || tout <= 0) return ret;
				if (tout > 0) tmo_sleep (tout);
				now = lasterr = tmo_now();
				if (timeout > 0) {
					tout = timeout - (now - start);
					if (tout < 0) tout = 0;
				} else {
					tout = -1;
				}
				continue;
			}
			return ret;
		}
		SLOGF (LOG_VERB, "received event with type %d (waiting for 0x%x)",
								revtype, listen_evtypes);
		if (listen_evtypes == 0) break;
		if ((1<<revtype) & listen_evtypes) break;
		if (ev) free (ev);
		if (timeout <= 0) {
			tout = timeout;
		} else {
			tout = timeout - (now - start);
			if (tout < 0) tout = 0;
		}
	}
	if (evtype) *evtype = revtype;
	if (evstr) {
		*evstr = ev;
	} else if (ev) {
		free (ev);
	}
	return RERR_OK;
}

int
ldt_waitifaceev (evinfo, listen_iface, listen_evtypes,
							timeout, flags)
	struct ldt_evinfo	*evinfo;
	const char			*listen_iface;
	int					flags;
	uint64_t				listen_evtypes;
	tmo_t					timeout;
{
	int					ret;
	char					*ev;
	int					needflags;
	tmo_t					now, start;
	struct ldt_evinfo	_evinfo;
	int					out_evtype;

	if (!evinfo) evinfo = &_evinfo;
	TMO_START(start,timeout);
	while (1) {
		ev = NULL;
		ret = ldt_waitev2 (	&ev, &out_evtype, listen_evtypes,
										TMO_GETTIMEOUT(start,timeout,now), flags);
		if (!RERR_ISOK(ret)) return ret;
		needflags = tp_getparseflags (out_evtype);
		ret = ldt_ifaceev_parse (ev, evinfo, needflags);
		if (!RERR_ISOK(ret)) {
			SLOGFE (LOG_ERR, "error parsing event: %s", rerr_getstr3(ret));
			if (ev) free (ev);
			return ret;
		}
		if (!((needflags & TP_PARSE_NEED_IFC) && listen_iface && 
					strcasecmp (evinfo->iface, listen_iface) != 0)) {
			break;
		}
		if (ev) free (ev);
	}
	evinfo->evtype = out_evtype;
	if (ev && evinfo == &_evinfo) free (ev);
	return RERR_OK;
}


static
int
ldt_ifaceev_parse (buf, evinfo, flags)
	char							*buf;
	struct ldt_evinfo	*evinfo;
	int							flags;
{
	struct xml	xml;
	int			ret;

	if (!buf) return RERR_PARAM;
	ret = xml_parse (&xml, buf, XMLPARSER_FLAGS_STANDARD|XMLPARSER_FLAG_NOTFREE\
											|XMLPARSER_FLAG_NOTCOPY);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error parsing xml: %s", rerr_getstr3(ret));
		return ret;
	}
	if (evinfo) *evinfo = (struct ldt_evinfo){.buf = buf};
	ret = ldt_ifaceev_parse2 (&xml, evinfo, flags);
	xml_hfree (&xml);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error printing xml: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;

}


static
int
ldt_ifaceev_parse2 (xml, evinfo, flags)
	struct xml			*xml;
	struct ldt_evinfo	*evinfo;
	int					flags;
{
	int	ret;
	char	*s;

	if (!xml) return RERR_PARAM;
	ret = xml_search (&s, xml, "iface", 0);
	if (!RERR_ISOK(ret) || !s) {
		if (flags & TP_PARSE_NEED_IFC) {
			SLOGFE (LOG_ERR2, "event without iface info: %s", rerr_getstr3(ret));
			return ret;
		} else {
			s = NULL;
		}
	}
	if (evinfo) evinfo->iface = (const char*)s;
	if (!evinfo) return RERR_OK;
	ret = xml_search (&s, xml, "pdev", 0);
	if (RERR_ISOK(ret) && s) {
		evinfo->pdev = (const char*)s;
	}
	ret = xml_search (&s, xml, "desc", 0);
	if (RERR_ISOK(ret) && s) {
		evinfo->desc = (const char*)s;
	}
	ret = xml_search (&s, xml, "remaddr", 0);
	if (RERR_ISOK(ret) && s) {
		evinfo->remaddr = (const char*)s;
	}
	ret = xml_search (&s, xml, "remport", 0);
	if (RERR_ISOK(ret) && s) {
		evinfo->remport = atoi (s);
	} else {
		evinfo->remport = -1;
	}
	evinfo->reason = 0;
	evinfo->s_reason = "none";
	ret = xml_search (&s, xml, "subflow", 0);
	if (RERR_ISOK(ret)) {
		evinfo->subflow = (const char*)s;
	}
	return RERR_OK;
}

struct needflags { int evt, f; };
static
int
tp_getparseflags (evtype)
	int	evtype;
{
	static const struct needflags needflags[] = {
		{ LDT_EVTYPE_UNSPEC, 0},
#if 0
		{ LDT_EVTYPE_DOWN, TP_PARSE_NEED_IFC},
		{ LDT_EVTYPE_UP, TP_PARSE_NEED_IFC},
#endif
		{ LDT_EVTYPE_IFDOWN, TP_PARSE_NEED_IFC },
		{ LDT_EVTYPE_IFUP, TP_PARSE_NEED_IFC },
		{ LDT_EVTYPE_TPDOWN, 0 },
		{ LDT_EVTYPE_REBIND, TP_PARSE_NEED_IFC},
		{ LDT_EVTYPE_SUBFLOW_UP, TP_PARSE_NEED_IFC},
		{ LDT_EVTYPE_SUBFLOW_DOWN, TP_PARSE_NEED_IFC},
		{ -1, -1 }};
	const struct needflags			*p;

	for (p = needflags; p->evt >= 0; p++) if (p->evt == evtype) return p->f;
	return 0;
}

#if LDT_EVTYPE_MAX > 31
# error "LDT_EVTYPE_MAX must not exceed 31"
#endif
static const struct top_flagmap evnamemap[] = {
	{ "unspec", 1<<LDT_EVTYPE_UNSPEC },
#if 0
	{ "down", 1<<LDT_EVTYPE_DOWN },
	{ "linkdown", 1<<LDT_EVTYPE_DOWN },
	{ "up", 1<<LDT_EVTYPE_UP },
	{ "linkup", 1<<LDT_EVTYPE_UP },
#endif
	{ "ifdown", 1<<LDT_EVTYPE_IFDOWN },
	{ "ifup", 1<<LDT_EVTYPE_IFUP },
	{ "ldtdown", 1<<LDT_EVTYPE_TPDOWN },
	{ "rebind", 1<<LDT_EVTYPE_REBIND },
	{ "subflowup", 1<<LDT_EVTYPE_SUBFLOW_UP },
	{ "subflowdown", 1<<LDT_EVTYPE_SUBFLOW_UP },
	{ NULL, -1 }};

int
ldt_getevmap (str)
	const char	*str;
{
	return top_parseflags (str, evnamemap);
}

const char *
ldt_evgetname (evtype)
	int	evtype;
{
	const struct top_flagmap	*p;

	if (evtype < 0 || evtype >= sizeof (int)*8-1) return NULL;
	evtype = 1 << evtype;
	for (p=evnamemap; p->s && p->v != evtype; p++);
	return p->s;
}

static const struct top_flagmap evhelpmap[] = {
	{ "error condition", 1<<LDT_EVTYPE_UNSPEC },
#if 0
	{ "remote host down", 1<<LDT_EVTYPE_DOWN },
	{ "remote host came up", 1<<LDT_EVTYPE_UP },
#endif
	{ "interface brought down", 1<<LDT_EVTYPE_IFDOWN },
	{ "new interface brought up", 1<<LDT_EVTYPE_IFUP },
	{ "ldt module unloaded", 1<<LDT_EVTYPE_TPDOWN },
	{ "address was rebinded", 1<<LDT_EVTYPE_REBIND },
	{ NULL, -1 }};

const char *
ldt_evgetdesc (evtype)
	int	evtype;
{
	const struct top_flagmap	*p;

	if (evtype < 0 || evtype >= sizeof (int)*8) return NULL;
	evtype = 1 << evtype;
	for (p=evhelpmap; p->s && p->v != evtype; p++);
	return p->s;
}


int
ldt_evhelp (buf, buflen, plen)
	char	*buf;
	int	buflen, plen;
{
	int	len;

	if (plen < 0) plen = 0;
	len = snprintf (buf, buflen, "%*cThe following events do exist:\n", plen, ' ');
	if (buf) buf+=len;
	buflen = (buflen >= len) ? buflen - len : 0;
	plen += 2;
	return top_prtflaghelp (buf, buflen, plen, evnamemap, evhelpmap);
}

int
ldt_prtevhelp (plen)
	int	plen;
{
	char	_buf[256], *buf;
	int	len;

	*_buf=0;
	len = ldt_evhelp (_buf, sizeof(_buf), plen);
	if (len >= sizeof(_buf)) {
		buf = malloc (len+1);
		if (!buf) return RERR_NOMEM;
		len = ldt_evhelp (buf, len+1, plen);
	} else {
		buf = _buf;
	}
	printf ("%s", buf);
	if (buf != _buf) free (buf);
	return len;
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
