/*
 * Eddi Message Distribution Module
 *
 * ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0 / GPL 2.0
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
 * Portions created by the Initial Developer are Copyright (C) 2013-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * Alternatively, the contents of this file may be used under the terms of
 * either the GNU General Public License Version 2 (the "GPL"), in which
 * case the provisions of the GPL are applicable instead of those above.
 * If you wish to allow use of your version of this file only under the
 * terms of the GPL, and not to allow others to use your version of this
 * file under the terms of the MPL, indicate your decision by deleting
 * the provisions above and replace them with the notice and other provisions
 * required by the GPL. If you do not delete the provisions above, a
 * recipient may use your version of this file under the terms of any one
 * of the MPL or the GPL.
 *
 * ***** END LICENSE BLOCK ***** */


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


#include "keddi.h"


static int eddi_nl_con_chan (struct sk_buff*, struct genl_info*);
static int eddi_nl_send (struct sk_buff*, struct genl_info*);
static int eddi_nl_dosend (struct net*, const char*, u32);
static int eddi_nl_release_notifier (struct notifier_block*, unsigned long, void*);
static struct eddi_nl_user *eddi_nl_adduser (u32);
static void eddi_nl_rmuser (u32);
static void eddi_nl_rmalluser (void);



static const struct nla_policy eddi_nl_policy_con[EDDI_NL_CON_ATTR_MAX + 1] = {
	[EDDI_NL_CON_ATTR_CHAN] = {	.type = NLA_U32 },
};

static const struct nla_policy eddi_nl_policy_send[EDDI_NL_SEND_ATTR_MAX + 1] = {
	[EDDI_NL_SEND_ATTR_EVENT] = {	.type = NLA_NUL_STRING },
};

static const struct genl_ops eddi_nl_ops[] = {
	{
		.cmd = EDDI_NL_CMD_CON_CHAN,
		.doit = eddi_nl_con_chan,
		.policy = eddi_nl_policy_con,
		/* can be retrieved by unprivileged users */
	},
	{
		.cmd = EDDI_NL_CMD_SEND,
		.doit = eddi_nl_send,
		.policy = eddi_nl_policy_send,
		/* can be retrieved by unprivileged users */
	},
};

static struct genl_family eddi_nl_family = {
	.name = EDDI_NL_NAME,
	.hdrsize = 0,
	.version = EDDI_NL_VERSION,
	.maxattr = EDDI_NL_CMD_MAX,
#if LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
   .id = GENL_ID_GENERATE,
#else
   .module = THIS_MODULE,
   .ops = eddi_nl_ops,
   .n_ops = ARRAY_SIZE (eddi_nl_ops),
#endif
};



static struct notifier_block eddi_netlink_notifier = {
        .notifier_call = eddi_nl_release_notifier
};

static int eddi_nl_seq = 0;
int eddi_nl_family_id = 0;

/* some statistics */
int eddi_nl_rx_num = 0;
int eddi_nl_rx_bytes = 0;
int eddi_nl_tx_num = 0;
int eddi_nl_tx_bytes = 0;
int eddi_nl_users_act = 0;
int eddi_nl_users_max = 0;
int eddi_nl_users_total = 0;
int eddi_nl_enable_debug = 0;


int
eddi_nl_register (void)
{
	int	ret;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,13,0)
   struct genl_ops   *ops;
   int               nops = ARRAY_SIZE (eddi_nl_ops);

   ops = kmalloc (sizeof (struct genl_ops)*nops, GFP_KERNEL);
   if (!ops) return -ENOMEM;
   memcpy (ops, eddi_nl_ops, nops*sizeof (struct genl_ops));
   ret = genl_register_family_with_ops (&eddi_nl_family, ops, nops);
   if (ret < 0)
      kfree (ops);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(4,10,0)
   ret = genl_register_family_with_ops (&eddi_nl_family, eddi_nl_ops);
#else
   ret = genl_register_family (&eddi_nl_family);
#endif
	if (ret < 0) return ret;
	ret = netlink_register_notifier (&eddi_netlink_notifier);
	if (ret < 0) {
		genl_unregister_family (&eddi_nl_family);
		return ret;
	}
	eddi_nl_family_id = eddi_nl_family.id;
	printk ("eddi registered with id %d\n", eddi_nl_family_id);
	return 0;
}

void
eddi_nl_unregister (void)
{
	genl_unregister_family (&eddi_nl_family);
	netlink_unregister_notifier (&eddi_netlink_notifier);
	eddi_nl_rmalluser ();
}


static DEFINE_MUTEX(eddi_mutex);

struct eddi_nl_user {
	u32						pid;
	u32						chan;
	struct eddi_nl_user	*next;
};

#define EDDI_NL_USER_NULL ((struct eddi_nl_user) { .pid = 0, .chan = 0, .next = NULL })

static struct eddi_nl_user	*head = NULL;




static
int
eddi_nl_con_chan (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	struct eddi_nl_user		*user;
	u32							chan, pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	attr = info->attrs[EDDI_NL_CON_ATTR_CHAN];
	if (!attr) return -EINVAL;
	chan = nla_get_u32(attr);
	if (eddi_nl_enable_debug) 
		printk ("eddi: setting chan for %d to %d\n", (int) pid, (int) chan);
	mutex_lock (&eddi_mutex);
	user = eddi_nl_adduser (pid);
	if (!user) {
		mutex_unlock (&eddi_mutex);
		return -ENOMEM;
	}
	user->chan = chan;
	mutex_unlock (&eddi_mutex);
	return 0;
}


