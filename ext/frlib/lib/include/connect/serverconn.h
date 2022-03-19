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

#ifndef _R__FRLIB_LIB_CONNECT_SERVERCONN_H
#define _R__FRLIB_LIB_CONNECT_SERVERCONN_H

#include <stdlib.h>
#include <sys/types.h>
#include <fr/base/tlst.h>
#include <fr/base/tmo.h>
#include <fr/connect/fdlist.h>

#ifdef __cplusplus
extern "C" {
#endif


/* the first arg is the fd, second flags and third the arg passed to scon_add */
typedef int (*scon_accept_t) (int, int, void*);

struct sconfuncs {
	void		*m_arg;
	/* accept (fd, flags, arg) - flags are from connection.h */
	int		(*m_accept) (int, int, void*);
	/* send (fd, buf, len, flags, arg)  - flags are MSG_... */
	ssize_t	(*m_send) (int, const void*, size_t, int, void*);
	/* recv (fd, buf, len, flags, arg)  - flags are MSG_... */
	ssize_t	(*m_recv) (int, void*, size_t, int, void*);
	/* recvmsg (fd, buf, flags, arg) - flags are MSG_..., buf will be malloc'ed */
	/* only one of m_recv or m_recvmsg might be set */
	ssize_t	(*m_recvmsg) (int, void**, int, void*);
	/* close (fd, arg) */
	int		(*m_close) (int, void*);
	/* checkBreak (fd, arg) - should return the number of breaks received */
	/*                        since the last call or a neg. error code */
	int		(*m_checkBreak) (int, void*);
	/* full (fd, arg, isfull)	will be called when sendbuffer is full */
	/*                       	isfull == 1 when buffer becomes larger than maxsnd */
	/*									isfull == 0 when buffer becomes smaller than maxsnd */
	int		(*m_full) (int, void*, int);
	void		*m_fullarg;
};

struct sconfd {
	int					fd, ffd, type;		/* ffd = father fd */
	int					(*fullmsg) (char*, int, int*);
	char					*welcomeMsg;
	struct sconfuncs	funcs;
	int					flags;
	uint32_t				stepping:1,
							step:1,
							isserial:1,
							nodebugprt:1,
							hasmaxrcv:1,
							hasmaxsnd:1,
							disablercv:1,
							disablesnd:1;
	uint32_t				icnt, ocnt, licnt, locnt;
	size_t				maxrcv, maxsnd;
	size_t				rcvsize, sndsize;
};

struct scondata {
	int		fd;
	char		*data;
	int		dlen;
	int		dflags;
	uint32_t	cnt;
};

#define SCON_MAGIC 0x2d07b66c

struct scon {
	struct fdlist	fdlist;
	struct tlst		rcvfraglst;
	struct tlst		rcvmsglst;
	struct tlst		sendlst;
	struct tlst		allfd;
	int				dosplit;
};


#define SCON_SINIT {\
	.fdlist = FDLIST_SINIT,\
	.rcvfraglst = TLST_SINIT_T (struct scondata),\
	.rcvmsglst = TLST_SINIT_T (struct scondata),\
	.sendlst = TLST_SINIT_T (struct scondata),\
	.allfd = TLST_SINIT_T (struct sconfd),\
}
#define SCON_INIT ((struct scon) SCON_SINIT)

#define xSCON_INIT	((struct scon) {\
	.dosplit = 0,\
})

#define SCON_F_CLIENTCONN	0x01000
#define SCON_F_ACCEPT		0x02000	/* set your own accept function */
#define SCON_F_RECVONCLOSE	0x04000	/* you will receive a scondata with dflags 
												 * SCON_DF_CLOSE set and data to NULL
												 * when connection is closed by foreign host
												 */
#define SCON_F_OOB			0x08000	/* check for oob msg */
#define SCON_F_NOCLOSE		0x10000	/* scon_close does not close the fd */
#define SCON_F_ERRNOCLOSE	0x20000	/* won't be closed on error */
#define SCON_F_FUNC        0x40000	/* set your own send/recv functions */
#define SCON_F_NOPRINT		0x80000	/* do not print debug content */
#define SCON_F_SERIAL		0x100000	/* connection is serial line */
#define SCON_F_EVTRIG		0x200000	/* connection is an event trigger fd */
#define SCON_F_NETLINK		0x400000	/* connection is a netlink device */
#define SCON_F_UEVENT		0x800000	/* connection is a uevent netlink */

#define SCON_DF_CLOSE		0x01
#define SCON_DF_OOB			0x02
#define SCON_DF_BREAK		0x04

#define FD_T_SERVER	0
#define FD_T_CLIENT	1
#define FD_T_CONN		2


int scon_termnl (char *data, int dlen, int *termlen);


