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

#ifndef _R__FRLIB_LIB_BASE_DLSTUB_H
#define _R__FRLIB_LIB_BASE_DLSTUB_H


#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdlib.h>
#include <dlfcn.h>

#include <fr/base/dl.h>
#include <fr/base/errors.h>

#define DLSTUB_F_NONE	0x00
#define DLSTUB_F_SILENT	0x01

#define EXTCALL(func)	_dlwrap_##func

int dlstub_ldfunc (	void **libh, const char *libcfg, const char *lib,
							const char **liblist, int flags);


#define DLSTUB_MKLDFUNC(libcfg,lib,list) \
	static void	*libh = NULL; \
	static int ldfunc () \
	{ \
		static const char	*_libcfg = libcfg; \
		static const char	*_lib	= lib; \
		static const char **_list = list; \
		static int			firstcall = 1; \
		int					flags; \
		\
		if (libh) return RERR_OK; \
		flags = firstcall ? 0 : DLSTUB_F_SILENT; \
		firstcall = 0; \
		return dlstub_ldfunc (&libh, _libcfg, _lib, _list, flags); \
	}\
	static void *mainh = NULL; \
	static void ldmain () \
	{ \
		if (!mainh) mainh = dlopen (NULL, RTLD_GLOBAL); \
	}

/*
 * creates a stub, that substitutes itself with the real function
 *
 * func  - the function name
 * t0    - the return type
 * t<n>  - the <n-th> argument type (starting with 1)
 * a<n>  - the <n-th> argument (starting with 1)
 */


#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A0(func,t0) \
	extern t0 (*_dlwrap_##func)();
#else
# define DLSTUB_MKSTUB_A0(func,t0) \
	static t0 _dlstub_##func (); \
	t0 (*_dlwrap_##func)() = _dlstub_##func; \
	static t0 _dlstub_##func () \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A1(func,t0,t1) \
	extern t0 (*_dlwrap_##func)(t1);
#else
# define DLSTUB_MKSTUB_A1(func,t0,t1) \
	static t0 _dlstub_##func (t1); \
	t0 (*_dlwrap_##func)(t1) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A2(func,t0,t1,t2) \
	extern t0 (*_dlwrap_##func)(t1,t2);
#else
# define DLSTUB_MKSTUB_A2(func,t0,t1,t2) \
	static t0 _dlstub_##func (t1,t2); \
	t0 (*_dlwrap_##func)(t1,t2) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1, t2 a2) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1, a2); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A3(func,t0,t1,t2,t3) \
	extern t0 (*_dlwrap_##func)(t1,t2,t3);
#else
# define DLSTUB_MKSTUB_A3(func,t0,t1,t2,t3) \
	static t0 _dlstub_##func (t1,t2,t3); \
	t0 (*_dlwrap_##func)(t1,t2,t3) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1, t2 a2, t3 a3) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1, a2, a3); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A4(func,t0,t1,t2,t3,t4) \
	extern t0 (*_dlwrap_##func)(t1,t2,t3,t4);
#else
# define DLSTUB_MKSTUB_A4(func,t0,t1,t2,t3,t4) \
	static t0 _dlstub_##func (t1,t2,t3,t4); \
	t0 (*_dlwrap_##func)(t1,t2,t3,t4) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1, t2 a2, t3 a3, t4 a4) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1, a2, a3, a4); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A5(func,t0,t1,t2,t3,t4,t5) \
	extern t0 (*_dlwrap_##func)(t1,t2,t3,t4,t5);
#else
# define DLSTUB_MKSTUB_A5(func,t0,t1,t2,t3,t4,t5) \
	static t0 _dlstub_##func (t1,t2,t3,t4,t5); \
	t0 (*_dlwrap_##func)(t1,t2,t3,t4,t5) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1, a2, a3, a4, a5); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A6(func,t0,t1,t2,t3,t4,t5,t6) \
	extern t0 (*_dlwrap_##func)(t1,t2,t3,t4,t5,t6);
#else
# define DLSTUB_MKSTUB_A6(func,t0,t1,t2,t3,t4,t5,t6) \
	static t0 _dlstub_##func (t1,t2,t3,t4,t5,t6); \
	t0 (*_dlwrap_##func)(t1,t2,t3,t4,t5,t6) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1, a2, a3, a4, a5, a6); \
	}
#endif

#if !defined DLSTUB_CFILE
# define DLSTUB_MKSTUB_A7(func,t0,t1,t2,t3,t4,t5,t6,t7) \
	extern t0 (*_dlwrap_##func)(t1,t2,t3,t4,t5,t6,t7);
#else
# define DLSTUB_MKSTUB_A7(func,t0,t1,t2,t3,t4,t5,t6,t7) \
	static t0 _dlstub_##func (t1,t2,t3,t4,t5,t6,t7); \
	t0 (*_dlwrap_##func)(t1,t2,t3,t4,t5,t6,t7) = _dlstub_##func; \
	static t0 _dlstub_##func (t1 a1, t2 a2, t3 a3, t4 a4, t5 a5, t6 a6, t7 a7) \
	{ \
		ldmain(); \
		if (!RERR_ISOK(frdlsym (&_dlwrap_##func,mainh,#func))) { \
			_dlwrap_##func = _dlstub_##func; \
			if (!RERR_ISOK(ldfunc())) return (t0)0; \
			if (!RERR_ISOK(frdlsym (&_dlwrap_##func,libh,#func))) { \
				_dlwrap_##func = _dlstub_##func; \
				return (t0)0; \
			} \
		} \
		return _dlwrap_##func (a1, a2, a3, a4, a5, a6, a7); \
	}
#endif













#endif	/* _R__FRLIB_LIB_BASE_DLSTUB_H */

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
