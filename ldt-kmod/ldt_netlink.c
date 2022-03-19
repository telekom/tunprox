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
#include <linux/version.h>

#include "ldt_kernel.h"
#include "ldt_version.h"
#include "ldt_dev.h"
#include "ldt_cfg.h"
#include "ldt_event.h"
#include "ldt_netlink.h"
#include "ldt_lock.h"


int ldt_nl_family_id = 0;

//static DEFINE_MUTEX(ldt_nl_mutex);
static struct tp_lock ldt_nl_mutex;
#define G_LOCK		do { tp_lock (&ldt_nl_mutex); } while (0)
#define G_UNLOCK	do { tp_unlock (&ldt_nl_mutex); } while (0)

static int ldt_nl_create_dev (struct sk_buff*, struct genl_info*);
static int ldt_nl_rm_dev (struct sk_buff*, struct genl_info*);
static int ldt_nl_get_version (struct sk_buff*, struct genl_info*);
static int ldt_nl_get_devlist (struct sk_buff*, struct genl_info*);
static int ldt_nl_show_dev (struct sk_buff*, struct genl_info*);
static int ldt_nl_show_devlist (struct sk_buff*, struct genl_info*);
static int ldt_nl_bind (struct sk_buff*, struct genl_info*);
static int ldt_nl_peer (struct sk_buff*, struct genl_info*);
static int ldt_nl_serverstart (struct sk_buff*, struct genl_info*);
static int ldt_nl_set_mtu (struct sk_buff*, struct genl_info*);
static int ldt_nl_set_queue (struct sk_buff*, struct genl_info*);
static int ldt_nl_ping (struct sk_buff*, struct genl_info*);
static int ldt_nl_subscribe (struct sk_buff*, struct genl_info*);

static int ldt_nl_release_notifier (struct notifier_block*, unsigned long, void*);
static int send_info (struct net*, u32, int, const char *, u32);
static int send_ret (struct net*, u32, int);
static int ldt_nl_adduser (u32, struct net*);
static void ldt_nl_rmuser (u32, struct net*);
static void ldt_nl_rmalluser (void);
static int do_send_event (struct net*, u32, u32, u32, const char*);





static const struct nla_policy ldt_nl_policy_create_dev[LDT_CMD_CREATE_DEV_ATTR_MAX + 1] = {
	[LDT_CMD_CREATE_DEV_ATTR_NAME]	= {	.type = NLA_NUL_STRING },
	[LDT_CMD_CREATE_DEV_ATTR_FLAGS]	= {	.type = NLA_U32 },
};

static const struct nla_policy ldt_nl_policy_rm_dev[LDT_CMD_RM_DEV_ATTR_MAX + 1] = {
	[LDT_CMD_RM_DEV_ATTR_NAME]	= {	.type = NLA_NUL_STRING },
};


static const struct nla_policy ldt_nl_policy_get_version[LDT_CMD_GET_VERSION_ATTR_MAX+1] = {};

static const struct nla_policy ldt_nl_policy_get_devlist[LDT_CMD_GET_DEVLIST_ATTR_MAX+1] = {};

static const struct nla_policy ldt_nl_policy_show_dev[LDT_CMD_SHOW_DEV_ATTR_MAX+1] = {
	[LDT_CMD_SHOW_DEV_ATTR_NAME]	= {	.type = NLA_NUL_STRING },
};

static const struct nla_policy ldt_nl_policy_show_devlist[LDT_CMD_SHOW_DEVLIST_ATTR_MAX+1] = {};


static const struct nla_policy ldt_nl_policy_bind[LDT_CMD_BIND_ATTR_MAX + 1] = {
	[LDT_CMD_BIND_ATTR_NAME] 		= {	.type = NLA_NUL_STRING },
	[LDT_CMD_BIND_ATTR_ADDR4]		= {	.type = NLA_U32 },
	[LDT_CMD_BIND_ATTR_ADDR6] 	= {	.type = NLA_BINARY },
	[LDT_CMD_BIND_ATTR_PORT]		= {	.type = NLA_U16 },
	[LDT_CMD_BIND_ATTR_FLAGS]		= {	.type = NLA_U32 },
};