int scon_send (struct scon*, int fd, const char *data, int dlen);
int scon_sendln (struct scon*, int fd, const char *data, int dlen);
int scon_sendoob (struct scon*, int fd, unsigned char data);
int scon_wait (struct scon*,  tmo_t timeout);
int scon_gettype (struct scon*, int fd);
int scon_getfd (struct sconfd**, struct scon*, int fd);
int scon_add (struct scon*, int fd, int (*fullmsg) (char*, int, int*), char *welcomeMsg, int flags, ...);
			/* the ... is of type scon_accept_t and void* for the arg  if SCON_F_ACCEPT is given */
int scon_new (struct scon*);
int scon_rmfd (struct scon*, int fd);
int scon_close (struct scon*, int fd);
int scon_free (struct scon*);
int scon_freemsg (struct scon*);
int scon_recv (struct scondata*, struct scon*);
int scon_setfullmsg (struct scon*, int fd, int (*fullmsg) (char*, int, int*));
int scon_setstepping (struct scon*, int fd);
int scon_unsetstepping (struct scon*, int fd);
int scon_step (struct scon*, int fd);

int scon_enablesnd (struct scon*, int fd);
int scon_disablesnd (struct scon*, int fd);
int scon_enablercv (struct scon*, int fd);
int scon_disablercv (struct scon*, int fd);
int scon_setmaxrcv (struct scon*, int fd, size_t maxrcv);
int scon_setmaxsnd (struct scon*, int fd, size_t maxsnd, int (*fullfunc)(int, void*, int), void *arg);

#define SCON_SEND(scon,fd,data,dlen)	scon_send (&(scon), (fd), (data), (dlen))
#define SCON_SENDLN(scon,fd,data)		scon_sendln (&(scon), (fd), (data), strlen (data))
#define SCON_SENDOOB(scon,fd,data)		scon_sendoob (&(scon), (fd), (data))
#define SCON_WAIT(scon,timeout)			scon_wait (&(scon),(tmo_t)(timeout))
#define SCON_GETTYPE(scon,fd)				scon_gettype (&(scon), (fd))
#define SCON_GETFD(sconfd,scon,fd)		scon_getfd (&(sconfd), &(scon), (fd))
#define SCON_HASFD(scon,fd)				scon_getfd (NULL, &(scon), (fd))
#define SCON_ADD(scon,fd,fullmsg,welcome,flags)	scon_add (&(scon),(fd),(fullmsg),(welcome),(flags))
#define SCON_ADD1(scon,fd,flags)			scon_add (&(scon),(fd),scon_termnl,NULL,(flags))
#define SCON_ADD2(scon,fd,fullmsg,flags,accept,arg)	scon_add (&(scon),(fd),(fullmsg),NULL,(flags)|SCON_F_ACCEPT,(accept),(arg))
#define SCON_ADD3(scon,fd,funcs,flags)	scon_add (&(scon),(fd),NULL,NULL,(flags)|SCON_F_FUNC,&(funcs))
#define SCON_NEW(scon)						scon_new (&(scon))
#define SCON_RMFD(scon,fd)					scon_rmfd (&(scon),(fd))
#define SCON_CLOSE(scon,fd)				scon_close (&(scon),(fd))
#define SCON_FREE(scon)						scon_free (&(scon))
#define SCON_FREEMSG(scon)					scon_freemsg (&(scon))
#define SCON_RECV(scondata,scon)			scon_recv (&(scondata),&(scon))
#define SCON_SETFULLMSG(scon,fd,fullmsg)	scon_setfullmsg (&(scon),(fd),(fullmsg))
#define SCON_SETSTEPPING(scon,fd)		scon_setstepping (&(scon),(fd))
#define SCON_UNSETSTEPPING(scon,fd)		scon_unsetstepping (&(scon),(fd))
#define SCON_STEP(scon,fd)					scon_step (&(scon),(fd))
#define SCON_SETMAXRCV(scon,fd,maxrcv)	scon_setmaxrcv (&(scon),(fd),(maxrcv))
#define SCON_SETMAXSND(scon,fd,maxsnd,full,arg)	scon_setmaxsnd (&(scon),(fd),(maxsnd),(full),(arg))
#define SCON_ENABLESND(scon,fd)			scon_enablesnd (&(scon),(fd))
#define SCON_DISABLESND(scon,fd)			scon_disablesnd (&(scon),(fd))
#define SCON_ENABLERCV(scon,fd)			scon_enablercv (&(scon),(fd))
#define SCON_DISABLERCV(scon,fd)			scon_disablercv (&(scon),(fd))






#ifdef __cplusplus
}	/* extern "C" */
#endif








#endif	/* _R__FRLIB_LIB_CONNECT_SERVERCONN_H */
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
