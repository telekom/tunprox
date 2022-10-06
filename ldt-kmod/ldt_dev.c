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
#include <net/arp.h>
#include <linux/if_arp.h>
#include <linux/list.h>
#include <linux/netdevice.h>
#include <linux/etherdevice.h>
#include <linux/rtnetlink.h>
#include <linux/spinlock.h>
#include <linux/in.h>
#include <linux/in6.h>
#include <net/net_namespace.h>
#include <linux/inetdevice.h>
#include <net/if_inet6.h>
#include <linux/version.h>


#include "ldt_uapi.h"
#include "ldt_debug.h"
#include "ldt_dev.h"
#include "ldt_tun.h"
#include "ldt_event.h"
#include "ldt_lock.h"


static struct net_device_stats* tpdev_getstats (struct net_device*);
static int tpdev_close (struct net_device*);
static int tpdev_open (struct net_device*);
static void tpdev_setup (struct net_device *);
static int tpdev_setflags (struct ldt_dev *, int);
static void tpdev_uninit (struct net_device*);
static void ldt_rm_tun2 (struct ldt_dev*);
static netdev_tx_t tpdev_xmit (struct sk_buff*, struct net_device*);
static int tpdev_change_mtu (struct net_device*, int);
static ssize_t get_devinfo (struct ldt_dev *, char*, size_t);
static int tpdev_ndevdown (struct ldt_dev*);
static int tpdev_ndevup (struct ldt_dev*);
static int tp_ev_up (struct net_device*);
static int tp_ev_down (struct net_device*);
static int tp_ev_ndev (struct notifier_block *, unsigned long, void*);
static void ldt_remove_all (void);



#define DEV_LOCK(tdev)		do { if (tdev) tp_lock (&(tdev)->lock); } while (0)
#define DEV_UNLOCK(tdev)	do { if (tdev) tp_unlock (&(tdev)->lock); } while (0)
#define DEV_OK(tdev) (ISLDTDEV(tdev) && ISACTIVETPDEV(tdev))
#define DEV_LOCK_CHK(tdev)  ( { int _ret; DEV_LOCK(tdev); _ret=DEV_OK(tdev); if (!_ret) DEV_UNLOCK(tdev); _ret; } )
#define SETACTIVE(tdev,on) do { smp_store_release (&(tdev->active), on); } while (0)


static struct notifier_block tpdev_watch_netdev_notifier = {
   .notifier_call = tp_ev_ndev,
};

static struct device_type	tp_devtype = {
	.name = "ldt",
};

static HLIST_HEAD(ldt_dev_list);
static struct tp_lock	ldt_dev_list_lock;
#define LDLLOCK	do { tp_lock (&ldt_dev_list_lock); } while (0)
#define LDLULOCK	do { tp_unlock (&ldt_dev_list_lock); } while (0)

int
ldt_dev_global_init (void)
{
	int   ret;
	
	tp_lock_init (&ldt_dev_list_lock);
	ret = register_netdevice_notifier (&tpdev_watch_netdev_notifier);
	if (ret < 0) {
		tp_err ("error registering netdev notifier: %d", ret);
		return ret;
	}
	return 0;
}

void
ldt_dev_global_destroy (void)
{
	unregister_netdevice_notifier (&tpdev_watch_netdev_notifier);
	ldt_remove_all ();
	tp_lock_destroy (&ldt_dev_list_lock);
}

