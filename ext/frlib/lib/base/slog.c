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
#include <unistd.h>
#include <sys/types.h>
#include <syslog.h>
#include <stdarg.h>
#include <sys/file.h>
#include <time.h>
#include <sys/time.h>
#include <regex.h>
#include <ctype.h>
#include <sys/stat.h>
#include <fcntl.h>

#include "frcontext.h"


#include "slog.h"
#include "config.h"
#include "errors.h"
#include "prtf.h"
#include "textop.h"
#include "iopipe.h"
#include "fileop.h"
#include "stacktrace.h"
#include "frfinish.h"
#include <fr/thread.h>

struct logconf {
	char			*module;
	int			refcount;
	int			do_file_log;
	const char	*logfile;
	int 			do_syslog;
	int 			do_log_console;
	int 			do_stderr_log;
	int 			do_exec_cmd;
	const char	*cmd;
	int 			loglevel;
	int 			log_exec_level;
	int 			log_throw_level;
	int 			log_stktr_level;
	int 			leave_open;
	int 			cmd_timeout;
	const char	*match;
	const char	*fmatch;
	int			isopen;
	int			fd;
};

static struct logconf	defconf = {
	/* module */			NULL,
	/* refcount */			0,
	/* do_file_log */		0,
	/* logfile */			NULL,
	/* do_syslog */		1,
	/* do_log_console */	1,
	/* do_stderr_log */	1,
	/* do_exec_cmd */		0,
	/* cmd */				NULL,
	/* loglevel */			LOG_INFO,
	/* log_exec_level */	LOG_ALERT,
	/* log_throw_level*/	-1,
	/* log_stktr_level*/	-1,
	/* leave_open */		0,
	/* cmd_timeout */		2,
	/* match */				NULL,
	/* fmatch */			NULL,
	/* isopen */			0,
	/* fd */					2
};

#if 0	/* disable due to problems */
#ifdef Linux	/* does not work on Solaris - who the hell knows why */
# define USE_FLOCK
#endif
#endif

static struct logconf	*logconf = NULL;
static int					logconf_num = 0;
static int					logconf_issorted = 1;
static int 					has_logmatch = 0;

static char					old_log[1024];
static int 					old_level = -1;
static int 					old_repeated = 0;
static struct logconf	*old_lc = NULL;
static int					old_autofinish = 0;

static char 		*_LPROG = NULL;
static const char *LPROG = "SLOG";
#ifdef DEBUG
static char			*deflogfile = NULL;
#endif
static int log_usepid = -1;

static int config_read = 0;
static int read_config  ();
static int check_lprog ();

static int file_log (struct logconf*, int, void*, const char *);
static int sys_log (struct logconf*, int, void*, const char *);
static int log_console (struct logconf*, int, void*, const char*);
static int stderr_log (int, void*, const char*);
static int stderr_log2 (struct logconf*, int, void*, const char*);
static int exec_cmd (struct logconf*, int, const char*);
static int do_shlogstr (int, int, void*, const char*);

static int str2prio (const char *);
static const char *prio2str (int);
static struct logconf* handle2struct (int);
static int module2handle (const char*);
static int slog_minminlevel ();
static int slog_minthrowlevel ();

static int logmatch (const char*, int);
static int create_fsmlist ();
static int fsmlist_free ();
static void autofinish (int, void*);
static int doautofinish();
static const char *getthrdstr ();


int	SLOG_H_FRLIB = -1;


int
slog_setpid (pid)
	int	pid;
{
	log_usepid = pid;
	return RERR_OK;
}

int
slog_set_prog (prog)
	const char	*prog;
{
	const char	*s;

	if (_LPROG) free (_LPROG);
	_LPROG = NULL;
	if (prog) {
		prog = s = ( s = rindex (prog, '/') ) ? (s+1) : prog;
		_LPROG = strdup (prog);
	}
	if (_LPROG) {
		LPROG = _LPROG;
	} else {
		LPROG = "SLOG";
	}
#if 0		/* not here */
	SLOG_H_FRLIB = slog_gethandle ("frlib");
#endif
	return RERR_OK;
}

const char*
slog_get_prog ()
{
	return LPROG;
}

