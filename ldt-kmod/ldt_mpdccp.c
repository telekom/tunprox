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


#include <linux/socket.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/netlink.h>
#include <net/sock.h>
#include <net/tcp_states.h>
#include <net/genetlink.h>
#include <linux/skbuff.h>
#include <linux/notifier.h>
#include <linux/errno.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/init.h>
#include <linux/kthread.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/delay.h>
#include <linux/interrupt.h>
#include <linux/etherdevice.h>
#include <linux/crypto.h>
#include <linux/err.h>
#include <linux/scatterlist.h>
#include <linux/udp.h>
#include <linux/ipv6.h>
#include <net/ipv6.h>
#include <net/udp.h>
#ifdef CONFIG_NET_UDP_TUNNEL
# include <net/udp_tunnel.h>
#endif
//#include <net/ip_tunnels.h>
#include <linux/version.h>
#if IS_ENABLED(CONFIG_IP_MPDCCP)
# include <net/mpdccp_link_info.h>
# include <net/mpdccp.h>
#endif
#include <uapi/linux/dccp.h>

#include "ldt_uapi.h"
#include "ldt_dev.h"
#include "ldt_tun.h"
#include "ldt_debug.h"
#include "ldt_event.h"
#include "ldt_addr.h"
#include "ldt_prot1.h"
#include "ldt_ip.h"
#include "ldt_tunaddr.h"
#include "ldt_queue.h"
#include "ldt_lock.h"


#ifdef NET_IP_ALIGN
# define TP_IPALIGN	NET_IP_ALIGN
#else
# define TP_IPALIGN	0
#endif

/* the following is a hack - to be removed later on! */
#ifdef DCCP_SOCKOPT_KEEPALIVE
# ifndef DCCPQ_POLICY_DROP_OLDEST
#   define DCCPQ_POLICY_DROP_OLDEST DCCPQ_POLICY_DROP_OLDEST
# endif
# ifndef DCCPQ_POLICY_DROP_NEWEST
#   define DCCPQ_POLICY_DROP_NEWEST DCCPQ_POLICY_DROP_NEWEST
# endif
#endif

#define TP_IPHDRLEN		(sizeof (struct iphdr)   + TP_IPALIGN)
#define TP_IP6HDRLEN		(sizeof (struct ipv6hdr) + TP_IPALIGN)


#define TP_DCCHDRLEN		(255*sizeof(u32))
#define TP_DCCPOPTLEN	(TP_DCCPHDRLLEN - sizeof(struct dccp_hdr))
#define TP_PROTLEN		(0)
#define TP_BHDRLEN		(TP_DCCHDRLEN + TP_PROTLEN)
#define TP_HDRLEN		(TP_IPHDRLEN  + TP_BHDRLEN)
#define TP_HDR6LEN	(TP_IP6HDRLEN + TP_BHDRLEN)
#define TP_MINHEADROOM	(TP_HDRLEN  + NET_SKB_PAD)
#define TP_MIN6HEADROOM	(TP_HDR6LEN + NET_SKB_PAD)

struct mpdccptun;
static int mpdccptun_new (struct ldt_tun*, const char *);
static int mpdccptun_bind (struct mpdccptun*, tp_addr_t*, int);
static int mpdccptun_dobind (struct mpdccptun*);
static int mpdccptun_peer (struct mpdccptun*, tp_addr_t*);
static void mpdccptun_remove (struct mpdccptun*);
static netdev_tx_t ldt_mpdccptun_xmit (struct mpdccptun*, struct sk_buff*);
static int mpdccptun_elab_xmit2 (struct mpdccptun*, struct sk_buff*);
static int mpdccptun_elab_xmit (struct mpdccptun*);
static int mpdccptun_xmit (struct mpdccptun*, char*, int, tp_addr_t*);
static int mpdccptun_prepare_skb (struct mpdccptun*, struct sk_buff*, int*);
static int mpdccptun_check_enqueue (struct mpdccptun*, struct sk_buff*);
static int mpdccptun_do_enqueue (struct mpdccptun*, struct sk_buff*);
static ssize_t mpdccptun_getinfo (struct mpdccptun*, char*, size_t);
static int mpdccptun_eventcreate (struct mpdccptun*, char*, size_t, const char*, const char*);
static int mpdccptun_elab_recv (struct mpdccptun*, struct sk_buff*);
static void mpdccptun_scrub_skb (struct sk_buff*);
#if IS_ENABLED(CONFIG_IP_MPDCCP)
static void tp_subflow_report (int, struct sock*, struct sock*, struct mpdccp_link_info*, int);
static int tp_subflow_reg (struct sock*);
static int tp_subflow_dereg (struct sock*);
#endif
static int mpdccptun_serverstart (struct mpdccptun*);
static int mpdccptun_doserverstart (struct mpdccptun*);
static int mpdccptun_setqueue (struct mpdccptun*, int, int);
static void _myclose (struct socket*);
static int mpdccptun_elab_accept (struct mpdccptun*);
static void accept_handler (struct work_struct*);
static void listen_handler (struct work_struct*);
static void xmit_handler (struct work_struct*);
static void xmit_handler_delayed (struct work_struct*);
static void do_xmit_handler (struct mpdccptun*);
static int mpdccptun_elab_connect (struct mpdccptun*);
static void connect_handler (struct work_struct*);
#if IS_ENABLED(CONFIG_IP_MPDCCP)
static int mpdccptun_linkdown (struct mpdccptun*);
#endif
static void conn_timer_handler (unsigned long data);

static int do_xmit_skb (struct mpdccptun *, struct sk_buff*);
static int mpdccptun_needheadroom (struct mpdccptun*);
static int mpdccptun_getmtu (void*);
static void mpdccptun_closesk (struct mpdccptun*);
static void mpdccptun_close_listen (struct mpdccptun*);


struct ldt_tunops	mpdccptun_ops = {
	.tp_new = mpdccptun_new,
	.tp_bind = (void*)mpdccptun_bind,
	.tp_peer = (void*)mpdccptun_peer,
	.tp_serverstart = (void*)mpdccptun_serverstart,
	.tp_remove = (void*)mpdccptun_remove,
	.tp_xmit = (void*)ldt_mpdccptun_xmit,
	.tp_gettuninfo = (void*)mpdccptun_getinfo,
	.tp_createvent = (void*)mpdccptun_eventcreate,
	.tp_prot1xmit = (void*)mpdccptun_xmit,
	.tp_needheadroom = (void*)mpdccptun_needheadroom,
	.tp_getmtu = mpdccptun_getmtu,
	.tp_setqueue = (void*)mpdccptun_setqueue,
	.ipv6 = 0,
};

struct ldt_tunops	mpdccptun_ops6 = {
	.tp_new = mpdccptun_new,
	.tp_bind = (void*)mpdccptun_bind,
	.tp_peer = (void*)mpdccptun_peer,
	.tp_serverstart = (void*)mpdccptun_serverstart,
	.tp_remove = (void*)mpdccptun_remove,
	.tp_xmit = (void*)ldt_mpdccptun_xmit,
	.tp_gettuninfo = (void*)mpdccptun_getinfo,
	.tp_createvent = (void*)mpdccptun_eventcreate,
	.tp_prot1xmit = (void*)mpdccptun_xmit,
	.tp_needheadroom = (void*)mpdccptun_needheadroom,
	.tp_getmtu = mpdccptun_getmtu,
	.tp_setqueue = (void*)mpdccptun_setqueue,
	.ipv6 = 1,
};


#define MPDCCPTUN_MAGIC	(0xcaee6c49)
#define ISMPDCCPTUN(tdat) ((tdat) && (tdat)->MAGIC == MPDCCPTUN_MAGIC)