int
ldt_create_dev (net, name, out_name, flags)
	struct net	*net;
	const char	*name;
	const char	**out_name;
	int			flags;
{
	struct net_device		*ndev;
	struct ldt_dev	*tdev;
	int						ret;
#ifdef NET_NAME_ENUM
	int						assigntype = NET_NAME_USER;
#endif

	if (!name || !*name) {
		name="ldt%d";
#ifdef NET_NAME_ENUM
		assigntype = NET_NAME_ENUM;
#endif
	} else if (strlen (name) > IFNAMSIZ-1) {
		tp_err ("name >>%s<< too long (max=%d)\n", name, IFNAMSIZ-1);
		return -EINVAL;
	}
	tp_debug ("create device %s\n", name);
#ifdef NET_NAME_ENUM
	ndev = alloc_netdev_mqs (	sizeof (struct ldt_dev), name, assigntype,
										tpdev_setup, 1, 1);
#else
	ndev = alloc_netdev_mqs (	sizeof (struct ldt_dev), name,
										tpdev_setup, 1, 1);
#endif
	if (!ndev) return -ENOMEM;
#ifdef CONFIG_NET_NS
	dev_net_set(ndev, net);
#endif

	tdev = (struct ldt_dev*) netdev_priv (ndev);
	ret = tpdev_setflags (tdev, flags);
	if (ret < 0) {
		free_netdev(ndev);
		return ret;
	}
	/* now register device */
	ret = register_netdev (ndev);
	if (ret < 0) {
		tp_err ("register ldt device %s failed\n", ndev->name);
		free_netdev (ndev);
		return ret;
	}
	LDLLOCK;
	hlist_add_head (&tdev->list, &ldt_dev_list);
	LDLULOCK;

	if (out_name) *out_name = ndev->name;
	return 0;
}

static
int
tpdev_close (
	struct net_device	*ndev)
{
	netif_stop_queue (ndev);
	return 0;
}

static
int
tpdev_open (
	struct net_device	*ndev)
{
	netif_start_queue (ndev);
	return 0;
}

static
struct net_device_stats *
tpdev_getstats (
	struct net_device	*ndev)
{
	return &ndev->stats;
}


static const struct net_device_ops	tp_devops = {
	.ndo_open				= tpdev_open,
	.ndo_stop				= tpdev_close,
	.ndo_start_xmit		= tpdev_xmit,
	.ndo_set_mac_address	= eth_mac_addr,
	.ndo_change_mtu		= tpdev_change_mtu,
	.ndo_uninit				= tpdev_uninit,
	.ndo_get_stats			= tpdev_getstats,
};

static
void
tpdev_setup (
	struct net_device	*ndev)
{
	struct ldt_dev	*tdev;

	/* lock must be held by caller */
	if (!ndev) return;
	ndev->netdev_ops = &tp_devops;
	tdev = (struct ldt_dev*) netdev_priv (ndev);
	*tdev = (struct ldt_dev) {
		.ndev = ndev,
	};
	tp_lock_init (&tdev->lock);
	ndev->mtu = 1350;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->tx_queue_len = 10000;
	tdev->ctime = tdev->mtime = get_seconds();
	SET_NETDEV_DEVTYPE(ndev, &tp_devtype);
	tdev->MAGIC = LDT_MAGIC;
	return;
}

static
int
tpdev_setflags (tdev, flags)
	struct ldt_dev	*tdev;
	int						flags;
{
	if (!tdev) return -EINVAL;

	if (flags & LDT_CREATE_DEV_F_CLIENT)
		tdev->client = 1;
	else if (flags & LDT_CREATE_DEV_F_SERVER)
		tdev->server = 1;
	return 0;
}


void
ldt_free_dev (tdev)
	struct ldt_dev	*tdev;
{
	if (!DEV_OK(tdev)) return;
	tpdev_close (tdev->ndev);
	SETACTIVE(tdev,0);
	tp_info ("remove dev %s\n", tdev->ndev->name);
	ldt_event_crsend (LDT_EVTYPE_IFDOWN, tdev, 0);
	dev_put (tdev->ndev);
	LDLLOCK;
	hlist_del (&tdev->list);
	LDLULOCK;
	unregister_netdev (tdev->ndev);
	free_netdev (tdev->ndev);
	return;
}

static
void
ldt_remove_all (void)
{
	struct ldt_dev		*tdev;
	struct hlist_node	*tmp;

	hlist_for_each_entry_safe (tdev, tmp, &ldt_dev_list, list) {
		ldt_free_dev (tdev);
	}
}


static
void
tpdev_uninit (ndev)
	struct net_device		*ndev;
{
	struct ldt_dev	*tdev;