int
frlogf (
	int			level,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	if (SLOG_H_FRLIB < 0) SLOG_H_FRLIB = slog_gethandle ("frlib");
	va_start (ap, fmt);
	ret = vsxhlogf (SLOG_H_FRLIB, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
	va_end (ap);
	return ret;
}

int
slogf (
	int			level,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	va_start (ap, fmt);
	ret = vsxhlogf (0, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
	va_end (ap);
	return ret;
}

int
smlogf (
	const char	*module,
	int			level,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	va_start (ap, fmt);
	ret = vsxmlogf (module, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
	va_end (ap);
	return ret;
}

int
shlogf (
	int			handle,
	int			level,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	va_start (ap, fmt);
	ret = vsxhlogf (handle, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
	va_end (ap);
	return ret;
}

int
sxlogf (
	int			level,
	const char	*fname,
	int			lineno,
	const char	*funcname,
	void			*ptr,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	va_start (ap, fmt);
	ret = vsxhlogf (0, level, fname, lineno, funcname, ptr, fmt, ap);
	va_end (ap);
	return ret;
}

int
sxmlogf (
	const char	*module,
	int			level,
	const char	*fname,
	int			lineno,
	const char	*funcname,
	void			*ptr,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	va_start (ap, fmt);
	ret = vsxmlogf (module, level, fname, lineno, funcname, ptr, fmt, ap);
	va_end (ap);
	return ret;
}

int
sxhlogf (
	int			handle,
	int			level,
	const char	*fname,
	int			lineno,
	const char	*funcname,
	void			*ptr,
	const char	*fmt,
	...)
{
	va_list		ap;
	int			ret;
	ucontext_t	ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	va_start (ap, fmt);
	ret = vsxhlogf (handle, level, fname, lineno, funcname, ptr, fmt, ap);
	va_end (ap);
	return ret;
}

int
vslogf (level, fmt, ap)
	int			level;
	const char	*fmt;
	va_list		ap;
{
	ucontext_t	ctx;

	return vsxhlogf (0, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
}

int
vsmlogf (module, level, fmt, ap)
	const char	*module;
	int			level;
	const char	*fmt;
	va_list		ap;
{
	int			handle;
	ucontext_t	ctx;

	CF_MAY_READ;
	handle = module2handle (module);
	return vsxhlogf (handle, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
}

#if defined CORR_SPRINTF
# undef CORR_SPRINTF
#endif
/* if glibc >= 2.1 then we have correct sprintf behaviour, and can
 *	perform faster elaboration
 */
#if defined __GLIBC__ && ( __GLIBC__ > 2 || ( __GLIBC__ == 2 && \
			defined __GLIBC_MINOR__ && __GLIBC_MINOR__ >= 1 ) )
# define CORR_SPRINTF 1
#endif

#define DOLOG2(islevel,needlevel)	(((islevel) != LOG_NONE) && \
			((needlevel) != LOG_NONE) && ((islevel) <= needlevel))
#define DOLOG(islevel,needlevel)		(DOLOG2 (((islevel) & LOG_MASK_LEVEL), \
															(needlevel)))

int
vshlogf (handle, level, fmt, ap)
	int			handle;
	int			level;
	const char	*fmt;
	va_list		ap;
{
	ucontext_t	ctx;

	return vsxhlogf (handle, level, NULL, 0, NULL, 
					(getcontext(&ctx) == 0 ? &ctx : NULL), fmt, ap);
}

int
vsxlogf (level, fname, lineno, funcname, ptr, fmt, ap)
	int			level;
	const char	*fmt;
	const char	*fname, *funcname;
	int			lineno;
	va_list		ap;
	void			*ptr;
{
	ucontext_t	ctx;

	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	return vsxhlogf (0, level, fname, lineno, funcname, ptr, fmt, ap);
}

int
vsxmlogf (module, level, fname, lineno, funcname, ptr, fmt, ap)
	const char	*module;
	int			level;
	const char	*fmt;
	const char	*fname, *funcname;
	int			lineno;
	va_list		ap;
	void			*ptr;
{
	int			handle;
	ucontext_t	ctx;

	CF_MAY_READ;
	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	handle = module2handle (module);
	return vsxhlogf (handle, level, fname, lineno, funcname, ptr, fmt, ap);
}

int
vsxhlogf (handle, level, fname, lineno, funcname, ptr, fmt, ap)
	int			handle;
	int			level;
	const char	*fmt;
	const char	*fname, *funcname;
	int			lineno;
	va_list		ap;
	void			*ptr;
{
	char				sstr[1024];
	char				*str, *s;
	int				len;
#ifndef CORR_SPRINTF
	FILE				*f;
#endif
	va_list			ap2;
	int				ret;
	struct logconf	*lc;
	ucontext_t		ctx;

	if (!fmt || !*fmt || level < 0) return RERR_PARAM;

	CF_MAY_READ;
	if (level == LOG_NONE) return RERR_OK;
	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	if (!has_logmatch && !(level & LOG_STDERR)) {
		lc = handle2struct (handle);
		if (lc && !(DOLOG(level, lc->loglevel) ||
						DOLOG(level, lc->log_exec_level))) {
			return RERR_OK;
		}
	}
	for (s=(char*)fmt; *s; s++) if (*s=='%') break;
	if (!*s) return sxhlogstr (handle, level, fname, lineno, funcname, ptr, fmt);
#if defined va_copy
	va_copy(ap2, ap);
#elif defined __va_copy
	__va_copy(ap2, ap);
#else
#  error undefined va_copy
	ap2 = ap;
#endif

#ifdef CORR_SPRINTF
	len = vsnprintf (sstr, sizeof(sstr), fmt, ap2);
	sstr[sizeof(sstr)-1] = 0;
	str = sstr;
#else	/* CORR_SPRINTF */
	len = sizeof(sstr)-1;
	f = fopen ("/dev/null", "w");
	if (f) {
		len = vfprintf (f, fmt, ap2);
		if (len <= 0) len = sizeof(sstr)-1;
		fclose (f);
	}
#endif	/* CORR_SPRINTF */
	va_end(ap2);
	if (len >= (ssize_t)sizeof(sstr)) {
		str = malloc (len+1);
		if (str) {
			vsnprintf (str, len+1, fmt, ap);
			str[len] = 0;
		} else {
			len = sizeof (sstr) - 1;
			str = sstr;
		}
	}
#ifndef CORR_SPRINTF
	if (len < sizeof(sstr)) {
		vsnprintf (sstr, len+1, fmt, ap);
		sstr[len] = 0;
		str = sstr;
	}
#endif	/* CORR_SPRINTF */
	ret = sxhlogstr (handle, level, fname, lineno, funcname, ptr, str);
	if (str && str != sstr) free (str);
	return ret;
}


int
slogstr (level, msg)
	int			level;
	const char	*msg;
{
	ucontext_t	ctx;

	return do_shlogstr (0, level, ((getcontext (&ctx) == 0) ? &ctx : NULL), msg);
}

int
smlogstr (module, level, msg)
	const char	*module;
	int			level;
	const char	*msg;
{
	int			handle;
	ucontext_t	ctx;

	CF_MAY_READ;
	handle = module2handle (module);
	return do_shlogstr (handle, level, ((getcontext (&ctx) == 0) ? &ctx : NULL), msg);
}


int
shlogstr (handle, level, msg)
	int			handle;
	int			level;
	const char	*msg;
{
	ucontext_t	ctx;

	return do_shlogstr (handle, level, ((getcontext (&ctx) == 0) ? &ctx : NULL), msg);
}

static
int
do_shlogstr (handle, level, ptr, msg)
	int			handle;
	int			level;
	const char	*msg;
	void			*ptr;
{
	char				msg2[128];
	int				do_stderr_log;
	int				do_ret_throwlevel;
	int				ret, ret2;
	int				origlevel, hasprtstderr=0;
	struct logconf	*lc;
	ucontext_t		ctx;

	if (!msg || !*msg || level < 0) return RERR_PARAM;
	if (!ptr && (getcontext(&ctx)==0)) ptr = &ctx;
	/* get configuration */
	CF_MAY_READ;
	handle = logmatch (msg, handle);
	lc = handle2struct (handle);
	do_stderr_log = level & LOG_STDERR;
	do_ret_throwlevel = level & LOG_RETTHROWLEVEL;
	origlevel = level;
	level &= LOG_MASK_LEVEL;

#if 0
	if (lc->loglevel == LOG_NONE) return RERR_OK;
#endif
	ret = RERR_OK;
	if (do_stderr_log) {
		ret2 = stderr_log (origlevel, ptr, msg);
		if (ret2 < 0) {
			ret = ret2;
		} else {
			hasprtstderr = 1;
		}
	}
	if (!DOLOG (level, lc->loglevel)) {
		if (lc->do_exec_cmd && DOLOG (level, lc->log_exec_level)) {
			ret = exec_cmd (lc, level, msg);
			if (!RERR_ISOK(ret)) return ret;
		}
		return RERR_OK;
	}
	if (level == (old_level & LOG_MASK_LEVEL) && 
						!strcmp (old_log, msg) && lc == old_lc) {
#if 0	/* don't exec command more than once per error */
		if (lc->do_exec_cmd && level <= lc->log_exec_level) {
			ret2 = exec_cmd (lc, level, msg);
			if (ret2 < 0) ret = ret2;
		}
#endif
		old_repeated++;
		if (!old_autofinish) doautofinish ();
		return RERR_OK;
	} else {
		if (old_repeated > 0) {
			sprintf (msg2, "... last message repeated %d times", old_repeated+1);
			if ((old_level & LOG_MASK_LEVEL) <= old_lc->log_stktr_level) {
				old_level |= LOG_PRTSTACK;
			}
			if (lc->do_stderr_log) {
				ret2 = stderr_log2 (old_lc, old_level, ptr, msg2);
				if (ret2 < 0) ret = ret2;
			}
			if (lc->do_log_console) {
				ret2 = log_console (old_lc, old_level, ptr, msg2);
				if (ret2 < 0) ret = ret2;
			}
			if (lc->do_file_log) {
				ret2 = file_log (old_lc, old_level, ptr, msg2);
				if (ret2 < 0) ret = ret2;
			}
			if (lc->do_syslog) {
				ret2 = sys_log (old_lc, old_level, ptr, msg2);
				if (ret2 < 0) ret = ret2;
			}
		}
		old_repeated=0;
		/* old_level=origlevel; */ /* we don't want stacktrace in repeat message */
		old_level=level;
		old_lc = lc;
		strncpy (old_log, msg, sizeof (old_log)-1);
		old_log[sizeof(old_log)-1]=0;
	}
	if (level <= lc->log_stktr_level) {
		origlevel |= LOG_PRTSTACK;
	}
	if (lc->do_stderr_log && !hasprtstderr) {
		ret2 = stderr_log2 (lc, origlevel, ptr, msg);
		if (!RERR_ISOK(ret2)) ret = ret2;
	}
	if (lc->do_log_console) {
		ret2 = log_console (lc, origlevel, ptr, msg);
		if (!RERR_ISOK(ret2)) ret = ret2;
	}
	if (lc->do_file_log) {
		ret2 = file_log (lc, origlevel, ptr, msg);
		if (!RERR_ISOK(ret2)) ret = ret2;
	}
	if (lc->do_syslog) {
		ret2 = sys_log (lc, origlevel, ptr, msg);
		if (!RERR_ISOK(ret2)) ret = ret2;
	}
	if (lc->do_exec_cmd && DOLOG (level, lc->log_exec_level)) {
		ret2 = exec_cmd (lc, level, msg);
		if (!RERR_ISOK(ret2)) ret = ret2;
	}
	if (!RERR_ISOK(ret2)) return ret;
	if (do_ret_throwlevel) {
		return lc->log_throw_level < 0 ? LOG_NONE : lc->log_throw_level;
	}
	return RERR_OK;
}

int
sxlogstr (level, fname, lineno, funcname, ptr, msg)
	int			level;
	const char	*msg;
	const char	*fname, *funcname;
	int			lineno;
	void			*ptr;
{
	ucontext_t	ctx;

	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	return sxhlogstr (0, level, fname, lineno, funcname, ptr, msg);
}

int
sxmlogstr (module, level, fname, lineno, funcname, ptr, msg)
	const char	*module;
	int			level;
	const char	*msg;
	const char	*fname, *funcname;
	int			lineno;
	void			*ptr;
{
	int			handle;
	ucontext_t	ctx;

	CF_MAY_READ;
	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	handle = module2handle (module);
	return sxhlogstr (handle, level, fname, lineno, funcname, ptr, msg);
}


int
sxhlogstr (handle, level, fname, lineno, funcname, ptr, msg)
	int			handle;
	int			level;
	const char	*msg;
	const char	*fname, *funcname;
	int			lineno;
	void			*ptr;
{
	ucontext_t	ctx;

	if (!ptr && (getcontext (&ctx) == 0)) ptr = &ctx;
	if (!fname && !funcname) {
		return do_shlogstr (handle, level, ptr, msg);
	} else if (!fname) {
		return sxhlogf (handle, level, NULL, 0, NULL, ptr, "%s: %s", funcname, 
								msg);
	} else if (!funcname) {
		return sxhlogf (handle, level, NULL, 0, NULL, ptr, "%s(%d): %s", fname,
								lineno, msg);
	} else {
		return sxhlogf (handle, level, NULL, 0, NULL, ptr, "%s (%s:%d): %s", 
								funcname, fname, lineno, msg);
	}
	return RERR_INTERNAL;	/* should never reach here */
}


int
slogfinish ()
{
	char	msg2[128];
	int	i;

	if (old_lc && old_repeated > 0) {
		sprintf (msg2, "... last message repeated %d times", old_repeated+1);
		if (old_lc->do_stderr_log) stderr_log2 (old_lc, old_level, NULL, msg2);
		if (old_lc->do_log_console) log_console (old_lc, old_level, NULL, msg2);
		if (old_lc->do_file_log) file_log (old_lc, old_level, NULL, msg2);
		if (old_lc->do_syslog) sys_log (old_lc, old_level, NULL, msg2);
	}
	old_repeated=0;
	*old_log=0;
	old_level=-1;
	old_lc = NULL;
	for (i=0; i<logconf_num; i++) {
		if (logconf[i].fd>2) close (logconf[i].fd);
		logconf[i].fd = 0;
		logconf[i].isopen = 0;
	}
	return RERR_OK;
}


int
slog_getlevel (level)
	const char	*level;
{
	return str2prio (level);
}


int
slog_gethandle (module)
	const char	*module;
{
	int	handle;

	CF_MAYREAD;
	handle = module2handle (module);
	if (handle > 0) {
		logconf[handle].refcount++;
	}
	return handle;
}

void
slog_releasehandle (handle)
	int	handle;
{
	if (handle > 0 && handle < logconf_num) {
		if (logconf[handle].refcount > 0) logconf[handle].refcount--;
	}
	return;
}

void
slog_reconfig ()
{
	read_config ();
}

int
slog_minlevel ()
{
	return shlog_minlevel (0);
}

int
smlog_minlevel (module)
	const char	*module;
{
	int	handle;

	CF_MAY_READ;
	if (has_logmatch) return slog_minminlevel ();
	handle = module2handle (module);
	return shlog_minlevel (handle);
}

int
shlog_minlevel (handle)
	int	handle;
{
	struct logconf	*lc;

	CF_MAY_READ;
	if (has_logmatch) return slog_minminlevel ();
	lc = handle2struct (handle);
	if (!lc) return -1;
	return lc->loglevel >= lc->log_exec_level ? lc->loglevel : lc->log_exec_level;
}

int
slog_throwlevel ()
{
	return shlog_throwlevel (0);
}

int
smlog_throwlevel (module)
	const char	*module;
{
	int	handle;

	CF_MAY_READ;
	if (has_logmatch) return slog_minthrowlevel ();
	handle = module2handle (module);
	return shlog_throwlevel (handle);
}

int
shlog_throwlevel (handle)
	int	handle;
{
	struct logconf	*lc;

	CF_MAY_READ;
	if (has_logmatch) return slog_minthrowlevel ();
	lc = handle2struct (handle);
	if (!lc) return -1;
	return lc->log_throw_level < 0 ? LOG_NONE : lc->log_throw_level;
}

static __thread char thrdstr[32];
static __thread int hasthrdstr=0;

static
const char *
getthrdstr()
{
	if (!hasthrdstr) {
		if (frthr_isinit() && frthr_self) {
			sprintf (thrdstr, ";%.16s.%d", frthr_self->name?frthr_self->name:"thrd", frthr_self->id);
		} else {
			thrdstr[0] = 0;
		}
		hasthrdstr = 1;
	}
	return thrdstr;
}


/*
 * the real logger functions 
 */

static
int
file_log (lc, level, ptr, msg)
	struct logconf	*lc;
	int				level;
	const char		*msg;
	void				*ptr;
{
	char					*s;
	int					has_newline;
	struct timeval		tv;
	struct timezone	tz;
	time_t				t;
	struct tm			*ts;
	char					timestring[32];
	int					fd;
	const char			*slevel;
	int					doprtstack;

	if (!lc || !lc->logfile) return RERR_PARAM;
	doprtstack = level & LOG_PRTSTACK;
	level &= LOG_MASK_LEVEL;
	for (s=(char*)msg; *s; s++);
	s--;
	has_newline = (*s == '\n') ? 1 : 0;
	gettimeofday (&tv, &tz);
	t = tv.tv_sec;
	*timestring=0;
	ts = localtime(&t);
	strftime (timestring, sizeof(timestring)-5, "%Y-%m-%d %T", ts);
	sprintf (timestring+strlen(timestring), ".%03d", ((int)(tv.tv_usec))/1000);
	if (log_usepid < 0) log_usepid = getpid ();

	if (lc && lc->isopen) {
		fd = lc->fd;
	} else {
		fd = open (lc->logfile, O_RDWR|O_CREAT|O_APPEND, 0666);
		if (fd < 0) return RERR_SYSTEM;
		if (lc && lc->leave_open) {
			lc->fd = fd;
			lc->isopen = 1;
		}
	}

	slevel = prio2str (level);
#ifdef USE_FLOCK
	flock (fd, LOCK_EX);
#endif
	fdprtf (fd, "%s %s(%d%s)%s%s<%s>:%s%s", timestring, LPROG?LPROG:"<NULL>", 
				(int)log_usepid, getthrdstr(), ((lc && lc->module) ? ":" : ""),
				((lc && lc->module) ? lc->module : ""),
				slevel, msg, has_newline?"":"\n");
	if (doprtstack) {
		stktr_prtstack (fd, 0, ptr);
	}
#ifdef USE_FLOCK
	flock (fd, LOCK_UN);
#endif
	if (!lc || !lc->leave_open) {
		close (fd);
	}

	return RERR_OK;
}

static
int
log_console (lc, level, ptr, msg)
	struct logconf	*lc;
	int				level;
	const char		*msg;
	void				*ptr;
{
	char					*s;
	int					has_newline;
	struct timeval		tv;
	struct timezone	tz;
	time_t				t;
	struct tm			*ts;
	char					timestring[32];
	int					fd;
	const char			*slevel;
	int					doprtstack;

	doprtstack = level & LOG_PRTSTACK;
	level &= LOG_MASK_LEVEL;
	for (s=(char*)msg; *s; s++);
	s--;
	has_newline = (*s == '\n') ? 1 : 0;
	gettimeofday (&tv, &tz);
	t = tv.tv_sec;
	*timestring=0;
	ts = localtime(&t);
	strftime (timestring, sizeof(timestring)-5, "%Y-%m-%d %T", ts);
	sprintf (timestring+strlen(timestring), ".%03d", ((int)(tv.tv_usec))/1000);
	if (log_usepid < 0) log_usepid = getpid ();

	if (lc && lc->isopen) {
		fd = lc->fd;
	} else {
		fd = open ("/dev/console", O_RDWR|O_CREAT|O_APPEND, 0666);
		if (fd < 0) return RERR_SYSTEM;
		if (lc && lc->leave_open) {
			lc->fd = fd;
			lc->isopen = 1;
		}
	}
	slevel = prio2str (level);
	fdprtf (fd, "%s %s(%d%s)%s%s<%s>:%s%s", timestring, LPROG?LPROG:"<NULL>", 
				(int)log_usepid, getthrdstr(), ((lc && lc->module) ? ":" : ""),
				((lc && lc->module) ? lc->module : ""),
				slevel, msg, has_newline?"":"\n");
	if (doprtstack) {
		stktr_prtstack (fd, 0, ptr);
	}
	if (!lc || !lc->leave_open) {
		close (fd);
	}

	return RERR_OK;
}

static
int
stderr_log2 (lc, level, ptr, msg)
	struct logconf	*lc;
	int				level;
	const char		*msg;
	void				*ptr;
{
	return stderr_log (level, ptr, msg);
}

static
int
stderr_log (level, ptr, msg)
	int			level;
	const char	*msg;
	void			*ptr;
{
	const char	*s, *slevel;
	int			has_newline;
	int			doprtstack;

	doprtstack = level & LOG_PRTSTACK;
	level &= LOG_MASK_LEVEL;

	for (s=msg; *s; s++);
	s--;
	has_newline = (*s == '\n') ? 1 : 0;

	slevel = prio2str (level);
	fprintf (stderr, "%s<%s>:%s%s", LPROG?LPROG:"<NULL>", 
				slevel, msg, has_newline?"":"\n");
	if (doprtstack) {
		stktr_prtstack (2, 0, ptr);
	}

	return RERR_OK;
}

static
int
sys_log (lc, level, ptr, str)
	struct logconf	*lc;
	int				level;
	const char		*str;
	void				*ptr;
{
	int	doprtstack;

	doprtstack = level & LOG_PRTSTACK;
	level &= LOG_MASK_LEVEL;
	if (level > LOG_DEBUG) level = LOG_DEBUG;
	if (!lc || !lc->isopen) {
		openlog (LPROG, 0, LOG_USER);
		if (lc) lc->isopen = 1;
	}
	syslog (level, "%s", str);
	if (doprtstack) {
		/* how can we do ? */
	}
	if (!lc || !lc->leave_open) {
		closelog ();
		if (lc) lc->isopen = 0;
	}
	return RERR_OK;
}

static
int
exec_cmd (lc, level, str)
	struct logconf	*lc;
	int				level;
	const char		*str;
{
	size_t		sz;
	char			*so;
	const char	*s;
	const char	*slevel;
	static int	block_recursive = 0;
	int			ret;
	char			*cmd;

	if (!lc) return RERR_PARAM;
	if (!lc->cmd) return RERR_OK;
	if (block_recursive) return RERR_OK;
	sz=strlen(lc->cmd)+4;
	slevel = prio2str (level);
	for (s=lc->cmd; *s; s++) {
		if (*s != '%') continue;
		s++;
		switch (*s) {
		case 'p':
			sz += LPROG?strlen (LPROG):8;
			break;
		case 'P':
			sz += 12;
			break;
		case 'l':
			sz += 12;
			break;
		case 'L':
			sz += strlen (slevel);
			break;
		case 'M':
			sz += lc->module ? strlen (lc->module) : 2;
			break;
		case 'm':
			sz += str ? strlen (str) + 2 : 8;
			break;
		}
	}
	cmd = malloc (sz);
	if (!cmd) return RERR_NOMEM;
	for (so=cmd, s=lc->cmd; *s; s++) {
		if (*s != '%') {
			*so++ = *s;
			continue;
		}
		s++;
		switch (*s) {
		case 'p':
			strcpy (so, LPROG?LPROG:"\"<NULL>\"");
			so += LPROG?strlen (LPROG):8;
			break;
		case 'P':
			if (log_usepid < 0) log_usepid = getpid ();
			so += sprintf (so, "%d", (int) log_usepid);
			break;
		case 'l':
			so += sprintf (so, "%d", level);
			break;
		case 'L':
			strcpy (so, slevel);
			so += strlen (slevel);
			break;
		case 'M':
			strcpy (so, lc->module ? lc->module : "\"\"");
			so += lc->module ? strlen (lc->module) : 2;
			break;
		case 'm':
			so += sprintf (so, "\"%s\"", str?str:"<NULL>");
			break;
		case '%':
			strcpy (so, "%");
			so += 1;
			break;
		default:
			*so++ = *s;
			break;
		}
	}
	*so = 0;
	block_recursive = 1; /* this is not thread safe - but at most, some 
								 *	commands won't be executed - which however is
								 *	neither critical nor probable - so we don't care 
								 */
	ret = iopipe (	cmd, NULL, 0, NULL, NULL, lc->cmd_timeout, 
							IOPIPE_F_RET_EXIT_CODE);
	if (!RERR_ISOK(ret)) {
		frlogf (LOG_ERR, "slog::exec_cmd(): error executing log command >>%s<<: "
							"%d", cmd, ret);
	}
	free (cmd);
	block_recursive = 0;
	return ret;
}

static
int
doautofinish ()
{
	int	ret;

	if (old_autofinish) return RERR_OK;
	old_autofinish = 1;	/* set it before frregfinish to avoid recursive call */
	ret = frregfinish (autofinish, FRFINISH_F_ONFINISH | FRFINISH_F_ONDIE, NULL);
	if (!RERR_ISOK(ret)) {
		old_autofinish = 0;
		return ret;
	}
	return RERR_OK;
}

static
void
autofinish (flags, arg)
	int	flags;
	void	*arg;
{
	slogfinish ();
}



/* 
 * functions for reading configuration 
 */

static int logconf_addmodule (const char*);
static int logconf_readmodules ();
static int logconf_readmodules_var (const char*);
static int logconf_readconf ();
static int logconf_sort ();
static int logconf_clear ();


static
int
read_config ()
{
	int	has_handle=0;

	cf_begin_read ();
	check_lprog ();
	if (config_read) {
		slogfinish ();
		config_read = 0;
		logconf_clear ();
		fsmlist_free ();
		if (SLOG_H_FRLIB > 0) {
			has_handle = 1;
			slog_releasehandle (SLOG_H_FRLIB);
			SLOG_H_FRLIB = 0;
		}
	}
	/* in the following we ignore errors */
	logconf_readmodules ();
	logconf_readconf ();
	logconf_sort ();
	create_fsmlist ();
	config_read = 1;
	if (has_handle) {
		SLOG_H_FRLIB = slog_gethandle ("frlib");
	}
	cf_end_read_cb (&read_config);

	return 1;
}

static
int
check_lprog ()
{
	char	fname[64];
	char	*buf;
	int	ret;

	if (_LPROG) return RERR_OK;
	sprintf (fname, "/proc/%d/cmdline", (int)getpid());
	buf = fop_read_fn (fname);
	if (!buf) return RERR_SYSTEM;
	ret = slog_set_prog (buf);
	free (buf);
	return ret;
}


static
int
logconf_addmodule (module)
	const char	*module;
{
	int				i;
	struct logconf	*p;

	if (!logconf || logconf_num < 1) {
		logconf = malloc (10*sizeof (struct logconf));
		if (!logconf) return RERR_NOMEM;
		logconf_num=10;
		bzero (logconf, 10*sizeof (struct logconf));
	}
	if (!module || !*module) return RERR_OK;
	for (i=1; i<logconf_num; i++) {
		if (logconf[i].module && !strcasecmp (logconf[i].module, module)) break;
	}
	if (i<logconf_num) return RERR_OK;
	for (i=1; i<logconf_num; i++) {
		if (!logconf[i].module) break;
	}
	if (i<logconf_num) {
		logconf[i].module = strdup (module);
		if (!logconf[i].module) return RERR_NOMEM;
		if (logconf_issorted && i>1 && strcasecmp (module, 
													logconf[i].module) > 0) {
			logconf_issorted=0;
		}
		return RERR_OK;
	}
	p = realloc (logconf, (logconf_num+10)*sizeof (struct logconf));
	if (!p) return RERR_NOMEM;
	logconf = p;
	logconf += logconf_num;
	bzero (p, 10*sizeof (struct logconf));
	logconf_num += 10;
	p->module = strdup (module);
	if (!p->module) return RERR_NOMEM;
	if (logconf_issorted && (p-logconf)>1 && strcasecmp (module, 
															(p-1)->module) > 0) {
		logconf_issorted=0;
	}
	return RERR_OK;
}

static
int
logconf_readmodules_var (var)
	const char	*var;
{
	const char	*cidx, *val;
	char			*idx, *module, *prog, *p;
	int			ret, num, i;

	if (!var || !*var) return RERR_PARAM;
	num = cf_getnumarr (var);
	if (num < 1) return RERR_OK;
	for (i=0; i< num; i++) {
		cidx = NULL;
		val = cf_getarrxwi (var, i, &cidx);
		if (!val || !cidx) continue;
		p = idx = strdup (idx);
		if (!idx) return RERR_NOMEM;
		prog = top_getfield (&p, ",", TOP_F_NOSKIPBLANK|TOP_F_NOSKIPCOMMENT);
		module = top_getfield (&p, ",", TOP_F_NOSKIPBLANK|TOP_F_NOSKIPCOMMENT);
		if (!prog || !*prog || !module || !*module) {
			free (idx);
			continue;
		}
		if ((strcasecmp (prog, LPROG) != 0) && (strcmp (prog, "*") != 0)) {
			free (idx);
			continue;
		}
		ret = logconf_addmodule (module);
		free (idx);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

static
int
logconf_readmodules ()
{
	int	ret, ret2;

	ret2 = RERR_OK;
	ret = logconf_readmodules_var ("logfile");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_file");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("write_log");		/* old */
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_syslog");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("use_syslog");	/* old */
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_console");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_stderr");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_level");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("loglevel");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_exec_cmd");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_cmd_exec");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_exec_level");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_throw");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_throw_level");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("log_stacktrace");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("logfile_leaveopen");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("logmatch");
	if (!RERR_ISOK(ret)) ret2 = ret;
	ret = logconf_readmodules_var ("logfmatch");
	if (!RERR_ISOK(ret)) ret2 = ret;
	return ret2;
}