typedef struct { char s[IFNAMSIZ+1]; } subflow_str;

struct mpdccptun {
	u32							MAGIC;
	struct ldt_tun		*tun;
	struct net_device			*ndev;
	const char					*name;
	u32							tostop;
	u32							ipv6:1,
									ismpdccp:1,
									bound:1,
									rebind:1,
									haspeer:1,
									noauthqueue:1,
									listening:1,
									isserver:1,
									isconnected:1,
									wasconnected:1,
									has_delayed_work:1,
									has_subflow_report:1;
	u16							tx_qlen;
	u16							qpolicy;
	unsigned long				last_unconnect;
	subflow_str					*subflow;
	int							num_subflow;
	int							bufsz_subflow;
	subflow_str					subflow_report;
	tp_tunaddr_t				addr;
	struct socket				*sock;
	struct socket				*active;
	struct work_struct		work_conn;
	struct work_struct		work_listen;
	struct work_struct		work_accept;
	struct work_struct		work_xmit;
	struct delayed_work		work_xmit_delayed;
	struct tp_queue			xmit_queue;
	struct timer_list			conn_timer;
	struct tp_lock				lock;
	struct tp_lock				lock2;
};



static int mpdccptun_xmit_skb (struct mpdccptun*, struct sk_buff*);
static int mpdccptun_dorcv_all (struct mpdccptun*, struct sock*);
static int mpdccptun_dorcv (struct mpdccptun*, struct sock*);
static int mpdccptun_dorcv2 (struct mpdccptun*, struct sock*);
static int rcv_prepare_skb (struct sk_buff**, struct sk_buff*);
static int dorcv_datagram (struct sk_buff**, struct sock*, int);
static int mpdccptun_needrcvcpy (struct sk_buff*);
static void tp_elab_data_ready (struct sock *);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)
static void tp_cli_data_ready (struct sock *, int);
static void tp_srv_data_ready (struct sock *, int);
static void tp_listen_ready (struct sock *, int);
#else
static void tp_cli_data_ready (struct sock *);
static void tp_srv_data_ready (struct sock *);
static void tp_listen_ready (struct sock *);
#endif
static void tp_set_tdat (struct socket*, struct mpdccptun*);
static void tp_unset_tdat (struct socket*);
#if IS_ENABLED(CONFIG_IP_MPDCCP)
static int tp_subflow_global_reg (void);
static int tp_subflow_global_dereg (void);
#endif



#define ISSTOP(tdat) (smp_load_acquire(&(tdat->tostop)))
#define CHKSTOP(ret) { if (ISSTOP(tdat)) { return (ret); } }
#define CHKSTOPVOID	{ if (ISSTOP(tdat)) { return; } }
#ifdef DOUSELOCK
#define DOLOCK(tdat) do { tp_lock (&(tdat)->lock); } while (0)
#define DOUNLOCK(tdat) do { tp_unlock (&(tdat)->lock); } while (0)
#else
#define DOLOCK(tdat) do {} while (0)
#define DOUNLOCK(tdat) do {} while (0)
#endif
#define DOLOCK2(tdat) do { tp_lock (&(tdat)->lock2); } while (0)
#define DOUNLOCK2(tdat) do { tp_unlock (&(tdat)->lock2); } while (0)
#define MYCLOSE(tdat,var)	do { \
	struct socket	*_sock; \
	DOLOCK(tdat); \
	_sock = (tdat)->var; \
	(tdat)->var = NULL; \
	DOUNLOCK(tdat); \
	_myclose (_sock); \
} while (0)

static int mpdccptun_getmtu (void *t)
{
	return 0;
}
static int mpdccptun_needheadroom (tdat)
	struct mpdccptun	*tdat;
{
	if (!tdat) return TP_MINHEADROOM > TP_MIN6HEADROOM ? TP_MINHEADROOM : TP_MIN6HEADROOM;
	if (tdat->ipv6) return TP_MIN6HEADROOM;
	return TP_MINHEADROOM;
}

int
ldt_mpdccp_register (void)
{
	int	ret;

#if IS_ENABLED(CONFIG_IP_MPDCCP)
	ret = ldt_tun_register ("mpdccp", &mpdccptun_ops);
	if (ret < 0) return ret;
	ret = ldt_tun_register ("mpdccp4", &mpdccptun_ops);
	if (ret < 0) return ret;
	ret = ldt_tun_register ("mpdccp6", &mpdccptun_ops6);
	if (ret < 0) return ret;
#endif
	ret = ldt_tun_register ("dccp", &mpdccptun_ops);
	if (ret < 0) return ret;
	ret = ldt_tun_register ("dccp4", &mpdccptun_ops);
	if (ret < 0) return ret;
	ret = ldt_tun_register ("dccp6", &mpdccptun_ops6);
	if (ret < 0) return ret;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	ret = tp_subflow_global_reg ();
	if (ret < 0) return ret;
#endif
	return 0;
}

//static
void
//__exit
ldt_mpdccp_unregister (void)
{
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	tp_subflow_global_dereg ();
	ldt_tun_unregister ("mpdccp");
	ldt_tun_unregister ("mpdccp4");
	ldt_tun_unregister ("mpdccp6");
#endif
	ldt_tun_unregister ("dccp");
	ldt_tun_unregister ("dccp4");
	ldt_tun_unregister ("dccp6");
}

static
int
mpdccptun_new (tun, type)
	struct ldt_tun	*tun;
	const char				*type;
{
	int					ipv6, ismp=1;
	struct mpdccptun	*tdat;

	if (!tun || !tun->tdev || !tun->tdev->ndev || !type) return -EINVAL;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	if (!strcasecmp (type, "mpdccp6")) {
		ipv6 = 1;
	} else if (!strcasecmp (type, "mpdccp4") || !strcasecmp (type, "mpdccp")) {
		ipv6 = 0;
	} else 
#endif
	if (!strcasecmp (type, "dccp6")) {
		ipv6 = 1;
		ismp = 0;
	} else if (!strcasecmp (type, "dccp4") || !strcasecmp (type, "dccp")) {
		ipv6 = 0;
		ismp = 0;
	} else {
		return -ENOTSUPP;
	}
	tp_info ("create %s tunnel\n", type);
	tdat = kmalloc (sizeof (struct mpdccptun), GFP_KERNEL);
	if (!tdat) return -ENOMEM;
	*tdat = (struct mpdccptun) {
			.MAGIC = MPDCCPTUN_MAGIC,
			.tun = tun,
			.ndev = tun->tdev->ndev,
			.name = tun->tdev->ndev->name,
			.ipv6 = ipv6,
			.ismpdccp = ismp,
			.tx_qlen = 1000,
#if defined DCCPQ_POLICY_DROP_NEWEST
			.qpolicy = DCCPQ_POLICY_DROP_NEWEST,
#else
			.qpolicy = DCCPQ_POLICY_SIMPLE,
#endif
	};
	ldt_tunaddr_init (&tdat->addr, ipv6);
	tun->tundata = tdat;
	tun->tunops = ipv6 ? &mpdccptun_ops6 : &mpdccptun_ops;
	INIT_WORK (&tdat->work_accept, accept_handler);
	INIT_WORK (&tdat->work_listen, listen_handler);
	INIT_WORK (&tdat->work_conn, connect_handler);
	INIT_WORK (&tdat->work_xmit, xmit_handler);
	INIT_DELAYED_WORK (&tdat->work_xmit_delayed, xmit_handler_delayed);
	tpq_init (&tdat->xmit_queue, TP_QUEUE_DROP_NEWEST, 1000);
	tp_lock_init (&tdat->lock);
	tp_lock_init (&tdat->lock2);
	setup_timer(&tdat->conn_timer, conn_timer_handler, (unsigned long)tdat);
	tp_debug3 ("tdat=%p, tun=%p\n", tdat, tdat->tun);
	return 0;
}


