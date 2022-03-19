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

#include <fr/base.h>
#include <fr/keddi.h>
#include "kieddi.h"
#include "fnl.h"


static int	eddi_fd = -1;
static int	eddi_chan = -1;

static int sendchan (int);



int
kieddi_connect (chan)
	int	chan;
{
	int	ret;

	if (eddi_fd >= 0 && (eddi_chan == chan || chan < 0)) return eddi_fd;
	if (chan < 0) chan = 0;
	if (eddi_fd < 0) {
		ret = eddi_fd = fnl_gopen (EDDI_NL_NAME, -1, /* flags= */ 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error connecting to kernel eddi: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
	ret = sendchan (chan);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error connecting to channel (%d): %s",
					chan, rerr_getstr3(ret));
		return ret;
	}
	eddi_chan = chan;
	FRLOGF (LOG_DEBUG, "connection opened to eddi: %d", eddi_fd);
	return eddi_fd;
}



int
kieddi_getfamid ()
{
	return fnl_getfamilyid (eddi_fd, NULL, -1, 0);
}


int
kieddi_close ()
{
	int	ret;

	if (eddi_fd < 0) return RERR_OK;
	ret = fnl_close (eddi_fd);
	eddi_fd = -1;
	return ret;
}



static
int
sendchan (chan)
	int	chan;
{
	char		msg[FNL_MSGMINLEN+16];
	int		ret, len;
	char		*ptr;
	uint32_t	xchan;

	if (eddi_fd < 0) return RERR_NOT_AVAILABLE;
	if (chan < 0) return RERR_PARAM;

	bzero (msg, sizeof (msg));
	ret = fnl_setcmd (msg, EDDI_NL_CMD_CON_CHAN);
	if (!RERR_ISOK(ret)) return ret;
	ptr = fnl_getmsgdata (msg, 0);
	if (!ptr) return RERR_INTERNAL;
	xchan = chan;
	ptr = fnl_putattr (ptr, EDDI_NL_CON_ATTR_CHAN, &xchan, sizeof (xchan));
	if (!ptr) return RERR_INTERNAL;
	len = ptr - msg;
	ret = fnl_send (eddi_fd, msg, len, -1, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sending request to kernel eddi: %s",
					rerr_getstr3(ret));
		return ret;
	}
	FRLOGF (LOG_DEBUG, "sent %d bytes", ret);
	return RERR_OK;
}



int
kieddi_send (ev)
	const char	*ev;
{
	char		*msg;
	int		ret, len;
	char		*ptr;

	if (eddi_fd < 0) return RERR_NOT_AVAILABLE;
	if (!ev) return RERR_PARAM;

	len = FNL_MSGMINLEN + strlen (ev) + 128;
	msg = malloc (len);
	bzero (msg, len);
	ret = fnl_setcmd (msg, EDDI_NL_CMD_SEND);
	if (!RERR_ISOK(ret)) {
		free (msg);
		return ret;
	}
	ptr = fnl_getmsgdata (msg, 0);
	ptr = fnl_putattr (ptr, EDDI_NL_SEND_ATTR_EVENT, ev, strlen(ev)+1);
	if (!ptr) {
		free (msg);
		return RERR_INTERNAL;
	}
	len = ptr - msg;
	ret = fnl_send (eddi_fd, msg, len, -1, 0);
	free (msg);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sending request to kernel eddi: %s",
					rerr_getstr3(ret));
		return ret;
	}
	FRLOGF (LOG_DEBUG, "sent %d bytes", ret);
	return RERR_OK;
}



int
kieddi_recv (ev)
	char	**ev;
{
	ssize_t	len;
	char		*buf, *ptr;
	int		ret;

	if (!ev) return RERR_PARAM;
	if (eddi_fd < 0) return RERR_NOT_AVAILABLE;

	ret = len = fnl_recv (eddi_fd, &buf, -1, 0);
	if (!RERR_ISOK(len)) {
		FRLOGF (LOG_ERR, "error receiving data: %s", rerr_getstr3 (ret));
		return ret;
	}
	if (!buf) return RERR_INTERNAL;
	ret = fnl_getcmd (buf);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	if (ret != EDDI_NL_CMD_SEND) {
		free (buf);
		return RERR_INVALID_CMD;
	}
	ptr = fnl_getmsgdata (buf, 0);
	while (ptr && ptr-buf < len) {
		ret = fnl_getattrid (ptr);
		if (!RERR_ISOK(ret)) {
			free (buf);
			return ret;
		}
		if (ret == EDDI_NL_SEND_ATTR_EVENT) break;
		ptr = fnl_getnextattr (ptr);
	}
	if (!(ptr && ret == EDDI_NL_SEND_ATTR_EVENT)) {
		free (buf);
		return RERR_NOT_FOUND;
	}
	ptr = fnl_getattrdata (ptr);
	if (!ptr) {
		free (buf);
		return RERR_INTERNAL;
	}
	len = strlen (ptr) + 1;
	memmove (buf, ptr, len);
	ptr = realloc (buf, len);
	if (!ptr) ptr = buf;
	*ev = ptr;
	return len-1;
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