static
int
logconf_readconf ()
{
	int				i, ret;
	struct logconf	*p;
	const char		*s;

	if (logconf_num < 1) {
		ret = logconf_addmodule (NULL);
		if (!RERR_ISOK(ret)) return ret;
	}
	/* read global logger options */
	p = logconf;
	if (!p) return RERR_INTERNAL;
	p->logfile = cf_getarr ("log_file", LPROG);
	if (!p->logfile) p->logfile = cf_getarr ("logfile", LPROG);
#ifdef DEBUG
	if (!p->logfile) {
		if (deflogfile) free (deflogfile);
		deflogfile = malloc (strlen (LPROG?LPROG:"frlib")+8);
		if (deflogfile) {
			sprintf (deflogfile, "./%s.log", (LPROG?LPROG:"frlib"));
			p->logfile = deflogfile;
		}
	}
#endif
	s = cf_getarr ("write_log", LPROG);
	if (s) {
		p->do_file_log = cf_isyes (s);
	} else {
		p->do_file_log = p->logfile?1:0;
	}
	s = cf_getarr ("log_syslog", LPROG);
	p->do_syslog = cf_isyes (s?s:cf_getarr2 ("use_syslog", LPROG, "no"));
	p->do_log_console = cf_isyes (cf_getarr2 ("log_console", LPROG, "no"));
	p->do_stderr_log = cf_isyes (cf_getarr2 ("log_stderr", LPROG, "no"));
	if (!p->logfile) p->do_file_log = 0;
	s = cf_getarr ("log_level", LPROG);
	if (!s) s = cf_getarr ("loglevel", LPROG);
#ifdef DEBUG
	p->loglevel = s?str2prio(s):LOG_DEBUG;
#else
	p->loglevel = s?str2prio(s):LOG_INFO;
#endif
	if (p->logfile) {
		p->leave_open = cf_isyes (cf_getarr2 ("logfile_leaveopen", LPROG,
															"no"));
	}
	p->cmd = cf_getarr ("log_exec_cmd", LPROG);
	if (!p->cmd) p->cmd = cf_getarr ("log_cmd_exec", LPROG);
	if (p->cmd) p->do_exec_cmd = 1;
	s = cf_getarr2 ("log_exec_level", LPROG, "alert");
	p->log_exec_level = s?str2prio(s):LOG_ALERT;
	p->cmd_timeout = cf_atoi (cf_getarr2 ("log_cmd_timeout", LPROG, "2"));

	s = cf_getarr ("log_throw", LPROG);
	if (!s) {
		p->log_throw_level = -1;
	} else if (!strcasecmp (s, "no")) {
		p->log_throw_level = -1;
	} else if (!strcasecmp (s, "yes")) {
		p->log_throw_level = LOG_CRIT;
	} else if (!strcasecmp (s, "all")) {
		p->log_throw_level = p->loglevel;
	} else {
		p->log_throw_level = str2prio (s);
	}
	s = cf_getarr ("log_throw_level", LPROG);
	if (s) {
		p->log_throw_level = str2prio (s);
	}
	s =cf_getarr ("log_stacktrace", LPROG);
	if (!s) {
		p->log_stktr_level = -1;
	} else if (!strcasecmp (s, "no")) {
		p->log_stktr_level = -1;
	} else if (!strcasecmp (s, "yes")) {
		p->log_stktr_level = LOG_CRIT;
	} else if (!strcasecmp (s, "all")) {
		p->log_stktr_level = p->loglevel;
	} else {
		p->log_stktr_level = str2prio (s);
	}

	/* read all module logger options */
	for (i=1; i<logconf_num; i++) {
		p = logconf+i;
		if (!p->module) continue;
		p->logfile = cf_get2arr ("log_file", LPROG, p->module);
		if (!p->logfile) p->logfile = cf_get2arr ("logfile", LPROG, p->module);
		s = cf_get2arr ("write_log", LPROG, p->module);
		if (s) {
			p->do_file_log = cf_isyes (s);
		} else {
			p->do_file_log = p->logfile?1:0;
		}
		s = cf_get2arr ("log_syslog", LPROG, p->module); 
		p->do_syslog = cf_isyes (s ? s : cf_get2arr2 ("use_syslog", LPROG, 
													p->module, "no"));
		p->do_log_console = cf_isyes (cf_get2arr2 ("log_console", LPROG, 
															p->module, "no"));
		p->do_stderr_log = cf_isyes (cf_get2arr2 ("log_stderr", LPROG, 
															p->module, "no"));
		if (!p->logfile) p->do_file_log = 0;
		s = cf_get2arr ("log_level", LPROG, p->module);
		if (!s) s = cf_get2arr2 ("loglevel", LPROG, p->module, "info");
		p->loglevel = s?str2prio(s):LOG_INFO;
		if (p->logfile) {
			p->leave_open = cf_isyes (cf_get2arr2 ("logfile_leaveopen", 
															LPROG, p->module, "no"));
		}
		p->match = cf_get2arr ("logmatch", LPROG, p->module);
		p->fmatch = cf_get2arr ("logfmatch", LPROG, p->module);
		p->cmd = cf_get2arr ("log_exec_cmd", LPROG, p->module);
		if (!p->cmd) p->cmd = cf_get2arr ("log_cmd_exec", LPROG, p->module);
		if (p->cmd) p->do_exec_cmd = 1;
		s = cf_get2arr2 ("log_exec_level", LPROG, p->module, "alert");
		p->log_exec_level = s?str2prio(s):LOG_ALERT;
		p->cmd_timeout = cf_atoi (cf_get2arr2 ("log_cmd_timeout", LPROG, 
															p->module, "2"));
		s = cf_get2arr ("log_throw", LPROG, p->module);
		if (!s) {
			p->log_throw_level = -1;
		} else if (!strcasecmp (s, "no")) {
			p->log_throw_level = -1;
		} else if (!strcasecmp (s, "yes")) {
			p->log_throw_level = LOG_CRIT;
		} else if (!strcasecmp (s, "all")) {
			p->log_throw_level = p->loglevel;
		} else {
			p->log_throw_level = str2prio (s);
		}
		s = cf_get2arr ("log_throw_level", LPROG, p->module);
		if (s) {
			p->log_throw_level = str2prio (s);
		}
		s =cf_get2arr ("log_stacktrace", LPROG, p->module);
		if (!s) {
			p->log_stktr_level = -1;
		} else if (!strcasecmp (s, "no")) {
			p->log_stktr_level = -1;
		} else if (!strcasecmp (s, "yes")) {
			p->log_stktr_level = LOG_CRIT;
		} else if (!strcasecmp (s, "all")) {
			p->log_stktr_level = p->loglevel;
		} else {
			p->log_stktr_level = str2prio (s);
		}
	}

	return RERR_OK;
}

