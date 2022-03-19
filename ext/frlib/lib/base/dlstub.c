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
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <dlfcn.h>

#include <fr/base/config.h>
#include <fr/base/slog.h>
#include <fr/base/errors.h>
#include <fr/base/textop.h>

#include <fr/base/dl.h>
#include <fr/base/dlstub.h>


static int tryldone (void **, const char *);
static int tryld (void**, const char*, int);
static int tryldlist (void**, const char**);
static int tryldcfg (void**, const char*);


int
dlstub_ldfunc (libh, libname, lib, liblist, flags)
	void			**libh;
	const char	*libname;
	const char	*lib;
	const char	**liblist;
	int			flags;
{
	int	ret = RERR_PARAM;

	if (!libh) return RERR_PARAM;
	if (libname) {
		ret = tryldcfg (libh, libname);
		if (RERR_ISOK(ret)) return ret;
	}
	if (lib) {
		ret = tryld (libh, lib, 0);
		if (RERR_ISOK(ret)) return ret;
	}
	if (liblist) {
		ret = tryldlist (libh, liblist);
		if (RERR_ISOK(ret)) return ret;
	}
	if (!RERR_ISOK(ret) && !(flags & DLSTUB_F_SILENT)) {
		FRLOGF (LOG_ERR, "error loading library >>%s<<: %s", 
					(lib ? lib : (libname ? libname : ((liblist && *liblist) ? 
					*liblist : "<NULL>"))), rerr_getstr3 (ret));
	}
	return ret;
}


static
int
tryldcfg (libh, libname)
	void			**libh;
	const char	*libname;
{
	const char	*lib;
	int			ret;

	if (!libh || !libname) return RERR_PARAM;
	cf_begin_read ();
	lib = cf_getarr ("lib", libname);
	if (!lib) {
		cf_end_read ();
		return RERR_NOT_FOUND;
	}
	ret = tryld (libh, lib, 1);
	cf_end_read ();
	return ret;
}


static
int
tryldlist (libh, liblist)
	void			**libh;
	const char	**liblist;
{
	int			ret = RERR_NOT_FOUND;

	if (!libh || !liblist) return RERR_PARAM;
	while (*liblist) {
		ret = tryld (libh, *liblist, 0);
		if (RERR_ISOK(ret)) return ret;
		liblist++;
	}
	return ret;
}


static
int
tryld (libh, lib, norec)
	void			**libh;
	const char	*lib;
	int			norec;
{
	int	ret;
	char	_buf[256], *buf, *s, *s2;

	if (!libh || !lib) return RERR_PARAM;
	ret = tryldone (libh, lib);
	if (RERR_ISOK(ret)) return ret;
	if (norec) return ret;
	buf = top_strcpdup (_buf, sizeof (_buf), lib);
	if (!buf) return RERR_NOMEM;
	while (1) {
		s = rindex (buf, '.');
		for (s2=s+1; isdigit (*s2); s2++);
		if (*s2) break;
		*s = 0;
		ret = tryldone (libh, buf);
		if (RERR_ISOK(ret)) break;
	}
	if (buf && buf != _buf) free (buf);
	return ret;
}



static
int
tryldone (libh, lib)
	void			**libh;
	const char	*lib;
{
	if (!libh || !lib) return RERR_PARAM;

	*libh = dlopen (lib, RTLD_LAZY | RTLD_LOCAL);
	if (!*libh) return RERR_NOT_FOUND;
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