static const struct nla_policy ldt_nl_policy_peer[LDT_CMD_PEER_ATTR_MAX + 1] = {
	[LDT_CMD_PEER_ATTR_NAME] 	= {	.type = NLA_NUL_STRING },
	[LDT_CMD_PEER_ATTR_ADDR4]	= {	.type = NLA_U32 },
	[LDT_CMD_PEER_ATTR_ADDR6] = {	.type = NLA_BINARY },
	[LDT_CMD_PEER_ATTR_PORT]	= {	.type = NLA_U16 },
};

static const struct nla_policy ldt_nl_policy_serverstart[LDT_CMD_SERVERSTART_ATTR_MAX + 1] = {
	[LDT_CMD_SERVERSTART_ATTR_NAME] 	= {	.type = NLA_NUL_STRING },
};

static const struct nla_policy ldt_nl_policy_set_mtu[LDT_CMD_SET_MTU_ATTR_MAX + 1] = {
	[LDT_CMD_SET_MTU_ATTR_NAME]		= { .type = NLA_NUL_STRING },
	[LDT_CMD_SET_MTU_ATTR_MTU]		= { .type = NLA_U32 },
};

static const struct nla_policy ldt_nl_policy_set_queue[LDT_CMD_SETQUEUE_ATTR_MAX + 1] = {
   [LDT_CMD_SETQUEUE_ATTR_NAME]		= { .type = NLA_NUL_STRING },
   [LDT_CMD_SETQUEUE_ATTR_TXQLEN]	= { .type = NLA_U16 },
   [LDT_CMD_SETQUEUE_ATTR_QPOLICY]	= { .type = NLA_U16 },
};


static const struct nla_policy ldt_nl_policy_ping[LDT_CMD_PING_ATTR_MAX + 1] = {
	[LDT_CMD_PING_ATTR_DATA]		= { .type = NLA_U32 },
};


static const struct nla_policy ldt_nl_policy_subscribe[LDT_CMD_GET_VERSION_ATTR_MAX+1] = {};


static const struct genl_ops ldt_nl_ops[] = {
	{
		.cmd = LDT_CMD_CREATE_DEV,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_create_dev,
		.policy = ldt_nl_policy_create_dev,
	},
	{
		.cmd = LDT_CMD_RM_DEV,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_rm_dev,
		.policy = ldt_nl_policy_rm_dev,
	},
	{
		.cmd = LDT_CMD_GET_VERSION,
		.doit = ldt_nl_get_version,
		.policy = ldt_nl_policy_get_version,
		/* can be retrieved by unprivileged users */
	},
	{
		.cmd = LDT_CMD_SHOW_DEV,
		.doit = ldt_nl_show_dev,
		.policy = ldt_nl_policy_show_dev,
		/* can be retrieved by unprivileged users */
	},
	{
		.cmd = LDT_CMD_GET_DEVLIST,
		.doit = ldt_nl_get_devlist,
		.policy = ldt_nl_policy_get_devlist,
		/* can be retrieved by unprivileged users */
	},
	{
		.cmd = LDT_CMD_SHOW_DEVLIST,
		.doit = ldt_nl_show_devlist,
		.policy = ldt_nl_policy_show_devlist,
		/* can be retrieved by unprivileged users */
	},
	{
		.cmd = LDT_CMD_BIND,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_bind,
		.policy = ldt_nl_policy_bind,
	},
	{
		.cmd = LDT_CMD_PEER,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_peer,
		.policy = ldt_nl_policy_peer,
	},
	{
		.cmd = LDT_CMD_SERVERSTART,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_serverstart,
		.policy = ldt_nl_policy_serverstart,
	},
	{
		.cmd = LDT_CMD_SET_MTU,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_set_mtu,
		.policy = ldt_nl_policy_set_mtu,
	},
	{
		.cmd = LDT_CMD_SET_QUEUE,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_set_queue,
		.policy = ldt_nl_policy_set_queue,
	},
	{
		.cmd = LDT_CMD_PING,
		.flags = GENL_ADMIN_PERM,
		.doit = ldt_nl_ping,
		.policy = ldt_nl_policy_ping,
	},
	{
		.cmd = LDT_CMD_SUBSCRIBE,
		.doit = ldt_nl_subscribe,
		.policy = ldt_nl_policy_subscribe,
		/* can be retrieved by unprivileged users */
	},
};