static
int
logconf_sort ()
{
	int				found=1, i;
	struct logconf	lc;

	if (logconf_issorted) return RERR_OK;	/* nothing to be done */
	while (found) {
		found=0;
		for (i=1; i<logconf_num-1; i++) {
			if (!logconf[i+1].module) continue;
			if (!logconf[i].module || strcasecmp (logconf[i].module, 
															logconf[i+1].module) > 0) {
				if (logconf[i].refcount || logconf[i+1].refcount) {
					if (!logconf[i].module) continue;
					/* in this case we cannot sort */
					return RERR_OK;
				}
				found=1;
				lc = logconf[i+1];
				logconf[i+1] = logconf[i];
				logconf[i] = lc;
			}
		}
	}
	/* trunk empty slots at end */
	for (i=logconf_num-1; i>1; i--) {
		if (!logconf[i].module) break;
	}
	logconf_num = i+1;
	logconf_issorted = 1;
	return RERR_OK;
}

static
int
logconf_clear ()
{
	int	i, ref;
	char	*s;

	for (i=0; i<logconf_num; i++) {
		s = logconf[i].module;
		ref = logconf[i].refcount;
		bzero (logconf+i, sizeof (struct logconf));
		if (ref) {
			logconf[i].refcount = ref;
			logconf[i].module = s;
		} else if (s) {
			free (s);
		}
	}
	/* trunk empty slots at end */
	for (i=logconf_num-1; i>1; i--) {
		if (!logconf[i].module) break;
	}
	logconf_num = i+1;
	return RERR_OK;
}



