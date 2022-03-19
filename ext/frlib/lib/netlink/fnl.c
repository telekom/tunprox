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
#define __USE_GNU
#include <sys/socket.h>
#include <sys/un.h>
#include <arpa/inet.h>
#include <linux/netlink.h>
#include <linux/genetlink.h>
#include <linux/filter.h>
#include <dirent.h>
#include <assert.h>


#include <fr/base.h>
#include <fr/connect.h>

#include "fnl.h"


struct fnl_mcastgrps {
	char	*name;
	int	grp;
};


struct fnlsock {
	int			sd;
	int			portid;
	int			sndseq;
	int			rcvseq;
	int			nlid;
	int			type;
	int			version;
	int			famver;
	int			famhdrsz;
	struct tlst	mcastgrps;
};
static struct tlst	socks;
static int				isinit = 0;

static void hfree_mcastgrps (struct tlst*);
static void hfree_fnlsock (struct fnlsock*);


int
fnl_open (nlid, timeout, flags)
	int	nlid, flags;
	tmo_t	timeout;
{
	return fnl_open2 (nlid, 0, timeout, flags);
}


int
fnl_open2 (nlid, groups, timeout, flags)
	int	nlid, groups, flags;
	tmo_t	timeout;
{
	int						sd, ret, on=1;
	struct sockaddr_nl	snl;
	socklen_t				addrlen;
	size_t					size;
	int						portid;
	struct fnlsock			sock, *sockp;

	if (!isinit) {
		ret = TLST_NEW (socks, struct fnlsock);
		if (!RERR_ISOK(ret)) return ret;
		isinit = 1;
	}
	sd = socket(PF_NETLINK, SOCK_DGRAM, nlid);
	if (sd < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error opening netlink device: %s", 
									rerr_getstr3 (ret));
		return ret;
	}
	size = 2*1024*1024;
	setsockopt(sd, SOL_SOCKET, SO_RCVBUFFORCE, &size, sizeof(size));
	
	addrlen = sizeof (struct sockaddr_nl);
	snl.nl_family = AF_NETLINK;
	snl.nl_pad = 0;
	snl.nl_pid = 0;	/* getpid (); */
	snl.nl_groups = groups;
	
	ret = bind(sd, (struct sockaddr *)&snl, addrlen);
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "bind failed: %s", rerr_getstr3 (ret));
		return ret;
	}

	/* get portid */
	ret = getsockname (sd, (struct sockaddr *)&snl, &addrlen);
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "cannot get socket name: %s",
						rerr_getstr3 (ret));
		return ret;
	}
	portid = snl.nl_pid;
	
	/* enable receiving of sender credentials */
	setsockopt(sd, SOL_SOCKET, SO_PASSCRED, &on, sizeof(on));

	sock = (struct fnlsock) {
		.sd = sd, 
		.portid = portid,
		.nlid = nlid,
		.sndseq = 0,
		.rcvseq = 0,
		.version = 1,
		.type = NLMSG_MIN_TYPE-1,
	};
	ret = TLST_SET (socks, sd, sock);
	if (!RERR_ISOK(ret)) {
		close (sd);
		return ret;
	}
	ret = TLST_GETPTR (sockp, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sockp->sd != sd) return RERR_NOT_FOUND;
	TLST_NEW (sockp->mcastgrps, struct fnl_mcastgrps);

	return sd;
}

int
fnl_gopen (name, timeout, flags)
	const char	*name;
	int			flags;
	tmo_t			timeout;
{
	return fnl_gopen2 (name, 0, timeout, flags);
}

int
fnl_gopen2 (name, groups, timeout, flags)
	const char	*name;
	int			flags, groups;
	tmo_t			timeout;
{
	int	ret, sd;

	if (!name || strlen (name) > GENL_NAMSIZ-1) return RERR_PARAM;
	ret = sd = fnl_open2 (NETLINK_GENERIC, groups, timeout, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error opening generic netlink interface: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = fnl_getfamilyid (sd, name, timeout, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting family id from kernel: %s",
					rerr_getstr3(ret));
		fnl_close (sd);
		return ret;
	}
	return sd;
}

int
fnl_close (sd)
	int	sd;
{
	struct fnlsock	*sock;
	int				ret;

	if (sd < 0) return RERR_PARAM;

	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;
	hfree_fnlsock (sock);
	ret = close (sd);
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}

int
fnl_setprotver (sd, ver)
	int	sd, ver;
{
	struct fnlsock	*sock;
	int				ret;

	if (sd < 0) return RERR_PARAM;

	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;
	sock->version = ver;
	return RERR_OK;
}