	tdev = LDTDEV (ndev);
	if (!tdev) return;
	tp_debug ("uninit dev %s\n", tdev->ndev->name);
	/* poison struct */
	tdev->MAGIC = 0;

	/* remove all tunnel */
	ldt_rm_tun2 (tdev);
	tp_lock_destroy (&tdev->lock);
	return;
}



/*      Called when a packet needs to be transmitted.
 *      Must return NETDEV_TX_OK , NETDEV_TX_BUSY.
 *        (can also return NETDEV_TX_LOCKED iff NETIF_F_LLTX)
 */
static
netdev_tx_t
tpdev_xmit (skb, ndev)
	struct sk_buff		*skb;
	struct net_device	*ndev;
{
	struct ldt_dev	*tdev;
	netdev_tx_t				ret;

	if (!ndev || !skb) return 0;
	tdev = LDTDEV (ndev);
	if (!tdev) {
		kfree_skb (skb);
		return NETDEV_TX_OK;
	}
	if (!ISACTIVE(tdev)) {
		return NETDEV_TX_BUSY;
	}
	ret = ldt_tun_xmit (&tdev->tun, skb);
	return ret;
}


/*      Called when a user wants to change the Maximum Transfer Unit
 *      of a device. If not defined, any request to change MTU will
 *      will return an error.
 */
static
int
tpdev_change_mtu (ndev, new_mtu)
	struct net_device	*ndev;
	int					new_mtu;
{
	struct ldt_dev	*tdev;
	int						ret = -ERANGE;

	if (!ndev) return -EINVAL;
	tdev = LDTDEV (ndev);
	if (!DEV_LOCK_CHK(tdev)) return 0;
	if (new_mtu < 1280) goto out;
	if (new_mtu > 65535) goto out;
	ndev->mtu = new_mtu;
	ret = 0;
	tdev->mtime = get_seconds();
out:
	DEV_UNLOCK(tdev);
	return 0;
}


int
ldt_get_devlist (net, devlist, dlen)
	struct net	*net;
	char			**devlist;
	u32			*dlen;
{
	struct net_device		*pn;
	int						len, num;
	char						*s;

	if (!net || !devlist || !dlen) return -EINVAL;
	len=num=0;
	read_lock (&dev_base_lock);
	for_each_netdev (net, pn) {
		if (!TPDEV_ISLDT(pn)) continue;
		num++;
	}
	*devlist = kmalloc (num*(IFNAMSIZ+1)+1, GFP_KERNEL);
	if (!*devlist) {
		read_unlock (&dev_base_lock);
		return -ENOMEM;
	}
	s=*devlist;
	*s = 0;
	for_each_netdev (net, pn) {
		if (!TPDEV_ISLDT(pn)) continue;
		strcpy (s, pn->name);
		s += strlen (s) + 1;
	}
	read_unlock (&dev_base_lock);
	*dlen = s - *devlist;
	return 0;
}

ssize_t
ldt_get_devinfo (tdev, info)
	struct ldt_dev	*tdev;
	char						**info;
{
	char		*buf=NULL;
	ssize_t	len, blen;

	if (!tdev) return -EINVAL;
	if (!ISLDTDEV(tdev)) return -EINVAL;
	DEV_LOCK_CHK(tdev);
	blen = get_devinfo (tdev, NULL, 0);
	if (blen < 0 || !info) {
		DEV_UNLOCK(tdev);
		return blen;
	}
	buf = kmalloc (blen+1, GFP_KERNEL);
	*info = buf;
	if (!buf) {
		DEV_UNLOCK(tdev);
		return -ENOMEM;
	}
	len = get_devinfo (tdev, buf, blen+1);
	DEV_UNLOCK(tdev);
	if (len < 0) return len;
	if (len > blen) {
		tp_warn ("write truncated");
	}
	return len;
}

