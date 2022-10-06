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


#ifndef _R__KERNEL_LDT_TUN_INT_H
#define _R__KERNEL_LDT_TUN_INT_H

#include <linux/types.h>
#include <linux/netdevice.h>
#include "ldt_addr.h"
#include "ldt_dev.h"
struct sock;

#define LDT_TUN_BIND_F_ADDRCHG	0x02

struct ldt_tun;
struct ldt_tunops {
	int (*tp_new)(struct ldt_tun*, const char *);
	int (*tp_bind)(void*, tp_addr_t*, int);
	int (*tp_peer)(void*, tp_addr_t*);
	int (*tp_serverstart)(void*);
	void (*tp_remove)(void*);
	netdev_tx_t (*tp_xmit)(void*,struct sk_buff*);
	int (*tp_prot1xmit)(void*, char*, int, tp_addr_t*);
	ssize_t (*tp_gettuninfo)(void*, char*, size_t);
	int (*tp_createvent)(void*, char*, size_t, const char*, const char*);
	int (*tp_needheadroom)(void*);
	int (*tp_getmtu)(void*);
	int (*tp_setqueue)(void*, int, int);
	int	ipv6;
};


struct ldt_dev;
struct ldt_tun {
	u32							pdevdown:1;
	void							*tundata;
	struct ldt_tunops			*tunops;
	struct ldt_dev				*tdev;
	time_t						ctime, mtime, atime;
};
			
#define TUNSTATADD(tun,key,val) \
	do { (tun)->stats.key += (val); } while (0)
#define TUNSTATINC(tun,key)	TUNSTATADD(tun,key,1)

int ldt_tun_reginit (void);
void ldt_tun_regdeinit (void);

int ldt_tun_init (struct ldt_tun*, const char *);
void ldt_tun_remove (struct ldt_tun *tun);
int ldt_tun_register (const char *, struct ldt_tunops*);
void ldt_tun_unregister (const char *);

#define TUN2NET(tun)		((tun)?TDEV2NET((tun)->tdev):(void*)0)

#include <generated/uapi/linux/version.h>
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,14,0)
# define smp_load_acquire(x) *(x)
# define smp_store_release(x,y) *(x) = (y)
#endif

#define TUNIFACT(tun)		((tun) && (tun)->tunops)
#define TUNIFFUNC(tun,f)	(TUNIFACT(tun) && (tun)->tunops->f)


/* the xmit we leave here for performance reasons 
 * note that the check for existence and rcu_read_lock is
 * already performt in ldt_dev - so don't do it again
 */
static
inline
netdev_tx_t
ldt_tun_xmit (struct ldt_tun *tun, struct sk_buff *skb)
{
	if (!TUNIFACT(tun)) return NETDEV_TX_BUSY;
	return tun->tunops->tp_xmit (tun->tundata, skb);
};


int ldt_tun_bind (struct ldt_tun*, tp_addr_t *addr);
int ldt_tun_rebind (struct ldt_tun*, int flags);
int ldt_tun_peer (struct ldt_tun*, tp_addr_t *addr);
int ldt_tun_serverstart (struct ldt_tun*);
int ldt_tun_getmtu (struct ldt_tun *tun);
int ldt_tun_needheadroom (struct ldt_tun *tun);
ssize_t ldt_tun_gettuninfo (	struct ldt_tun *tun, char *buf,
											size_t blen);
int ldt_tun_createvent (	struct ldt_tun *tun, char *evbuf,
										size_t evlen, const char *evtype,
										const char *desc);
int ldt_tun_prot1xmit (struct ldt_tun *tun, char *buf, int blen,
									tp_addr_t *addr, int force);

int ldt_tun_setqueue (struct ldt_tun*, int txlen, int qpolicy);



#endif	/* _R__KERNEL_LDT_TUN_INT_H */

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
