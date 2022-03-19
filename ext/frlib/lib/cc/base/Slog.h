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

#ifndef _R__FRLIB_LIB_CC_BASE_SLOG_H
#define _R__FRLIB_LIB_CC_BASE_SLOG_H


#include <fr/base/errors.h>
#include <fr/base/slog.h>
#include <fr/base/misc.h>
#include <stdarg.h>
#include <stdexcept>

#define SCLOGF(m,level,...)	\
		do { \
			if (m) {\
				m->xlogf ((level), __FILE__, __LINE__, FR__FUNC__, 0, \
							__VA_ARGS__); \
			} \
		} while (0)
#define SCCLOGF(level,...)	\
		do { \
			xlogf ((level), __FILE__, __LINE__, FR__FUNC__, 0, __VA_ARGS__) \
		} while (0)


class Slog {
public:
	~Slog ();

	Slog (const char *module);

	int setProg (const char *prog)
	{
		return slog_set_prog ((char*)prog);
	};
	const char *getProg ()
	{
		return slog_get_prog ();
	};
	int setPID (int pid)
	{
		return slog_setpid (pid);
	};

	int logf (int level, const char *fmt, ...)
				__attribute__((format(printf, 3, 4)))
	{
		va_list	ap;
		int		ret;

		if (level > minLevel()) return RERR_OK;
		va_start (ap, fmt);
		ret = do_vlogf (level, NULL, 0, NULL, NULL, (char*)fmt, ap);
		va_end (ap);
		return ret;
	};
	int vlogf (int level, const char *fmt, va_list ap)
	{
		if (level > minLevel()) return RERR_OK;
		return do_vlogf (level, NULL, 0, NULL, NULL, (char*)fmt, ap);
	};
	int logStr (int level, const char *str)
	{
		if (level > minLevel()) return RERR_OK;
		return do_logstr (level, NULL, 0, NULL, NULL, (char*)str);
	};

	/* the following functions are needed for the SCLOGF macros */
	int xlogf (	int level,
					const char *fname,
					int lineno,
					const char *funcname,
					void *ptr,
					const char *fmt, ...)
			__attribute__((format(printf, 7, 8)))
	{
		va_list	ap;
		int		ret;

		if ((level&LOG_MASK_LEVEL) > minLevel()) return RERR_OK;
		va_start (ap, fmt);
		ret = do_vlogf (level, fname, lineno, funcname, ptr, (char*)fmt, ap);
		va_end (ap);
		return ret;
	};
	int vxlogf (int level,
					const char *fname,
					int lineno,
					const char *funcname,
					void *ptr,
					const char *fmt,
					va_list ap)
	{
		if ((level&LOG_MASK_LEVEL) > minLevel()) return RERR_OK;
		return do_vlogf (level, fname, lineno, funcname, ptr, (char*)fmt, ap);
	};
	int xlogStr (	int level,
						const char *fname,
						int lineno,
						const char *funcname,
						void *ptr,
						const char *str)
	{
		if ((level&LOG_MASK_LEVEL) > minLevel()) return RERR_OK;
		return do_logstr (level, fname, lineno, funcname, ptr, (char*)str);
	};
	int finish ()
	{
		return slogfinish ();
	};
	int strToLevel (const char *level)
	{
		return slog_getlevel ((char*)level);
	};
	int minLevel ()
	{
		return minlevel;
	};

protected:
	Slog ();
	int do_vlogf (	int level,
						const char *fname,
						int lineno,
						const char *funcname,
						void *ptr,
						const char *fmt,
						va_list ap);
	int do_logstr (int level,
						const char *fname,
						int lineno,
						const char *funcname,
						void *ptr,
						const char *str);
	int do_throw (	const char *fname,
						int lineno,
						const char *funcname,
						const char *str);
	

public:
	static Slog * getGlobalSlog ();

protected:
	int			handle;
	int			minlevel;
	int			throwlevel;
	static Slog *gslog;
};



/*
 * the following exists only, to destinguish between different throws,
 * however, you can still catch std::exception or std::runtime_error
 * from which it is derived
 */
class SlogExcept: public std::runtime_error
{
public:
	SlogExcept (const std::string& msg): std::runtime_error (msg) {};
};








#endif	/* _R__FRLIB_LIB_CC_BASE_SLOG_H */


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