static
int
mpdccptun_bind (tdat, addr, flags)
	struct mpdccptun	*tdat;
	tp_addr_t			*addr;
	int					flags;
{
	int	ret;

	if (!tdat) return -EINVAL;
	if (!addr) return 0;
	if (ISSTOP(tdat)) return 0;
	ret = ldt_tunaddr_bind (&tdat->addr, addr, flags);
	if (ret < 0) {
		tp_err ("error copying address: %d\n", ret);
		return ret;
	}
	//tdat->bind_flags = flags;
	if (tdat->bound) tdat->rebind = 1;
	if (!tdat->isserver && tdat->isconnected) {
		ret = queue_work (system_wq, &tdat->work_conn);
		if (ret < 0) {
			tp_err ("error queuing work: %d\n", ret);
			return ret;
		}
	} else if (tdat->isserver && tdat->listening) {
		ret = queue_work (system_wq, &tdat->work_listen);
		if (ret < 0) {
			tp_err ("error queuing work: %d\n", ret);
			return ret;
		}
	}
	tp_debug ("new address set");
	return 0;
}

static
void
mpdccptun_close_listen (tdat)
	struct mpdccptun	*tdat;
{
	struct socket	*_active = NULL;
	struct socket	*_pending = NULL;
	struct socket	*_sock = NULL;

	if (!tdat) return;
	tp_debug3 ("release old socket");
	DOLOCK (tdat);
	if (tdat->active) {
		tp_unset_tdat (tdat->active);
		_active = tdat->active;
		tdat->active = NULL;
	}
	_sock = tdat->sock;
	tdat->listening = 0;
	tdat->isconnected = 0;
	DOUNLOCK (tdat);
	if (_pending) {
		tp_debug3 ("close pending socket");
		_myclose (_pending);
	}
	if (_active) {
		tp_debug3 ("close active socket");
		_myclose (_active);
	}
	if (_sock) {
		tp_debug3 ("shutdown listen socket");
		kernel_sock_shutdown(tdat->sock, SHUT_RDWR);
	}
}

static
void
mpdccptun_closesk (tdat)
	struct mpdccptun	*tdat;
{
	struct socket	*_active = NULL;
	struct socket	*_sock = NULL;

	if (!tdat) return;
	tp_debug3 ("release old socket");
	DOLOCK (tdat);
	if (tdat->active) {
		tp_unset_tdat (tdat->active);
		_active = tdat->active;
		tdat->active = NULL;
	}
	if (tdat->sock) {
		tp_unset_tdat (tdat->sock);
		_sock = tdat->sock;
		tdat->sock = NULL;
	}
	tdat->bound = 0;
	tdat->rebind = 0;
	tdat->listening = 0;
	tdat->isconnected = 0;
	DOUNLOCK (tdat);
	if (_active) {
		tp_debug3 ("close active socket");
		_myclose (_active);
	}
	if (_sock) {
		tp_debug3 ("close listen/client socket");
		_myclose (_sock);
	}
}

static
int
mpdccptun_dobind (tdat)
	struct mpdccptun	*tdat;
{
	int			ret;
	int			val;
	mm_segment_t old_fs;

	tp_debug3 ("enter");
	if (!tdat) return -EINVAL;
	if (ISSTOP(tdat)) return 0;
	if ((!tdat->addr.bound || tdat->addr.anylport) && tdat->isserver) {
		tp_err ("server cannot bind to anyport\n");
		return -ENOTCONN;
	}
	if (tdat->bound) {
		if (!tdat->rebind) return 0;
		mpdccptun_closesk (tdat);
	}
		
	tp_debug3 ("create socket");
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
	ret = sock_create_kern (TP_ADDR_FAM(tdat->addr.laddr), SOCK_DCCP,
									IPPROTO_DCCP, &tdat->sock);
	// Todo: setting network namespace manually
#else
	ret = sock_create_kern (NDEV2NET(tdat->ndev), TP_ADDR_FAM(tdat->addr.laddr),
									SOCK_DCCP, IPPROTO_DCCP, &tdat->sock);
#endif
	if (ret < 0) {
		tp_err ("error creating socket: %d", ret);
		return ret;
	}
	old_fs = get_fs();
	set_fs(KERNEL_DS);

	if (tp_addr_getport (&tdat->addr.laddr) > 0) {
		val = 1;
		ret = tdat->sock->ops->setsockopt(tdat->sock, SOL_SOCKET, SO_REUSEADDR,
              	(char*)&val, sizeof(val));
		if (ret < 0) {
			tp_warn ("warn: error setting reuseaddr: %d", ret);
		}
	}

#if IS_ENABLED(CONFIG_IP_MPDCCP)
	if (tdat->ismpdccp) {
		tp_debug2 ("switch to multipath");
		val = 1;
		ret = tdat->sock->ops->setsockopt(tdat->sock, SOL_DCCP, DCCP_SOCKOPT_MULTIPATH,
              	(char*)&val, sizeof(val));
		if (ret < 0) {
			tp_err ("error switching to multipath: %d", ret);
			set_fs(old_fs);
			goto dorelease;
		}
	}
#endif
#if defined DCCP_SOCKOPT_QPOLICY_TXQLEN
	tpq_set_maxlen (&tdat->xmit_queue, tdat->tx_qlen);
	val = tdat->tx_qlen;
	ret = tdat->sock->ops->setsockopt(tdat->sock, SOL_DCCP, DCCP_SOCKOPT_QPOLICY_TXQLEN,
              (char*)&val, sizeof(val));
	if (ret < 0) {
		tp_err ("error setting tx qlen: %d\n", ret);
		set_fs(old_fs);
		goto dorelease;
	}
#endif
#if defined DCCP_SOCKOPT_QPOLICY_ID
	val = tdat->qpolicy;
	ret = tdat->sock->ops->setsockopt(tdat->sock, SOL_DCCP, DCCP_SOCKOPT_QPOLICY_ID,
              (char*)&val, sizeof(val));
	set_fs(old_fs);
	if (ret < 0) {
		tp_err ("error setting qpolicy: %d\n", ret);
		goto dorelease;
	}
#endif
	tp_debug3 ("bind socket to address");
	ret = kernel_bind (	tdat->sock, &tdat->addr.laddr.ad,
								TP_ADDR_SIZE (tdat->addr.laddr));
	if (ret < 0) {
		tp_err ("error binding socket: %d\n", ret);
dorelease:
		MYCLOSE(tdat, sock);
		return ret;
	}
	tp_set_tdat (tdat->sock, tdat);
	tdat->bound = 1;

	tp_debug3 ("done");

	return 0;
}

static
int
mpdccptun_peer (tdat, addr)
	struct mpdccptun	*tdat;
	tp_addr_t			*addr;
{
	int	ret;

	if (!tdat) return -EINVAL;
	CHKSTOP(-EPERM);
	if (addr) {
		ret = ldt_tunaddr_setpeer (&tdat->addr, addr, 0);
		if (ret < 0) {
			return ret;
		}
		tdat->haspeer = 1;
	}
	if (!tdat->haspeer) {
		tp_err ("no peer address");
		return -ENOTCONN;
	}
	if (tdat->isserver) {
		tp_debug ("we are server - don't do anything\n");
		return 0;
	}
	ret = queue_work (system_wq, &tdat->work_conn);
	return 0;
}


static
void
connect_handler(work)
	struct work_struct	*work;
{
	struct mpdccptun	*tdat;
	int					ret;