/*
 * other static functions
 */


struct lognames {
	const char	*str;
	int			prio;
};
static struct lognames lognames[] = {
	{ "panic", LOG_EMERG },
	{ "emergency", LOG_EMERG },
	{ "emerg", LOG_EMERG },
	{ "alert", LOG_ALERT },
	{ "critical", LOG_CRIT },
	{ "crit", LOG_CRIT },
	{ "error", LOG_ERR },
	{ "err", LOG_ERR },
	{ "warning", LOG_WARNING },
	{ "warn", LOG_WARNING },
	{ "notice", LOG_NOTICE },
	{ "info", LOG_INFO },
	{ "debug", LOG_DEBUG },
	{ "verbose", LOG_VERB },
	{ "verb", LOG_VERB },
	{ "vverbose", LOG_VVERB },
	{ "vverb", LOG_VVERB },
	{ "debug2", LOG_VERB },	/* for backward compatibility */
	{ "LOG_EMERG", LOG_EMERG },
	{ "LOG_PANIC", LOG_EMERG },
	{ "LOG_ALERT", LOG_ALERT },
	{ "LOG_CRIT", LOG_CRIT },
	{ "LOG_ERR", LOG_ERR },
	{ "LOG_ERROR", LOG_ERR },
	{ "LOG_WARNING", LOG_WARNING },
	{ "LOG_WARN", LOG_WARNING },
	{ "LOG_NOTICE", LOG_NOTICE },
	{ "LOG_INFO", LOG_INFO },
	{ "LOG_DEBUG", LOG_DEBUG },
	{ "LOG_VERBOSE", LOG_VERB },
	{ "LOG_VERB", LOG_VERB },
	{ "LOG_VVERBOSE", LOG_VVERB },
	{ "LOG_VVERB", LOG_VVERB },
	{ "LOG_ALL", LOG_VVERB },
	{ "all", LOG_VVERB },
	{ "none", LOG_NONE },
	{ "LOG_NONE", LOG_NONE },
	{ NULL, -1 }};



