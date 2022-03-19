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

#include <linux/kernel.h>
#include <linux/kobject.h>
#include <linux/types.h>
#include <linux/string.h>
#include <linux/init.h>
#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/inetdevice.h>
#include <net/if_inet6.h>
#include <linux/in.h>
#include <linux/in6.h>

#include "ldt_dev.h"
#include "ldt_tun.h"
#include "ldt_cfg.h"
#include "ldt_kernel.h"
#include "ldt_event.h"
#include "ldt_lock.h"

#define TUNFUNCHK(tun,f) { if (!(tun)) return -EINVAL; else if (!TUNIFFUNC((tun),f)) return 0; }
#define SETACTIVE(tun,on)	do { smp_store_release (&((tun)->active), (on)); } while (0)

static struct ldt_tunops* tp_findtun (const char*);
static int tptun_rebind (struct ldt_tun*);

int
ldt_tun_init (tun, type)
	struct ldt_tun	*tun;
	const char		*type;
{
	int	ret;

	if (!tun || !tun->tdev) return -EINVAL;
	if (!type) {
#if IS_ENABLED(CONFIG_IP_MPDCCP)
		type = "mpdccp4";
#else
		type = "dccp4";
#endif
	}
	*tun = (struct ldt_tun) { .tdev = tun->tdev };
	if (ldt_cfg_enable_debug >= 2)
		printk ("ldt_tuninit(): create tunnel of type %s\n", type);
	tun->tunops = tp_findtun (type);
	if (!tun->tunops) {
		printk ("ldt_tuninit(): cannot find tunnel type %s\n", type);
		return -ENOENT;
	}
	
	tun->ctime = tun->mtime = tun->atime = get_seconds();
	ret = tun->tunops->tp_new (tun, type);
	if (ret < 0) {
		if (ldt_cfg_enable_debug)
			printk ("ldt_tuninit(): error setting up tunnel %s: %d\n", type, ret);
		ldt_tun_remove (tun);
		tun->tunops = NULL;
		return ret;
	}
	SETACTIVE (tun, 1);
	if (ldt_cfg_enable_debug >= 2)
		printk ("ldt_tuninit(): tunnel creating successfully\n");
	return 0;
}


void
ldt_tun_remove (tun)
	struct ldt_tun	*tun;
{
	if (!tun || !tun->tunops) return;
	SETACTIVE (tun, 0);
	if (tun->tunops && tun->tunops->tp_remove) 
		tun->tunops->tp_remove(tun->tundata);
	*tun = (struct ldt_tun) { .tdev = tun->tdev, .id = tun->id, };
};


int
ldt_tun_bind (tun, addr, dev, flags)
	struct ldt_tun	*tun;
	tp_addr_t		*addr;
	const char		*dev;
	int				flags;
{
	int			ret=0;

	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt_tun_bind(): enter\n");
	TUNFUNCHK(tun,tp_bind);
	SETACTIVE (tun, 0);
	if (addr) {
		if (ldt_cfg_enable_debug >= 3)
			printk ("ldt_tun_bind(): call tp_bind\n");
		ret = tun->tunops->tp_bind(tun->tundata, addr, 0);
	}
	if (dev) {
		strncpy (tun->pdev, dev, sizeof (tun->pdev)-1);
		tun->pdev[sizeof(tun->pdev)-1] = 0;
	}
	if (dev && !addr) {
		if (ldt_cfg_enable_debug >= 3)
			printk ("ldt_tun_bind(): rebind\n");
		tun->devbound=1;
		ret = tptun_rebind (tun);
	} else {
		tun->devbound=0;
	}
	tun->mtime = get_seconds();
	if (ret == 0) 
		ldt_event_crsend (LDT_EVTYPE_REBIND, tun, 0);
	tun->isbound = 1;
	SETACTIVE (tun, 1);
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt_tun_bind(): done\n");
	return ret;
}