	if (!work) return;
	tdat = container_of (work, struct mpdccptun, work_conn);
	tp_debug3 ("tdat=%p, tun=%p\n", tdat, tdat->tun);
	ret = mpdccptun_elab_connect (tdat);
	if (ret < 0) {
		tp_err ("error connecting to peer: %d\n", ret);
		ldt_event_crsend (LDT_EVTYPE_CONN_ESTAB_FAIL, tdat->tun, (-1)*ret);
		return;
	}
	tp_debug2 ("new connection established\n");
	ldt_event_crsend (LDT_EVTYPE_CONN_ESTAB, tdat->tun, 0);
	return;
}

static
int
mpdccptun_elab_connect (tdat)
	struct mpdccptun	*tdat;
{
	int	ret;

	if (!tdat) return -EINVAL;
	if (timer_pending (&tdat->conn_timer)) {
		del_timer (&tdat->conn_timer);
		setup_timer(&tdat->conn_timer, conn_timer_handler, (unsigned long)tdat);
	}
	ret = mpdccptun_dobind (tdat);
	if (ret < 0) {
		tp_err ("error binding socket: %d\n", ret);
		return ret;
	}
	CHKSTOP(-EPERM);
	if (!tdat->haspeer) return -ENOTCONN;
	tdat->sock->sk->sk_data_ready = tp_cli_data_ready;
	ret = kernel_connect (tdat->sock, &tdat->addr.raddr.ad, TP_ADDR_SIZE(tdat->addr.raddr), 0);
	if (ret < 0) {
		tp_err ("error in connect: %d\n", ret);
		return ret;
	}
	tdat->isconnected = 1;
	tdat->wasconnected = 1;
	if (tdat->ismpdccp) {
		mod_timer(&tdat->conn_timer, jiffies + 5*HZ);	/* 5 secs */
	}
	return 0;
}

static
void
conn_timer_handler (data)
	unsigned long	data;
{
	struct mpdccptun	*tdat = (struct mpdccptun*)data;
	if (!tdat) return;
	if (!tdat->ismpdccp) return;
	if (tdat->num_subflow > 0) return;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	mpdccptun_linkdown (tdat);
#endif
}


static
int
mpdccptun_serverstart (tdat)
	struct mpdccptun	*tdat;
{
	if (!tdat) return -EINVAL;
	if (tdat->isconnected) {
		tp_err ("we are already client, cannot become server");
		return -ENOTCONN;
	}
	if (tdat->isserver) return 0;
	tdat->isserver = 1;
	queue_work (system_wq, &tdat->work_listen);
	return 0;
}


static
void
listen_handler(work)
	struct work_struct	*work;
{
	struct mpdccptun	*tdat;
	int					ret;

	tdat = container_of (work, struct mpdccptun, work_listen);
	ret = mpdccptun_doserverstart (tdat);
	if (ret < 0) {
		tp_err ("error listening: %d\n", ret);
		ldt_event_crsend (LDT_EVTYPE_CONN_LISTEN_FAIL, tdat->tun, 0);
		return;
	}
	tp_debug2 ("server is listening\n");
	ldt_event_crsend (LDT_EVTYPE_CONN_LISTEN, tdat->tun, 0);
	return;
}


static
int
mpdccptun_doserverstart (tdat)
	struct mpdccptun	*tdat;
{
	int	ret;

	if (!tdat) return -EINVAL;
	if (!tdat->isserver) return -ENOTCONN;
	ret = mpdccptun_dobind (tdat);
	if (ret < 0) {
		tp_err ("error binding socket: %d\n", ret);
		return ret;
	}
	if (!tdat->sock || !tdat->sock->sk) return -ENOTCONN;
	if (tdat->listening) {
		mpdccptun_close_listen (tdat);
	}
	tp_debug ("set socket to listen mode\n");
	CHKSTOP(-EPERM);
	tdat->sock->sk->sk_data_ready = tp_listen_ready;
	ret = kernel_listen (tdat->sock, 20);
	if (ret < 0)
		tp_err ("error in listen: %d\n", ret);
	tdat->listening = 1;
	return ret;
}

static
int
mpdccptun_setqueue (tdat, txqlen, qpolicy)
	struct mpdccptun	*tdat;
	int					txqlen, qpolicy;
{
	int				ret = 0;
	int				val;
	mm_segment_t	old_fs;

	if (!tdat) return -EINVAL;
	//if (!tdat->sock || !tdat->sock->sk) return -EPERM;
	tp_debug ("set queue txqlen=%d, qpolicy=%d\n", txqlen, qpolicy);
	CHKSTOP(-EPERM);
	if (txqlen >= 0) {
		tdat->tx_qlen = txqlen;
	}
	if (qpolicy >= 0) {
		switch (qpolicy) {
		case LDT_CMD_SETQUEUE_QPOLICY_DROP_OLDEST:
#ifdef DCCPQ_POLICY_DROP_OLDEST
			tdat->qpolicy = DCCPQ_POLICY_DROP_OLDEST;
#endif
			tpq_set_policy (&tdat->xmit_queue, TP_QUEUE_DROP_OLDEST);
			break;
		case LDT_CMD_SETQUEUE_QPOLICY_DROP_NEWEST:
#ifdef DCCPQ_POLICY_DROP_NEWEST
			tdat->qpolicy = DCCPQ_POLICY_DROP_NEWEST;
#endif
			tpq_set_policy (&tdat->xmit_queue, TP_QUEUE_DROP_NEWEST);
			break;
		default:
			tp_warn ("unsupported queuing policy %d\n", qpolicy);
			goto dorelease;
		}
	}
	if (tdat->bound) {
		old_fs = get_fs();
		set_fs(KERNEL_DS);
#ifdef DCCP_SOCKOPT_QPOLICY_TXQLEN
		if (txqlen >= 0) {
			tpq_set_maxlen (&tdat->xmit_queue, txqlen);
			ret = tdat->sock->ops->setsockopt(tdat->sock, SOL_DCCP, DCCP_SOCKOPT_QPOLICY_TXQLEN,
              		(char*)&txqlen, sizeof(txqlen));
			if (ret < 0) {
				tp_err ("error setting tx qlen: %d\n", ret);
				set_fs(old_fs);
				goto dorelease;
			}
		}
#endif
#ifdef DCCP_SOCKOPT_QPOLICY_ID
		if (qpolicy >= 0) {
			val = tdat->qpolicy;
			ret = tdat->sock->ops->setsockopt(tdat->sock, SOL_DCCP, DCCP_SOCKOPT_QPOLICY_ID,
              		(char*)&val, sizeof(val));
			if (ret < 0) {
				tp_err ("error setting qpolicy: %d\n", ret);
				set_fs(old_fs);
				goto dorelease;
			}
		}
#endif
		set_fs(old_fs);
	}

dorelease:
	if (ret < 0)
		tp_err ("error set txqlen: %d\n", ret);
	return ret;
}


static
void
mpdccptun_remove (tdat)
	struct mpdccptun	*tdat;
{
	if (!tdat) return;
	if (ISSTOP(tdat)) return;
	smp_store_release (&(tdat->tostop), 1);

	/* first make tunnel unavailable */
	if (tdat->tun) {
		tdat->tun->tundata = NULL;
		tdat->tun->tunops = NULL;
	}

#if 0
	/* cancel all pending work */
	cancel_work(&tdat->work_conn);
	cancel_work(&tdat->work_listen);
	cancel_work(&tdat->work_accept);
	cancel_work(&tdat->work_xmit);
	cancel_delayed_work(&tdat->work_xmit_delayed);
#endif