static struct genl_family ldt_nl_family = {
	.name = LDT_NAME,
	.hdrsize = 0,
	.version = LDT_NL_VERSION,
	.maxattr = LDT_CMD_MAX,
	.netnsok = true,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) 
	.id = GENL_ID_GENERATE,
#else
	.module = THIS_MODULE,
	.ops = ldt_nl_ops,
	.n_ops = ARRAY_SIZE (ldt_nl_ops),
#endif
};



static struct notifier_block ldt_netlink_notifier = {
        .notifier_call = ldt_nl_release_notifier
};

struct ldt_nl_user {
	struct list_head			list;
	struct list_head			dellist;
	u32							pid;
	struct net					*net;
	int							valid;
};

#define LDT_NL_USER_NULL ((struct ldt_nl_user) { .valid = 0, })

//static struct list_head	tpnl_head = (struct list_head) { .next = &tpnl_head, .prev = &tpnl_head };
static struct list_head	tpnl_head;


int
ldt_nl_register (void)
{
	int	ret;
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0) 
	struct genl_ops	*ops;
	int					nops = ARRAY_SIZE (ldt_nl_ops);
#endif

	INIT_LIST_HEAD (&tpnl_head);
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0) 
	ops = kmalloc (sizeof (struct genl_ops)*nops, GFP_KERNEL);
	if (!ops) return -ENOMEM;
	memcpy (ops, ldt_nl_ops, nops*sizeof (struct genl_ops));
	ret = genl_register_family_with_ops (&ldt_nl_family, ops, nops);
	if (ret < 0) 
		kfree (ops);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0) 
	ret = genl_register_family_with_ops (&ldt_nl_family, ldt_nl_ops);
#else
	ret = genl_register_family (&ldt_nl_family);
#endif
	if (ret < 0) return ret;
	ret = netlink_register_notifier (&ldt_netlink_notifier);
	if (ret < 0) {
		genl_unregister_family (&ldt_nl_family);
		return ret;
	}
	ldt_nl_family_id = ldt_nl_family.id;
	tp_lock_init (&ldt_nl_mutex);
	printk ("ldt registered with id %d\n", ldt_nl_family_id);
	return 0;
}

void
ldt_nl_unregister (void)
{
	genl_unregister_family (&ldt_nl_family);
	netlink_unregister_notifier (&ldt_netlink_notifier);
	ldt_nl_rmalluser();
	tp_lock_destroy (&ldt_nl_mutex);
}





static
int
ldt_nl_create_dev (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	const char					*name;
	const char					*type;
	u32							pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	struct net					*net;
	int							ret;
	int							flags;

	if (!skb) return -EINVAL;
	if (!info) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_CREATE_DEV_ATTR_NAME];
	if (attr) {
		name = (const char*)nla_data (attr);
		if (name && !*name) name=NULL;
	} else {
		name = NULL;
	}
	attr = info->attrs[LDT_CMD_CREATE_DEV_ATTR_TYPE];
	if (attr) {
		type = (const char*)nla_data (attr);
		if (type && !*type) type=NULL;
	} else {
		type = NULL;
	}
	attr = info->attrs[LDT_CMD_CREATE_DEV_ATTR_FLAGS];
	if (attr) {
		flags = (int)nla_get_u32 (attr);
	} else {
		flags = 0;
	}
	if (ldt_cfg_enable_debug) 
		printk ("ldt: adding ldt (%s) device %s (flags=%x)\n",
						type?type:"default", name?name:"tp%d", flags);
	ret = ldt_create_dev (net, name, &name, type, flags);
	if (ret < 0 || !name || !*name) return send_ret (net, pid, ret);
	return send_info (net, pid, 0, name, strlen (name) + 1);
}