static
int
str2prio (level)
	const char	*level;
{
	struct lognames	*ln;
	int					prio;

	if (!level || !*level) return LOG_NONE;
	for (ln=lognames; ln->str; ln++)
		if (!strcasecmp (ln->str, level)) break;
	if (ln->str) {
		prio = ln->prio;
	} else if (isdigit (*level)) {
		prio = atoi (level);
		if (prio < 0) prio = 0;
		else if (prio > LOG_NONE) prio = LOG_NONE;
	} else {
		return LOG_NONE;
	}
	return prio;
}

static
const char*
prio2str (level)
	int	level;
{
	struct lognames	*ln;

	for (ln=lognames; ln->str; ln++) {
		if (ln->prio == level) return ln->str;
	}
	return "unknown";
}

static
int
module2handle (module)
	const char	*module;
{
	int	ret, i, min, max, pivot;

	if (logconf_num <= 1 || !module || !*module) return 0;
	if (logconf_issorted) {
		min = 1;
		max = logconf_num-1;
		while (max >= min) {
			pivot = (max + min) / 2;
			for (i=pivot; i<=max; i++) {
				if (logconf[i].module) break;
			}
			if (i>max) {
				max = pivot-1;
				continue;
			}
			ret = strcasecmp (module, logconf[i].module);
			if (ret == 0) return i;		/* we've found it */
			if (ret < 0) {
				max = pivot-1;
			} else {
				min = i+1;
			}
		}
	} else {		/* not sorted */
		for (i=1; i<logconf_num; i++) {
			if (!logconf[i].module) continue;
			if (!strcasecmp (module, logconf[i].module)) return i;
		}
	}
	return 0;	/* not found - return handle 0 as default */
}

