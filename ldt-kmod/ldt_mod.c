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


#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/genetlink.h>
#include <linux/version.h>

#include "ldt_netlink.h"
#include "ldt_event.h"
#include "ldt_sysctl.h"
#include "ldt_dev.h"
#include "ldt_version.h"
#include "ldt_uapi.h"
#if IS_ENABLED(CONFIG_IP_DCCP)
# include "ldt_mpdccp.h"
#endif
#include "ldt_debug.h"


static
int
__init
ldt_module_init (void)
{
	int	ret;

	tp_prtk ("load module version %s", LDT_VERSION);
	ret = ldt_nl_register ();
	if (ret < 0) return ret;
	ret = ldt_sysctl_init ();
	if (ret < 0) return ret;
	ret = ldt_dev_global_init ();
	if (ret < 0) return ret;
#if IS_ENABLED(CONFIG_IP_DCCP)
	ret = ldt_mpdccp_register ();
	if (ret < 0) return ret;
#endif
	tp_prtk ("module version %s successfully loaded\n", LDT_VERSION);
	return 0;
}


static
void
__exit
ldt_module_exit (void)
{
	ldt_dev_global_destroy ();
#if IS_ENABLED(CONFIG_IP_DCCP)
	ldt_mpdccp_unregister ();
#endif
	ldt_event_crsend (LDT_EVTYPE_TPDOWN, NULL, 0);
	ldt_sysctl_exit ();
	ldt_nl_unregister ();
	tp_prtk ("module ver %s unloaded\n", LDT_VERSION);
}



module_init (ldt_module_init);
module_exit (ldt_module_exit);


MODULE_LICENSE("Propietary");
MODULE_AUTHOR("Frank Reker <frank@reker.net>");
MODULE_VERSION(LDT_VERSION);
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
MODULE_ALIAS_GENL_FAMILY(LDT_NAME);
#endif







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
