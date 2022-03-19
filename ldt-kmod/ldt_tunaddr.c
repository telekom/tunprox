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

#include "ldt_addr.h"
#include "ldt_tunaddr.h"
#include "ldt_dev.h"
#include "ldt_tun.h"
#include "ldt_cfg.h"


void
ldt_tunaddr_init (tad, ipv6)
	tp_tunaddr_t	*tad;
	int				ipv6;
{
	if (!tad) return;
	*tad = (tp_tunaddr_t) { .ipv6 = ipv6 ? 1 : 0, .anyraddr = 1, };
	tad->laddr.ad.sa_family = ipv6 ? AF_INET6 : AF_INET;
	tad->raddr.ad.sa_family = ipv6 ? AF_INET6 : AF_INET;
}

int
ldt_tunaddr_bind (tad, addr, flags)
	tp_tunaddr_t	*tad;
	tp_addr_t		*addr;
	int				flags;
{
	if (!tad || !addr) return -EINVAL;
	if (TP_ADDRP_FAM(addr) != TP_ADDR_FAM(tad->raddr)) {
		if (ldt_cfg_enable_debug)
			printk ("ldt_tunaddr_bind(): try to bind to wrong address family"
						" - expecting %s", tad->ipv6 ? "ipv6" : "ipv4");
		return -EINVAL;
	}
	if (flags & LDT_TUN_BIND_F_ADDRCHG) {
		if (!tad->bound) return -ENOENT;
		tp_addr_cpiponly (&tad->laddr, addr);
		tad->anyladdr = tp_addr_isany (addr);
	} else {
		tp_addr_cp (&tad->laddr, addr);
		tad->anyladdr = tp_addr_isany (addr);
		tad->lport = tp_addr_getport (addr);
		tad->anylport = tad->lport == 0;
	}
	tad->bound = 1;
	return 0;
}

int
ldt_tunaddr_setpeer (tad, addr, dyn)
	tp_tunaddr_t	*tad;
	tp_addr_t		*addr;
	int				dyn;
{
	if (!tad || !addr) return -EINVAL;
	if (dyn && !tad->anyraddr) return 0;	/* nothing to be done */
	if (TP_ADDRP_FAM(addr) != TP_ADDR_FAM(tad->raddr)) {
		if (ldt_cfg_enable_debug)
			printk ("ldt_tunaddr_setpeer(): try to connect to wrong address family"
						" - expecting %s", tad->ipv6 ? "ipv6" : "ipv4");
		return -EINVAL;
	}
	tp_addr_cp (&tad->raddr, addr);
	if (!dyn) {
		tad->anyraddr = tp_addr_isany (addr);
		tad->hasraddr = !tad->anyraddr;
	} else {
		tad->hasraddr = 1;
	}
	return 0;
}


int
ldt_tunaddr_prt (tad, buf, bufsz, spc)
	tp_tunaddr_t	*tad;
	char				*buf;
	int				bufsz;
	unsigned			spc;
{
	int	len=0;
	char	xbuf[sizeof("xxxx:xxxx:xxxx:xxxx:xxxx:xxxx:255.255.255.255")+2];

	if (!tad) return 0;
#define MYPRTIP(ad) (tp_addr_sprt_ip (xbuf, sizeof (xbuf), &(ad)) > 0 ? xbuf : "")
#define _FSTR	(buf ? buf + len : NULL)
#define _FLEN	(bufsz > len ? bufsz - len : 0)
	if (TP_ADDR_ISIPV6(tad->laddr))
		len += snprintf (_FSTR, _FLEN, "%*c<ipv6/>\n", spc, ' ');
	if (!tad->bound) {
		len += snprintf (_FSTR, _FLEN, "%*c<locaddr>unbound</locaddr>\n", spc, ' ');
	} else if (tad->anyladdr) {
		len += snprintf (_FSTR, _FLEN, "%*c<locaddr>%s</locaddr>\n", spc, ' ',
												TP_ADDR_ISIPV6(tad->laddr)?"::":"0.0.0.0");
		len += snprintf (_FSTR, _FLEN, "%*c<actlocaddr>%s</actlocaddr>\n",
												spc, ' ', MYPRTIP(tad->laddr));
	} else {
		len += snprintf (_FSTR, _FLEN, "%*c<locaddr>%s</locaddr>\n", spc, ' ',
												MYPRTIP(tad->laddr));
	}
	if (!tad->bound) {
		/* nothing */
	} else if (tad->anylport) {
		len += snprintf (_FSTR, _FLEN, "%*c<locport>0</locport>\n", spc, ' ');
		len += snprintf (_FSTR, _FLEN, "%*c<actlocport>%u</actlocport>\n",
												spc, ' ', tp_addr_getuport (&tad->laddr));
	} else {
		len += snprintf (_FSTR, _FLEN, "%*c<locport>%u</locport>\n", spc, ' ',
												tp_addr_getuport (&tad->laddr));
	}
	if (tad->anyraddr) {
		len += snprintf (_FSTR, _FLEN, "%*c<remaddr>%s</remaddr>\n", spc, ' ',
												TP_ADDR_ISIPV6(tad->raddr)?"::":"0.0.0.0");
		len += snprintf (_FSTR, _FLEN, "%*c<remport>0</remport>\n", spc, ' ');
		if (tad->hasraddr) {
			len += snprintf (_FSTR, _FLEN, "%*c<actremaddr>%s</actremaddr>\n",
												spc, ' ', MYPRTIP(tad->raddr));
			len += snprintf (_FSTR, _FLEN, "%*c<actremport>%u</actremport>\n",
												spc, ' ', tp_addr_getuport (&tad->raddr));
		}
	} else {
		len += snprintf (_FSTR, _FLEN, "%*c<remaddr>%s</remaddr>\n", spc, ' ',
												MYPRTIP(tad->raddr));
		len += snprintf (_FSTR, _FLEN, "%*c<remport>%u</remport>\n", spc, ' ',
												tp_addr_getuport (&tad->raddr));
	}
#undef _FSTR
#undef _FLEN
#undef MYPRTIP
	return len;
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