static
int
ldt_nl_rm_dev (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	const char					*name;
	u32							pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	struct net					*net;

	if (!skb) return -EINVAL;
	if (!info) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_RM_DEV_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	if (ldt_cfg_enable_debug) 
		printk ("ldt: removing ldt device %s\n", name);
	ldt_free_dev (LDTDEV_BYNAME (net, name));
	return send_ret (net, pid, 0);
}

static
int
ldt_nl_get_version (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid;
	const struct nlmsghdr	*nlh;
	struct net					*net;
	int							ret;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	if (ldt_cfg_enable_debug) {
		printk ("ldt: return ldt version\n");
	}
	ret = send_info (net, pid, 0, LDT_VERSION, strlen (LDT_VERSION)+1);
	if (ret < 0) return ret;
	return 0;
}

static
int
ldt_nl_get_devlist (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid;
	const struct nlmsghdr	*nlh;
	struct net					*net;
	int							ret;
	char							*data;
	u32							len;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	if (ldt_cfg_enable_debug) {
		printk ("ldt: show info for all devices\n");
	}
	ret = ldt_get_devlist (net, &data, &len);
	if (ret < 0) return send_ret (net, pid, ret);
	ret = send_info (net, pid, 0, data, len);
	kfree (data);
	if (ret < 0) return ret;
	return 0;
}

static
int
ldt_nl_show_dev (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	const char					*name;
	struct net					*net;
	ssize_t						ret;
	char							*data;
	u32							len;
	struct ldt_dev		*tdev;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_SHOW_DEV_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	if (!name || !*name) return send_ret (net, pid, -EINVAL);
	if (ldt_cfg_enable_debug) {
		printk ("ldt: show dev [%s]\n", name);
	}
	tdev = LDTDEV_BYNAME (net, name);
	if (!tdev) return send_ret (net, pid, -EINVAL);
	ret = ldt_get_devinfo (tdev, &data);
	dev_put (tdev->ndev);
	if (ret < 0) return send_ret (net, pid, ret);
	len = (u32)ret;
	ret = send_info (net, pid, 0, data, len);
	kfree (data);
	if (ret < 0) return ret;
	return 0;
}

static
int
ldt_nl_show_devlist (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid;
	const struct nlmsghdr	*nlh;
	struct net					*net;
	ssize_t						ret;
	char							*data;
	u32							len;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	if (ldt_cfg_enable_debug) {
		printk ("ldt: show info for all devices\n");
	}
	ret = ldt_get_alldevinfo (net, &data);
	if (ret < 0) return send_ret (net, pid, ret);
	len = (u32)ret;
	ret = send_info (net, pid, 0, data, len);
	kfree (data);
	if (ret < 0) return ret;
	return 0;
}