int
fnl_settype (sd, type)
	int	sd, type;
{
	struct fnlsock	*sock;
	int				ret;

	if (sd < 0) return RERR_PARAM;

	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;
	sock->type = type;
	return RERR_OK;
}

int
fnl_setcmd (msg, cmd)
	char	*msg;
	int	cmd;
{
	struct genlmsghdr	*gnlh;

	if (!msg) return RERR_PARAM;
	gnlh = (struct genlmsghdr*) (msg + NLMSG_HDRLEN);
	gnlh->cmd = cmd;
	return RERR_OK;
}

int
fnl_getcmd (msg)
	const char	*msg;
{
	const struct genlmsghdr	*gnlh;

	if (!msg) return RERR_PARAM;
	gnlh = (const struct genlmsghdr*) (msg + NLMSG_HDRLEN);
	return gnlh->cmd;
}

int
fnl_getmsgtype (msg)
	const char	*msg;
{
	const struct nlmsghdr	*nlh;

	if (!msg) return RERR_PARAM;
	nlh = (const struct nlmsghdr*) msg;
	return nlh->nlmsg_type;
}

int
fnl_getmsglen (msg)
	const char	*msg;
{
	const struct nlmsghdr	*nlh;

	if (!msg) return RERR_PARAM;
	nlh = (const struct nlmsghdr*) msg;
	return nlh->nlmsg_len;
}

char *
fnl_getprothdr (msg)
	const char	*msg;
{
	if (!msg) return NULL;
	return (char*)(void*)msg + NLMSG_HDRLEN + GENL_HDRLEN;
}

char *
fnl_jmpprothdr (ptr, hdrlen)
	const char	*ptr;
	int			hdrlen;
{
	if (!ptr) return NULL;
	return (char*)(void*)ptr + NLA_ALIGN (hdrlen);
}

char *
fnl_getmsgdata (msg, hdrlen)
	const char	*msg;
	int			hdrlen;
{
	return fnl_jmpprothdr (fnl_getprothdr (msg), hdrlen);
}

int
fnl_send (sd, msg, msglen, timeout, flags)
	int				sd, flags;
	char				*msg;
	size_t			msglen;
	tmo_t				timeout;
{
	return fnl_send2 (sd, msg, msglen, NULL, 0, 0, timeout, flags);
}

int
fnl_send2 (sd, msg, msglen, creds, group, sflags, timeout, flags)
	int				sd, sflags, flags, group;
	char				*msg;
	size_t			msglen;
	struct ucred	*creds;
	tmo_t				timeout;
{
	struct iovec			iov = {
		.iov_base = (void *) msg,
		.iov_len = msglen,
	};
	struct sockaddr_nl	peer = {
		.nl_family = AF_NETLINK,
		.nl_pid = 0,
		.nl_groups = group,
	};
	struct msghdr			hdr = {
		.msg_name = (void *) &peer,
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov = &iov,
		.msg_iovlen = 1,
	};
	char						buf[CMSG_SPACE(sizeof(struct ucred))];
	struct cmsghdr			*cmsg;
	int						ret;
	struct nlmsghdr		*nlh;
	struct genlmsghdr		*gnlh;
	struct fnlsock			*sock;

	if (sd < 0 || !msg) return RERR_PARAM;

	if (msglen < NLMSG_HDRLEN) return RERR_INVALID_LEN;

	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;
	if (!(flags & FNL_F_NOHDR)) {
		nlh = (struct nlmsghdr*) msg;
		bzero (nlh, NLMSG_HDRLEN);
		nlh->nlmsg_len = msglen;
		nlh->nlmsg_type = sock->type;
		nlh->nlmsg_pid = sock->portid;
		nlh->nlmsg_seq = sock->sndseq;
		nlh->nlmsg_flags = sflags | NLM_F_REQUEST;
		if (sock->nlid == NETLINK_GENERIC) {
			if (msglen < NLMSG_HDRLEN + GENL_HDRLEN) return RERR_INVALID_LEN;
			gnlh = (struct genlmsghdr*) (msg + NLMSG_HDRLEN);
			gnlh->version = sock->version;
		}
	}

	if (creds) {
		hdr.msg_control = buf;
		hdr.msg_controllen = sizeof(buf);
		cmsg = CMSG_FIRSTHDR(&hdr);
		cmsg->cmsg_level = SOL_SOCKET;
		cmsg->cmsg_type = SCM_CREDENTIALS;
		cmsg->cmsg_len = CMSG_LEN(sizeof(struct ucred));
		memcpy(CMSG_DATA(cmsg), creds, sizeof(struct ucred));
	}

	ret = sendmsg(sd, &hdr, 0);
	if (ret < 0) {
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error sending message: %s", rerr_getstr3(ret));
		return ret;
	}
	sock->sndseq++;

	return ret;
}

