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

#ifndef _R__FRLIB_LIB_BASE_SLOG_H
#define _R__FRLIB_LIB_BASE_SLOG_H

#ifdef __cplusplus
extern "C" {
#endif

#include <stdlib.h>
#include <stdarg.h>
#include <syslog.h>
#include <fr/base/frcontext.h>

#define LOG_NONE				10
#define LOG_WARN				LOG_WARNING
#define LOG_ERROR				LOG_ERR
#define LOG_PANIC				LOG_EMERG
#define LOG_VERBOSE			8
#define LOG_VERB				8
#define LOG_VVERBOSE			9
#define LOG_VVERB				9
#define LOG_MASK_LEVEL		0x0f
#define LOG_STDERR			0x10
#define LOG_WARN2				(LOG_WARN|LOG_STDERR)
#define LOG_ERR2				(LOG_ERR|LOG_STDERR)
#define LOG_CRIT2				(LOG_CRIT|LOG_STDERR)
#define LOG_PRTSTACK			0x20	/* prints out a stack trace */
#define LOG_RETTHROWLEVEL	0x40	/* needed internally for c++ binding */

int slog_set_prog (const char *prog);
const char *slog_get_prog ();
int slog_setpid (int pid);
void slog_reconfig ();

int slogf (int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));
int vslogf (int level, const char *fmt, va_list);
int slogstr (int level, const char *msg);

int smlogf (const char *module, int level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int vsmlogf (const char *module, int level, const char *fmt, va_list);
int smlogstr (const char *module, int level, const char *msg);

int shlogf (int handle, int level, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
int vshlogf (int handle, int level, const char *fmt, va_list);
int shlogstr (int handle, int level, const char *msg);

int slogfinish ();
int slog_getlevel (const char *level);
int slog_gethandle (const char *module);
void slog_releasehandle (int handle);
int slog_minlevel ();
int smlog_minlevel (const char *module);
int shlog_minlevel (int handle);
#define slog_haslevel(level)				((level) <= slog_minlevel())
#define smlog_haslevel(level,module)	((level) <= smlog_minlevel(module))
#define shlog_haslevel(level,handle)	((level) <= shlog_minlevel(handle))

/* the following functions are used by the c++ binding */
int slog_throwlevel ();
int smlog_throwlevel (const char *module);
int shlog_throwlevel (int handle);


/* the following functions are used by the SLOGF macros */
int sxlogf (int level, const char *fname, int lineno, const char *funcname,
				void *ptr, const char *fmt, ...) __attribute__((format(printf, 6, 7)));
int vsxlogf (int level, const char *fname, int lineno, const char *funcname,
				void *ptr, const char *fmt, va_list);
int sxlogstr (int level, const char *fname, int lineno, const char *funcname,
				void *ptr, const char *msg);

int sxmlogf (const char *module, int level, const char *fname, int lineno, 
				const char *funcname, void *ptr, const char *fmt, ...)
				__attribute__((format(printf, 7, 8)));
int vsxmlogf (const char *module, int level, const char *fname, int lineno, 
				const char *funcname, void *ptr, const char *fmt, va_list);
int sxmlogstr (const char *module, int level, const char *fname, int lineno, 
				const char *funcname, void *ptr, const char *msg);

int sxhlogf (int handle, int level, const char *fname, int lineno, 
				const char *funcname, void *ptr, const char *fmt, ...)
				__attribute__((format(printf, 7, 8)));
int vsxhlogf (int handle, int level, const char *fname, int lineno, 
				const char *funcname, void *ptr, const char *fmt, va_list);
int sxhlogstr (int handle, int level, const char *fname, int lineno, 
				const char *funcname, void *ptr, const char *msg);

#include <fr/base/misc.h>
#define SLOGF(level,...) do { \
			char			_r__errstr[96]; \
			ucontext_t	_r__ctx; \
			*_r__errstr = 0; \
			getcontext (&_r__ctx); \
			sxhlogf (0, (level), __FILE__, __LINE__, \
						FR__FUNC__, &_r__ctx, __VA_ARGS__); \
		} while (0)
#define SMLOGF(module,level,...) do { \
			char			_r__errstr[96]; \
			ucontext_t	_r__ctx; \
			*_r__errstr = 0; \
			getcontext (&_r__ctx); \
			sxmlogf ((module), (level), __FILE__, \
						__LINE__, FR__FUNC__, &_r__ctx, __VA_ARGS__); \
		} while (0)
#define SHLOGF(handle,level,...) do { \
			char			_r__errstr[96]; \
			ucontext_t	_r__ctx; \
			*_r__errstr = 0; \
			getcontext (&_r__ctx); \
			sxhlogf ((handle), (level), __FILE__, \
						__LINE__, FR__FUNC__, &_r__ctx, __VA_ARGS__); \
		} while (0)

/* for backward compatibility */
#define SLOGFE SLOGF
#define SMLOGFE SMLOGF
#define SHLOGFE SHLOGF


#ifdef LOG_OLD_BINDING
#define logf			slogf
#define vlogf			vslogf
#define logstr			slogstr
#define logfinish		slogfinish
#define log_getlevel	slog_getlevel
#endif



/* 
 * the following is used only internally to frlib
 * do not use outside this scope!
 */
extern int SLOG_H_FRLIB;
int frlogf (int level, const char *fmt, ...) __attribute__((format(printf, 2, 3)));

#define FRLOGF(level,...) do { \
			char			_r__errstr[96]; \
			ucontext_t	_r__ctx; \
			*_r__errstr = 0; \
			getcontext (&_r__ctx); \
			sxhlogf (SLOG_H_FRLIB, (level), __FILE__, \
						__LINE__, FR__FUNC__, NULL, __VA_ARGS__); \
		} while (0)















#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_SLOG_H */


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
