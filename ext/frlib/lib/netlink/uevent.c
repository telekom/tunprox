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
 * Portions created by the Initial Developer are Copyright (C) 2003-2020
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <stddef.h>
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
#include <linux/filter.h>
#include <dirent.h>
#include <assert.h>
#include <signal.h>
#include <sys/types.h>


#include <fr/base.h>
#include "fnl.h"
#include "uevent.h"



#define UDEV_MAGIC 0xcafe1dea
struct udev_header {
	/* udev version text */
	char version[16];
	/*
 	* magic to protect against daemon <-> library message format mismatch
 	* used in the kernel from socket filter rules; needs to be stored in network order
 	*/
	unsigned int magic;
	/* properties buffer */
	unsigned short properties_off;
	unsigned short properties_len;
	/*
 	* hashes of some common device properties strings to filter with socket filters in
 	* the client used in the kernel from socket filter rules; needs to be stored in
 	* network order
 	*/
	unsigned int filter_subsystem;
	unsigned int filter_devtype;
};




int
uevent_open (tout, isudev)
	tmo_t	tout;
	int	isudev;
{
	return fnl_open2 (NETLINK_KOBJECT_UEVENT, isudev ? 2 : 1, tout, 0);
}

int
uevent_close (sd)
	int	sd;
{
	return fnl_close (sd);
}

int
uevent_recv (sd, uevent, tout)
	int	sd;
	char	**uevent;
	tmo_t	tout;
{
	int						ret;
	struct ucred			*cred;
	char						*buf = NULL;
	struct udev_header	*udh;
	int						buflen;

	ret = buflen = fnl_recv2 (sd, NULL, &buf, &cred, tout, FNL_F_NOHDR);
	if (!RERR_ISOK(ret)) return ret;
	
	if (!buf) {
		if (cred) free (cred);
		return RERR_SYSTEM;
	}

	if (!cred) {
		FRLOGF (LOG_ERR2, "no sender credentials receive, message ignored");
		free (buf);
		return RERR_SYSTEM;
	}

	if (cred->uid != 0) {
		FRLOGF (LOG_ERR2, "sender uid=%u, message ignored\n", cred->uid);
		free (cred);
		free (buf);
		return RERR_SYSTEM;
	}
	free (cred);

	if (!strncmp(buf, "libudev", 7)) {
		/* udev message needs proper version magic */
		udh = (struct udev_header *) buf;
		if (udh->magic != htonl(UDEV_MAGIC)) {
			FRLOGF (LOG_ERR, "invalid udev message");
			free (buf);
			return RERR_INVALID_FORMAT;
		}
		if (udh->properties_off < sizeof(struct udev_header)) {
			FRLOGF (LOG_ERR, "invalid udev message length");
			free (buf);
			return RERR_INVALID_FORMAT;
		}
		if ((ssize_t)udh->properties_off+32 > (ssize_t)buflen) {
			FRLOGF (LOG_ERR, "invalid udev message length");
			free (buf);
			return RERR_INVALID_FORMAT;
		}
	} else {
		if (buflen < 32) {
			FRLOGF (LOG_ERR, "invalid message length");
			free (buf);
			return RERR_INVALID_FORMAT;
		}
	}

	if (uevent) {
		*uevent = buf;
	} else {
		free (buf);
	}

	return buflen;
}



ssize_t
uevent_sconrecv (fd, buf, flags, arg)
	int		fd;
	void		**buf;
	int		flags;
	void		*arg;
{
	ssize_t	rlen;
	char		*ubuf;

	if (fd < 0 || !buf) return RERR_PARAM;
	rlen = uevent_recv (fd, &ubuf, 200000LL);
	if (!RERR_ISOK(rlen)) return rlen;
	*buf = ubuf;
	return rlen;
}

#if 0
int
uevent_trigterm (data, dlen, tlen)
   char  *data;
   int   dlen, *tlen;
{
	if (!data) return 0;
	if (tlen) *tlen = 0;
	if (ubuf) return 0;
	return dlen;
}
#endif

ssize_t
uevent_sconsend (fd, buf, len, flags, arg)
	int			fd;
	const void	*buf;
	size_t		len;
	int			flags;
	void			*arg;
{
	/* it is not possible to send, so this function is a noop, but we
   	need it for SCONS
 	*/
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
