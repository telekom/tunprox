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

#ifndef _R__KERNEL_LDT_ADDR_INT_H
#define _R__KERNEL_LDT_ADDR_INT_H

#include <linux/socket.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/string.h>


typedef union {
	struct sockaddr		ad;
	struct sockaddr_in	v4;
	struct sockaddr_in6	v6;
} tp_addr_t;


#define TP_ADDR_FAM(a)		((a).ad.sa_family)
#define TP_ADDRP_FAM(a)		(TP_ADDR_FAM(*a))
#define TP_ADDR_ISIPV6(a)	(TP_ADDR_FAM(a) == AF_INET6)
#define TP_ADDRP_ISIPV6(a)	(TP_ADDR_ISIPV6(*a))
#define TP_ADDR_ISIPV4(a)	(TP_ADDR_FAM(a) == AF_INET)
#define TP_ADDRP_ISIPV4(a)	(TP_ADDR_ISIPV4(*a))
#define TP_ADDR_SIZE(a)		(TP_ADDR_ISIPV6(a) ? sizeof (struct sockaddr_in6) : sizeof (struct sockaddr_in))
#define TP_ADDRP_SIZE(a)	(TP_ADDR_SIZE(*a))

static inline void tp_addr_setipv4 (
	tp_addr_t	*ad,
	u32			ad4,
	u16			port)
{
	if (!ad) return;
	ad->v4 = (struct sockaddr_in) {
		.sin_family = AF_INET,
		.sin_addr.s_addr = ad4,
		.sin_port = htons (port),
	};
}

static inline void tp_addr_setipv6 (
	tp_addr_t	*ad,
	u8				ad6[16],
	u16			port)
{
	if (!ad) return;
	ad->v6 = (struct sockaddr_in6) {
		.sin6_family = AF_INET6,
		.sin6_port = htons (port),
	};
	memcpy (ad->v6.sin6_addr.s6_addr, ad6, 16);
}

static inline void tp_addr_setipv4only (
	tp_addr_t	*ad,
	u32			ad4)
{
	if (!ad) return;
	if (TP_ADDRP_FAM(ad) != AF_INET) return;
	ad->v4.sin_addr.s_addr = ad4;
}

static inline void tp_addr_setipv6only (
	tp_addr_t	*ad,
	u8				ad6[16])
{
	if (!ad) return;
	if (TP_ADDRP_FAM(ad) != AF_INET6) return;
	memcpy (ad->v6.sin6_addr.s6_addr, ad6, 16);
}

static inline void tp_addr_cpiponly (
	tp_addr_t	*ad,
	tp_addr_t	*ad2)
{
	if (!ad) return;
	if (TP_ADDRP_FAM(ad) != TP_ADDRP_FAM(ad2)) return;
	if (TP_ADDRP_ISIPV6(ad)) {
		tp_addr_setipv6only (ad, ad2->v6.sin6_addr.s6_addr);
	} else if (TP_ADDRP_ISIPV4(ad)) {
		tp_addr_setipv4only (ad, ad2->v4.sin_addr.s_addr);
	}
}

static inline void tp_addr_setport (
	tp_addr_t	*ad,
	u16			port)
{
	if (!ad) return;
	if (TP_ADDRP_ISIPV6(ad)) {
		ad->v6.sin6_port = htons (port);
	} else {
		ad->v4.sin_port = htons (port);
	}
}

static inline u16 tp_addr_getport (tp_addr_t *ad)
{
	if (!ad) return 0;
	if (TP_ADDRP_ISIPV6(ad)) {
		return htons (ad->v6.sin6_port);
	} else {
		return htons (ad->v4.sin_port);
	}
}
#define tp_addr_getuport(ad)		((unsigned)tp_addr_getport(ad))


static inline int tp_addr_isany (
	tp_addr_t	*ad)
{
	if (!ad) return 1;
	if (TP_ADDRP_ISIPV6(ad)) {
		int	i;
#if defined __UAPI_DEF_IN6_ADDR_ALT
		for (i=0; i<4; i++)
         if (ad->v6.sin6_addr.s6_addr32[i]) return 0;
#else
		for (i=0; i<16; i++)
         if (ad->v6.sin6_addr.s6_addr[i]) return 0;
#endif
		return 1;
	} else {
		return !(ad->v4.sin_addr.s_addr);
	}
}

static inline int tp_addr_sprt_ip (
	char			*buf,
	size_t		size,
	tp_addr_t	*ad)
{
	if (!buf || !ad) return 0;
	/* %pIS does not exist on 3.10, so do it the hard way */
	if (TP_ADDRP_ISIPV6(ad)) {
		return snprintf (buf, size, "%pI6", &ad->v6.sin6_addr);
	} else {
		return snprintf (buf, size, "%pI4", &ad->v4.sin_addr);
	}
}

static inline int tp_addr_sprt (
	char			*buf,
	size_t		size,
	tp_addr_t	*ad)
{
	if (!buf || !ad) return 0;
	if (TP_ADDRP_ISIPV6(ad)) {
		return snprintf (buf, size, "[%pI6]:%d", &ad->v6.sin6_addr, ad->v6.sin6_port);
	} else {
		return snprintf (buf, size, "%pI4:%d", &ad->v4.sin_addr, ad->v4.sin_port);
	}
}


static inline int tp_addr_eq (
	tp_addr_t	*ad1,
	tp_addr_t	*ad2)
{
	if (!ad1 || !ad2) return 0;
	if (TP_ADDRP_FAM(ad1) != TP_ADDRP_FAM(ad2)) return 0;
	if (TP_ADDRP_ISIPV6(ad1)) {
		if (ad1->v6.sin6_port != ad2->v6.sin6_port) return 0;
		if (memcmp (ad1->v6.sin6_addr.s6_addr, ad2->v6.sin6_addr.s6_addr, 16)) return 0;
		return 1;
	} else {
		if (ad1->v4.sin_port != ad2->v4.sin_port) return 0;
		if (ad1->v4.sin_addr.s_addr != ad2->v4.sin_addr.s_addr) return 0;
		return 1;
	}
}

static inline void tp_addr_cp (
	tp_addr_t	*ad1,
	tp_addr_t	*ad2)
{
	if (!ad1 || !ad2) return;
	if (TP_ADDRP_ISIPV6(ad2)) {
		tp_addr_setipv6 (ad1, ad2->v6.sin6_addr.s6_addr, htons(ad2->v6.sin6_port));
	} else {
		tp_addr_setipv4 (ad1, ad2->v4.sin_addr.s_addr, htons(ad2->v4.sin_port));
	}
}







#endif	/* _R__KERNEL_LDT_ADDR_INT_H */


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
