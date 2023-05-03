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

#ifndef _R__KERNEL_LDT_DEV_H
#define _R__KERNEL_LDT_DEV_H

#include <linux/types.h>
#include <linux/netdevice.h>

#include "ldt_addr.h"
#include "ldt_debug.h"
#include "ldt_tun.h"
#include "ldt_lock.h"


#define LDT_MAGIC		((u32)(0xa0ec9374L))
struct ldt_dev {
	u32						MAGIC;
	struct hlist_node		list;
	struct net_device		*ndev;
	struct net_device		*pdev;
	struct tp_lock			lock;
	u32						active;
	u32						hastun:1,
								client:1,
								server:1;
	unsigned long					ctime,mtime;
	struct ldt_tun			tun;
};


#define ISLDTDEV(tdev) ((tdev) && ((tdev)->MAGIC == LDT_MAGIC) \
										&& ((tdev)->ndev))
#define ISLDTDEV2(tdev,_ndev)	(ISLDTDEV(tdev) && (tdev)->ndev == (_ndev))
#define ISACTIVE(tdev) (smp_load_acquire (&((tdev)->active)))
#define ISACTIVETPDEV(tdev) (ISLDTDEV(tdev) && ISACTIVE(tdev))
#define ISLDTNDEV(ndev) (LDTDEV2(ndev) != NULL)
#define TPDEV_ISLDT(ndev) ((ndev)->dev.type && (ndev)->dev.type->name && \
					!strcmp (ndev->dev.type->name, "ldt") && ISLDTNDEV(ndev))

static inline int TDEV_HOLDACTIVE(struct ldt_dev *tdev)
{
	if (!ISACTIVETPDEV(tdev)) return 0;
	dev_hold (tdev->ndev);
	if (!ISACTIVETPDEV(tdev)) {
		dev_put (tdev->ndev);
		return 0;
	}
	return 1;
}
#define TDEV_PUT(tdev)	do { if (ISLDTDEV(tdev)) dev_put ((tdev)->ndev); } while (0)

#define NDEV2NET(ndev)	((ndev)?read_pnet(&((ndev)->nd_net)):(void*)0)
#define TDEV2NET(tdev)	((tdev)?NDEV2NET((tdev)->ndev):(void*)0)


static
inline
struct ldt_dev *
LDTDEV (struct net_device	*ndev)
{
	struct ldt_dev	*tdev;
	if (!ndev) return NULL;
	tdev = (struct ldt_dev*) netdev_priv (ndev);
	if (!ISLDTDEV2(tdev,ndev)) {
		tp_note ("netdevice >>%s<< not a ldt device", ndev->name);
		return NULL;
	}
	return tdev;
};
static
inline
struct ldt_dev *
LDTDEV2 (struct net_device	*ndev)
{
	struct ldt_dev	*tdev;
	if (!ndev) return NULL;
	tdev = (struct ldt_dev*) netdev_priv (ndev);
	if (!ISLDTDEV2(tdev,ndev)) return NULL;
	return tdev;
};


/* Note: calling LDTDEV_BYNAME needs
 *			a dev_put to release the device
 */
static
inline
struct ldt_dev *
LDTDEV_BYNAME (struct net *net, const char *name)
{
	struct ldt_dev	*tdev;
	struct net_device		*ndev = dev_get_by_name (net, name);
	if (!ndev) {
		tp_note ("invalid netdevice >>%s<<", name);
		return NULL;
	}
	tdev = LDTDEV (ndev);
	if (!tdev) dev_put (ndev);
	return tdev;
};


int ldt_dev_global_init (void);
void ldt_dev_global_destroy (void);

int ldt_create_dev (struct net*, const char *name, 
								const char **out_name, int flags);
void ldt_free_dev (struct ldt_dev *tdev);
int ldt_dev_set_group (struct ldt_dev*, const char *name);

int ldt_dev_set_mtu (struct ldt_dev *tdev, int mtu);

int ldt_get_devlist (struct net *net, char**devlist, u32 *dlen);
ssize_t ldt_get_devinfo (struct ldt_dev *tdev, char **info);
ssize_t ldt_get_alldevinfo (struct net *net, char **info);

int ldt_rm_tun (struct ldt_dev *tdev);
int ldt_dev_newtun (struct ldt_dev *tdev, const char *tuntype);
int ldt_dev_bind (struct ldt_dev *tdev, tp_addr_t *laddr);
int ldt_dev_bind2dev (	struct ldt_dev *tdev, const char *dev);
int ldt_dev_peer (struct ldt_dev *tdev, tp_addr_t *raddr);
int ldt_dev_serverstart (struct ldt_dev *tdev);
int ldt_dev_setqueue (struct ldt_dev*, int txlen, int qpolicy);


int ldt_dev_evsend (struct ldt_dev *tdev, int evtype, int reason);



#endif	/* _R__KERNEL_LDT_DEV_H */

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
