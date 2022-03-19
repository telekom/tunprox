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
#include <net/sock.h>
#include <linux/errno.h>
#include <linux/in6.h>
#include <linux/in.h>
#include <linux/types.h>
#include <linux/netdevice.h>
#include <linux/ip.h>
#include <linux/err.h>

#include "ldt_ip.h"
#include "ldt_prot1.h"

static int getipv4msglen (char*, int);
static int getipv6msglen (char*, int);
static int getipv6exthdrlen (int, char*, int, int*);



int
ldt_msglen (buf, len)
	char	*buf;
	int	len;
{
	if (!buf || len < 0) return -EINVAL;
	if (len < 4) return -EMSGSIZE;
	switch (TP_GETPKTTYPE (buf[0])) {
	case 4:
		return getipv4msglen (buf, len);
	case 6:
		return getipv6msglen (buf, len);
	case 1:
		return ldt_prot1msglen (buf, len);
	default:
		return -EBADMSG;
	}
}


static
int
getipv4msglen (buf, len)
	char	*buf;
	int	len;
{
	if (!buf || len < 0) return -EINVAL;
	if (len < 4) return -EMSGSIZE;
	return (int)(u32) htons (*(u16*)(buf+2));
}

static
int
getipv6msglen (buf, len)
	char	*buf;
	int	len;
{
	if (!buf || len < 0) return -EINVAL;
	if (len < 6) return -EMSGSIZE;
	return (int) (40 + (u32) htons (*(u16*)(buf+4)));
}



int
ldt_iphdrlen (data, sz, l4prot)
	char	*data;
	int	sz;
	int	*l4prot;
{
	if (!data || sz < 0) return -EINVAL;
	if (sz == 0) return 0;
	switch (TP_GETPKTTYPE(data[0])) {
	case 1: return sz;
	case 4: return ldt_ipv4hdrlen (data, sz, l4prot);
	case 6: return ldt_ipv6hdrlen (data, sz, l4prot);
	}
	return sz;
}

int
ldt_ipv4hdrlen (data, sz, l4prot)
	char	*data;
	int	sz;
	int	*l4prot;
{
	int	len;

	if (!data || sz < 0) return -EINVAL;
	if (sz < 20) return sz;
	len = (data[0] & 0x0f) << 2;
	if (sz < len) return sz;
	if (l4prot) *l4prot = ((unsigned)(u8)(data[9]));
	return len;
}


int
ldt_ipv6hdrlen (data, sz, l4prot)
	char	*data;
	int	sz;
	int	*l4prot;
{
	int	ret;

	if (!data || sz < 0) return -EINVAL;
	if (sz < 40) return sz;
	ret = getipv6exthdrlen ((unsigned)data[6], data+40, sz-40, l4prot);
	if (ret < 0) return ret;
	return ret + 40;
}


static
int
getipv6exthdrlen (prot, data, sz, l4prot)
	int	prot, sz, *l4prot;
	char	*data;
{
	int	ret, len;

	if (!data || sz < 0) return -EINVAL;
	switch (prot) {
	case 0: /* hop by hop */
	case 43: /* routing */
	case 60: /* destination options */
		if (sz < 2) return sz;
		len = (((unsigned)data[1]) << 3) + 1;
		prot = (unsigned)data[0];
		if (sz < len) return sz;
		ret = getipv6exthdrlen (prot, data+len, sz-len, l4prot);
		if (ret < 0) return ret;
		return len + ret;
	case 44: /* fragment */
		prot = (unsigned)data[0];
		if (sz < 8) return sz;
		ret = getipv6exthdrlen (prot, data+8, sz-8, l4prot);
		if (ret < 0) return ret;
		return 8 + ret;
	case 51: /* authentication */
		/* to be done ... */
	case 50: /* esp */
	case 135: /* mobility */
	case 59: /* end of header */
	default:
		if (l4prot) *l4prot = prot;
		return 0;
	}
	return sz;
}

int
ldt_l4hdrlen (prot, data, sz)
	int	prot, sz;
	char	*data;
{
	if (!data || sz < 0) return -EINVAL;
	switch (prot) {
	case 6: /* TCP */
		if (sz < 12) return -EBADMSG;
		return (((u8)data[12]) & 0xf0) >> 2;
	case 17: /* UDP */
	case 136: /* UDPLite */
		return 8;
	case 33: /* DCCP */
		if (sz < 4) return -EBADMSG;
		return ((unsigned)(u8)(data[4])) << 2;
	}
	return sz;
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