	/* may close sockets */
	if (tdat->bound) {
		mpdccptun_closesk (tdat);
	}

	/* delete other data */
	
	tp_debug2 ("destroy (work)queues and timers\n");
	tpq_destroy (&tdat->xmit_queue);
	del_timer (&tdat->conn_timer);

	/* poison struct */
	*tdat = (struct mpdccptun) { .MAGIC = 0, .tostop = 1, };
	/* free struct */
	kfree (tdat);
	tp_debug3 ("done");
}




static
int
mpdccptun_eventcreate (tdat, evbuf, evlen, evtype, desc)
	struct mpdccptun	*tdat;
	char					*evbuf;
	size_t				evlen;
	const char			*evtype, *desc;
{
	int	ret;

	if (!tdat) return -EINVAL;
	if (!evtype) return -EINVAL;
	CHKSTOP(-EPERM);
	if (!desc) desc = "";
	if (!evbuf) evlen = 0;
	if (evlen > 0) evlen--;
	if (tdat->has_subflow_report) {
		ret = snprintf (evbuf, evlen, "<event type=\"%s\">\n"
						"  <desc>%s</desc>\n"
						"  <iface>%s</iface>\n"
						"  <subflow>%s</subflow>\n"
						"</event>\n", evtype, desc, tdat->name,
						tdat->subflow_report.s);
	} else {
		ret = snprintf (evbuf, evlen, "<event type=\"%s\">\n"
						"  <desc>%s</desc>\n"
						"  <iface>%s</iface>\n"
						"</event>\n", evtype, desc, tdat->name);
	}
	if (evbuf) evbuf[evlen]=0;
	return ret;
}



static
ssize_t
mpdccptun_getinfo (tdat, info, ilen)
	struct mpdccptun	*tdat;
	char				*info;
	size_t			ilen;
{
	int	len=0;
	//char	buf[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255")+2];
	//int	i;

	if (!tdat) return -EINVAL;
//#define MYPRTIP(ad) (tp_addr_sprt_ip (buf, sizeof (buf), &(ad)) > 0 ? buf : "")
#define _FSTR	(info ? info + len : NULL)
#define _FLEN	(ilen > len ? ilen - len : 0)
	CHKSTOP(-EPERM);
	len += snprintf (_FSTR, _FLEN, "    <type>%sdccp%c</type>\n",
							tdat->ismpdccp ? "mp" : "", tdat->ipv6 ? '6' : '4');
	len += ldt_tunaddr_prt (&tdat->addr, _FSTR, _FLEN, 4);
	len += snprintf (_FSTR, _FLEN, "    <status>%s,%s,tunup,ifup</status>\n", 
								(tdat->num_subflow > 0 ? "up" : "down"),
								(tdat->tun->pdevdown ? "pdevdown" : "pdevup"));
	len += snprintf (_FSTR, _FLEN, "    <subflowlist>\n");
#if 0		/* to be fixed: locking can create dead lock with mutex in ldt_dev.c */
	DOLOCK2(tdat);
	for (i=0; i<tdat->num_subflow; i++) {
		len += snprintf (_FSTR, _FLEN, "      <subflow>%s</subflow>\n", tdat->subflow[i].s);
	}
	DOUNLOCK2(tdat);
#endif
	len += snprintf (_FSTR, _FLEN, "    </subflowlist>\n");
	return len;
#undef _FSTR
#undef _FLEN
//#undef MYPRTIP
}




static
netdev_tx_t
ldt_mpdccptun_xmit (tdat, skb)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
{
	int	ret;

	ret = mpdccptun_do_enqueue (tdat, skb);
	if (ret == -EAGAIN) {
		/* here we should disable the xmit mech, but assume that it is
		 * only for short */
		tp_debug3 ("we are busy");
		return NETDEV_TX_BUSY;
	} else if (ret < 0) {
		tp_debug2 ("error enqueuing (%d)", ret);
	}
	return NETDEV_TX_OK;
}

static
int
mpdccptun_do_enqueue (tdat, skb)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
{
	struct sk_buff		*skb2;
	int					ret, expired;

	if (!tdat || !skb) {
		/* drop */
		if (skb) kfree_skb(skb);
		return -EINVAL;
	}

	ret = mpdccptun_check_enqueue (tdat, skb);
	if (ret < 0) {
		tp_debug2 ("drop packets (reason=%d)\n", ret);
		if (ret != -EAGAIN) {
			kfree_skb (skb);
		}
		return ret;
	}

	/* extend header */
	if (likely(skb_headroom(skb) < (tdat->ipv6 ? TP_HDR6LEN : TP_HDRLEN))) {
		/* note that we extend more than what we need and check for -
		 * for usage by underlying devices
		 */
		skb2 = skb;
		skb = skb_realloc_headroom(skb2, mpdccptun_needheadroom (tdat));
		if (!skb) {
			kfree_skb (skb2);
			return -ENOMEM;
		} else if (skb != skb2) {
			consume_skb (skb2);
		}
	}
	ret = mpdccptun_prepare_skb (tdat, skb, &expired);
	if (ret < 0) {
		kfree_skb (skb);
		if (ret == -EAGAIN) ret = -EINVAL;	/* should not happen */
		return ret;
	} else if (expired) {
		tp_debug2 ("ttl expired - drop packet\n");
		kfree_skb (skb);
		return 0;
	}

	/* enqueue skb */
	tpq_enqueue (&tdat->xmit_queue, skb);

	/* do not schedule if we have delayed work */
	if (!tdat->has_delayed_work) {
		/* does not matter if it's already on the queue */
		queue_work (system_wq, &tdat->work_xmit);
	}
	return 0;
}

static
int
mpdccptun_check_enqueue (tdat, skb)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
{
	if (!tdat || !skb) return -EINVAL;
	if (!tdat->wasconnected) return -ENOTCONN;
	if (!tdat->isconnected) {
		/* we allow for enqueuing for up to 1 minute */
		if (jiffies > tdat->last_unconnect + 60*HZ) return -ENOTCONN;
		/* do not allow prot 1 message */
		if (TP_ISPROT1(skb->data[0])) return -ENOTCONN;
	}
	/* we are allowed - now check for queue */
	if (tpq_isfull (&tdat->xmit_queue)) {
		return tdat->isconnected ? -EAGAIN : -ENOTCONN;
	}
	if ((!tdat->isconnected) &&
			(tdat->xmit_queue.policy == TP_QUEUE_INF && 
			tdat->xmit_queue.queue.qlen >= 10000)) {
		return -ENOTCONN;
	}
	return 0;
}

static
void
xmit_handler (work)
	struct work_struct	*work;
{
	struct mpdccptun	*tdat;

	if (!work) return;
	tdat = container_of (work, struct mpdccptun, work_xmit);
	do_xmit_handler (tdat);
}

static
void
xmit_handler_delayed (work)
	struct work_struct	*work;
{
	struct delayed_work	*dwork;
	struct mpdccptun		*tdat;

	if (!work) return;
	dwork = container_of(work, struct delayed_work, work);
	tdat = container_of (dwork, struct mpdccptun, work_xmit_delayed);
	do_xmit_handler (tdat);
}


static
void
do_xmit_handler (tdat)
	struct mpdccptun	*tdat;
{
	int	cnt=0;
	int	ret;

	if (!tdat) return;
	tdat->has_delayed_work = 0;
	while ((ret = mpdccptun_elab_xmit (tdat)) > 0) {
		/* dequeue at most 5 skb's at a time - to give accept / connect
		 * a chance to be executed - they are on the same queue 
		 */
		if (++cnt > 5) break;
	}
	if (ret > 0) {
		/* insert directly - there is still work to be done */
		queue_work (system_wq, &tdat->work_xmit);
	} else if (ret < 0) {
		/* retry in one second */
		tdat->has_delayed_work = 1;
		queue_delayed_work (system_wq, &tdat->work_xmit_delayed, HZ);
	}
	return;
}

