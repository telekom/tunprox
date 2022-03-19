/*
 * Copyright (C) 2015-2021 by Frank Reker, Deutsche Telekom AG
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


int
ldt_get_version (version)
	char	**version;
{
	char		*msg;
	int		ret, len;
	char		*ptr;
	char		*info;
	uint32_t	ilen;

	if (!version) return RERR_PARAM;

	len = FNL_MSGMINLEN + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_GET_VERSION);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
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
	ret = ldt_nl_getanswer (&info, &ilen);
	ldt_mayclose ();
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error receiving answer: %s", rerr_getstr3(ret));
		return ret;
	}
	if (!info) return RERR_INTERNAL;
	*version = info;
	return RERR_OK;
}

int
ldt_get_devlist (devlist)
	char	***devlist;
{
	char		*msg;
	int		ret, len, num;
	char		*ptr;
	char		*info, **arg;
	uint32_t	ilen, i, j;

	if (!devlist) return RERR_PARAM;

	len = FNL_MSGMINLEN + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, LDT_CMD_GET_DEVLIST);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
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
	ret = ldt_nl_getanswer (&info, &ilen);
	ldt_mayclose ();
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error receiving answer: %s", rerr_getstr3(ret));
		return ret;
	}
	/* parse answer */
	for (num=2, i=0; i<ilen; i++) if (!info[i]) num++;
	len = num * sizeof (char*);
	ptr = realloc (info, ilen+len+1);
	if (!ptr) {
		free (info);
		return RERR_NOMEM;
	}
	info = ptr + len;
	memmove (info, ptr, ilen);
	info[ilen]=0;
	arg = (char**)(void*)ptr;
	arg[0] = info;
	for (num=1, i=0; i<ilen; i++) {
		if (!info[i]) arg[num++] = info+i+1;
	}
	arg[num] = NULL;
	for (i=j=0; i<num; i++) {
		if (*arg[i]) {
			if (i>j) arg[j] = arg[i];
			j++;
		}
	}
	num = j;
	arg[num] = NULL;
	*devlist = arg;
	return num;
}

int
ldt_get_devinfo (name, info, ilen)
	const char	*name;
	char			**info;
	uint32_t		*ilen;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!name || !info) return RERR_PARAM;

	len = FNL_MSGMINLEN + strlen (name) + 2 + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,	LDT_CMD_SHOW_DEV);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (	ptr, LDT_CMD_SHOW_DEV_ATTR_NAME, name,
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
	ret = ldt_nl_getanswer (info, ilen);
	ldt_mayclose ();
	return ret;
}

int
ldt_get_alldevinfo (info, ilen)
	char		**info;
	uint32_t	*ilen;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (!info) return RERR_PARAM;
	len = FNL_MSGMINLEN + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg,	LDT_CMD_SHOW_DEVLIST);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
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
	ret = ldt_nl_getanswer (info, ilen);
	ldt_mayclose ();
	return ret;
}



static int parsestat (struct ldt_status_t*, char*);
static int tp_parse_status (struct ldt_status_t**, int*, const char *, struct xml*);
int
ldt_get_status (statlist, numstat, iface)
	struct ldt_status_t	**statlist;
	int						*numstat;
	const char				*iface;
{
	char			*info;
	uint32_t		ilen;
	int			ret;
	struct xml	xml;

	if (!statlist || !numstat) return RERR_PARAM;
	*statlist = NULL;
	*numstat = 0;
	ret = ldt_get_alldevinfo (&info, &ilen);
	if (!RERR_ISOK(ret)) return ret;
	if (!info) return RERR_INTERNAL;
	ret = xml_parse (&xml, info, XMLPARSER_FLAGS_STANDARD|XMLPARSER_FLAG_NOTFREE);
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR, "error parsing xml: %s", rerr_getstr3(ret));
		free (info);
		return ret;
	}
	ret = tp_parse_status (statlist, numstat, iface, &xml);
	xml_hfree (&xml);
	free (info);
	if (!RERR_ISOK(ret)) {
		if (*statlist) free (*statlist);
		*statlist = NULL;
		*numstat = 0;
		SLOGFE (LOG_ERR, "error parsing status: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
tp_parse_status (statlist, numstat, iface, xml)
	struct ldt_status_t	**statlist;
	int							*numstat;
	const char					*iface;
	struct xml					*xml;
{
	struct ldt_status_t	*p;
	struct xml_cursor			cursor, tcurs;
	struct xml_tag				*tag;
	int							ret;
	const char					*xif;
	char							*status;
	int							hasiface=0;

	if (!xml) return RERR_PARAM;
	ret = xmlcurs_new (&cursor, xml);
	if (!RERR_ISOK(ret)) return ret;
	ret = xmlcurs_searchtag (&cursor, "dev", 0);
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) {
		SLOGF (LOG_ERR, "no >>dev<< tag found");
		return RERR_NOT_FOUND;
	}
	while (1) {
		tag = xmlcurs_tag (&cursor);
		if (!tag) return RERR_INTERNAL;
		xif = xmltag_getattr (tag, "name");
		if (iface && xif && strcasecmp (iface, xif) != 0) goto next;
		ret = xmlcurs_newtag (&tcurs, tag);
		if (!RERR_ISOK(ret)) return ret;
		hasiface = 1;
		ret = xmltag_search (&status, tag, "status", 0);
		if (!RERR_ISOK(ret)) status = NULL;
		p = realloc (*statlist, (*numstat+1)*sizeof (struct ldt_status_t));
		if (!p) return RERR_NOMEM;
		*statlist = p;
		p += *numstat;
		(*numstat)++;
		*p = (struct ldt_status_t){};
		strncpy (p->iface, xif, sizeof (p->iface)-1);
		p->iface[sizeof (p->iface)-1] = 0;
		parsestat (p, status);
		ret = xmlcurs_next (&tcurs);
		if (ret == RERR_NOT_FOUND) break;
		if (!RERR_ISOK(ret)) return ret;
next:
		ret = xmlcurs_next (&cursor);
		if (ret == RERR_NOT_FOUND) break;
		if (!RERR_ISOK(ret)) return ret;
	}
	if (!*numstat && iface) {
		p = *statlist = malloc (sizeof (struct ldt_status_t));
		if (!p) return RERR_NOMEM;
		*numstat = 1;
		*p = (struct ldt_status_t) { };
		strncpy (p->iface, xif, sizeof (p->iface)-1);
		p->iface[sizeof (p->iface)-1] = 0;
		p->linkup = 0;
		p->pdevup = 0;
		p->tunup = 0;
		p->ifup = (hasiface) ? 1 : 0;
	}
	return RERR_OK;
}


static
int
parsestat (stat, status)
	struct ldt_status_t	*stat;
	char							*status;
{
	char	*s;

	if (!stat) return RERR_PARAM;
	if (!status) {
		stat->linkup = 1;
		stat->pdevup = 1;
		stat->tunup = 1;
		stat->ifup = 1;
		return RERR_OK;
	}
	stat->linkup = 0;
	stat->pdevup = 0;
	stat->tunup = 1;
	stat->ifup = 1;
	while ((s = top_getfield (&status, ",|", 0))) {
		sswitch (s) {
		sicase ("up")
		sicase ("linkup")
			stat->linkup = 1;
			break;
		sicase ("pdevup")
			stat->pdevup = 1;
			break;
		sicase ("tundown")
			stat->tunup = 0;
			break;
		sicase ("ifdown")
			stat->ifup = 0;
			break;
		} esac;
	}
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
