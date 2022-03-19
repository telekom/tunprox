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
#include <stdarg.h>
#include <string.h>
#include <strings.h>
#include <stdarg.h>
#include <fr/base/frcontext.h>

/* c++ includes */
#include <string>
#include <stdexcept>
#include <exception>


/* frlib includes */
#include "Slog.h"

#include <fr/base/errors.h>
#include <fr/base/slog.h>
#include <fr/base/misc.h>
#include <fr/base/prtf.h>


Slog	*Slog::gslog = NULL;



Slog::~Slog ()
{
	finish ();
	if (this==gslog) gslog = NULL;
	if (handle > 0) slog_releasehandle (handle);
}


Slog*
Slog::getGlobalSlog ()
{
	if (gslog) return gslog;
	gslog = new Slog ();
	return gslog;
};





Slog::Slog (
	const char	*module)
{
	handle = slog_gethandle ((char*)module);
	minlevel = shlog_minlevel (handle);
	throwlevel = shlog_throwlevel (handle);
};



Slog::Slog ()
{
	handle = 0;
	minlevel = slog_minlevel ();
	throwlevel = slog_throwlevel ();
};




int
Slog::do_vlogf (	int			level,
						const char	*fname,
						int			lineno,
						const char	*funcname,
						void			*ptr,
						const char	*fmt,
						va_list		ap)
{
	int			ret;
	char			*buf;
	char			*msg;
	va_list		ap2;
	ucontext_t	ctx;

	if (!ptr && (getcontext(&ctx)==0)) ptr = &ctx;
#ifdef va_copy
	va_copy (ap2, ap);
#else
	ap2 = ap;
#endif
	ret = vsxhlogf (handle, level | LOG_RETTHROWLEVEL, fname, lineno, funcname,
						ptr, fmt, ap);
	if (!RERR_ISOK(ret)) return ret;
	if (ret == LOG_NONE) return RERR_OK;
	if (level > ret) return RERR_OK;
	buf = vasprtf ((char*)fmt, ap2);
	if (!buf) return RERR_NOMEM;
	msg = (char*)alloca (strlen (buf)+1);
	if (!msg) {
		free (buf);
		return RERR_NOMEM;
	}
	strcpy (msg, buf);
	free (buf);		/* we need to free before do_throw! */
	return do_throw (fname, lineno, funcname, msg);
}

int
Slog::do_logstr (	int level,
						const char	*fname,
						int			lineno,
						const char	*funcname,
						void			*ptr,
						const char	*str)
{
	int	ret;
	ucontext_t	ctx;

	if (!ptr && (getcontext(&ctx)==0)) ptr = &ctx;
	ret = sxhlogstr (handle, level | LOG_RETTHROWLEVEL, fname, lineno, funcname,
							ptr, str);
	if (!RERR_ISOK(ret)) return ret;
	if (ret == LOG_NONE) return RERR_OK;
	if (level > ret) return RERR_OK;
	return do_throw (fname, lineno, funcname, str);
}


int
Slog::do_throw (	const char	*fname,
						int			lineno,
						const char	*funcname,
						const char	*str)
{
	char			*msg = NULL;
	std::string	msg2;

	if (!fname && !funcname) {
		msg = (char*)str;
	} else if (!fname) {
		msg = asprtf ((char*)"%s: %s", (char*)funcname, (char*)str);
	} else if (!funcname) {
		msg = asprtf ((char*)"%s(%d): %s", (char*)fname, lineno, (char*)str);
	} else {
		msg = asprtf ((char*)"%s (%s:%d): %s", (char*)funcname, (char*)fname,
							lineno, (char*)str);
	}
	if (!msg) return RERR_NOMEM;
	msg2 = msg;
	if (msg != str) free (msg);
	throw SlogExcept (msg2);
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