static
struct logconf*
handle2struct (handle)
	int	handle;
{
	if (handle < 0 || handle > logconf_num || !logconf) handle = 0;
	if (handle > 0 && !logconf[handle].module) handle = 0;
	if (!logconf || logconf_num < 1) {
		return &defconf;
	}
	return logconf+handle;
}


static
int
slog_minthrowlevel ()
{
	int	i, min;

	CF_MAY_READ;
	min=10;
	for (i=0; i<logconf_num; i++) {
		if (logconf[i].log_throw_level > min) min = logconf[i].log_throw_level;
	}
	if (min == 10) min = defconf.log_throw_level;
	return min;
}

static
int
slog_minminlevel ()
{
	int	i, min;

	CF_MAY_READ;
	min=10;
	for (i=0; i<logconf_num; i++) {
		if (logconf[i].loglevel > min) min = logconf[i].loglevel;
		if (logconf[i].log_exec_level > min) min = logconf[i].log_exec_level;
	}
	if (min == 10) min = defconf.loglevel;
	return min;
}



/*
 * fsm functions 
 */

struct logfsm {
	int		handle;
	char		*str;
	int		len;
	int		isfast;
	regex_t	reg;
	int		use;
};

struct logfsm	*logfsm = NULL;
int				logfsm_num = 0;

