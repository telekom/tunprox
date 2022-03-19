/*
 * Copyright (C) 2015-2021 by Frank Reker, Deutsche Telekom AG
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


#include "ldt_kernel.h"
#include "ldt_cfg.h"
#include "ldt_dev.h"
#include "ldt_tun.h"
#include "ldt_event.h"
#include "ldt_lock.h"


static void ldt_dev_setup (struct net_device *);
static int ldt_dev_init (struct ldt_dev *, int);
static void ldt_uninit (struct net_device*);
static void ldt_free_dev2 (struct ldt_dev*);
static void unlinkdev (struct ldt_dev*);
static void linkdev (struct ldt_dev*);
static netdev_tx_t ldt_xmit (struct sk_buff*, struct net_device*);
static int change_mtu (struct net_device*, int);
static ssize_t get_devinfo (struct ldt_dev *, char*, size_t);
static ssize_t get_alldevinfo (struct net *, char*, size_t);


int ldt_numdev = 0;
static struct ldt_dev	*ldt_devlist = NULL;

//static DEFINE_MUTEX(ldt_mutex);
static struct tp_lock	ldt_mutex;
#define G_LOCK  do { tp_lock (&ldt_mutex); } while (0)
#define G_UNLOCK  do { tp_unlock (&ldt_mutex); } while (0)

#define SETACTIVE(tdev,on) do { smp_store_release (&(tdev->active), on); } while (0)
#define CHKTDEV(tdev,ret) { \
		if (!ISACTIVETPDEV(tdev)) { \
			G_UNLOCK; \
			return ret;\
		}\
	}

int
ldt_dev_global_init ()
{
	tp_lock_init (&ldt_mutex);
	return 0;
}
int
ldt_dev_global_destroy ()
{
	tp_lock_destroy (&ldt_mutex);
	return 0;
}


int
ldt_create_dev (net, name, out_name, type, flags)
	struct net	*net;
	const char	*name;
	const char	**out_name;
	const char	*type;
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
		printk ("ldt_create_dev(): name >>%s<< too long (max=%d)\n", name,
						IFNAMSIZ-1);
		return -EINVAL;
	}
#ifdef NET_NAME_ENUM
	ndev = alloc_netdev_mqs (	sizeof (struct ldt_dev), name, assigntype,
										ldt_dev_setup, 1, 1);
#else
	ndev = alloc_netdev_mqs (	sizeof (struct ldt_dev), name,
										ldt_dev_setup, 1, 1);
#endif
	if (!ndev) return -ENOMEM;
#ifdef CONFIG_NET_NS
	dev_net_set(ndev, net);
#endif
	tdev = (struct ldt_dev*) netdev_priv (ndev);
	G_LOCK;
	ret = ldt_dev_init (tdev, flags);
	if (ret < 0) {
		free_netdev(ndev);
		G_UNLOCK;
		return ret;
	}
	tdev->MAGIC = LDT_MAGIC;
	/* now register device */
	ret = register_netdev (ndev);
	if (ret < 0) {
		printk ("register ldt device %s failed\n", ndev->name);
		ldt_free_dev2 (tdev);
		tdev->MAGIC = 0;
		free_netdev (ndev);
		G_UNLOCK;
		return ret;
	}

	if (out_name) *out_name = ndev->name;
	tdev->ctime = tdev->mtime = get_seconds();
	ret = ldt_tun_init (&tdev->tun, type);
	if (ret < 0) {
		printk ("error initializing %s tunnel: %d\n", type?type:"default", ret);
		unregister_netdev (tdev->ndev);
		tdev->MAGIC = 0;
		free_netdev (tdev->ndev);
		G_UNLOCK;
		return ret;
	}
	SETACTIVE(tdev,1);
	G_UNLOCK;
	ldt_event_crsend (LDT_EVTYPE_IFUP, tdev, 0);
	/* we are done */
	if (ldt_cfg_enable_debug) {
		printk ("ldt device >>%s<< successfully created\n", ndev->name);
	}
	return 0;
}

static const struct net_device_ops	ldt_devops = {
	.ndo_start_xmit = ldt_xmit,
	.ndo_set_mac_address = eth_mac_addr,
	.ndo_change_mtu = change_mtu,
	.ndo_uninit = ldt_uninit,
};

