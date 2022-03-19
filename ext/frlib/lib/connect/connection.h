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

#ifndef _R__FRLIB_LIB_CONNECT_CONNECTION_H
#define _R__FRLIB_LIB_CONNECT_CONNECTION_H



#include <sys/types.h>
#include <stdarg.h>
#include <fr/base/tmo.h>
#include <sys/select.h>

#ifdef __cplusplus
extern "C" {
#endif



#define CONN_T_TCP						0x00
#define CONN_T_UDP						0x01
#define CONN_T_UNIX						0x02
#define CONN_T_SERIAL					0x03
#define CONN_T_MASK						0x0f

#define CONN_F_NONE						0x00
#define CONN_F_TWM						0x10
#define CONN_F_SSL						0x20
#define CONN_F_NOBLK						0x40
#define CONN_F_NORDYCHK					0x80

#define CONN_F_BYTEWISE					0x1000


/* long names for backward compatibility */

#define CONN_SOCKTYPE_TCP					CONN_T_TCP
#define CONN_SOCKTYPE_UDP					CONN_T_UDP
#define CONN_SOCKTYPE_UNIX					CONN_T_UNIX
#define CONN_SOCKTYPE_SERIAL				CONN_T_SERIAL
#define CONN_MASK_SOCKTYPE					CONN_T_MASK

#define CONN_FLAG_NONE						CONN_F_NONE
#define CONN_FLAG_THROW_WELCOME_MSG		CONN_F_TWM
#define CONN_FLAG_TWM						CONN_F_TWM
#define CONN_FLAG_SSL						CONN_F_SSL
#define CONN_FLAG_NOBLOCK					CONN_F_NOBLK
#define CONN_FLAG_NOREADYCHECK			CONN_F_NORDYCHK



int conn_recvdirect (int fd, char *msg, int msglen, int flags);
int conn_recvnum (int fd, char *msg, int msglen, int timeout);
int conn_recvnum64 (int fd, char *msg, int msglen, tmo_t timeout);
int conn_recvln (int fd, char **msg, int timeout);
int conn_recvln64 (int fd, char **msg, tmo_t timeout);
int conn_recvln64f (int fd, char **msg, tmo_t timeout, int flags);
int conn_senddirect (int fd, const char *msg, int msglen, int flags);
int conn_sendnum (int fd, const char *msg, int msglen);
int conn_sendln (int fd, const char *msg);
int conn_sendf (int fd, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int conn_vsendf (int fd, const char *fmt, va_list ap);
int conn_close (int fd);
int conn_free (int fd);
int conn_accept (int fd, const char *welcomeMsg, int flags, int timeout);
int conn_accept64 (int fd, const char *welcomeMsg, int flags, tmo_t timeout);
int conn_open_server (const char *host, int port, int flags, int backlog);
int conn_open_client (const char *host, int port, int flags, int timeout);
int conn_open_client64 (const char *host, int port, int flags, tmo_t timeout);
int conn_recv_oob (int fd, unsigned char *c);
int conn_send_oob (int fd, unsigned char c);

/* the following two functions are only available if compiled with ssl support */

int conn_starttls_client (int fd, int flags);
int conn_starttls_server (int fd, int flags);



int conn_setNonBlocking (int fd);
int conn_setBlocking (int fd);









#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_CONNECT_CONNECTION_H */

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
