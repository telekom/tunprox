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


#ifndef _R__KERNEL_LDT_H
#define _R__KERNEL_LDT_H

#include <linux/types.h>
#include <linux/socket.h>


/* Note that this file is shared with user space. */

#ifdef __cplusplus
extern "C" {
#endif


#define LDT_NAME "ldt"
#define LDT_NL_VERSION 3	/* netlink protocol version */


/* netlink definitions */

enum ldt_nl_commands {
	LDT_CMD_UNSPEC,
	LDT_CMD_CREATE_DEV,
	LDT_CMD_RM_DEV,				/* remove device */
	LDT_CMD_GET_VERSION,
	LDT_CMD_GET_DEVLIST,
	LDT_CMD_SHOW_DEV,
	LDT_CMD_SHOW_DEVLIST,
	LDT_CMD_RM_TUN,
	LDT_CMD_NEWTUN,
	LDT_CMD_BIND,
	LDT_CMD_BIND2DEV,
	LDT_CMD_PEER,
	LDT_CMD_SERVERSTART,
	LDT_CMD_SET_MTU,
	LDT_CMD_SET_QUEUE,
	LDT_CMD_SUBSCRIBE,
	LDT_CMD_SEND_INFO,			/* answer */
	LDT_CMD_SEND_EVENT,			/* unsolicate event answer */
	LDT_CMD_EVSEND,
	__LDT_CMD_MAX
};
#define LDT_CMD_MAX (__LDT_CMD_MAX - 1)



enum ldt_attrs_create_dev {
	LDT_CMD_CREATE_DEV_ATTR_UNSPEC,
	LDT_CMD_CREATE_DEV_ATTR_NAME,	/* NLA_NUL_STRING */
	LDT_CMD_CREATE_DEV_ATTR_FLAGS,	/* NLA_U32 */
	__LDT_CMD_CREATE_DEV_ATTR_MAX
};
#define LDT_CMD_CREATE_DEV_ATTR_MAX (__LDT_CMD_CREATE_DEV_ATTR_MAX - 1)

#define LDT_CREATE_DEV_F_NONE		0x00
#define LDT_CREATE_DEV_F_CLIENT		0x01
#define LDT_CREATE_DEV_F_SERVER		0x02

enum ldt_attrs_rm_dev {
	LDT_CMD_RM_DEV_ATTR_UNSPEC,
	LDT_CMD_RM_DEV_ATTR_NAME,	/* NLA_NUL_STRING */
	__LDT_CMD_RM_DEV_ATTR_MAX
};
#define LDT_CMD_RM_DEV_ATTR_MAX (__LDT_CMD_RM_DEV_ATTR_MAX - 1)


enum ldt_attrs_get_version {
	LDT_CMD_GET_VERSION_ATTR_UNSPEC,
	__LDT_CMD_GET_VERSION_ATTR_MAX
};
#define LDT_CMD_GET_VERSION_ATTR_MAX (__LDT_CMD_GET_VERSION_ATTR_MAX - 1)

enum ldt_attrs_get_devlist {
	LDT_CMD_GET_DEVLIST_ATTR_UNSPEC,
	__LDT_CMD_GET_DEVLIST_ATTR_MAX
};
#define LDT_CMD_GET_DEVLIST_ATTR_MAX (__LDT_CMD_GET_DEVLIST_ATTR_MAX - 1)

enum ldt_attrs_show_dev {
	LDT_CMD_SHOW_DEV_ATTR_UNSPEC,
	LDT_CMD_SHOW_DEV_ATTR_NAME,		/* NLA_NUL_STRING */
	__LDT_CMD_SHOW_DEV_ATTR_MAX
};
#define LDT_CMD_SHOW_DEV_ATTR_MAX (__LDT_CMD_SHOW_DEV_ATTR_MAX - 1)

enum ldt_attrs_show_devlist {
	LDT_CMD_SHOW_DEVLIST_ATTR_UNSPEC,
	__LDT_CMD_SHOW_DEVLIST_ATTR_MAX
};
#define LDT_CMD_SHOW_DEVLIST_ATTR_MAX (__LDT_CMD_SHOW_DEVLIST_ATTR_MAX - 1)


enum ldt_attrs_rm_tun {
	LDT_CMD_RM_TUN_ATTR_UNSPEC,
	LDT_CMD_RM_TUN_ATTR_NAME,		/* NLA_NUL_STRING */
	__LDT_CMD_RM_TUN_ATTR_MAX
};
#define LDT_CMD_RM_TUN_ATTR_MAX (__LDT_CMD_RM_TUN_ATTR_MAX - 1)

enum ldt_attrs_newtun {
	LDT_CMD_NEWTUN_ATTR_UNSPEC,
	LDT_CMD_NEWTUN_ATTR_NAME,			/* NLA_NUL_STRING */
	LDT_CMD_NEWTUN_ATTR_TUN_TYPE,	/* NLA_NUL_STRING */
	__LDT_CMD_NEWTUN_ATTR_MAX
};
#define LDT_CMD_NEWTUN_ATTR_MAX	(__LDT_CMD_NEWTUN_ATTR_MAX - 1)

enum ldt_attr_bind {
	LDT_CMD_BIND_ATTR_UNSPEC,
	LDT_CMD_BIND_ATTR_NAME,		/* NLA_NUL_STRING */
	LDT_CMD_BIND_ATTR_ADDR4,		/* NLA_U32 */
	LDT_CMD_BIND_ATTR_ADDR6, 		/* NLA_BINARY */
	LDT_CMD_BIND_ATTR_PORT,		/* NLA_U16 */
	__LDT_CMD_BIND_ATTR_MAX
};
#define LDT_CMD_BIND_ATTR_MAX	(__LDT_CMD_BIND_ATTR_MAX - 1)

enum ldt_attr_bind2dev {
	LDT_CMD_BIND2DEV_ATTR_UNSPEC,
	LDT_CMD_BIND2DEV_ATTR_NAME,		/* NLA_NUL_STRING */
	LDT_CMD_BIND2DEV_ATTR_DEV, 		/* NLA_NUL_STRING - binding device */
	__LDT_CMD_BIND2DEV_ATTR_MAX
};
#define LDT_CMD_BIND2DEV_ATTR_MAX	(__LDT_CMD_BIND2DEV_ATTR_MAX - 1)

enum ldt_attr_peer {
	LDT_CMD_PEER_ATTR_UNSPEC,
	LDT_CMD_PEER_ATTR_NAME,		/* NLA_NUL_STRING */
	LDT_CMD_PEER_ATTR_ADDR4,		/* NLA_U32 */
	LDT_CMD_PEER_ATTR_ADDR6, 		/* NLA_BINARY */
	LDT_CMD_PEER_ATTR_PORT,		/* NLA_U16 */
	__LDT_CMD_PEER_ATTR_MAX
};
#define LDT_CMD_PEER_ATTR_MAX	(__LDT_CMD_PEER_ATTR_MAX - 1)

enum ldt_attr_serverstart {
	LDT_CMD_SERVERSTART_ATTR_UNSPEC,
	LDT_CMD_SERVERSTART_ATTR_NAME,		/* NLA_NUL_STRING */
	__LDT_CMD_SERVERSTART_ATTR_MAX
};
#define LDT_CMD_SERVERSTART_ATTR_MAX	(__LDT_CMD_SERVERSTART_ATTR_MAX - 1)


enum ldt_attrs_set_mtu {
	LDT_CMD_SET_MTU_ATTR_UNSPEC,
	LDT_CMD_SET_MTU_ATTR_NAME,	/* NLA_NUL_STRING */
	LDT_CMD_SET_MTU_ATTR_MTU,		/* NLA_U32 */
	__LDT_CMD_SET_MTU_ATTR_MAX
};
#define LDT_CMD_SET_MTU_ATTR_MAX (__LDT_CMD_SET_MTU_ATTR_MAX - 1)


enum ldt_attrs_setqueue {
	LDT_CMD_SETQUEUE_ATTR_UNSPEC,
	LDT_CMD_SETQUEUE_ATTR_NAME,			/* NLA_NUL_STRING */
	LDT_CMD_SETQUEUE_ATTR_TXQLEN,		/* NLA_U16 */
	LDT_CMD_SETQUEUE_ATTR_QPOLICY,		/* NLA_U16 */
	__LDT_CMD_SETQUEUE_ATTR_MAX
};
#define LDT_CMD_SETQUEUE_ATTR_MAX (__LDT_CMD_SETQUEUE_ATTR_MAX - 1)

#define LDT_CMD_SETQUEUE_QPOLICY_DROP_NEWEST	0
#define LDT_CMD_SETQUEUE_QPOLICY_DROP_OLDEST	1
#define LDT_CMD_SETQUEUE_QPOLICY_MAX				1


enum ldt_attrs_send_info {
	LDT_CMD_SEND_INFO_ATTR_UNSPEC,
	LDT_CMD_SEND_INFO_ATTR_RET,				/* NLA_BINARY - int */
	LDT_CMD_SEND_INFO_ATTR_INFO,				/* NLA_BINARY */
	__LDT_CMD_SEND_INFO_ATTR_MAX,
};
#define LDT_CMD_SEND_INFO_ATTR_MAX (__LDT_CMD_SEND_INFO_ATTR_MAX - 1)

enum ldt_attrs_send_event {
	LDT_CMD_SEND_EVENT_ATTR_UNSPEC,
	LDT_CMD_SEND_EVENT_ATTR_EVTYPE,			/* NLA_U32 */
	LDT_CMD_SEND_EVENT_ATTR_IARG,			/* NLA_U32 */
	LDT_CMD_SEND_EVENT_ATTR_SARG,			/* NLA_NUL_STRING */
	__LDT_CMD_SEND_EVENT_ATTR_MAX,
};
#define LDT_CMD_SEND_EVENT_ATTR_MAX (__LDT_CMD_SEND_EVENT_ATTR_MAX - 1)

enum ldt_attrs_evsend {
	LDT_CMD_EVSEND_ATTR_UNSPEC,
	LDT_CMD_EVSEND_ATTR_NAME,		/* NLA_NUL_STRING */
	LDT_CMD_EVSEND_ATTR_EVTYPE,	/* NLA_U32 */
	LDT_CMD_EVSEND_ATTR_REASON,	/* NLA_U32 */
	__LDT_CMD_EVSEND_ATTR_MAX
};
#define LDT_CMD_EVSEND_ATTR_MAX (__LDT_CMD_EVSEND_ATTR_MAX - 1)


/* event definition */

enum ldt_event_type {
	LDT_EVTYPE_UNSPEC,
	LDT_EVTYPE_INIT,						/* initial handshake successfull */
	LDT_EVTYPE_DOWN,						/* keep alive timed out */
	LDT_EVTYPE_UP,						/* remote host up again */
	LDT_EVTYPE_TUNDOWN,					/* tunnel brought down */
	LDT_EVTYPE_TUNUP,					/* new tunnel brought up */
	LDT_EVTYPE_IFDOWN,					/* interface brought down */
	LDT_EVTYPE_IFUP,						/* new interface brought up */
	LDT_EVTYPE_TPDOWN,					/* ldt module unloaded */
	LDT_EVTYPE_PDEVDOWN,				/* underlying physical device went down */
	LDT_EVTYPE_PDEVUP,					/* underlying physical device came up */
	LDT_EVTYPE_NIFDOWN,					/* some network if went down */
	LDT_EVTYPE_NIFUP,					/* some network if came up */
	LDT_EVTYPE_REBIND,					/* tunnel was rebound (tunbind, setpeer) */
	LDT_EVTYPE_SUBFLOW_UP,				/* new subflow came up */
	LDT_EVTYPE_SUBFLOW_DOWN,			/* subflow removed */
	LDT_EVTYPE_CONN_ESTAB,				/* connection established */
	LDT_EVTYPE_CONN_ESTAB_FAIL,		/* connection establishment failed */
	LDT_EVTYPE_CONN_ACCEPT,			/* connection accepted */
	LDT_EVTYPE_CONN_ACCEPT_FAIL,		/* connection failed to accept */
	LDT_EVTYPE_CONN_LISTEN,			/* server listening */
	LDT_EVTYPE_CONN_LISTEN_FAIL,		/* listening failed */
	__LDT_EVTYPE_MAX,
};
#define LDT_EVTYPE_MAX (__LDT_EVTYPE_MAX - 1)



#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif /* _R__KERNEL_LDT_H */


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