int
ldt_tun_rebind (tun)
	struct ldt_tun	*tun;
{
	int	ret;

	TUNFUNCHK(tun,tp_bind);
	SETACTIVE (tun, 0);
	ret = tptun_rebind (tun);
	SETACTIVE (tun, 1);
	return ret;
}

static
int
tptun_rebind (tun)
	struct ldt_tun	*tun;
{
	struct net_device	*ndev;
	tp_addr_t			ad;
	int					ret;

	if (!tun) return -EINVAL;
	if (!tun->devbound) return 0;
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt::tptun_rebind(): enter\n");
	ndev = dev_get_by_name (TUN2NET(tun), tun->pdev);
	if (!ndev) return -ENOENT;
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt::tptun_rebind(): search address\n");
#if defined (CONFIG_IPV6) && LINUX_VERSION_CODE >= KERNEL_VERSION(3,11,0)
	if (tun->tunops->ipv6) {
		struct inet6_ifaddr  *ia6;
		list_for_each_entry(ia6, &(ndev->ip6_ptr->addr_list), if_list) {
			tp_addr_setipv6 (&ad, ia6->addr.s6_addr, 0);
			break;
		}
	} else
#endif
		tp_addr_setipv4 (&ad, ndev->ip_ptr->ifa_list->ifa_local, 0);
	dev_put (ndev);
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt::tptun_rebind(): call tp_bind\n");
	ret = tun->tunops->tp_bind(tun->tundata, &ad, 
										tun->devbound ? LDT_TUN_BIND_F_ADDRCHG : 0);
	if (ret < 0) return ret;
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt::tptun_rebind(): done\n");
	ldt_event_crsend (LDT_EVTYPE_REBIND, tun, 0);
	tun->isbound = 1;
	return 0;
}

int
ldt_tun_peer (tun, addr)
	struct ldt_tun	*tun;
	tp_addr_t				*addr;
{
	int	ret;

	TUNFUNCHK(tun,tp_peer);
	SETACTIVE (tun, 0);
	ret = tun->tunops->tp_peer(tun->tundata, addr);
	tun->mtime = get_seconds();
	if (ret == 0) 
		ldt_event_crsend (LDT_EVTYPE_REBIND, tun, 0);
	SETACTIVE (tun, 1);
	return ret;
}

int
ldt_tun_serverstart (tun)
	struct ldt_tun	*tun;
{
	int	ret;

	TUNFUNCHK(tun,tp_serverstart);
	SETACTIVE (tun, 0);
	ret = tun->tunops->tp_serverstart (tun->tundata);
	SETACTIVE (tun, 1);
	return ret;
}

int
ldt_tun_setqueue (tun, txlen, qpolicy)
	struct ldt_tun	*tun;
	int						txlen, qpolicy;
{
	int	ret;

	TUNFUNCHK(tun,tp_setqueue);
	SETACTIVE (tun, 0);
	ret = tun->tunops->tp_setqueue (tun->tundata, txlen, qpolicy);
	SETACTIVE (tun, 1);
	return ret;
}


int
ldt_tun_getmtu (tun)
	struct ldt_tun	*tun;
{
	TUNFUNCHK(tun,tp_getmtu);
	return tun->tunops->tp_getmtu(tun->tundata);
}

int
ldt_tun_needheadroom (tun)
	struct ldt_tun	*tun;
{
	TUNFUNCHK(tun,tp_needheadroom);
	return tun->tunops->tp_needheadroom(tun->tundata);
}

ssize_t
ldt_tun_gettuninfo (tun, buf, blen)
	struct ldt_tun	*tun;
	char						*buf;
	size_t					blen;
{
	TUNFUNCHK(tun,tp_gettuninfo);
	return tun->tunops->tp_gettuninfo (tun->tundata, buf, blen);
}

int
ldt_tun_maxmtu (tun)
	struct ldt_tun	*tun;
{
	TUNFUNCHK(tun, tp_maxmtu);
	return tun->tunops->tp_maxmtu (tun->tundata);
}


