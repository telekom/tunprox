/* ***** BEGIN LICENSE BLOCK *****
 * Version: MPL 2.0
 *
 *  This Source Code Form is subject to the terms of the Mozilla Public
 *  License, v. 2.0. If a copy of the MPL was not distributed with this
 *  file, You can obtain one at http://mozilla.org/MPL/2.0/.
 *
 * Software distributed under the License is distributed on an "AS IS" basis,
 * WITHOUT WARRANTY OF ANY KIND, either express or implied. See the License
 * for more details governing rights and limitations under the License.
 *
 * The Original Code is part of the frlib.
 *
 * The Initial Developer of the Original Code is
 * Frank Reker <frank@reker.net>.
 * Portions created by the Initial Developer are Copyright (C) 2003-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif

#include "errors.h"
#include "frinit.h"
#include "slog.h"
#include "config.h"
#include "fileop.h"



static const char	*PROG = "";
static const char	*PROGPATH = "";
static int	gargc = 0;
static char	**gargv = NULL;
static int	gflags = 0;
static int	hasinit = 0;
static int 	hasautoinit = 0;

static int autoinit ();


int
frinit (argc, argv, flags)
	int	argc, flags;
	char	**argv;
{
	int	ret, ret2;

	if (hasautoinit && gargv) {
		if (*gargv) free (*gargv);
		free (gargv);
		gargv = NULL;
		hasautoinit = 0;
	}
	gargc = argc;
	gargv = argv;
	if (argv && argc>0) PROGPATH=argv[0];
	if (PROGPATH) {
		PROG = (PROG = rindex (PROGPATH, '/')) ? (PROG+1) : PROGPATH;
	}
	hasinit = 1;
	ret = fr_setflags (flags);
	ret2 = slog_set_prog (PROG);
	if (RERR_ISOK (ret)) ret = ret2;
	return ret;
}


int
fr_setflags (flags)
	int	flags;
{
	if (!hasinit) autoinit ();
	if (!PROG || !*PROG) return RERR_PARAM;
	gflags = flags;
	if (!(flags & FR_F_NOUSCORE)) {
		flags |= FR_F_USCORE;
	}
	return cf_default_cfname (PROG, flags);
}

int
fr_addflags (flags)
	int	flags;
{
	if (!hasinit) autoinit ();
	return fr_setflags (gflags | flags);
}

int
fr_unsetflags (flags)
	int	flags;
{
	if (!hasinit) autoinit ();
	return fr_setflags (gflags & ~flags);
}

int
fr_getflags ()
{
	if (!hasinit) autoinit ();
	return gflags;
}

const char*
fr_getprog ()
{
	if (!hasinit) autoinit ();
	return PROG;
}


const char*
fr_getprogpath ()
{
	if (!hasinit) autoinit ();
	return PROGPATH;
}


int
fr_getargs (argc, argv)
	int	*argc;
	char	***argv;
{
	if (!hasinit) autoinit ();
	if (argc) *argc = gargc;
	if (argv) *argv = gargv;
	return RERR_OK;
}


int
frdaemonize ()
{
	return frdaemonize_ret (0);
}

int
frdaemonize_ret (rval)
	int	rval;
{
	pid_t	pid;

	pid = fork ();
	if (pid < 0) {
		FRLOGF (LOG_ERR2, "cannot fork into background: %s",
								rerr_getstr3 (RERR_SYSTEM));
		return RERR_SYSTEM;
	} else if (pid > 0) {
		/* father */
		exit (rval);
	}
	freopen ("/dev/null", "r+", stdin);
	freopen ("/dev/null", "r+", stdout);
	freopen ("/dev/null", "r+", stderr);
	setsid ();
	pid = fork ();
	if (pid < 0) {
		FRLOGF (LOG_WARN, "cannot fork for the second time (%s), continue "
								"anyway...", rerr_getstr3 (RERR_SYSTEM));
	} else if (pid > 0) {
		/* father */
		exit (rval);
	}
	return RERR_OK;
}


#if !defined __GNUC__
#	pragma init(autoinit);
	static
#else
	static
	__attribute__((constructor)) 
#endif
int
autoinit ()
{
#if !defined Linux && !defined CYGWIN
	char	fname[64];
#endif
	char	*buf, *s, **argl;
	int	flen, num, i;

	if (hasinit) return RERR_OK;
#if !defined Linux && !defined CYGWIN
	sprintf (fname, "/proc/%d/cmdline", (int)getpid());
	buf = fop_read_fn2 (fname, &flen);
#else
	buf = fop_read_fn2 ("/proc/self/cmdline", &flen);
#endif
	if (!buf) return RERR_SYSTEM;
	for (num=1, i=0, s=buf; i<flen; i++, s++) if (!*s) num++;
	if (flen > 0 && buf[flen-1]!=0) num++;
	argl = malloc (num*sizeof (char*));
	if (!argl) {
		free (buf);
		return RERR_NOMEM;
	}
	argl[0] = buf;
	for (num=1, i=0, s=buf; i<flen; i++, s++) {
		if (*s) continue;
		if ((i+1) >= flen) break;
		argl[num] = s+1;
		num++;
	}
	argl[num] = NULL;
	/* ignore return value */
	frinit (num, argl, FR_F_DEFAULT);
	/* the buffers buf and argl must not be freed !!! */
	hasautoinit = 1;
	return RERR_OK;
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
