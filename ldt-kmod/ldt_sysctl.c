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
#include <linux/sysctl.h>
#include <linux/init.h>
#include <linux/errno.h>

#include "ldt_sysctl.h"

unsigned int tp_cfg_enable_debug = 0;
unsigned int tp_cfg_show_key = 0;
unsigned int tp_cfg_loglevel = 5;
unsigned int tp_cfg_logflags = TP_CFG_LOG_F_RATELIMIT | TP_CFG_LOG_F_PRTFILE;


static unsigned i_0 = 0;
static unsigned i_1 = 1;
static unsigned i_3 = 3;
static unsigned i_9 = 9;

static unsigned old_loglevel = 5;

static
int
proc_loglevel (
	struct ctl_table	*ctl,
	int					write,
	void __user			*buffer,
	size_t				*lenp,
	loff_t				*ppos)
{
	unsigned				val;
	struct ctl_table	tbl = {
										.data = &val,
										.maxlen = sizeof (unsigned),
										.extra1 = &i_0,
										.extra2 = &i_9,
									};
	int					ret;

	val = tp_cfg_loglevel;
	ret = proc_dointvec_minmax (&tbl, write, buffer, lenp, ppos);
	if (write && ret == 0) {
		old_loglevel = tp_cfg_loglevel;
		tp_cfg_loglevel = val;
		tp_cfg_enable_debug = (val >= 7) ? val - 6 : 0;
	}
	return ret;
}

static
int
proc_debug (
	struct ctl_table	*ctl,
	int					write,
	void __user			*buffer,
	size_t				*lenp,
	loff_t				*ppos)
{
	unsigned				val;
	struct ctl_table	tbl = {
										.data = &val,
										.maxlen = sizeof (unsigned),
										.extra1 = &i_0,
										.extra2 = &i_3,
									};
	int					ret;

	val = tp_cfg_enable_debug;
	ret = proc_dointvec_minmax (&tbl, write, buffer, lenp, ppos);
	if (write && ret == 0) {
		tp_cfg_enable_debug = val;
		tp_cfg_loglevel = (val > 0) ? val + 6 : old_loglevel;
	}
	return ret;
}


static struct ctl_table net_ldt_table[] = {
	{
		.procname       = "debug",
		.data           = &tp_cfg_enable_debug,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_debug,
		.extra1			 = &i_0,
		.extra2			 = &i_3,
	},
	{
		.procname       = "loglevel",
		.data           = &tp_cfg_loglevel,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_loglevel,
		.extra1			 = &i_0,
		.extra2			 = &i_9,
	},
	{
		.procname       = "logflags",
		.data           = &tp_cfg_logflags,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
	},
	{
		.procname       = "show_key",
		.data           = &tp_cfg_show_key,
		.maxlen         = sizeof(unsigned int),
		.mode           = 0644,
		.proc_handler   = proc_dointvec_minmax,
		.extra1			 = &i_0,
		.extra2			 = &i_1,
	},
	{ }
};


static struct ctl_table_header	*sysctl_hdr = NULL;

int
ldt_sysctl_init (void)
{
	sysctl_hdr = register_sysctl("net/ldt", net_ldt_table);
	if (!sysctl_hdr) return -ENOMEM;
	return 0;
}


void
ldt_sysctl_exit (void)
{
	if (sysctl_hdr) unregister_sysctl_table (sysctl_hdr);
	sysctl_hdr = NULL;
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
