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

#ifndef _R__KERNEL_LDT_DEBUG_H
#define _R__KERNEL_LDT_DEBUG_H

#include "ldt_sysctl.h"

#define TP_PRTK_RATE(a)	(\
			((a) || !(tp_cfg_logflags & TP_CFG_LOG_F_RATELIMIT) || \
			 printk_ratelimit()))

#define TP_FILE	( { const char *s = strrchr (__FILE__, '/'); s ? s+1 : __FILE__; } )
#define TP_EOL(fmt)	( ((fmt) && *(fmt) && (fmt)[strlen(fmt)-1] == '\n') ? "" : "\n" )
#define TP_PRTK(enable,norate,hdr,fmt,a...) \
				do { \
					if (enable && TP_PRTK_RATE(norate)) { \
						if (tp_cfg_logflags & TP_CFG_LOG_F_PRTFILE) \
							printk (hdr "%s(%s:%d):%s(): " fmt "%s", KBUILD_MODNAME, \
										TP_FILE, __LINE__,__func__, ##a, TP_EOL(fmt)); \
						else \
							printk (hdr "%s:%s(): " fmt "%s", KBUILD_MODNAME, \
										__func__, ##a, TP_EOL(fmt)); \
					} \
				} while(0)
#define TP_PRTKL(mlevel,norate,hdr,fmt,a...) \
				TP_PRTK((tp_cfg_loglevel >= (mlevel)), norate, hdr, fmt, ##a)

#define tp_debug(fmt,a...)  TP_PRTKL(7, 0, KERN_DEBUG,   fmt, ##a)
#define tp_debug2(fmt,a...) TP_PRTKL(8, 0, KERN_DEBUG,   fmt, ##a)
#define tp_debug3(fmt,a...) TP_PRTKL(9, 0, KERN_DEBUG,   fmt, ##a)
#define tp_info(fmt,a...)   TP_PRTKL(6, 0, KERN_INFO,    fmt, ##a)
#define tp_note(fmt,a...)   TP_PRTKL(5, 1, KERN_NOTICE,  fmt, ##a)
#define tp_warn(fmt,a...)   TP_PRTKL(4, 1, KERN_WARNING, fmt, ##a)
#define tp_err(fmt,a...)    TP_PRTKL(3, 1, KERN_ERR,     fmt, ##a)
#define tp_crit(fmt,a...)   TP_PRTKL(2, 1, KERN_CRIT,    fmt, ##a)
#define tp_alert(fmt,a...)  TP_PRTKL(1, 1, KERN_ALERT,   fmt, ##a)
#define tp_emerg(fmt,a...)  TP_PRTKL(0, 1, KERN_EMERG,   fmt, ##a)
#define tp_prtk(fmt,a...)   TP_PRTK( 1, 1, "",           fmt, ##a)






#endif	/* _R__KERNEL_LDT_DEBUG_H */


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
