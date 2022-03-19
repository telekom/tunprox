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

#include <netlink/netlink.h>
#include <netlink/attr.h>
#include <netlink/msg.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/family.h>
#include <netlink/genl/ctrl.h>

#include <fr/base.h>
#include <eddi/keddi.h>
#include "kieddi.h"

static int 					eddi_nl_id = -1;
static struct nl_sock	* eddi_sk = NULL;
static int					eddi_fd = -1;
static int					eddi_chan = -1;

static int openconn ();
static int sendchan (int);



int
kieddi_connect (chan)
	int	chan;
{
	int	ret;

	if (eddi_fd >= 0 && (eddi_chan == chan || chan < 0)) return eddi_fd;
	if (chan < 0) chan = 0;
	ret = openconn ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error connecting to kernel eddi: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = kieddi_getfamid ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting eddi netlink family id: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = sendchan (chan);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error connecting to channel (%d): %s",
					rerr_getstr3(ret));
		return ret;
	}
	eddi_chan = chan;
	FRLOGF (LOG_DEBUG, "connection opened to eddi (family %d): %d",
					eddi_nl_id, eddi_fd);
	return eddi_fd;
}



int
kieddi_getfamid ()
{
	int	famid, ret;

	if (eddi_nl_id > 0) return eddi_nl_id;
	if (!eddi_sk) {
		ret = openconn ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error connecting to kernel eddi: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
	ret = famid = genl_ctrl_resolve (eddi_sk, "eddi");
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error resolving netlink name to id: %s (%d)",
					rerr_getstr3(RERR_CONNECTION), ret);
		return RERR_CONNECTION;
	}
	eddi_nl_id = famid;
	return famid;
}


int
kieddi_close ()
{
	if (!eddi_sk) return RERR_OK;
	nl_close (eddi_sk);
	nl_socket_free (eddi_sk);
	eddi_sk = NULL;
	eddi_fd = -1;
	return RERR_OK;
}


static
int
openconn ()
{
	struct nl_sock	*sk;
	int				ret, fd;

	if (!eddi_sk) {
		sk = nl_socket_alloc ();
		if (!sk) return RERR_NOMEM;
		ret = genl_connect (sk);
		if (ret < 0) {
			nl_socket_free (sk);
			FRLOGF (LOG_ERR, "error connecting to generic netlink controller: %s (%d)",
						rerr_getstr3(RERR_CONNECTION), ret);
			return RERR_CONNECTION;
		}
		nl_socket_disable_seq_check (sk);
		nl_socket_enable_msg_peek (sk);
		nl_socket_set_nonblocking (sk);
		eddi_sk = sk;
	}
	if (eddi_fd < 0) {
		ret = fd = nl_socket_get_fd (sk);
		if (ret < 0) {
			nl_close (sk);
			nl_socket_free (sk);
			return RERR_NOT_AVAILABLE;
		}
		eddi_fd = fd;
	}
	return fd;
}




static
int
sendchan (chan)
	int	chan;
{
	struct nl_msg	*msg;
	int				ret;

	if (chan < 0) return RERR_PARAM;
	if (!eddi_sk) return RERR_NOT_AVAILABLE;
	msg = nlmsg_alloc();
	if (!msg) return RERR_NOMEM;
	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, eddi_nl_id, 0, 0,
							EDDI_NL_CMD_CON_CHAN, 1)) {
		nlmsg_free (msg);
		return RERR_SYSTEM;
	}
	NLA_PUT_U32 (msg, EDDI_NL_CON_ATTR_CHAN, (uint32_t)chan);
	ret = nl_send_auto (eddi_sk, msg);
	nlmsg_free (msg);
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error sending message: %s (%d)",
						rerr_getstr3 (RERR_CONNECTION), ret);
		return RERR_CONNECTION;
	}
	FRLOGF (LOG_DEBUG, "sent %d bytes", ret);
	return RERR_OK;
nla_put_failure:
	nlmsg_free (msg);
	FRLOGF (LOG_ERR, "error putting data to message: %s",
					rerr_getstr3 (RERR_CONNECTION));
	return RERR_CONNECTION;
}



int
kieddi_send (ev)
	const char	*ev;
{
	struct nl_msg	*msg;
	int				ret;
	size_t			len;

	if (!ev) return RERR_PARAM;
	if (!eddi_sk) return RERR_NOT_AVAILABLE;
	len = strlen (ev);
	msg = nlmsg_alloc_size (len+1024);
	if (!msg) return RERR_NOMEM;
	if (!genlmsg_put(msg, NL_AUTO_PORT, NL_AUTO_SEQ, eddi_nl_id, 0, 0,
							EDDI_NL_CMD_SEND, 1)) {
		nlmsg_free (msg);
		return RERR_SYSTEM;
	}
	NLA_PUT_STRING (msg, EDDI_NL_SEND_ATTR_EVENT, ev);
	ret = nl_send_auto (eddi_sk, msg);
	nlmsg_free (msg);
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error sending message: %s (%d)",
						rerr_getstr3 (RERR_CONNECTION), ret);
		return RERR_CONNECTION;
	}
	FRLOGF (LOG_DEBUG, "sent %d bytes", ret);
	return RERR_OK;
nla_put_failure:
	nlmsg_free (msg);
	FRLOGF (LOG_ERR, "error putting data to message: %s",
					rerr_getstr3 (RERR_CONNECTION));
	return RERR_CONNECTION;
}


int
kieddi_recv (ev)
	char	**ev;
{
	ssize_t					n;
	struct sockaddr_nl	nla;
	void						*buf;
	struct nlattr			*tb[EDDI_NL_SEND_ATTR_MAX+1];
	int						ret;
	char						*str;
	struct nlmsghdr		*hdr;

	if (!ev) return RERR_PARAM;
	if (!eddi_sk) return RERR_NOT_AVAILABLE;
	n = nl_recv (eddi_sk, &nla, (unsigned char **)&buf, NULL);
	if (n < 0) {
		FRLOGF (LOG_ERR, "error receiving data: %s (%d)",
						rerr_getstr3 (RERR_CONNECTION), (int)n);
		return RERR_CONNECTION;
	}
	if (!nlmsg_ok (buf, n)) {
		free (buf);
		FRLOGF (LOG_ERR, "received invalid message");
		return RERR_CONNECTION;
	}
	hdr = (struct nlmsghdr *) buf;
	if (	hdr->nlmsg_type == NLMSG_DONE ||
			hdr->nlmsg_type == NLMSG_ERROR ||
			hdr->nlmsg_type == NLMSG_NOOP ||
			hdr->nlmsg_type == NLMSG_OVERRUN) {
		free (buf);
		/* skip message */
		return 0;
	}

	ret = genlmsg_parse (buf, 0, tb, EDDI_NL_SEND_ATTR_MAX, NULL);
	if (ret < 0) {
		free (buf);
		FRLOGF (LOG_ERR, "error parsing message: %s (%d)",
						rerr_getstr3 (RERR_CONNECTION), ret);
		return RERR_CONNECTION;
	}
	if (!tb[EDDI_NL_SEND_ATTR_EVENT]) {
		free (buf);
		FRLOGF (LOG_ERR, "received message without event");
		return RERR_CONNECTION;
	}
	str = nla_get_string (tb[EDDI_NL_SEND_ATTR_EVENT]);
	if (!str) {
		free (buf);
		FRLOGF (LOG_ERR, "received message without event");
		return RERR_CONNECTION;
	}
	n = strlen (str) + 1;
	memmove (buf, str, n);
	buf = realloc (buf, n);
	*ev = buf;
	return n-1;
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