static
int
ldt_nl_bind (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid, addr4;
	u8								addr6[16];
	u16							port=0;
	tp_addr_t					laddr;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	const char					*name;
	struct net					*net;
	const char					*dev = NULL;
	int							ret;
	int							flags;
	struct ldt_dev				*tdev;
	int							ipv6, hasladdr;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_BIND_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	hasladdr = 0;
	attr = info->attrs[LDT_CMD_BIND_ATTR_ADDR4];
	if (attr) {
		addr4 = nla_get_u32 (attr);
		hasladdr = 1;
		ipv6 = 0;
	} else {
		attr = info->attrs[LDT_CMD_BIND_ATTR_ADDR6];
		if (attr) {
			if (nla_len(attr) != 16) return send_ret (net, pid, -EINVAL);
			memcpy (addr6, nla_data (attr), 16);
			hasladdr = 1;
			ipv6 = 1;
		}
	}
	if (hasladdr) {
		attr = info->attrs[LDT_CMD_BIND_ATTR_PORT];
		if (!attr) return send_ret (net, pid, -EINVAL);
		port = nla_get_u16 (attr);
		if (!ipv6) {
			tp_addr_setipv4 (&laddr, addr4, port);
		} else {
			tp_addr_setipv6 (&laddr, addr6, port);
		}
	} else {
		tp_addr_setipv4 (&laddr, 0, 0);
		ipv6 = 0;
	}
	attr = info->attrs[LDT_CMD_BIND_ATTR_DEV];
	if (attr) {
		dev = (const char*)nla_data (attr);
	} else if (!hasladdr)
		return send_ret (net, pid, -EINVAL);
	attr = info->attrs[LDT_CMD_BIND_ATTR_FLAGS];
	if (attr) {
		flags = nla_get_u32 (attr);
	} else {
		flags = 0;
	}
	
	if (ldt_cfg_enable_debug) {
		/* output always addr and gw, due to a bug in android kernel, that cannot handle
			NULL pointers in printk's %pI
		 */
		printk ("binding tunnel [%s] to %pISc:%u (flags=0x%02x)\n",
					name, &laddr, (unsigned)port, flags);
	}
	tdev = LDTDEV_BYNAME (net, name);
	if (!tdev) {
		printk ("ldt_nl_bind(): cannot find device %s\n", name);
		return send_ret (net, pid, -EINVAL);
	}
	if (ldt_cfg_enable_debug >= 3)
		printk ("ldt_nl_bind(): call ldt_dev_bind\n");
	ret = ldt_dev_bind (tdev, hasladdr?&laddr:NULL, dev, flags);
	dev_put (tdev->ndev);
	if (ldt_cfg_enable_debug >= 1)
		printk ("ldt_nl_bind(): done -> %d\n", ret);
	return send_ret (net, pid, ret);
}

static
int
ldt_nl_peer (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid, addr4;
	u8								addr6[16];
	u16							port;
	tp_addr_t					raddr;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	const char					*name;
	struct net					*net;
	int							ret;
	struct ldt_dev		*tdev;
	int							ipv6;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_PEER_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	attr = info->attrs[LDT_CMD_PEER_ATTR_ADDR4];
	if (attr) {
		addr4 = nla_get_u32 (attr);
		ipv6 = 0;
	} else {
		attr = info->attrs[LDT_CMD_PEER_ATTR_ADDR6];
		if (!attr) return send_ret (net, pid, -EINVAL);
		if (nla_len(attr) != 16) return send_ret (net, pid, -EINVAL);
		memcpy (addr6, nla_data (attr), 16);
		ipv6 = 1;
	}
	attr = info->attrs[LDT_CMD_PEER_ATTR_PORT];
	if (!attr) return send_ret (net, pid, -EINVAL);
	port = nla_get_u16 (attr);
	if (!ipv6) {
		tp_addr_setipv4 (&raddr, addr4, port);
	} else {
		tp_addr_setipv6 (&raddr, addr6, port);
	}
	if (ldt_cfg_enable_debug) {
		printk ("tunnel [%s] peer is %pISc\n", name, &raddr);
	}
	tdev = LDTDEV_BYNAME (net, name);
	if (!tdev) return send_ret (net, pid, -EINVAL);
	ret = ldt_dev_peer (tdev, &raddr);
	dev_put (tdev->ndev);
	return send_ret (net, pid, ret);
}

static
int
ldt_nl_serverstart (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	const char					*name;
	struct net					*net;
	int							ret;
	struct ldt_dev		*tdev;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_SERVERSTART_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	if (ldt_cfg_enable_debug) {
		printk ("start server [%s]\n", name);
	}
	tdev = LDTDEV_BYNAME (net, name);
	if (!tdev) return send_ret (net, pid, -EINVAL);
	ret = ldt_dev_serverstart (tdev);
	dev_put (tdev->ndev);
	return send_ret (net, pid, ret);
}


