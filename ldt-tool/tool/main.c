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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>

#include <fr/base.h>
#include "cmd.h"
#include "conman.h"
#include <ldt.h>

#define LDT_TOOL_VERSION	"0.4.0"


const char	*PROG = "ldt";


static
void
usage ()
{
	printf ("%s: usage: %s [<options>] <cmd> [<args>]\n", PROG, PROG);
	printf (	"  options are: \n"
				"    -h                - this help screen\n"
				"    -c <config file>  - config file to use\n"
				"    -t <timeout>      - timeout (default 5 sec)\n"
				"    -V                - print version of ldt tool\n"
				"                        note - the kernel version is the version for\n"
				"                        which ldt was compiled. to obtain the \n"
				"                        version of the running module: ldt ver\n"
				"  valid commands are: \n"
				"  (to get help on a specific command - type: <cmd> -h)\n"
				"    newdev | new - creates a new ldt device\n"
				"    rmdev - removes a ldt dev\n"
				"    getversion | getver - prints version of running ldt module\n"
				"    getdevlist | getlist - prints a list of ldt devices\n"
				"    showdev - shows information for a specific device\n"
				"    showinfo | info - shows specific info for specific device\n"
				"    showall - shows information for all ldt devices\n"
				"    newtun | addtun - creates a new tunnel\n"
				"    tunbind - binds tunnel to address\n"
				"    setpeer | peer - sets peer address to connect to\n"
				"    serverstart - sets server to listen mode\n"
				"    rmtun - remove a tunnel from a ldt device\n"
				"    setmtu - sets mtu for given ldt device\n"
				"    printev | prtev - prints (all) ldt events\n"
				"    setqueue - set tx queue length and/or queueing policy\n"
				"    conman - start connection manager\n"
				"\n");
}



int
main (argc, argv)
	int	argc;
	char	**argv;
{
	int			c, ret;
	const char	*cmd;

	PROG = fr_getprog ();
	while ((c = getopt (argc, argv, "+hc:t:V")) != -1) {
		switch (c) {
		case 'h':
			usage ();
			exit (0);
		case 'c':
			cf_set_cfname (optarg);
			break;
		case 't':
			ldt_settimeout (cf_atotm (optarg));
			break;
		case 'V':
			printf ("library version: %s\n", LDT_LIB_VERSION);
			printf ("kernel  version: %s\n", LDT_KERN_VERSION);
			printf ("tool    version: %s\n", LDT_TOOL_VERSION);
			return 0;
		}
	}
	if (optind < argc && !strcmp (argv[optind], "--")) optind++;
	if (optind < argc) {
		cmd = argv[optind];
		optind++;
	} else {
		cmd = "showall";
	}
	argc -= optind - 1;
	argv += optind - 1;
	optind = 0;
	opterr = 0;
	sswitch (cmd) {
	sicase ("newdev")
	sicase ("new")
	sicase ("adddev")
	sicase ("add")
		ret = cmd_newdev (argc, argv);
		break;
	sicase ("rmdev")
	sicase ("rm")
		ret = cmd_rmdev (argc, argv);
		break;
	sicase ("getversion")
	sicase ("getver")
	sicase ("version")
	sicase ("ver")
		ret = cmd_getversion (argc, argv);
		break;
	sicase ("getlist")
	sicase ("getdevlist")
	sicase ("list")
		ret = cmd_getdevlist (argc, argv);
		break;
	sicase ("showdev")
	sicase ("show")
		ret = cmd_showdev (argc, argv);
		break;
	sicase ("showinfo")
	sicase ("info")
		ret = cmd_showinfo (argc, argv);
		break;
	sicase ("showall")
		ret = cmd_showall (argc, argv);
		break;
	sicase ("rmtun")
		ret = cmd_rmtun (argc, argv);
		break;
	sicase ("newtun")
	sicase ("addtun")
		ret = cmd_newtun (argc, argv);
		break;
	sicase ("tunbind")
		ret = cmd_tunbind (argc, argv);
		break;
	sicase ("setpeer")
	sicase ("peer")
		ret = cmd_setpeer (argc, argv);
		break;
	sicase ("serverstart")
	sicase ("listen")
		ret = cmd_serverstart (argc, argv);
		break;
	sicase ("setmtu")
		ret = cmd_set_mtu (argc, argv);
		break;
	sincase ("printev")
	sincase ("prtev")
		ret = cmd_prtev (argc, argv);
		break;
	sicase ("setqueue")
		ret = cmd_setqueue (argc, argv);
		break;
	sicase ("conman")
		ret = cmd_conman (argc, argv);
		break;
	sdefault
		argv--;
		argc++;
		ret = cmd_showdev (argc, argv);
		cmd = "showdev";
		break;
	} esac;
	if (!RERR_ISOK(ret)) {
		SLOGFE (LOG_ERR2, "error executing command >>%s<<: %s", cmd,
					rerr_getstr3 (ret));
		return ret;
	}
	return 0;
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