int
fnl_recv (sd, msg, timeout, flags)
	int	sd, flags;
	char	**msg;
	tmo_t	timeout;
{
	return fnl_recv2 (sd, NULL, msg, NULL, timeout, flags);
}

int
fnl_recv2 (sd, nla, msg, creds, timeout, flags)
	int						sd, flags;
	struct sockaddr_nl	*nla;
	char						**msg;
	tmo_t						timeout;
	struct ucred			**creds;
{
	ssize_t					n;
	int						xflags = MSG_PEEK;
	struct iovec			iov = {
		.iov_len = 4096,
	};
	struct sockaddr_nl	xnla;
	struct msghdr			vmsg = {
		.msg_name = (void *) (nla ? nla : &xnla),
		.msg_namelen = sizeof(struct sockaddr_nl),
		.msg_iov = &iov,
		.msg_iovlen = 1,
		.msg_control = NULL,
		.msg_controllen = 0,
		.msg_flags = 0,
	};
	struct cmsghdr			*cmsg;
	struct nlmsghdr		*nlh;
	struct fnlsock			*sock;
	int						ret;
	tmo_t						tout, start=-1, now;

	if (!msg) return RERR_PARAM;
	if (nla) memset(nla, 0, sizeof (struct sockaddr_nl));

	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;

	iov.iov_base = *msg = malloc(iov.iov_len);

	vmsg.msg_controllen = CMSG_SPACE(sizeof(struct ucred));
	vmsg.msg_control = malloc(vmsg.msg_controllen);
	if (!vmsg.msg_control) {
		free (iov.iov_base);
		return RERR_NOMEM;
	}
	memset (vmsg.msg_control, 0, vmsg.msg_controllen);
retry:

	if (timeout < 0) {
		tout = -1;
	} else if (start == -1) {
		tout = timeout;
		start = tmo_now();
	} else {
		now = tmo_now();
		tout = timeout - (now - start);
		if (tout < 0) return RERR_TIMEDOUT;
	}
	ret = fd_isready (sd, tout);
	if (!RERR_ISOK(ret)) {
		free (vmsg.msg_control);
		free (*msg);
		return ret;
	}
	n = recvmsg (sd, &vmsg, xflags);
	if (!n) {
		free (vmsg.msg_control);
		//free (*msg);
		return 0;
	} else if (n < 0) {
		if (errno == EINTR) {
			goto retry;
		} else if (errno == EAGAIN) {
			free (vmsg.msg_control);
			free (*msg);
			return RERR_BUSY;
		} else {
			free (vmsg.msg_control);
			free (*msg);
			return RERR_SYSTEM;
		}
	}

	if ((ssize_t)iov.iov_len < n || (vmsg.msg_flags & MSG_TRUNC)) {
		iov.iov_len *= 2;
		iov.iov_base = *msg = realloc(*msg, iov.iov_len);
		goto retry;
	} else if (vmsg.msg_flags & MSG_CTRUNC) {
		vmsg.msg_controllen *= 2;
		vmsg.msg_control = realloc(vmsg.msg_control, vmsg.msg_controllen);
		goto retry;
	} else if (xflags != 0) {
		xflags = 0;
		goto retry;
	}

	if (vmsg.msg_namelen != sizeof(struct sockaddr_nl)) {
		free (vmsg.msg_control);
		free (*msg);
		return RERR_SERVER;
	}

	if (n < NLMSG_HDRLEN) {
		free (vmsg.msg_control);
		free (*msg);
		return RERR_INVALID_LEN;
	}
	nlh = (struct nlmsghdr*)*msg;
	if (!(flags & FNL_F_NOHDR)) {
		if (((nlh->nlmsg_type == NLMSG_NOOP) || (nlh->nlmsg_type == NLMSG_ERROR) 
					|| (nlh->nlmsg_type == NLMSG_DONE) 
					|| (nlh->nlmsg_type == NLMSG_OVERRUN))) {
			free (vmsg.msg_control);
			free (*msg);
			return RERR_SERVER;
		}
		if ((ssize_t)nlh->nlmsg_seq != (ssize_t)sock->rcvseq) {
			FRLOGF (LOG_WARN, "received message with wrong sequence number: "
								"%u - expected: %d",
								nlh->nlmsg_seq, sock->rcvseq);
			sock->rcvseq = nlh->nlmsg_seq + 1;
			if (flags & FNL_F_SEQCHK) {
				free (vmsg.msg_control);
				free (*msg);
				return RERR_SERVER;
			}
		} else {
			sock->rcvseq++;
		}
		if (nlh->nlmsg_len != n) {
			FRLOGF (LOG_ERR, "received invalid message length");
			return RERR_INVALID_LEN;
		}
	}