static
int
mpdccptun_elab_xmit (tdat)
	struct mpdccptun	*tdat;
{
	struct sk_buff		*skb;
	int					ret;
	struct tp_queue	*q;

	if (!tdat) return -EINVAL;
	q = &tdat->xmit_queue;
	skb = tpq_dequeue (q);
	if (!skb) return 0;	/* no message to elaborate */
	tp_debug3 ("packet dequeued");
	ret = mpdccptun_elab_xmit2 (tdat, skb);
	if (ret == -EAGAIN) {
		tp_debug3 ("packet requeued");
		tpq_requeue (q, skb);
		return -EAGAIN;
	}
	if (ret < 0) {
		tp_debug ("error xmit skb: %d", ret);
		return ret;
	}
	return 1;
}


static
int
mpdccptun_elab_xmit2 (tdat, skb)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
{
	int	ret, sz;

	sz = skb->len;

	ret = mpdccptun_xmit_skb (tdat, skb);
	if (ret < 0) {
		tdat->ndev->stats.tx_dropped++;
		if (ret == -EAGAIN) {
		} else {
			tdat->ndev->stats.tx_errors++;
			kfree_skb (skb);
		}
		tp_debug ("error %d - dropping packet\n", ret);
		return ret;
	}
	tdat->ndev->stats.tx_packets++;
	tdat->ndev->stats.tx_bytes+=sz;
	return 0;
}



static
int
mpdccptun_xmit (tdat, data, sz, raddr)
	struct mpdccptun	*tdat;
	char					*data;
	int					sz;
	tp_addr_t			*raddr;
{
	struct sk_buff	*skb;
	int				len, ret;

	if (!tdat || !data || sz < 0) return -EINVAL;
	len = mpdccptun_needheadroom (tdat);
	tp_debug3 ("sending meta packet of size %d\n", sz);
	skb = dev_alloc_skb (len + sz);
	if (!skb) return -ENOMEM;
	skb_reserve (skb, len);
	memcpy (skb_put (skb, sz), data, sz);
	ret = mpdccptun_do_enqueue (tdat, skb);
	if (ret == -EAGAIN) {
		kfree_skb (skb);
	}
	/* else: do not free skb - not even on error - already done */
	return ret;
}


static
int
mpdccptun_prepare_skb (tdat, skb, expired)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
	int					*expired;
{
	struct iphdr		*iph;
	struct ipv6hdr		*iph6;

	if (!tdat || !skb || !expired) return -EINVAL;
	*expired = 0;
	mpdccptun_scrub_skb (skb);
	switch (TP_GETPKTTYPE (skb->data[0])) {
	case 4:
		skb_reset_inner_network_header (skb);
		skb_reset_network_header(skb);
		iph = ip_hdr(skb);
		if (iph->ttl<=1) {
			*expired = 1;
			return 0;
		}
		// TODO: recalculate header checksum
		// iph->ttl--;
		skb_set_inner_transport_header (skb, iph->ihl << 2);
		break;
	case 6:
		skb_reset_inner_network_header (skb);
		skb_reset_network_header(skb);
		iph6 = ipv6_hdr (skb);
		if (iph6->hop_limit<=1) {
			*expired = 1;
			return 0;
		}
		// TODO: recalculate header checksum
		// iph6->hop_limit--;
		break;
	}

	return 0;
}


static
int
mpdccptun_xmit_skb (tdat, skb)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
{
	int					ret, len;

	if (!skb) return -EINVAL;
	if (!tdat) return -EINVAL;
	if (ISSTOP(tdat)) {
		tp_debug2 ("device %s marked for being stopped", tdat->name);
		return -EPERM;
	}

	len = skb->len;
	ret = do_xmit_skb (tdat, skb);
	if (ret < 0) {
		if (ret != -EAGAIN) {
			tp_note ("error sending skb: %d\n", ret);
		}
		return ret;
	}

	tp_debug2 ("packet with %d bytes sent\n", len);

	return len;
}

static
int
do_xmit_skb (tdat, skb)
	struct mpdccptun	*tdat;
	struct sk_buff		*skb;
{
	struct socket	*sock;
	int				ret;

	if (!tdat || !skb) return -EINVAL;
	if (!tdat->listening) {
		sock = tdat->sock;
	} else {
		sock = tdat->active;
	}
	if (!sock) return -ENOTCONN;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	if (tdat->ismpdccp) {
		ret = mpdccp_xmit_skb (sock->sk, skb);
	} else
#endif
	{
		struct kvec		kvec = (struct kvec) {
			.iov_base = skb->data,
			.iov_len = skb->len,
		};
		struct msghdr	msg =  (struct msghdr) {
			.msg_iter = {
				.type = ITER_IOVEC,
				.count = 1,
				.kvec = &kvec,
			},
			.msg_flags = MSG_DONTWAIT,
		};
		ret = kernel_sendmsg (sock, &msg, &kvec, 1, skb->len);
		if (ret == 0) kfree_skb (skb);
	}
	if (ret < 0) {
		tp_note ("%s error sending message: %d", 
					tdat->ismpdccp ? "mpdccp" : "dccp", ret);
		return ret;
	}
	return 0;
}

static
void
mpdccptun_scrub_skb (skb)
   struct sk_buff *skb;
{
   if (!skb) return;
   skb_orphan(skb);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,14,0)
   skb->tstamp.tv64 = 0;
#else
   skb->tstamp = 0;
#endif
   skb->pkt_type = PACKET_HOST;
   skb->skb_iif = 0;
   skb_dst_drop(skb);
   nf_reset(skb);
   nf_reset_trace(skb);

#if LINUX_VERSION_CODE > KERNEL_VERSION(3,11,0)
   skb->ignore_df = 0;
   //secpath_reset(skb);
# ifdef CONFIG_NET_SWITCHDEV
   skb->offload_fwd_mark = 0;
# endif
#endif

}




static
int
mpdccptun_dorcv_all (tdat, sk)
	struct mpdccptun	*tdat;
	struct sock			*sk;
{
	int	ret=0;

	if (!tdat) return -EINVAL;
	while (!ISSTOP(tdat) && (ret = mpdccptun_dorcv2 (tdat, sk)) == 0);
	if (ret == -EAGAIN) ret=0;
	if (ret < 0)
		tdat->ndev->stats.rx_errors++;
	return ret;
}



static
int
mpdccptun_dorcv (tdat, sk)
	struct mpdccptun	*tdat;
	struct sock			*sk;
{
	int	ret;

	if (!tdat) return -EINVAL;
	if (ISSTOP(tdat)) return 0;
	ret = mpdccptun_dorcv2 (tdat, sk);
	if (ret == -EAGAIN) ret=0;
	if (ret < 0)
		tdat->ndev->stats.rx_errors++;
	return ret;
}

static
int
mpdccptun_dorcv2 (tdat, sk)
	struct mpdccptun	*tdat;
	struct sock			*sk;
{
	int					ret;
	struct sk_buff		*skb=NULL;

	if (!tdat) return -EINVAL;
	ret = dorcv_datagram (&skb, sk, MSG_DONTWAIT);
	if (ret == -EAGAIN) return ret;
	if (ret < 0) {
		tp_note ("error receiving data: %d", ret);
		return ret;
	}
	if (ret == 0 || !skb || !skb->data || skb->len==0) return 0;