static int fsm_match (struct logfsm*, const char*);
static int fsmfree (struct logfsm*);
static int create_fsm (struct logconf*);

static
int
create_fsm (lc)
	struct logconf	*lc;
{
	struct logfsm	*p;
	int				len, ret;
	char				errstr[128];
	const char		*s;

	if (!lc) return RERR_PARAM;
	if (!lc->match && !lc->fmatch) return RERR_OK;
	p = realloc (logfsm, (logfsm_num+1)*sizeof (struct logfsm));
	if (!p) return RERR_NOMEM;
	logfsm = p;
	p += logfsm_num;
	bzero (p, sizeof (struct logfsm));
	if (!lc->fmatch) {
		for (s=lc->match; s && *s; s++) {
			if (isalnum (*s) || *s=='_' || *s==':') continue;
			if (*s == '(' && s[1] == ')') {
				s++; continue;
			}
			break;
		}
		if (s && !*s) lc->fmatch = lc->match;	/* we are fast */
	}
	if (lc->fmatch) {
		p->str = (char*)lc->fmatch;
		p->len = strlen (p->str);
		p->isfast = 1;
	} else {
		len = strlen (lc->match);
		p->len = len + 2;
		p->str = malloc (len + 3);
		if (!p->str) return RERR_NOMEM;
		strcpy (p->str, lc->match);
		strcpy (p->str+len, ".*");		/* we always want to match from beginning,
													but don't care about end */
		ret = regcomp (&(p->reg), p->str, REG_EXTENDED|REG_ICASE|REG_NOSUB);
		if (ret) {
			regerror (ret, &(p->reg), errstr, sizeof(errstr));
			config_read = 1;	/* hack */
			FRLOGF (LOG_ERR, "error in compiling regular expression >>%s<<: %s",
									p->str, errstr);
			config_read = 0;
			regfree (&(p->reg));
			free (p->str);
			p->str = NULL;
			return RERR_CONFIG;
		}
	}
	p->handle = lc - logconf;
	p->use = 1;
	logfsm_num++;
	has_logmatch = 1;
	return RERR_OK;
}

static
int
fsmfree (fsm)
	struct logfsm	*fsm;
{
	if (!fsm) return RERR_PARAM;
	if (!fsm->isfast) {
		if (fsm->str) {
			free (fsm->str);
			regfree (&(fsm->reg));
		}
	}
	bzero (fsm, sizeof (struct logfsm));
	return RERR_OK;
}

static
int
fsmlist_free ()
{
	int	i;

	if (logfsm) {
		for (i=0; i<logfsm_num; i++) {
			fsmfree (logfsm+i);
		}
		free (logfsm);
	}
	logfsm_num = 0;
	has_logmatch = 0;
	return RERR_OK;
}


static
int
create_fsmlist ()
{
	int	i, ret, ret2;

	ret2 = RERR_OK;
	for (i=0; i<logconf_num; i++) {
		if (logconf[i].match || logconf[i].fmatch) {
			ret = create_fsm (logconf+i);
			if (!RERR_ISOK(ret)) ret2 = ret;
		}
	}
	return ret2;
}

static
int
fsm_match (fsm, str)
	struct logfsm	*fsm;
	const char		*str;
{
	int	ret;

	if (!fsm || !str || !*str) return 0;
	if (!fsm->use) return 0;
	if (fsm->isfast && fsm->str) {
		ret = strncasecmp (str, fsm->str, fsm->len);
	} else {
		ret = regexec (&(fsm->reg), str, 0, NULL, 0);
	}
	return (ret == 0);
}


static
int
logmatch (str, defhandle)
	const char	*str;
	int			defhandle;
{
	int	i;

	if (!logfsm || !str || !*str) return defhandle;
	for (i=0; i<logfsm_num; i++) {
		if (fsm_match (logfsm+i, str)) {
			return logfsm[i].handle;
		}
	}
	return defhandle;
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