static
int
ldt_nl_set_mtu (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	const char					*name;
	u32							pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	struct net					*net;
	u32							mtu;
	int							ret;
	struct ldt_dev		*tdev;

	if (!skb) return -EINVAL;
	if (!info) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_SET_MTU_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	attr = info->attrs[LDT_CMD_SET_MTU_ATTR_MTU];
	if (!attr) return send_ret (net, pid, -EINVAL);
	mtu = nla_get_u32 (attr);
	if (ldt_cfg_enable_debug) 
		printk ("ldt: set mtu on ldt device %s\n", name?name:"ldt%d");
	tdev = LDTDEV_BYNAME (net, name);
	if (!tdev) return send_ret (net, pid, -EINVAL);
	ret = ldt_dev_set_mtu (tdev, mtu);
	dev_put (tdev->ndev);
	return send_ret (net, pid, ret);
}

static
int
ldt_nl_set_queue (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	const char					*name;
	u32							pid;
	int							txqlen;
	int							qpolicy;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	struct net					*net;
	struct ldt_dev		*tdev;
	int							ret;

	if (!skb) return -EINVAL;
	if (!info) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_SETQUEUE_ATTR_NAME];
	if (!attr) return send_ret (net, pid, -EINVAL);
	name = (const char*)nla_data (attr);
	attr = info->attrs[LDT_CMD_SETQUEUE_ATTR_TXQLEN];
	if (!attr) {
		txqlen = -1;
	} else {
		txqlen = (int)(unsigned)nla_get_u16 (attr);
	}
	attr = info->attrs[LDT_CMD_SETQUEUE_ATTR_QPOLICY];
	if (!attr) {
		qpolicy = -1;
	} else {
		qpolicy = (int)(unsigned)nla_get_u16 (attr);
		if (qpolicy > LDT_CMD_SETQUEUE_QPOLICY_MAX) {
			printk ("ldt: invalid queueing policy %d\n", qpolicy);
			return send_ret (net, pid, -ERANGE);
		}
	}
	if (ldt_cfg_enable_debug)
		printk ("ldt: set queue (txqlen = %d, qpolicy = %d)\n",
					txqlen, qpolicy);
	tdev = LDTDEV_BYNAME (net, name);
	if (!tdev) return send_ret (net, pid, -EINVAL);
	ret = ldt_dev_setqueue (tdev, txqlen, qpolicy);
	dev_put (tdev->ndev);
	return send_ret (net, pid, ret);
}


static
int
ldt_nl_ping (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							data;
	u32							pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	struct net					*net;
	int							ret;

	if (!skb) return -EINVAL;
	if (!info) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	attr = info->attrs[LDT_CMD_PING_ATTR_DATA];
	if (!attr) return send_ret (net, pid, -EINVAL);
	data = nla_get_u32 (attr);
	if (ldt_cfg_enable_debug) 
		printk ("ldt: got ping (data=%d)\n", data);
	ret = ldt_event_pong (net, data);
	return send_ret (net, pid, ret);
}

int
ldt_nl_subscribe (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	u32							pid;
	const struct nlmsghdr	*nlh;
	struct net					*net;
	int							ret;

	if (!skb) return -EINVAL;
	if (!info) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	if (ldt_cfg_enable_debug) 
		printk ("ldt: got event subscription\n");
	ret = ldt_nl_adduser (pid, net);
	return send_ret (net, pid, ret);
}


static
int
ldt_nl_release_notifier (nb, action, data)
	struct notifier_block	*nb;
	unsigned long				action;
	void							*data;
{
	struct netlink_notify	*n = data;

	if (n && action==NETLINK_URELEASE) {
		ldt_nl_rmuser (n->portid, n->net);
	}
	return NOTIFY_DONE;
}

static
int
send_ret (net, pid, rval)
	struct net	*net;
	u32			pid;
	int			rval;
{
	int	ret;

	if (ldt_cfg_enable_debug)
		printk ("ldt: sending return value: %d to %d\n", rval, (int) pid);
	if (rval != 0) {
		/* don't really send it */
		return rval;
	}
	ret = send_info (net, pid, rval, NULL, 0);
	if (ret < 0) {
		printk ("ldt: error sending return value (%d) to %d: %d\n",
					rval, (int)pid, ret);
		return ret;
	}
	return rval;
}

