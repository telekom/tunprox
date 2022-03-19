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

#ifndef _R__FRLIB_LIB_CONNECT_SERIALCONN_H
#define _R__FRLIB_LIB_CONNECT_SERIALCONN_H

#include <sys/types.h>

#ifdef __cplusplus
extern "C" {
#endif



#define SERCON_F_ECHO			0x01
#define SERCON_F_RTSCTS			0x02
#define SERCON_F_INTBRK			0x04	/* needs TERMCTRL */
#define SERCON_F_TERMCTRL		0x08	/* doesn't work yet */
#define SERCON_F_INTBRKFAKE	0x10
#define SERCON_F_NOBRK			0x20
#define SERCON_F_NOBLOCK		0x40
#define SERCON_M_CHGMASK		0x2f




int sercon_open (const char *dev, int baud, int flags);
int sercon_close (int fd);

int sercon_setflags (int fd, int flags);
int sercon_unsetflags (int fd, int flags);
int sercon_setbaud (int fd, int baud);

int sercon_isserial (int fd);

ssize_t sercon_send (int fd, const char *buf, size_t buflen);
ssize_t sercon_recv (int fd, char *buf, size_t buflen);
int sercon_getbrk (int fd);	/* returns the number of breaks received til
											the last call - works only if SERCON_F_INTBRK
											is not set */
int sercon_sendbrk (int fd);
int sercon_clear (int fd);		/* clears internal state and buffers */


/* for servercon only */
int sercon_closeArg (int fd, void *arg);
ssize_t sercon_sendArg (int fd, const void *buf, size_t buflen, int flags, void *arg);
ssize_t sercon_recvArg (int fd, void *buf, size_t buflen, int flags, void *arg);






#ifdef __cplusplus
}	/* extern "C" */
#endif





#endif	/* _R__FRLIB_LIB_CONNECT_SERIALCONN_H */

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