	for (cmsg = CMSG_FIRSTHDR(&vmsg); cmsg; cmsg = CMSG_NXTHDR(&vmsg, cmsg)) {
		if (cmsg->cmsg_level == SOL_SOCKET &&
					cmsg->cmsg_type == SCM_CREDENTIALS) {
			if (creds) {
				*creds = calloc(1, sizeof(struct ucred));
				memcpy(*creds, CMSG_DATA(cmsg), sizeof(struct ucred));
			}
			break;
		}
	}

	free (vmsg.msg_control);
	return n;
}


char *
fnl_putdata (ptr, data, dlen)
	char			*ptr;
	const void	*data;
	int			dlen;
{
	if (!ptr || !data || dlen <= 0) return ptr;
	memcpy (ptr, data, dlen);
	memset (ptr+dlen, 0, NLA_ALIGN(dlen)-dlen);
	return ptr + NLA_ALIGN(dlen);
}

char *
fnl_putattr (ptr, attrid, data, dlen)
	char			*ptr;
	const void	*data;
	int			attrid, dlen;
{
	struct nlattr	attr = {
		.nla_len = dlen + NLA_HDRLEN,
		.nla_type = attrid,
	};

	if (!ptr || attrid < 0 || attrid > (1 << 8*sizeof(attr.nla_type)) - 1) {
		return NULL;
	}
	if (dlen < 0 || dlen + NLA_HDRLEN > (1 << 8*sizeof(attr.nla_len)) - 1) {
		return NULL;
	}
	ptr = fnl_putdata (ptr, &attr, sizeof (attr));
	return fnl_putdata (ptr, data, dlen);
}

int
fnl_getattr (ptr, attrid, data, dlen)
	const char	*ptr;
	void			**data;
	int			*attrid, *dlen;
{
	struct nlattr	*attr;

	if (!ptr) return RERR_PARAM;
	attr = (struct nlattr*) ptr;
	if (attrid) *attrid = attr->nla_type;
	if (dlen) *dlen = attr->nla_len;
	if (data) *data = (void*) (ptr + NLA_HDRLEN);
	return NLA_ALIGN((int)(attr->nla_len));
}

int
fnl_getattrid (ptr)
	const char	*ptr;
{
	const struct nlattr	*attr;

	if (!ptr) return RERR_PARAM;
	attr = (const struct nlattr*) ptr;
	return attr->nla_type;
}

int
fnl_getattrlen (ptr)
	const char	*ptr;
{
	const struct nlattr	*attr;

	if (!ptr) return RERR_PARAM;
	attr = (const struct nlattr*) ptr;
	return attr->nla_len - NLA_HDRLEN;
}

char*
fnl_getattrdata (ptr)
	const char	*ptr;
{
	if (!ptr) return NULL;
	return (char*)(void*)ptr + NLA_HDRLEN;
}

char*
fnl_getnextattr (ptr)
	const char	*ptr;
{
	return (char*)ptr + fnl_getattr (ptr, NULL, NULL, NULL);
}