ssize_t
ldt_get_alldevinfo (net, info)
	struct net	*net;
	char			**info;
{
	char						*obuf=NULL, *p;
	ssize_t					len, blen, xlen;
	struct ldt_dev	*tdev;
	struct net_device		*pn;

	if (!net || !info) return -EINVAL;
	len=32;
	obuf = kmalloc (len, GFP_KERNEL);
	if (!obuf) return -ENOMEM;
	strcpy (obuf, "<ldtlist>\n");
	p = obuf + strlen ("<ldtlist>\n");
	read_lock (&dev_base_lock);
	for_each_netdev (net, pn) {
		if (!TPDEV_ISLDT(pn)) continue;
		tdev = LDTDEV(pn);
		if (!tdev) continue;
		if (!DEV_LOCK_CHK(tdev)) continue;
		blen = get_devinfo (tdev, NULL, 0);
		if (blen < 0) {
			DEV_UNLOCK(tdev);
			read_unlock (&dev_base_lock);
			return blen;
		}
		len += blen;
		xlen = p-obuf;
		p = krealloc (obuf, len, GFP_KERNEL);
		if (!p) {
			DEV_UNLOCK(tdev);
			read_unlock (&dev_base_lock);
			kfree (obuf);
			return -ENOMEM;
		}
		obuf = p;
		p += xlen;
		blen = get_devinfo (tdev, p, blen+1);
		DEV_UNLOCK(tdev);
		if (blen < 0) {
			read_unlock (&dev_base_lock);
			kfree (obuf);
			return blen;
		}
		p += blen;
	}
	read_unlock (&dev_base_lock);
	strcpy (p, "</ldtlist>\n");
	p += strlen ("</ldtlist>\n");
	len = p-obuf;
	if (info) {
		*info = obuf;
	} else {
		kfree (obuf);
	}
	return len;
}


static
ssize_t
get_devinfo (tdev, info, ilen)
	struct ldt_dev	*tdev;
	char						*info;
	size_t					ilen;
{
	ssize_t	len=0, ret;

	/* lock must be held by caller */
	if (!ISLDTDEV(tdev) || !ISACTIVE(tdev)) return -EINVAL;
#define _FSTR	(info ? info + len : NULL)
#define _FLEN	(ilen > len ? ilen - len : 0)
	len += snprintf (_FSTR, _FLEN, "<dev name=\"%s\">\n", tdev->ndev->name);
	len += snprintf (_FSTR, _FLEN, "  <mtu>%d</mtu>\n", (int)tdev->ndev->mtu);
	len += snprintf (_FSTR, _FLEN, "  <ctime>%lld</ctime><mtime>%lld</mtime>\n",
												(long long)tdev->ctime, (long long)tdev->mtime);
	len += snprintf (_FSTR, _FLEN, "  <tun>\n");
	ret = ldt_tun_gettuninfo (&tdev->tun, _FSTR, _FLEN);
	if (ret < 0) {
		tp_warn ("cannot get info from tunnel of device %s: %d",
					tdev->ndev->name, (int)ret);
	} else {
		len += ret;
	}
	len += snprintf (_FSTR, _FLEN, "  </tun>\n");
	len += snprintf (_FSTR, _FLEN, "</dev>\n");
	return len;
#undef _FSTR
#undef _FLEN
}


int
ldt_rm_tun (tdev)
	struct ldt_dev	*tdev;
{
	if (!tdev) return -EINVAL;
	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	if (tdev->ndev->flags & IFF_UP) {
		DEV_UNLOCK(tdev);
		return -EPERM;
	}
	SETACTIVE(tdev,0);
	ldt_rm_tun2 (tdev);
	tdev->mtime = get_seconds();
	SETACTIVE(tdev,1);
	DEV_UNLOCK(tdev);
	return 0;
}

static
void
ldt_rm_tun2 (tdev)
	struct ldt_dev	*tdev;
{
	struct ldt_tun	*tun;

	tun = &tdev->tun;
	if (tun->tunops)
		ldt_event_crsend (LDT_EVTYPE_TUNDOWN, tun, 0);
	ldt_tun_remove (tun);
	*tun = (struct ldt_tun) { .tdev = tdev };
	tdev->hastun=0;
}