int
ldt_tun_createvent (tun, evbuf, evlen, evtype, desc)
	struct ldt_tun	*tun;
	char						*evbuf;
	size_t					evlen;
	const char				*evtype;
	const char				*desc;
{
	TUNFUNCHK(tun,tp_createvent);
	return tun->tunops->tp_createvent(tun->tundata, evbuf, evlen, evtype, desc);
}


int
ldt_tun_prot1xmit (tun, buf, blen, addr, force)
	struct ldt_tun *tun;
	char					*buf;
	int					blen;
	tp_addr_t			*addr;
	int					force;
{
	int	ret;

	if (!tun || !tun->tunops || !tun->tunops->tp_prot1xmit) return -EINVAL;
	if (!force && !TUNISACTIVE(tun)) {
		return -EAGAIN;
	}
	ret = tun->tunops->tp_prot1xmit (tun->tundata, buf, blen, addr);
	return ret;
}





struct tun_reg {
	const char					*type;
	struct ldt_tunops	*tunops;
	struct tun_reg				*next, *prev;
};
static struct tun_reg	*tun_reg_list = NULL;

//static DEFINE_MUTEX(tun_mutex);
static struct tp_lock   tun_mutex;
#define G_LOCK  do { tp_lock (&tun_mutex); } while (0)
#define G_UNLOCK  do { tp_unlock (&tun_mutex); } while (0)

static struct tun_reg* tun_reg_find_lock (const char*);
static struct tun_reg* tun_reg_find (const char*);

int
ldt_tun_reginit (void)
{
	tp_lock_init (&tun_mutex);
	return 0;
}
void
ldt_tun_regdeinit (void)
{
	tp_lock_destroy (&tun_mutex);
}

int
ldt_tun_register (type, tunops)
	const char					*type;
	struct ldt_tunops	*tunops;
{
	struct tun_reg	*p;

	if (!type || !*type || !tunops || !tunops->tp_new) return -EINVAL;
	G_LOCK;
	p = tun_reg_find (type);
	if (p) {
		G_UNLOCK;
		return -EEXIST;
	}
	p = kmalloc (sizeof (struct tun_reg), GFP_KERNEL);
	if (!p) {
		G_UNLOCK;
		return -ENOMEM;
	}
	*p = (struct tun_reg) { .type = type, .tunops = tunops };
	p->next = tun_reg_list;
	if (tun_reg_list) {
		p->prev = tun_reg_list->prev;
		tun_reg_list->prev = p;
	} else {
		p->prev = p;
	}
	tun_reg_list = p;
	G_UNLOCK;
	return 0;
}


void
ldt_tun_unregister (type)
	const char					*type;
{
	struct tun_reg	*p;

	if (!type || !*type) return;
	G_LOCK;
	p = tun_reg_find (type);
	if (!p) {
		G_UNLOCK;
		return;
	}
	if (p->next) p->next->prev = p->prev;
	if (p == tun_reg_list)
		tun_reg_list = p->next;
	else
		p->prev->next = p->next;
	G_UNLOCK;
	*p = (struct tun_reg) { .type = "" };
	kfree (p);
}

static
struct ldt_tunops*
tp_findtun (type)
	const char	*type;
{
	struct tun_reg	*p;

	p = tun_reg_find_lock (type);
	if (!p) return NULL;
	return p->tunops;
}

static
struct tun_reg*
tun_reg_find_lock (type)
	const char	*type;
{
	struct tun_reg	*p;

	G_LOCK;
	p = tun_reg_find (type);
	G_UNLOCK;
	return p;
}

static
struct tun_reg*
tun_reg_find (type)
	const char	*type;
{
	struct tun_reg	*p;

	if (!type || !*type) return NULL;
	for (p=tun_reg_list; p; p=p->next)
		if (!strcasecmp (p->type, type)) return p;
	return NULL;
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