static
int
eddi_nl_send (skb, info)
	struct sk_buff		*skb;
	struct genl_info	*info;
{
	struct eddi_nl_user		*user, *p;
	u32							chan, pid;
	const struct nlmsghdr	*nlh;
	const struct nlattr		*attr;
	const char					*ev;
	struct net					*net;

	if (!skb) return -EINVAL;
	nlh = nlmsg_hdr(skb);
	if (!nlh) return -EINVAL;
	pid = nlh->nlmsg_pid;
	if (!info) return -EINVAL;
	attr = info->attrs[EDDI_NL_SEND_ATTR_EVENT];
	if (!attr) return -EINVAL;
	ev = (const char*)nla_data (attr);
	if (!ev) return -EINVAL;
	eddi_nl_rx_num++;
	eddi_nl_rx_bytes += strlen (ev) + 1;
	net = genl_info_net (info);
	if (!net) return -EINVAL;
	if (eddi_nl_enable_debug) 
		printk ("sending event >>%s<<\n", ev);
	mutex_lock (&eddi_mutex);
	user = eddi_nl_adduser (pid);
	if (!user) {
		mutex_unlock (&eddi_mutex);
		return -ENOMEM;
	}
	chan = user->chan;
	for (p=head; p; p=p->next) {
		if (p->chan == chan && p->pid != pid) {
			eddi_nl_dosend (net, ev, p->pid);
		}
	}
	mutex_unlock (&eddi_mutex);
	return 0;
}


static
int
eddi_nl_dosend (net, ev, pid)
	struct net	*net;
	const char	*ev;
	u32			pid;
{
	struct sk_buff *skb;
	void				*p;
	int				ret;

	if (!ev || !net) return -EINVAL;
	if (eddi_nl_enable_debug) 
		printk ("eddi: sending data to %d\n", (int)pid);
	skb = genlmsg_new (strlen(ev)+64, GFP_KERNEL);
	if (!skb) return -ENOMEM;
	/* create the message headers */
	p = genlmsg_put (skb, pid, eddi_nl_seq++, &eddi_nl_family, /* flags = */ 0,
							EDDI_NL_CMD_SEND);
	if (!p) {
		nlmsg_free (skb);
		printk ("eddi: error creating message");
		return -ENOMEM;
	}
	/* add attribute */
	ret = nla_put_string (skb, EDDI_NL_SEND_ATTR_EVENT, ev);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("eddi: error creating message (ev): %d", ret);
		return ret;
	}
	/* finalize message */
	genlmsg_end (skb, p);
	/* send message */
	ret = genlmsg_unicast (net, skb, pid);
	if (ret < 0) {
		nlmsg_free (skb);
		printk ("eddi: error sending message: %d", ret);
		return ret;
	}
	/* add statistics */
	eddi_nl_tx_num++;
	eddi_nl_tx_bytes += strlen (ev) + 1;
	return 0;
}



static
int
eddi_nl_release_notifier (nb, action, data)
	struct notifier_block	*nb;
	unsigned long				action;
	void							*data;
{
	struct netlink_notify	*n = data;

	if (action==NETLINK_URELEASE) {
		eddi_nl_rmuser (n->portid);
	}
	return NOTIFY_DONE;
}






/* 
 *	handle the list of users
 */


static
struct eddi_nl_user*
eddi_nl_adduser (pid)
	u32	pid;
{
	struct eddi_nl_user		*user;

	if (pid <= 0) return NULL;
	if (!head) {
		user = kmalloc (sizeof (struct eddi_nl_user), GFP_KERNEL);
		if (!user) return NULL;
		*user = EDDI_NL_USER_NULL;
		head = user;
	} else if (head->pid == pid) {
		user = head;
	} else {
		for (user=head; user->next && user->next->pid != pid; user=user->next);
		if (user->next) {
			user = user->next;
		} else {
			user->next = kmalloc (sizeof (struct eddi_nl_user), GFP_KERNEL);
			if (!user->next) return NULL;
			user = user->next;
			*user = EDDI_NL_USER_NULL;
		}
	}
	if (user->pid == 0) {
		user->pid = pid;
		eddi_nl_users_act++;
		if (eddi_nl_users_act > eddi_nl_users_max) eddi_nl_users_max = eddi_nl_users_act;
		eddi_nl_users_total++;
	}
	return user;
}


static
void
eddi_nl_rmuser (pid)
	u32	pid;
{
	struct eddi_nl_user	*p, *user;

	if (!head) return;
	if (head->pid == pid) {
		user = head;
		head = head->next;
	} else {
		for (p=head; p->next && p->next->pid != pid; p=p->next);
		if (!p->next) return;
		user = p->next;
		p->next  = p->next->next;
	}
	*user = EDDI_NL_USER_NULL;
	kfree (user);
	eddi_nl_users_act--;
	if (eddi_nl_enable_debug) 
		printk ("eddi: release user %d\n", (int)pid);
	return;
}

static
void
eddi_nl_rmalluser (void)
{
	struct eddi_nl_user	*user;

	while (head) {
		user = head;
		head = head->next;
		*user = EDDI_NL_USER_NULL;
		kfree (user);
	}
	eddi_nl_users_act = 0;
	return;
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