int
ldt_dev_newtun (tdev, tuntype)
	struct ldt_dev	*tdev;
	const char				*tuntype;
{
	int						ret;
	struct ldt_tun	*tun;
	int						mhead;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	SETACTIVE(tdev,0);
	if (tdev->hastun) {
		ret = -EALREADY;
		goto out;
	}
	tun = (&tdev->tun);
	*tun = (struct ldt_tun) { .tdev = tdev };
	ret = ldt_tun_init (tun, tuntype);
	if (ret < 0)  goto out;
	mhead = ldt_tun_needheadroom (tun);
	if (mhead > 0 && tdev->ndev->needed_headroom < mhead) tdev->ndev->needed_headroom = mhead;
	tdev->hastun = 1;
	tdev->mtime = get_seconds();
	ldt_event_crsend (LDT_EVTYPE_TUNUP, tun, 0);
out:
	SETACTIVE(tdev,1);
	DEV_UNLOCK(tdev);
	return ret;
}

int
ldt_dev_bind2dev (tdev, dev)
	struct ldt_dev	*tdev;
	const char				*dev;
{
	struct net_device	*ndev;
	int					ret;

	if (!tdev || !dev) return -EINVAL;
	ndev = dev_get_by_name (TDEV2NET(tdev), dev);
	if (!ndev) return -ENOENT;
	if (!DEV_LOCK_CHK(tdev)) {
		dev_put (ndev);
		return -EINVAL;
	}
	if (tdev->pdev) dev_put (tdev->pdev);
	tdev->pdev = ndev;
	ret = ldt_tun_rebind (&tdev->tun, 0);
	tdev->tun.pdevdown=0;
	DEV_UNLOCK(tdev);
	return ret;
}
	
int
ldt_dev_bind (tdev, laddr)
	struct ldt_dev	*tdev;
	tp_addr_t				*laddr;
{
	int	ret;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	ret = ldt_tun_bind (&tdev->tun, laddr);
	DEV_UNLOCK(tdev);
	return ret;
}

int
ldt_dev_peer (tdev, raddr)
	struct ldt_dev	*tdev;
	tp_addr_t				*raddr;
{
	int	ret;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	ret = ldt_tun_peer (&tdev->tun, raddr);
	DEV_UNLOCK(tdev);
	return ret;
}

int
ldt_dev_serverstart (tdev)
	struct ldt_dev	*tdev;
{
	int	ret;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	ret = ldt_tun_serverstart (&tdev->tun);
	DEV_UNLOCK(tdev);
	return ret;
}

int
ldt_dev_setqueue (tdev, txlen, qpolicy)
	struct ldt_dev	*tdev;
	int						txlen, qpolicy;
{
	int	ret;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	ret = ldt_tun_setqueue (&tdev->tun, txlen, qpolicy);
	DEV_UNLOCK(tdev);
	return ret;
}


int
ldt_dev_set_mtu (tdev, mtu)
	struct ldt_dev	*tdev;
	int						mtu;
{
	/* locking is done by tpdev_change_mtu */
	if (!tdev || !tdev->ndev) return -EINVAL;
	if (!ISLDTDEV(tdev)) return -EINVAL;
	return tpdev_change_mtu (tdev->ndev, mtu);
}

int
ldt_dev_evsend (tdev, evtype, reason)
	struct ldt_dev	*tdev;
	int				evtype;
	int				reason;
{
	int	kind, ret;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	ret = kind = ldt_event_getkind (evtype);
	if (ret<0) goto out;
	switch (kind) {
	case TP_EVKIND_GLOBAL:
		ret = ldt_event_crsend (evtype, NULL, reason);
		break;
	case TP_EVKIND_TDEV:
		ret = ldt_event_crsend (evtype, tdev, reason);
		break;
	case TP_EVKIND_TUN:
	case TP_EVKIND_TUNCONNECT:
	case TP_EVKIND_TUNFAIL:
		ret = ldt_event_crsend (evtype, &tdev->tun, reason);
		break;
	case TP_EVKIND_NDEV:
		ret = ldt_event_crsend (evtype, tdev->ndev, reason);
		break;
	default:
		ret = -EOPNOTSUPP;
		break;
	}
out:
	DEV_UNLOCK(tdev);
	return ret;
}