/* do some basic setups the rest is done in ldt_dev_init() */
static
void
ldt_dev_setup (ndev)
	struct net_device	*ndev;
{
	struct ldt_dev	*tdev;

	/* lock must be held by caller */
	if (!ndev) return;
	ndev->netdev_ops = &ldt_devops;
	tdev = (struct ldt_dev*) netdev_priv (ndev);
	*tdev = (struct ldt_dev) {
		.ndev = ndev,
	};
	ndev->mtu = 1300;
	ndev->type = ARPHRD_PPP;
	ndev->flags = IFF_POINTOPOINT | IFF_NOARP | IFF_MULTICAST;
	ndev->tx_queue_len = 10000;
	return;
}

static
int
ldt_dev_init (tdev, flags)
	struct ldt_dev	*tdev;
	int						flags;
{
	if (!tdev) return -EINVAL;

	/* lock must be held by caller */
	/* do the real setup */
	tdev->tun = (struct ldt_tun){ .tdev = tdev };
	/* eval flags */
	if (flags & LDT_CREATE_DEV_F_CLIENT)
		tdev->client = 1;
	else if (flags & LDT_CREATE_DEV_F_SERVER)
		tdev->server = 1;
	/* link the device */
	linkdev (tdev);
	return 0;
}


static
void
linkdev (tdev)
	struct ldt_dev	*tdev;
{
	if (!tdev) return;
	/* lock must be held by caller */
	tdev->next = NULL;
	if (ldt_devlist) {
		ldt_devlist->prev->next = tdev;
		tdev->prev = ldt_devlist->prev;
		ldt_devlist->prev = tdev;
	} else {
		ldt_devlist = tdev;
		tdev->prev = tdev;
	}
	ldt_numdev++;
	tdev->islinked = 1;
	return;
}

static
void
unlinkdev (tdev)
	struct ldt_dev	*tdev;
{
	if (!tdev || !tdev->islinked) return;
	/* lock must be held by caller */
	if (tdev == ldt_devlist) {
		ldt_devlist = tdev->next;
	} else {
		tdev->prev->next = tdev->next;
	}
	if (tdev->next) {
		tdev->next->prev = tdev->prev;
	} else if (ldt_devlist) {
		ldt_devlist->prev = tdev->prev;
	}
	tdev->next = NULL;
	tdev->prev = NULL;
	tdev->islinked = 0;
	ldt_numdev--;
	return;
}


void
ldt_free_dev (tdev)
	struct ldt_dev	*tdev;
{
	G_LOCK;
	CHKTDEV(tdev,)
	SETACTIVE(tdev,0);
	if (ldt_cfg_enable_debug)
		printk ("ldt: remove dev %s\n", tdev->ndev->name);
	ldt_event_crsend (LDT_EVTYPE_IFDOWN, tdev, 0);
	//ldt_free_dev2 (tdev);
	dev_put (tdev->ndev);
	unregister_netdev (tdev->ndev);
	free_netdev (tdev->ndev);
	G_UNLOCK;
	/* note: from here on tdev does not exist any more!! */
	return;
}

static
void
ldt_uninit (ndev)
	struct net_device		*ndev;
{
	struct ldt_dev	*tdev;

	tdev = LDTDEV (ndev);
	if (tdev && ldt_cfg_enable_debug)
		printk ("ldt: uninit dev %s\n", tdev->ndev->name);
	ldt_free_dev2 (tdev);
	return;
}


static
void
ldt_free_dev2 (tdev)
	struct ldt_dev	*tdev;
{
	/* lock must be held by caller */
	if (!tdev) return;
	tdev->MAGIC = 0;
	/* unlink if linked */
	if (tdev->islinked) {
		unlinkdev (tdev);
	}

	/* remove tunnel */
	ldt_tun_remove (&tdev->tun);

	tdev->setmtu = 0;
	return;
}

void
ldt_dev_remove_all ()
{
	if (ldt_cfg_enable_debug)
		printk ("ldt: remove all devices\n");
	while (ldt_devlist) {
		dev_hold (ldt_devlist->ndev);
		ldt_free_dev (ldt_devlist);
	}
}