static int ldt_sndinfo_seq = 0;
static
int
send_info (net, pid, rval, data, len)
	struct net	*net;
	u32			pid;
	const char	*data;
	u32			len;
	int			rval;
{
	struct sk_buff *skb;
	void				*p;
	int				ret, num;
	u32				xval;
	const char		*s;

#define CHUNKSZ	4096
	if (!net) return -EINVAL;
	if (ldt_cfg_enable_debug) 
		printk ("ldt: sending data to %d\n", (int)pid);
	if (!len && data) len = strlen (data) + 1;
	if (!data) len = 0;
	skb = genlmsg_new (len+(len/CHUNKSZ*4)+128, GFP_KERNEL);
	if (!skb) return -ENOMEM;
	/* create the message headers */
	p = genlmsg_put (	skb, pid, ldt_sndinfo_seq++, &ldt_nl_family,
							/* flags = */ 0, LDT_CMD_SEND_INFO);
	if (!p) {
		nlmsg_free (skb);
		printk ("ldt: error creating message\n");
		return -ENOMEM;
	}
	/* add attribute */
	xval = (rval < 0) ? (rval * (-1)) : rval;
	ret = nla_put_u32 (skb, LDT_CMD_SEND_INFO_ATTR_RET, xval);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("ldt: error creating message: %d\n", ret);
		return ret;
	}
	if (data) {
		s = data;
		num=0;
		while (len > CHUNKSZ) {
			num++;
			ret = nla_put (skb, LDT_CMD_SEND_INFO_ATTR_INFO, CHUNKSZ, s);
			if (ret < 0) {
				nlmsg_free (skb);
				printk ("ldt: error creating message: %d\n", ret);
				return ret;
			}
			s += CHUNKSZ;
			len -= CHUNKSZ;
		}
		if (len > 0 || s == data) {
			num++;
			ret = nla_put (skb, LDT_CMD_SEND_INFO_ATTR_INFO, len, s);
			if (ret < 0) {
				nlmsg_free (skb);
				printk ("ldt: error creating message: %d\n", ret);
				return ret;
			}
		}
		len += s-data;
		if (ldt_cfg_enable_debug >= 2)
			printk ("ldt: sent %d bytes of data in %d chunks of size %d\n",
						(int)len, num, CHUNKSZ);
	}
	/* finalize message */
	genlmsg_end (skb, p);

	/* send message */
	ret = genlmsg_unicast (net, skb, pid);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("ldt: error sending message: %d\n", ret);
		return ret;
	}
	return 0;
}

static
int
do_send_event (net, pid, evtype, iarg, sarg)
	struct net	*net;
	u32			pid;
	u32			evtype;
	u32			iarg;
	const char	*sarg;
{
	struct sk_buff *skb;
	void				*p;
	int				ret, len;

	if (!net) return -EINVAL;
	if (ldt_cfg_enable_debug) 
		printk ("ldt: sending event (%d) to user %d\n", (int)evtype, pid);
	len = sarg ? strlen (sarg)+1 : 0;
	skb = genlmsg_new (len+128, GFP_KERNEL);
	if (!skb) return -ENOMEM;
	/* create the message headers */
	p = genlmsg_put (	skb, pid, ldt_sndinfo_seq++, &ldt_nl_family,
							/* flags = */ 0, LDT_CMD_SEND_EVENT);
	if (!p) {
		nlmsg_free (skb);
		printk ("ldt: error creating message\n");
		return -ENOMEM;
	}
	/* add attribute */
	ret = nla_put_u32 (skb, LDT_CMD_SEND_EVENT_ATTR_EVTYPE, evtype);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("ldt: error creating message: %d\n", ret);
		return ret;
	}
	ret = nla_put_u32 (skb, LDT_CMD_SEND_EVENT_ATTR_IARG, iarg);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("ldt: error creating message: %d\n", ret);
		return ret;
	}
	if (sarg) {
		ret = nla_put (skb, LDT_CMD_SEND_EVENT_ATTR_SARG, len, sarg);
		if (ret < 0) {
			nlmsg_free (skb);
			printk ("ldt: error creating message: %d\n", ret);
			return ret;
		}
	}
	/* finalize message */
	genlmsg_end (skb, p);

	/* send message */
	if (ldt_cfg_enable_debug && printk_ratelimit())
		printk ("ldt: sendmsg\n");
	ret = genlmsg_unicast (net, skb, pid);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("ldt: error sending message: %d\n", ret);
		return ret;
	}
	return 0;
}