	ret = mpdccptun_elab_recv(tdat, skb);
	if (ret < 0) {
		if (ret == -EBADMSG)
		tp_debug ("error receiving packet: %d", ret);
		kfree_skb (skb);
		return ret;
	}
	return 0;
}



static
int
mpdccptun_elab_recv (tdat, skb)
	struct mpdccptun		*tdat;
	struct sk_buff		*skb;
{
	int	ret, sz;

	if (!tdat || !skb) return -EINVAL;
	/* set fw mark */
	/* we have to set it always - because it was a drop count in sock-recv */
	if (TP_SKBISPROT1(skb)) {
		return ldt_prot1_recv (tdat->tun, skb);
	}

	skb->dev = tdat->ndev;
	switch (TP_GETPKTTYPE(skb->data[0])) {
	case 4:
		skb->protocol = htons (ETH_P_IP);
		break;
	case 6:
		skb->protocol = htons (ETH_P_IPV6);
		break;
	default:		
		tp_debug ("received unsupported protocol %d", TP_GETPKTTYPE (skb->data[0]));
		return -EBADMSG;
	}

	/* deliver packet to device */
	tp_debug3 ("deliver to %s\n", tdat->name);
	sz = skb->len;
	ret = netif_rx (skb);
	if (ret != NET_RX_SUCCESS) {
		tp_debug ("packet (%d bytes) dropped by netif_rx\n", sz);
		/* skb must not be freed here! */
	}

	tp_debug3 ("received %d bytes", sz);
	/* done */
	tdat->ndev->stats.rx_packets++;
	tdat->ndev->stats.rx_bytes+=sz;
	return 0;
}



static
int
dorcv_datagram (skbuf, sk, flags)
	struct sk_buff		**skbuf;
	struct sock			*sk;
	int					flags;
{
	int				peeked, off=0, err=0, ret;
	struct sk_buff	*skb;

	if (!skbuf || !sk) return -EINVAL;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
	skb = __skb_recv_datagram (sk, flags, &peeked, &off, &err);
#else
	skb = __skb_recv_datagram (sk, flags, NULL, &peeked, &off, &err);
#endif
	if (!skb) return err;
	tp_debug3 ("receive datagramm of %d bytes\n", skb->len);
	
/* workaround for kernel bug */
#if LINUX_VERSION_CODE >= KERNEL_VERSION(4,14,110) && LINUX_VERSION_CODE < KERNEL_VERSION(4,15,0)
	if (!skb->sk) {
		/* assume bug */
		skb->sk = sk;
		skb->destructor = sock_rfree;
	}
#elif LINUX_VERSION_CODE >= KERNEL_VERSION(4,10,0) && LINUX_VERSION_CODE < KERNEL_VERSION(4,19,0)
	if (!skb->sk) {
		tp_warn ("skb->sk is null - check for possible kernel bug!!!\n");
	}
#endif
	ret = rcv_prepare_skb (skbuf, skb);
	if (ret < 0) {
		kfree_skb (skb);
		return ret;
	}
	return ret;
}

static
int
rcv_prepare_skb (skbuf, skb)
	struct sk_buff			**skbuf, *skb;
{
	struct sk_buff	*nskb;

	/* clean some data */
	mpdccptun_scrub_skb (skb);

	if (mpdccptun_needrcvcpy (skb)) {
		tp_debug3 ("copy packet\n");
		if (in_interrupt() || in_atomic())
			nskb = skb_copy (skb, GFP_ATOMIC);
		else
			nskb = skb_copy (skb, GFP_KERNEL);
		if (!nskb) {
			tp_err ("error in copying datagram\n");
			return -ENOMEM;
		}
		kfree_skb (skb);
		skb = nskb;
	}
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
   skb->data += ((struct dccp_hdr*)(skb->data))->dccph_doff * 4;
   skb->len -= ((struct dccp_hdr*)(skb->data))->dccph_doff * 4;
#endif

	*skbuf = skb;
	return skb->len;
}

static
int
mpdccptun_needrcvcpy (skb)
	struct sk_buff	*skb;
{
	int				type;
	unsigned char	*ptr;
	int				len;

	if (!skb) return 0;
	if (skb_shinfo(skb)->nr_frags == 0 && !skb_shinfo(skb)->frag_list) return 0;
	tp_debug3 ("skb is fragmented (nr_frags=%d, frag_list=%p)\n",
					skb_shinfo(skb)->nr_frags, skb_shinfo(skb)->frag_list);
	if (skb->len < sizeof (struct dccp_hdr) + 4) return 1;
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,7,0)
   ptr = skb->data + ((struct dccp_hdr*)(skb->data))->dccph_doff * 4;
   len = skb->len - ((struct dccp_hdr*)(skb->data))->dccph_doff * 4;
#else
	ptr = skb->data;
	len = skb->len;
#endif
	type = *ptr >> 4;
	if (type == 2) {
		/* authenticated packets are copied for now - we should change that */
		return 1;
	}
	switch (type) {
	case 4:
		if (len < sizeof (struct iphdr)) return 1;
		/* the ip headr must fully fit into main fragment */
		if (len < ((struct iphdr*)ptr)->ihl * 4) return 1;
		return 0;
	case 6:
		if (len < 1260) return 1;
		return 0;
	}
	/* internal packets are copied */
	return 1;
}




static
void
tp_set_tdat (sock, tdat)
	struct socket		*sock;
	struct mpdccptun	*tdat;
{
	struct sock			*sk = sock ? sock->sk : NULL;

	if (!sk) return;
	sk->sk_user_data = tdat;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	if (tdat && tdat->ismpdccp)
		tp_subflow_reg (sk);
#endif
}

static
void
tp_unset_tdat (sock)
	struct socket		*sock;
{
	struct sock			*sk = sock ? sock->sk : NULL;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	struct mpdccptun	*tdat;
#endif

	if (!sk) return;
#if IS_ENABLED(CONFIG_IP_MPDCCP)
	tdat = sk->sk_user_data;
	if (tdat && tdat->ismpdccp)
		tp_subflow_dereg (sk);
#endif
	sk->sk_user_data = NULL;
}

static
void
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)
tp_cli_data_ready (sk, bytes)
	struct sock	*sk;
	int			bytes;
#else
tp_cli_data_ready (sk)
	struct sock	*sk;
#endif
{
	tp_elab_data_ready (sk);
}

static
void
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)
tp_srv_data_ready (sk, bytes)
	struct sock	*sk;
	int			bytes;
#else
tp_srv_data_ready (sk)
	struct sock	*sk;
#endif
{
	tp_elab_data_ready (sk);
}

static
void
tp_elab_data_ready (sk)
	struct sock	*sk;
{
	struct mpdccptun	*tdat;

	if (!sk) return;
	tp_debug3 ("new packet arrived\n");
	tdat = sk->sk_user_data;
	if (!ISMPDCCPTUN(tdat) || !tdat->bound) {
		tp_debug ("no data structure in socket\n");
		return;
	}
	mpdccptun_dorcv (tdat, sk);
}


static
void
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,15,0)
tp_listen_ready (sk, bytes)
	struct sock	*sk;
	int			bytes;
#else
tp_listen_ready (sk)
	struct sock	*sk;
#endif
{
	struct mpdccptun	*tdat;
	int					ret;

	if (!sk) return;
	tp_debug ("new connection request arrived\n");
	tdat = sk->sk_user_data;
	if (!ISMPDCCPTUN(tdat) || !tdat->bound) {
		tp_err ("no data structure in socket\n");
		return;
	}
	ret = queue_work (system_wq, &tdat->work_accept);
	if (ret < 0) {
		tp_err ("error in scheduling accept: %d\n", ret);
		return;
	}
}