/* **************************************
 * watch physical device
 * **************************************/

static
int
tpdev_ndevup (tdev)
	struct ldt_dev	*tdev;
{
	int	ret;

	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	ret = ldt_tun_rebind (&tdev->tun, LDT_TUN_BIND_F_ADDRCHG);
	tdev->tun.pdevdown=0;
	ldt_event_crsend (LDT_EVTYPE_PDEVUP, &tdev->tun, 0);
	DEV_UNLOCK(tdev);
	return ret;
}

static
int
tpdev_ndevdown (tdev)
	struct ldt_dev	*tdev;
{
	if (!DEV_LOCK_CHK(tdev)) return -EINVAL;
	tdev->tun.pdevdown=1;
	ldt_event_crsend (LDT_EVTYPE_PDEVDOWN, &tdev->tun, 0);
	DEV_UNLOCK(tdev);
	return 0;
}






static
int
tp_ev_up (ndev)
	struct net_device	*ndev;
{
	struct ldt_dev	*p;
	struct net_device		*pn;
	struct net				*net;
	int						found=0;

	if (!ndev) return -EINVAL;
	ldt_event_crsend (LDT_EVTYPE_NIFUP, ndev, 0);
	net = NDEV2NET(ndev);
	read_lock (&dev_base_lock);
	for_each_netdev (net, pn) {
		if (!TPDEV_ISLDT(pn)) continue;
		p = LDTDEV(pn);
		if (!p) continue;
		if (ndev == p->pdev) {
			dev_hold (p->ndev);
			found=1;
			break;
		}
	}
	read_unlock (&dev_base_lock);
	if (found) {
		tpdev_ndevup (p);
		dev_put (p->ndev);
	}
	return 0;
}

static
int
tp_ev_down (ndev)
	struct net_device	*ndev;
{
	struct ldt_dev	*p;
	struct net_device		*pn;
	struct net				*net;
	int						found=0;

	if (!ndev) return -EINVAL;
	ldt_event_crsend (LDT_EVTYPE_NIFDOWN, ndev, 0);
	net = NDEV2NET(ndev);
	read_lock (&dev_base_lock);
	for_each_netdev (net, pn) {
		if (!TPDEV_ISLDT(pn)) continue;
		p = LDTDEV(pn);
		if (!p) continue;
		if (ndev == p->pdev) {
			dev_hold (p->ndev);
			found=1;
			break;
		}
	}
	read_unlock (&dev_base_lock);
	if (found) {
		tpdev_ndevdown (p);
		dev_put (p->ndev);
	}
	return 0;
}

static
int
tp_ev_register (
	struct net_device	*ndev)
{
	struct ldt_dev	*tdev;

	if (!ndev) return -EINVAL;
	if (!TPDEV_ISLDT (ndev)) return 0;
	tp_debug ("finish tp device registration");
	tdev = LDTDEV (ndev);
	if (!tdev) return -EINVAL;
	tdev->ctime = tdev->mtime = get_seconds();
	SETACTIVE(tdev,1);
	ldt_event_crsend (LDT_EVTYPE_IFUP, tdev, 0);
	/* we are done */
	tp_info ("ldt device >>%s<< successfully created\n", ndev->name);
	return 0;
}


static
int
tp_ev_ndev (nblk, event, ptr)
	struct notifier_block	*nblk;
	unsigned long				event;
	void							*ptr;
{
	struct net_device			*ndev;

#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0) 
	ndev = netdev_notifier_info_to_dev (ptr);
#else
	ndev = (struct net_device *)ptr;
#endif
	if (!ndev) return NOTIFY_DONE;
	tp_debug ("watch dev >>%s<< triggered event %lu\n", ndev->name, event);
	switch (event) {
	case NETDEV_DOWN:
		tp_ev_down (ndev);
		break;
	case NETDEV_UP:
		tp_ev_up (ndev);
		break;
	case NETDEV_REGISTER:
		tp_ev_register (ndev);
		break;
	}
	return NOTIFY_DONE;
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