int
ldt_nl_send_event (net, evtype, iarg, sarg)
	struct net	*net;
	u32			evtype;
	u32			iarg;
	const char	*sarg;
{
	struct ldt_nl_user	*p;

	if (ldt_cfg_enable_debug)
		printk ("ldt: send event %d\n", evtype);
	list_for_each_entry_rcu (p, &tpnl_head, list) {
		if (ldt_cfg_enable_debug >= 3)
			printk ("ldt_event_send: check user %d (valid==%d)\n", (int) p->pid, p->valid);
		if (p->valid && (p->net == net || !net)) {
			do_send_event (p->net, p->pid, evtype, iarg, sarg);
		}
	}
	return 0;
}


/* 
 *	handle the list of users
 */


static
int
ldt_nl_adduser (pid, net)
	u32			pid;
	struct net	*net;
{
	struct ldt_nl_user	*user, *p;
	struct list_head			delhead;

	if (pid <= 0) return -EINVAL;
	/* check wether user already exists */
	list_for_each_entry_rcu (p, &tpnl_head, list) {
		if (p->pid == pid && p->net == net) {
			if (!p->valid) p->valid = 1;
			return 0;
		}
	}

	/* create new user */
	user = kmalloc (sizeof (struct ldt_nl_user), GFP_KERNEL);
	if (!user) return -ENOMEM;
	*user = LDT_NL_USER_NULL;
	INIT_LIST_HEAD (&(user->dellist));
	user->valid = 1;
	user->pid = pid;
	user->net = net;

	/* just a dummy list for elements to be deleted */
	INIT_LIST_HEAD (&delhead);

	G_LOCK;
	/* add new user first */
	list_add_rcu (&(user->list), &tpnl_head);
	if (ldt_cfg_enable_debug)
		printk ("ldt: add user %d\n", (int)pid);

	/* now traverse the list for elements to be deleted */
	list_for_each_entry_rcu (p, &tpnl_head, list) {
		if (!p->valid) {
			list_del_rcu (&(p->list));
			list_add_rcu (&(p->dellist), &delhead);
		}
	}
	G_UNLOCK;

	/* now we are safe and can delete users in dellist */
	list_for_each_entry_safe (user, p, &delhead, dellist) {
		if (ldt_cfg_enable_debug)
			printk ("ldt: delete user %d\n", (int)user->pid);
		*user = LDT_NL_USER_NULL;
		kfree (user);
	}
	return 0;
}


static
void
ldt_nl_rmuser (pid, net)
	u32			pid;
	struct net	*net;
{
	struct ldt_nl_user	*p;

	list_for_each_entry_rcu (p, &tpnl_head, list) {
		if (p->valid && (p->net == net || !net) && p->pid == pid) {
			/* we don't realy delete here, this could create dead locks,
			 * thus we only mark it to be deleted and delete it in next
			 * add user call */
			p->valid = 0;	
			if (ldt_cfg_enable_debug)
				printk ("ldt: release user %d\n", (int)pid);
		}
	}
}

static
void
ldt_nl_rmalluser (void)
{
	struct ldt_nl_user	*user, *p;
	struct list_head			delhead;

	INIT_LIST_HEAD (&delhead);

	/* first traverse the list and move elements to delhead */
	G_LOCK;
	list_for_each_entry_rcu (p, &tpnl_head, list) {
		list_del_rcu (&(p->list));
		list_add_rcu (&(p->dellist), &delhead);
	}
	G_UNLOCK;

	/* now we are safe and can delete users in dellist */

	list_for_each_entry_safe (user, p, &delhead, dellist) {
		*user = LDT_NL_USER_NULL;
		kfree (user);
	}
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