/*      Called when a packet needs to be transmitted.
 *      Must return NETDEV_TX_OK , NETDEV_TX_BUSY.
 *        (can also return NETDEV_TX_LOCKED iff NETIF_F_LLTX)
 */
static
netdev_tx_t
ldt_xmit (skb, dev)
	struct sk_buff		*skb;
	struct net_device	*dev;
{
	struct ldt_dev	*tdev;
	netdev_tx_t				ret;

	if (!dev || !skb) return 0;
	tdev = LDTDEV (dev);
	if (!tdev) {
		/* drop packet */
		kfree_skb (skb);
		return NETDEV_TX_OK;
	}
	if (!ISACTIVE(tdev)) {
		return NETDEV_TX_BUSY;
	}
	if (!TUNIFFUNC(&tdev->tun,tp_xmit)) {
		/* drop packet */
		kfree_skb (skb);
		return NETDEV_TX_OK;
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
change_mtu (ndev, new_mtu)
	struct net_device	*ndev;
	int					new_mtu;
{
	struct ldt_dev	*tdev;
	int						ret = -ERANGE;

	if (!ndev) return -EINVAL;
	G_LOCK;
	tdev = LDTDEV (ndev);
	CHKTDEV(tdev,0);
	if (new_mtu < 1280) goto out;
	if (new_mtu > 65535) goto out;
	ndev->mtu = new_mtu;
	tdev->setmtu = 1;
	ret = 0;
	tdev->mtime = get_seconds();
out:
	G_UNLOCK;
	return 0;
}

int
ldt_get_devlist (net, devlist, dlen)
	struct net	*net;
	char			**devlist;
	u32			*dlen;
{
	struct ldt_dev	*p;
	int						len, num;
	char						*s;

	if (!net || !devlist || !dlen) return -EINVAL;
	G_LOCK;
	for (p=ldt_devlist, len=0, num=0; p; p=p->next) {
#ifdef CONFIG_NET_NS
		if (TDEV2NET (p) != net) continue;
#endif
		len+=strlen (p->ndev->name) + 1;
		num++;
	}
	*devlist = kmalloc (len+1, GFP_KERNEL);
	if (!*devlist) {
		G_UNLOCK;
		return -ENOMEM;
	}
	s=*devlist;
	*s = 0;
	for (p=ldt_devlist; p; p=p->next) {
#ifdef CONFIG_NET_NS
		if (TDEV2NET (p) != net) continue;
#endif
		strcpy (s, p->ndev->name);
		s += strlen (p->ndev->name)+1;
	}
	*dlen = len ? len : 1;
	G_UNLOCK;
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
	G_LOCK;
	blen = get_devinfo (tdev, NULL, 0);
	if (blen < 0 || !info) {
		G_UNLOCK;
		return blen;
	}
	buf = kmalloc (blen+1, GFP_KERNEL);
	*info = buf;
	if (!buf) {
		G_UNLOCK;
		return -ENOMEM;
	}
	len = get_devinfo (tdev, buf, blen+1);
	G_UNLOCK;
	if (len < 0) return len;
	if (len > blen) {
		printk ("ldt_get_devinfo(): write truncated\n");
	}
	return len;
}

ssize_t
ldt_get_alldevinfo (net, info)
	struct net	*net;
	char			**info;
{
	char		*buf=NULL;
	ssize_t	len, blen;

	if (!net || !info) return -EINVAL;
	G_LOCK;
	blen = get_alldevinfo (net, NULL, 0);
	if (blen < 0 || !info) {
		G_UNLOCK;
		return blen;
	}
	buf = kmalloc (blen+1, GFP_KERNEL);
	*info = buf;
	if (!buf) {
		G_UNLOCK;
		return -ENOMEM;
	}
	len = get_alldevinfo (net, buf, blen+1);
	G_UNLOCK;
	if (len < 0) return len;
	if (len > blen) {
		printk ("ldt_get_alldevinfo(): write truncated\n");
	}
	return len;
}


static
ssize_t
get_alldevinfo (net, info, ilen)
	struct net	*net;
	char			*info;
	size_t		ilen;
{
	struct ldt_dev	*p;
	ssize_t					len=0, ret;

	/* lock must be held by caller */
#ifdef CONFIG_NET_NS
	if (!net) return -EINVAL;
#endif
#define _FSTR	(info ? info + len : NULL)
#define _FLEN	(ilen > len ? ilen - len : 0)
	len += snprintf (_FSTR, _FLEN, "<ldtlist>\n");
	for (p=ldt_devlist; p; p=p->next) {
#ifdef CONFIG_NET_NS
		if (TDEV2NET (p) != net) continue;
#endif
		ret = get_devinfo (p, _FSTR, _FLEN);
		if (ret < 0) return ret;
		len += ret;
	}
	len += snprintf (_FSTR, _FLEN, "</ldtlist>\n");
	return len;
#undef _FSTR
#undef _FLEN
}


static
ssize_t
get_devinfo (tdev, info, ilen)
	struct ldt_dev	*tdev;
	char				*info;
	size_t			ilen;
{
	ssize_t			len=0, ret;

	/* lock must be held by caller */
	if (!ISLDTDEV(tdev) || !ISACTIVE(tdev)) return -EINVAL;
#define _FSTR	(info ? info + len : NULL)
#define _FLEN	(ilen > len ? ilen - len : 0)
	len += snprintf (_FSTR, _FLEN, "<dev name=\"%s\">\n", tdev->ndev->name);
	len += snprintf (_FSTR, _FLEN, "  <mtu>%d</mtu>\n", (int)tdev->ndev->mtu);
	len += snprintf (_FSTR, _FLEN, "  <ctime>%lld</ctime><mtime>%lld</mtime>\n",
												(long long)tdev->ctime, (long long)tdev->mtime);
	ret = ldt_tun_gettuninfo (&tdev->tun, _FSTR, _FLEN);
	if (ret < 0) {
		printk ("warning: cannot get info from tunnel device %s: "
					"%d\n", tdev->ndev->name, (int)ret);
	} else {
		len += ret;
	}
	len += snprintf (_FSTR, _FLEN, "</dev>\n");
	return len;
#undef _FSTR
#undef _FLEN
}


int
ldt_dev_bind (tdev, laddr, dev, flags)
	struct ldt_dev	*tdev;
	tp_addr_t		*laddr;
	const char		*dev;
	int				flags;
{
	int						ret;
	struct ldt_tun	*tun;

	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt_dev_bind(): enter\n");
	G_LOCK;
	CHKTDEV(tdev,-EINVAL);
	tun = &tdev->tun;
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt_dev_bind(): call tun_bind()\n");
	ret = ldt_tun_bind (tun, laddr, dev, flags);
	G_UNLOCK;
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt_dev_bind(): done -> %d\n", ret);
	return ret;
}

int
ldt_dev_peer (tdev, raddr)
	struct ldt_dev	*tdev;
	tp_addr_t				*raddr;
{
	int	ret;

	G_LOCK;
	CHKTDEV(tdev,-EINVAL);
	ret = ldt_tun_peer (&tdev->tun, raddr);
	G_UNLOCK;
	return ret;
}

int
ldt_dev_serverstart (tdev)
	struct ldt_dev	*tdev;
{
	int	ret;

	G_LOCK;
	CHKTDEV(tdev,-EINVAL);
	ret = ldt_tun_serverstart (&tdev->tun);
	G_UNLOCK;
	return ret;
}

int
ldt_dev_setqueue (tdev, txlen, qpolicy)
	struct ldt_dev	*tdev;
	int						txlen, qpolicy;
{
	int	ret;

	G_LOCK;
	CHKTDEV(tdev,-EINVAL);
	ret = ldt_tun_setqueue (&tdev->tun, txlen, qpolicy);
	G_UNLOCK;
	return ret;
}


int
ldt_dev_set_mtu (tdev, mtu)
	struct ldt_dev	*tdev;
	int						mtu;
{
	/* locking is done by change_mtu */
	if (!tdev || !tdev->ndev) return -EINVAL;
	if (!ISLDTDEV(tdev)) return -EINVAL;
	return change_mtu (tdev->ndev, mtu);
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
