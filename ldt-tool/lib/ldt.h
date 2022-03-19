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


#ifndef _R__LDT_LIB_NETLINK_LDT_KERNEL_INTERFACE_H
#define _R__LDT_LIB_NETLINK_LDT_KERNEL_INTERFACE_H

#include <fr/base/tmo.h>
#include <fr/connect/addr.h>
#include <ldt/ldt_kernel.h>
#include <ldt/ldt_version.h>

#define LDT_LIB_VERSION	"0.1"
#define LDT_KERN_VERSION	LDT_VERSION

#ifdef __cplusplus
extern "C" {
#endif


int ldt_open ();
int ldt_close ();
int ldt_mayclose ();

void ldt_settimeout (tmo_t tout);
tmo_t ldt_gettimeout ();

int ldt_create_dev (const char *name, const char *type, char **out_name,
								uint32_t flags);
int ldt_rm_dev (const char *name);

int ldt_get_version (char **version);
int ldt_get_devlist (char ***devlist);
int ldt_get_devinfo (const char *name, char **info, uint32_t *ilen);
int ldt_get_alldevinfo (char **info, uint32_t *ilen);

int ldt_setpeer (const char *name, frad_t *raddr);
int ldt_serverstart (const char *name);
int ldt_setqueue (const char *nam, int txqlen, int qpolicy);
int ldt_tunbind (const char *name, frad_t *laddr,
							const char *dev, uint32_t flags);
int ldt_set_mtu (const char *name, uint32_t mtu);
int ldt_ping (uint32_t data);



/* event functions */

int ldt_subscribe ();
int ldt_event_open ();
int ldt_event_close ();
int ldt_event_recv (int *evtype, uint32_t *iarg, char **sarg, tmo_t tout);


struct ldt_evinfo {
	char			*buf;
	int			evtype;
	const char	*iface;
	int			tunid;
	const char	*desc;
	const char	*remaddr;
	int			remport;
	const char	*pdev;
	int			reason;
	const char	*s_reason;
	const char	*subflow;
};
#define LDT_EVINFO_HFREE(evinfo)	do { if ((evinfo)->buf) free ((evinfo)->buf); } while (0)


#define LDT_F_CONTONERR	1
int ldt_waitev (char **evstr, int evtype, tmo_t timeout, int flags);
int ldt_waitev2 (char **evstr, int *evtype, int listen_evtypes,
							tmo_t timeout, int flags);
int ldt_waitifaceev (	struct ldt_evinfo*, const char *listen_iface,
									int listen_evtypes,
									tmo_t timeout, int flags);

#define LDT_EVTYPE_ALL	((1<<(LDT_EVTYPE_MAX+1))-1)
int ldt_getevmap (const char *str);
int ldt_evhelp (char *buf, int buflen, int plen);
int ldt_prtevhelp (int plen);
const char *ldt_evgetname (int evtype);
const char *ldt_evgetdesc (int evtype);

struct ldt_status_t {
	char		iface[32];
	uint32_t	linkup:1,
				pdevup:1,
				tunup:1,
				ifup:1;
};
int ldt_get_status (struct ldt_status_t **statlist, int *numstat, const char *iface);


/* the following functions are internally used, do not use them directly */
int ldt_nl_getret ();
int ldt_nl_getanswer (char **data, uint32_t *dlen);
int ldt_nl_send (char *msg, size_t len);
const char *ldt_getauthfail_reason (int reason);




#ifdef __cplusplus
}	/* extern "C" */
#endif


#endif	/* _R__LDT_LIB_NETLINK_LDT_KERNEL_INTERFACE_H */

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