static
void
accept_handler(work)
	struct work_struct	*work;
{
	struct mpdccptun	*tdat;
	int					ret;

	if (!work) return;
	tdat = container_of (work, struct mpdccptun, work_accept);
	tp_debug3 ("tdat=%p, tun=%p\n", tdat, tdat->tun);
	ret = mpdccptun_elab_accept (tdat);
	if (ret < 0) {
		tp_err ("error accepting connection: %d\n", ret);
		ldt_event_crsend (LDT_EVTYPE_CONN_ACCEPT_FAIL, tdat->tun, 0);
		return;
	}
	tp_debug2("new connection accepted");
	ldt_event_crsend (LDT_EVTYPE_CONN_ACCEPT, tdat->tun, 0);
	return;
}

static
int
mpdccptun_elab_accept (tdat)
	struct mpdccptun	*tdat;
{
	struct socket	*sock;
	struct socket	*sk_todel = NULL;
	int				ret;

	if (!tdat) return -EINVAL;
	ret = kernel_accept (tdat->sock, &sock, O_NONBLOCK);
	if (ret < 0) {
		tp_err ("error accepting connection: %d", ret);
		return ret;
	}
	DOLOCK (tdat);
	if (tdat->active) {
		tp_unset_tdat (tdat->active);
		sk_todel = tdat->active;
	}
	tp_debug ("connection accepted, becoming active");
	tdat->active = sock;
	tp_set_tdat (sock, tdat);
	sock->sk->sk_data_ready = tp_srv_data_ready;
	tdat->isconnected = 1;
	tdat->wasconnected = 1;
	DOUNLOCK (tdat);
	if (sk_todel)
		_myclose (sk_todel);
	/* to avoid race */
	tp_debug ("receive already queued data");
	lock_sock (sock->sk);
	mpdccptun_dorcv_all (tdat, sock->sk);
	release_sock (sock->sk);
	tp_debug2 ("done");
	return 0;
}


static
void
_myclose (sock)
	struct socket	*sock;
{
	if (!sock) return;
	kernel_sock_shutdown(sock, SHUT_RDWR);
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,2,0)
	sk_release_kernel(sock->sk);
#else
	sock_release (sock);
#endif
}



#if IS_ENABLED(CONFIG_IP_MPDCCP)

#ifdef MPDCCP_SUBFLOW_NOTIFIER
static int tp_subflow_cb (struct notifier_block*, unsigned long, void*);
static struct notifier_block tp_subflow_notifier = {
	.notifier_call = tp_subflow_cb,
};

static
int
tp_subflow_reg (
	struct sock *sk)
{
	return 0;
}

static
int
tp_subflow_dereg (
	struct sock *sk)
{
	return 0;
}

static
int
tp_subflow_global_reg ()
{
	return register_mpdccp_subflow_notifier (&tp_subflow_notifier);
}

static
int
tp_subflow_global_dereg ()
{
	return unregister_mpdccp_subflow_notifier (&tp_subflow_notifier);
}

static
int
tp_subflow_cb (nblk, event, ptr)
	struct notifier_block	*nblk;
	unsigned long		event;
	void			*ptr;
{
	struct mpdccp_subflow_notifier	*info = ptr;

	tp_subflow_report ((int)event, info->sk, info->subsk, info->link, info->role);
	return NOTIFY_DONE;
}

#else
/* do it the old way */

static
int
tp_subflow_global_reg ()
{
	return 0;
}

static
int
tp_subflow_global_dereg ()
{
	return 0;
}


static
int
tp_subflow_reg (
	struct sock	*sk)
{
	return mpdccp_set_subflow_report (sk, tp_subflow_report);
}

static
int
tp_subflow_dereg (
	struct sock *sk)
{
	return mpdccp_set_subflow_report (sk, NULL);
}

#endif

/* for backward compatibility with older version of mpdccp */
#ifndef MPDCCP_EV_SUBFLOW_DESTROY
#  define MPDCCP_EV_SUBFLOW_DESTROY MPDCCP_SUBFLOW_DESTROY
#endif
#ifndef MPDCCP_EV_SUBFLOW_CREATE
#  define MPDCCP_EV_SUBFLOW_CREATE MPDCCP_SUBFLOW_CREATE
#endif

static
void
tp_subflow_report (action, meta_sk, sk, link, role)
	int							action, role;
	struct sock					*meta_sk, *sk;
	struct mpdccp_link_info	*link;
{
	struct mpdccptun		*tdat;
	subflow_str				*p;
	const char				*name;
	int						i, ret;

	if (!meta_sk || !link) return;
	tdat = meta_sk->sk_user_data;
	if (!ISMPDCCPTUN(tdat) || ISSTOP(tdat)) {
		return;
	}
	if (link->is_devlink) {
		name = link->ndev_name;
	} else {
		name = link->name;
	}
	if (!name) name = "unnamed";
	switch (action) {
	case MPDCCP_EV_SUBFLOW_CREATE:
		tp_info ("add subflow %s\n", name);
		DOLOCK2(tdat);
		if (tdat->num_subflow >= tdat->bufsz_subflow) {
			if (in_atomic()) {
				p = krealloc (tdat->subflow, sizeof (subflow_str) * (tdat->bufsz_subflow+1), GFP_ATOMIC);
			} else {
				p = krealloc (tdat->subflow, sizeof (subflow_str) * (tdat->bufsz_subflow+1), GFP_KERNEL);
			}
			if (!p) {
				DOUNLOCK2(tdat);
				return;
			}
			tdat->subflow = p;
			tdat->bufsz_subflow++;
		}
		strcpy (tdat->subflow[tdat->num_subflow].s, name);
		tdat->num_subflow++;
		DOUNLOCK2(tdat);
		strcpy (tdat->subflow_report.s, name);
		tdat->has_subflow_report = 1;
		ret = ldt_event_crsend (LDT_EVTYPE_SUBFLOW_UP, tdat->tun, 0);
		if (ret < 0) {
			tp_err ("error sending subflow up event: %d\n", ret);
		}
		tdat->has_subflow_report = 0;
		break;
	case MPDCCP_EV_SUBFLOW_DESTROY:
		tp_info ("remove subflow %s\n", name);
		DOLOCK2(tdat);
		for (i=0; i<tdat->num_subflow; i++) {
			if (!strcasecmp (name, tdat->subflow[i].s)) {
				for (; i<tdat->num_subflow-1; i++) {
					strcpy (tdat->subflow[i].s, tdat->subflow[i+1].s);
				}
				tdat->subflow[i].s[0] = 0;
				tdat->num_subflow--;
				break;
			}
		}
		DOUNLOCK2(tdat);
		
		strcpy (tdat->subflow_report.s, name);
		tdat->has_subflow_report = 1;
		ret = ldt_event_crsend (LDT_EVTYPE_SUBFLOW_DOWN, tdat->tun, 0);
		if (ret < 0) {
			tp_err ("error sending subflow down event: %d\n", ret);
		}
		tdat->has_subflow_report = 0;
		break;
#ifdef MPDCCP_EV_ALL_SUBFLOW_DOWN
	case MPDCCP_EV_ALL_SUBFLOW_DOWN:
		mpdccptun_linkdown (tdat);
		break;
#endif
	}
	return;
}


static
int
mpdccptun_linkdown (tdat)
	struct mpdccptun	*tdat;
{
	if (!tdat) return -EINVAL;
	if (tdat->listening) {
		MYCLOSE (tdat, active);
		return 0;
	}
	/* we are client and need to reconnect - TBD */
	tdat->last_unconnect = jiffies;
	tdat->isconnected = 0;
	MYCLOSE(tdat, sock);
	return 0;
}


#endif






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
