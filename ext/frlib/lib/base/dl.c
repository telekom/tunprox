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
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <dlfcn.h>

#include "slog.h"
#include "misc.h"
#include "errors.h"
#include "dl.h"
#include "prtf.h"


int
frdlsymf (
	void			*func,
	void			*lib,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!fmt || !*fmt || !func || !lib) return RERR_PARAM;

	va_start (ap, fmt);
	ret = vfrdlsymf (func, lib, fmt, ap);
	va_end (ap);
	return ret;
}


int
vfrdlsymf (func, lib, fmt, ap)
	void			*func, *lib;
	const char	*fmt;
	va_list		ap;
{
	char	*str, buf[256];
	int	ret;

	if (!fmt || !*fmt || !func || !lib) return RERR_PARAM;
	str = va2sprtf (buf, sizeof(buf), fmt, ap);
	if (!str) {
		FRLOGF (LOG_ERR, "error creating string: %s", rerr_getstr3 (RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	ret = frdlsym (func, lib, str);
	if (str != buf) free (str);
	return ret;
}

int
frdlsym (func, lib, fname)
	void			*func, *lib;
	const char	*fname;
{
	char	*error;

	if (!func || !lib || !fname || !*fname) return RERR_PARAM;
	dlerror ();
	*(void **) (&(*(void**)func)) = dlsym (lib, fname);
	error = dlerror ();
	if (error) {
		*(void **) (&(*(void**)func)) = NULL;
		if (1 || !strncasecmp (error, "undefined", 9)) {
			FRLOGF (LOG_DEBUG, "error getting symbol >>%s<< from lib (%p): %s",
					fname, lib, error);
			return RERR_NOT_FOUND;
		} else {
			FRLOGF (LOG_ERR, "error getting symbol >>%s<< from lib (%p): %s",
					fname, lib, error);
			return RERR_SYSTEM;
		}
	}
	return RERR_OK;
}


int
frdlopen (lib, lname, flags)
	void			**lib;
	const char	*lname;
	int			flags;
{
	if (!lib || !lname) return RERR_PARAM;
	//dlerror();
	*lib = dlopen (lname, flags);
	if (!*lib) {
		FRLOGF (LOG_INFO, "error open lib (%s): %s", lname, dlerror());
		return RERR_SYSTEM;
	}
	return RERR_OK;
}

int
vfrdlopenf (lib, flags, fmt, ap)
	void			**lib;
	int			flags;
	const char	*fmt;
	va_list		ap;
{
	char	*str, buf[256];
	int	ret;

	if (!fmt || !*fmt || !lib) return RERR_PARAM;
	str = va2sprtf (buf, sizeof(buf), fmt, ap);
	if (!str) {
		FRLOGF (LOG_ERR, "error creating string: %s", rerr_getstr3 (RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	ret = frdlopen (lib, str, flags);
	if (str != buf) free (str);
	return ret;
}


int
frdlopenf (
	void			**lib,
	int			flags,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!fmt || !*fmt || !lib) return RERR_PARAM;

	va_start (ap, fmt);
	ret = vfrdlopenf (lib, flags, fmt, ap);
	va_end (ap);
	return ret;
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
