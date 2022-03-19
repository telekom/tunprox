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

#ifndef _R__FRLIB_LIB_NETLINK_FNL_H
#define _R__FRLIB_LIB_NETLINK_FNL_H

#ifndef __USE_GNU
#define __USE_GNU
#endif
#include <sys/socket.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>

#include <fr/base/tmo.h>

#ifdef __cplusplus
extern "C" {
#endif

struct ucred;

/* flags */
#define FNL_F_NONE		0x00
#define FNL_F_NOHDR		0x01	/* send/recv don't bother about netlink header */
#define FNL_F_SEQCHK		0x02	/* recv do check sequence numbers */


/* open/close functions */

int fnl_open (int nlid, tmo_t timeout, int flags);
int fnl_open2 (int nlid, int groups, tmo_t timeout, int flags);
int fnl_gopen (const char *name, tmo_t timeout, int flags);
int fnl_gopen2 (const char *name, int groups, tmo_t timeout, int flags);
int fnl_close (int sd);

int fnl_getfamilyid (int sd, const char *name, tmo_t timeout, int flags);
int fnl_getgrpid (int sd, const char *name, tmo_t timeout, int flags);
int fnl_settype (int sd, int type);
int fnl_setprotver (int sd, int ver);




/* send/recv functions */

#define FNL_MSGMINLEN	(NLMSG_HDRLEN + GENL_HDRLEN + NLA_ALIGNTO + \
									NLA_HDRLEN + 1)

int fnl_send (	int sd, char *msg, size_t msglen, tmo_t timeout, int flags);
int fnl_send2 (int sd, char *msg, size_t msglen, struct ucred *creds,
					int group, int sflags, tmo_t timeout, int flags);
int fnl_recv (	int sd, char **msg, tmo_t timeout, int flags);
int fnl_recv2 (int sd, struct sockaddr_nl *nla, char **msg,
					struct ucred **creds, tmo_t timeout, int flags);



/* message info functions */

int fnl_getmsglen (const char *msg);
int fnl_getmsgtype (const char *msg);
int fnl_getcmd (const char *msg);
int fnl_setcmd (char *msg, int cmd);
char *fnl_getmsgdata (const char *msg, int hdrlen);
char *fnl_getprothdr (const char *msg);
char *fnl_jmpprothdr (const char *ptr, int hdrlen);




/* attribute parsing / creating functions */

char *fnl_getnextattr (const char *ptr);
int fnl_getattr (const char *ptr, int *attrid, void **data, int *dlen);
char *fnl_getattrdata (const char *ptr);
int fnl_getattrlen (const char *ptr);
int fnl_getattrid (const char *ptr);

char *fnl_putattr (char *ptr, int attrid, const void *data, int dlen);
char *fnl_putdata (char *ptr, const void *data, int dlen);







#ifdef __cplusplus
}	/* extern "C" */
#endif




#endif	/* _R__FRLIB_LIB_NETLINK_FNL_H */

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
