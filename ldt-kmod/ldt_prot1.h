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

#ifndef _R__KERNEL_LDT_PROT1_INT_H
#define _R__KERNEL_LDT_PROT1_INT_H

#include <linux/types.h>
#include <linux/skbuff.h>

#define TP_PROT1_T_KEEPALIVE	1
#define TP_PROT1_T_HANDSHAKE	2
#define TP_PROT1_T_SNDCFG		3
#define TP_PROT1_T_FASTAUTH	4
#define TP_PROT1_T_AUTHFAIL	5

/* handshake subtypes */
#define TP_HST_UDP	1

/* send config subtypes */
#define TP_SNDCFG_IP		1
#define TP_SNDCFG_MPTCP	2
#define TP_SNDCFG_UCFG	3

/* fastauth subtypes */
#define TP_FASTAUTH_REQ	1
#define TP_FASTAUTH_ACK	2



#define TP_GETPKTTYPE(data)	(((u8)(data))>>4)
#define TP_ISPROT1(data)	(TP_GETPKTTYPE(data)==1)
#define TP_PROT1GETTYPE(data,sz)	((!(data)||(sz)<4)?0:(data)[3])
#define TP_PROT1GETSUBTYPE(data,sz)	((!(data)||(sz)<4)?0:(data)[2])
#define TP_ISHANDSHAKE(data,sz)	((!(data)||(sz)<8)?0:((TP_ISPROT1((data)[0])\
												&&((data)[3]==TP_PROT1_T_HANDSHAKE))?1:0))
#define TP_ISAUTHFAIL(data,sz)	((!(data)||(sz)<8)?0:((TP_ISPROT1((data)[0])\
												&&((data)[3]==TP_PROT1_T_AUTHFAIL))?1:0))
#define TP_ISFASTAUTH(data,sz)	((!(data)||(sz)<12)?0:((TP_ISPROT1((data)[0])\
												&&((data)[3]==TP_PROT1_T_FASTAUTH))?1:0))
#define TP_SKBISPROT1(skb)	((skb)->data && (skb)->len>0 && TP_ISPROT1((skb)->data[0]))
#define TP_SKBPROT1GETTYPE(skb)	(TP_PROT1GETTYPE((skb)->data,(skb)->len))
#define TP_SKBPROT1GETSUBTYPE(skb)	(TP_PROT1GETSUBTYPE((skb)->data,(skb)->len))
#define TP_SKBISHANDSHAKE(skb)	(TP_ISHANDSHAKE((skb)->data,(skb)->len))
#define TP_SKBISFASTAUTH(skb)		(TP_ISFASTAUTH((skb)->data,(skb)->len))

#define TP_PROT1_MAXSZ	1020	/* 255 << 2 */

static inline int ldt_prot1msglen (char *data, int sz)
{
	if (!data || sz < 4) return -EBADMSG;
	if (!TP_ISPROT1(data[0])) return -EBADMSG;
	return ((u32)(u8)(data[1])) << 2;
}

struct ldt_tun;
int ldt_prot1_recv (struct ldt_tun *tun, struct sk_buff *skb);





#endif	/* _R__KERNEL_LDT_PROT1_INT_H */


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