int
fnl_getfamilyid (sd, name, timeout, flags)
	int			sd, flags;
	const char	*name;
	tmo_t			timeout;
{
	struct fnlsock			*sock;
	int						ret, len, alen, fid=RERR_SERVER;	
	char						msg[FNL_MSGMINLEN+GENL_NAMSIZ+16];
	char						*ptr, *answer=NULL;
	char						*mcptr, *mptr, *dptr;
	int						mclen, mlen;
	struct fnl_mcastgrps	mcdata;

	if (sd < 0) return RERR_PARAM;

	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;
	if (sock->nlid != NETLINK_GENERIC) return RERR_INVALID_TYPE;
	if (!name || sock->type > GENL_ID_CTRL) return sock->type;
	if (!name || (strlen (name) > GENL_NAMSIZ-1)) return RERR_PARAM;
	sock->type = GENL_ID_CTRL;
	bzero (msg, sizeof (msg));
	ret = fnl_setcmd (msg, CTRL_CMD_GETFAMILY);
	if (!RERR_ISOK(ret)) return ret;
	ptr = fnl_getmsgdata (msg, 0);
	if (!ptr) return RERR_INTERNAL;
	ptr = fnl_putattr (ptr, CTRL_ATTR_FAMILY_NAME, name, strlen (name)+1);
	len = ptr - msg;
	ret = fnl_send (sd, msg, len, timeout, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sending request to kernel: %s",
					rerr_getstr3(ret));
		return ret;
	}
	while (1) {
		answer = NULL;
		ret = fnl_recv (sd, &answer, timeout, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error receiving answer from kernel: %s",
							rerr_getstr3(ret));
			return ret;
		}
		if (ret > 0) break;
		if (answer) free (answer);
	}
	alen = ret;
	ptr = fnl_getmsgdata (answer, 0);
	if (!ptr) return RERR_INTERNAL;
	sock->type = NLMSG_MIN_TYPE-1;
	while (ptr-answer < alen) {
		ret = fnl_getattrid (ptr);
		if (!RERR_ISOK (ret)) {
			free (answer);
			return ret;
		}
		switch (ret) {
		case CTRL_ATTR_FAMILY_ID:
			dptr = fnl_getattrdata (ptr);
			fid = (int) (int32_t)(uint32_t)*(uint16_t*)(void*)dptr;
			sock->type = fid;
			break;
		case CTRL_ATTR_VERSION:
			dptr = fnl_getattrdata (ptr);
			sock->famver = (int) (int32_t) *(uint32_t*)(void*)dptr;
			break;
		case CTRL_ATTR_HDRSIZE:
			dptr = fnl_getattrdata (ptr);
			sock->famhdrsz = (int) (int32_t) *(uint32_t*)(void*)dptr;
			break;
		case CTRL_ATTR_MCAST_GROUPS:
			mclen = fnl_getattrlen (ptr) + (ptr-answer);
			mcptr = fnl_getattrdata (ptr);
			if (mclen > alen) {
				free (answer);
				return RERR_INVALID_LEN;
			}
			hfree_mcastgrps (&(sock->mcastgrps));
			while (mcptr-answer < mclen) {
				mlen = fnl_getattrlen (mcptr) + (mcptr-answer);
				mptr = fnl_getattrdata (mcptr);
				mcdata = (struct fnl_mcastgrps) { .grp = -1, };
				while (mptr-answer < mlen) {
					ret = fnl_getattrid (mptr);
					switch (ret) {
					case CTRL_ATTR_MCAST_GRP_ID:
						dptr = fnl_getattrdata (mptr);
						mcdata.grp = (int) (int32_t) *(uint32_t*)(void*)dptr;
						break;
					case CTRL_ATTR_MCAST_GRP_NAME:
						dptr = fnl_getattrdata (mptr);
						mcdata.name = strdup (dptr);
						break;
					};
					mptr = fnl_getnextattr (mptr);
					if (mcdata.grp >= 0 && mcdata.name) {
						ret = TLST_INSERT (sock->mcastgrps, mcdata, tlst_cmpstr);
						if (!RERR_ISOK(ret)) {
							free (mcdata.name);
							return ret;
						}
					}
				}
				mcptr = fnl_getnextattr (mcptr);
			}
			break;
		};
		ptr = fnl_getnextattr (ptr);
	}
	if (answer) free (answer);

	return fid;
}

int
fnl_getgrpid (sd, name, timeout, flags)
	int			sd, flags;
	const char	*name;
	tmo_t			timeout;
{
	int						ret;
	struct fnlsock			*sock;
	struct fnl_mcastgrps	*mcgrp;

	if (!name || sd < 0) return RERR_PARAM;
	ret = TLST_GETPTR (sock, socks, sd);
	if (!RERR_ISOK(ret)) return ret;
	if (sock->sd != sd) return RERR_NOT_FOUND;
	if (sock->type < NLMSG_MIN_TYPE) return RERR_NOT_FOUND;
	ret = TLST_SEARCH (sock->mcastgrps, name, tlst_cmpstr);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (mcgrp, sock->mcastgrps, ret);
	if (!RERR_ISOK(ret)) return ret;
	if (!mcgrp) return RERR_INTERNAL;
	return mcgrp->grp;
}



static
void
hfree_mcastgrps (mcastgrps)
	struct tlst		*mcastgrps;
{
	unsigned					i;
	struct fnl_mcastgrps	*mgrp;

	if (!mcastgrps) return;
	TLST_FOREACHPTR2 (mgrp, *mcastgrps, i) {
		if (!mgrp || !mgrp->name) continue;
		free (mgrp->name);
		mgrp->name = NULL;
	}
	TLST_FREE (*mcastgrps);
	TLST_NEW (*mcastgrps, struct fnl_mcastgrps);
}

static
void
hfree_fnlsock (sock)
	struct fnlsock	*sock;
{
	if (!sock) return;
	hfree_mcastgrps (&(sock->mcastgrps));
	bzero (sock, sizeof (struct fnlsock));
	sock->sd = -1;
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
