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
 * Portions created by the Initial Developer are Copyright (C) 2003-2020
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <errno.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdint.h>


#include <fr/base.h>

#include "parseevent.h"


static int adjust_name (char*, int);
static int parse_attrlist (struct event*, struct tlst*, struct tlst*, char*, int);
static int parse_arglist (struct event*, struct tlst*, char**, int);
static int do_parse (struct event*, char*);
static int do_mayfreeattr (struct event*, struct ev_attr*);
static int do_mayfreevar (struct event*, struct ev_var*);
static int do_mayfreearg (struct event*, struct ev_arg*);
static int arr_shift (struct tlst*, int);
static int do_addattr (struct event*, const char*, struct ev_val, int);
static int do_addattr_s (struct event*, const char*, const char*, int);
static int do_addvar (struct event*, const char*, int, int, struct ev_val, int);
static int do_addvar2 (struct event*, const char*, int, int, struct ev_val, int);
static int do_addvar_s (struct event*, const char*, int, int, const char*, int);
static int do_setarg (struct event*, struct tlst*, struct ev_val, int, int,int);
static int do_setarg_s (struct event*, struct tlst*, const char*, int, int, int);
static int do_printattr (struct tlst*, int, int);
static int do_printvar (struct tlst*, int, int);
static int do_printarg (struct tlst*, int, int);
static int do_printevent (struct event*, int, int);
static int do_printtarget (struct event*, int, int);
static int do_printval (struct ev_val, int, int);
static int do_getarg_r (struct ev_arg*, struct event*, int);
static int do_gettarg_r (struct ev_arg*, struct event*, int);
static int do_getarg_tr (struct ev_arg*, struct tlst*, int);
static int do_getattrpos_r (const char **, struct ev_val*, struct event*, int);
static int do_getvarptrpos_r (struct ev_var**, struct event*, int);
static int do_evvar_cmp (void*, void*);
static int do_evvar_cmp2 (void*, void*);
static int do_getvarpos_r (const char**, int*, int*, struct ev_val*, struct event*, int);
static int do_getvar_r (struct ev_val*, struct event*, const char*, int, int);
static int do_getvarptr_r (struct ev_var**, struct event*, const char*, int, int, int);
static int do_getattr_r (struct ev_val*, struct event*, const char*);
static int do_mkstrep_write (char*, struct ev_val, int);
static int do_mkstrep_len (struct ev_val, int);
static int do_mkstrep (char**, struct ev_val, int);
static int pmatoi (char*);
static int cpyargs (struct tlst*, struct tlst*, struct event*, struct bufref*);
static int cpyattr (struct tlst*, struct tlst*, struct event*, struct bufref*);
static int cpyvar (struct tlst*, struct tlst*, struct event*, struct bufref*);
static int rettype (int);
static int isnano (int);
#define return_type(t) return rettype(t)
#define return_isnano(t) return isnano(t)

static void _ev_rmstr (struct event*, const char*);
static const char * _ev_addstr (struct event*, const char*, int);
static const char * _ev_strdup (struct event*, const char*);
static char * _ev_addorrmstr (struct event*, char*);


#define MYT_ISDATE(t) EVP_T_ISDATE(t)
#define MYT_ISTIME(t) EVP_T_ISTIME(t)


int
ev_cparse (evout, evstr, flags)
	struct event	*evout;
	const char		*evstr;
	int				flags;
{
	return ev_parse (evout, (char*)evstr, flags | EVP_F_CPY);
}


int
ev_parse (evout, evstr, flags)
	struct event	*evout;
	char				*evstr;
	int				flags;
{
	int	ret;

	if (!evout || !evstr) return RERR_PARAM;
	if (!(flags & EVP_F_EXIST)) {
		ev_new (evout);
	}
	if (flags & EVP_F_CPY) {
		evstr = strdup (evstr);
		if (!evstr) return RERR_NOMEM;
	}
	ret = ev_addbuf2 (evout, evstr, strlen (evstr) + 1);
	if (!RERR_ISOK(ret)) {
		if (flags & EVP_F_CPY) free (evstr);
		return ret;
	}
	evout->flags = flags;
	ret = do_parse (evout, evstr);
	if (!RERR_ISOK(ret)) {
		ev_free (evout);
		return ret;
	}
	return RERR_OK;
}


int
ev_new (ev)
	struct event	*ev;
{
	if (!ev) return RERR_PARAM;
	bzero (ev, sizeof (struct event));
	TLST_NEW (ev->arg, struct ev_arg);
	TLST_NEW (ev->targetarg, struct ev_arg);
	TLST_NEW (ev->attr, struct ev_attr);
	TLST_NEW (ev->var, struct ev_var);
	ev->bufref = BUFREF_INIT;
	return RERR_OK;
}

int
ev_addbuf (ev, buf)
	struct event	*ev;
	char				*buf;
{
	if (!ev) return RERR_PARAM;
	return bufref_add2 (&ev->bufref, buf);
}

int
ev_addbuf2 (ev, buf, blen)
	struct event	*ev;
	char				*buf;
	size_t			blen;
{
	if (!ev) return RERR_PARAM;
	return bufref_add (&ev->bufref, buf, blen);
}

int
ev_addbufref (ev, buf)
	struct event	*ev;
	char				*buf;
{
	if (!ev) return RERR_PARAM;
	return bufref_addref (&ev->bufref, buf);
}

int
ev_rmbuf (ev, buf)
	struct event	*ev;
	char				*buf;
{
	if (!ev) return RERR_PARAM;
	return bufref_rmbuf (&ev->bufref, buf);
}
	

int
ev_free (ev)
	struct event	*ev;
{
	if (!ev) return RERR_PARAM;
	TLST_FREE (ev->arg);
	TLST_FREE (ev->targetarg);
	TLST_FREE (ev->attr);
	TLST_FREE (ev->var);
	bufref_free (&ev->bufref);
	bzero (ev, sizeof (struct event));
	return ev_new (ev);
}

int
ev_clear (ev)
	struct event	*ev;
{
#if 0		/* still buggy??? */
	if (!ev) return RERR_PARAM;
	TLST_RESET (ev->arg);
	TLST_RESET (ev->targetarg);
	TLST_RESET (ev->attr);
	TLST_RESET (ev->var);
	bufref_reset (&ev->bufref);
	ev->name = NULL;
	ev->targetname = NULL;
	return RERR_OK;
#else
	return ev_free (ev);
#endif
}

static
const char *
_ev_strcpy (dest, dstref, str)
	struct event	*dest;
	struct bufref	*dstref;
	const char		*str;
{
	const char	*str2;

	if (!str || !dstref || !dest) return NULL;
	str2 = bufref_refcpy (dstref, str);
	if (!str2) {
		str2 = _ev_addstr (dest, str, 1);
	}
	return str2;
}
	

int
ev_copy (dest, src)
	struct event	*dest, *src;
{
	int				ret;
	struct bufref	dstref = BUFREF_INIT;

	if (!dest || !src) return RERR_PARAM;
	ret = bufref_cpy (&dstref, &src->bufref);
	if (!RERR_ISOK(ret)) return ret;

	dest->name = _ev_strcpy (dest, &dstref, src->name);
	if (!dest->name && src->name) return RERR_NOMEM;

	dest->targetname = _ev_strcpy (dest, &dstref, src->targetname);
	if (!dest->targetname && src->targetname) return RERR_NOMEM;
	dest->hastarget = src->hastarget;

	ret = cpyargs (&(dest->arg), &(src->arg), dest, &dstref);
	if (!RERR_ISOK(ret)) return ret;
	ret = cpyargs (&(dest->targetarg), &(src->targetarg), dest, &dstref);
	if (!RERR_ISOK(ret)) return ret;
	ret = cpyattr (&(dest->attr), &(src->attr), dest, &dstref);
	if (!RERR_ISOK(ret)) return ret;
	ret = cpyvar (&(dest->var), &(src->var), dest, &dstref);
	if (!RERR_ISOK(ret)) return ret;

	ret = bufref_cpyfinish (&dest->bufref, &dstref);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

int
ev_setnamef (
	struct event	*ev,
	const char		*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!fmt || !*fmt || !ev) return RERR_PARAM;
	va_start (ap, fmt);
	ret = ev_vsetnamef (ev, fmt, ap);
	va_end (ap);
	return ret;
}

int
ev_vsetnamef (ev, fmt, ap)
	struct event	*ev;
	const char		*fmt;
	va_list			ap;
{
	char	*buf;
	int	ret;

	buf = vasprtf (fmt, ap);
	if (!buf) {
		if (errno == ENOMEM) return RERR_NOMEM;
		return RERR_SYSTEM;
	}
	buf = _ev_addorrmstr (ev, buf);
	if (!buf) return RERR_NOMEM;
	ret = ev_setname (ev, buf, 0);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, buf);
		return ret;
	}
	return RERR_OK;
}

int
ev_setname (ev, name, flags)
	struct event	*ev;
	const char		*name;
	int				flags;
{
	if (!ev) return RERR_PARAM;
	if (ev->name) {
		_ev_rmstr (ev, ev->name);
		ev->name = NULL;
		if (!name) return RERR_OK;
	}
	if (flags & EVP_F_CPY) {
		name = _ev_addstr (ev, name, 1);
		if (!name) return RERR_NOMEM;
	}
	ev->name = name;
	return RERR_OK;
}

int
ev_settnamef (
	struct event	*ev,
	const char		*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!fmt || !*fmt || !ev) return RERR_PARAM;
	va_start (ap, fmt);
	ret = ev_vsettnamef (ev, fmt, ap);
	va_end (ap);
	return ret;
}

int
ev_vsettnamef (ev, fmt, ap)
	struct event	*ev;
	const char		*fmt;
	va_list			ap;
{
	char	*buf;
	int	ret;

	buf = vasprtf (fmt, ap);
	if (!buf) {
		if (errno == ENOMEM) return RERR_NOMEM;
		return RERR_SYSTEM;
	}
	buf = _ev_addorrmstr (ev, buf);
	if (!buf) return RERR_NOMEM;
	ret = ev_settname (ev, buf, 0);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, buf);
		return ret;
	}
	return RERR_OK;
}

int
ev_settname (ev, name, flags)
	struct event	*ev;
	const char		*name;
	int				flags;
{
	if (!ev) return RERR_PARAM;
	if (ev->targetname) {
		_ev_rmstr (ev, ev->targetname);
		ev->targetname = NULL;
		ev->hastarget = 0;
		if (!name) return RERR_OK;
	}
	if (flags & EVP_F_CPY) {
		name = _ev_addstr (ev, name, 1);
		if (!name) return RERR_NOMEM;
	}
	ev->targetname = name;
	if (ev->targetname) ev->hastarget = 1;
	return RERR_OK;
}


int
ev_addattr_s (ev, var, val, flags)
	struct event	*ev;
	const char		*var, *val;
	int				flags;
{
	return do_addattr_s (ev, var, val, flags);
}


int
ev_addattr_i (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	int64_t			val;
	int				flags;
{
	struct ev_val	ival;

	ival.typ = EVP_T_INT;
	ival.i = val;
	return do_addattr (ev, var, ival, flags);
}

int
ev_addattr_d (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	tmo_t				val;
	int				flags;
{
	struct ev_val	ival;

	if (flags & EVP_F_NANO) {
		ival.typ = EVP_T_NDATE;
	} else {
		ival.typ = EVP_T_DATE;
	}
	ival.d = val;
	return do_addattr (ev, var, ival, flags);
}

int
ev_addattr_t (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	tmo_t				val;
	int				flags;
{
	struct ev_val	ival;

	if (flags & EVP_F_NANO) {
		ival.typ = EVP_T_NTIME;
	} else {
		ival.typ = EVP_T_TIME;
	}
	ival.d = val;
	return do_addattr (ev, var, ival, flags);
}

int
ev_addattr_f (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	double			val;
	int				flags;
{
	struct ev_val	ival;

	ival.typ = EVP_T_FLOAT;
	ival.f = val;
	return do_addattr (ev, var, ival, flags);
}


int
ev_addattr_v (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	struct ev_val	val;
	int				flags;
{
	return do_addattr (ev, var, val, flags);
}



int
ev_addattrf (
	struct event	*ev,
	int				flags,
	const char		*var,
	const char		*valfmt,
	...)
{
	va_list	ap;
	int		ret;

	va_start (ap, valfmt);
	ret = ev_vaddattrf (ev, flags, var, valfmt, ap);
	va_end (ap);
	return ret;
}


int
ev_vaddattrf (ev, flags, var, valfmt, ap)
	struct event	*ev;
	const char		*var, *valfmt;
	int				flags;
	va_list			ap;
{
	char	*str;
	int	ret;

	str = vasprtf (valfmt, ap);
	if (!str) return RERR_NOMEM;
	str = _ev_addorrmstr (ev, str);
	if (!str) return RERR_NOMEM;
	flags &= ~EVP_F_CPY;
	ret = do_addattr_s (ev, var, str, flags);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, str);
		return ret;
	}
	return RERR_OK;
}



int
ev_addvar_s (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var, *val;
	int				idx1, idx2;
	int				flags;
{
	return do_addvar_s (ev, var, idx1, idx2, val, flags);
}


int
ev_addvar_i (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	int64_t			val;
	int				flags;
{
	struct ev_val	ival;

	ival.typ = EVP_T_INT;
	ival.i = val;
	return do_addvar (ev, var, idx1, idx2, ival, flags);
}

int
ev_addvar_d (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	tmo_t				val;
	int				flags;
{
	struct ev_val	ival;

	if (flags & EVP_F_NANO) {
		ival.typ = EVP_T_NDATE;
	} else {
		ival.typ = EVP_T_DATE;
	}
	ival.d = val;
	return do_addvar (ev, var, idx1, idx2, ival, flags);
}

int
ev_addvar_t (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	tmo_t				val;
	int				flags;
{
	struct ev_val	ival;

	if (flags & EVP_F_NANO) {
		ival.typ = EVP_T_NTIME;
	} else {
		ival.typ = EVP_T_TIME;
	}
	ival.d = val;
	return do_addvar (ev, var, idx1, idx2, ival, flags);
}

int
ev_addvar_f (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	double			val;
	int				flags;
{
	struct ev_val	ival;

	ival.typ = EVP_T_FLOAT;
	ival.f = val;
	return do_addvar (ev, var, idx1, idx2, ival, flags);
}

int
ev_addvar_v (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	struct ev_val	val;
	int				flags;
{
	if (val.typ == EVP_T_STR) {
		return do_addvar_s (ev, var, idx1, idx2, val.s, flags);
	} else {
		return do_addvar (ev, var, idx1, idx2, val, flags);
	}
}

int
ev_addvar_rm (ev, var, idx1, idx2, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	int				flags;
{
	struct ev_val	ival;

	bzero (&ival, sizeof (struct ev_val));
	ival.typ = EVP_T_RM;
	ival.i = 0;
	return do_addvar (ev, var, idx1, idx2, ival, flags);
}



int
ev_addvarf (
	struct event	*ev,
	int				flags,
	const char		*var,
	int				idx1,
	int				idx2,
	const char		*valfmt,
	...)
{
	va_list	ap;
	int		ret;

	va_start (ap, valfmt);
	ret = ev_vaddvarf (ev, flags, var, idx1, idx2, valfmt, ap);
	va_end (ap);
	return ret;
}


int
ev_vaddvarf (ev, flags, var, idx1, idx2, valfmt, ap)
	struct event	*ev;
	const char		*var, *valfmt;
	int				idx1, idx2;
	int				flags;
	va_list			ap;
{
	char	*str;
	int	ret;

	str = vasprtf (valfmt, ap);
	if (!str) return RERR_NOMEM;
	str = _ev_addorrmstr (ev, str);
	if (!str) return RERR_NOMEM;
	ret = do_addvar_s (ev, var, idx1, idx2, str, flags);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, str);
		return ret;
	}
	return RERR_OK;
}



int
ev_setarg_s (ev, arg, pos, flags)
	struct event	*ev;
	const char		*arg;
	int				pos, flags;
{
	if (!ev) return RERR_PARAM;
	return do_setarg_s (ev, &(ev->arg), arg, pos, 0, flags);
}

int
ev_settarg_s (ev, arg, pos, flags)
	struct event	*ev;
	const char		*arg;
	int				pos, flags;
{
	if (!ev) return RERR_PARAM;
	return do_setarg_s (ev, &(ev->targetarg), arg, pos, 1, flags);
}

int
ev_setarg_i (ev, arg, pos, flags)
	struct event	*ev;
	int64_t			arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	xarg.typ = EVP_T_INT;
	xarg.i = arg;
	return do_setarg (ev, &(ev->arg), xarg, pos, 0, flags);
}

int
ev_settarg_i (ev, arg, pos, flags)
	struct event	*ev;
	int64_t			arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	xarg.typ = EVP_T_INT;
	xarg.i = arg;
	return do_setarg (ev, &(ev->targetarg), xarg, pos, 1, flags);
}

int
ev_setarg_d (ev, arg, pos, flags)
	struct event	*ev;
	tmo_t				arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	if (flags & EVP_F_NANO) {
		xarg.typ = EVP_T_NDATE;
	} else {
		xarg.typ = EVP_T_DATE;
	}
	xarg.d = arg;
	return do_setarg (ev, &(ev->arg), xarg, pos, 0, flags);
}

int
ev_settarg_d (ev, arg, pos, flags)
	struct event	*ev;
	tmo_t				arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	if (flags & EVP_F_NANO) {
		xarg.typ = EVP_T_NDATE;
	} else {
		xarg.typ = EVP_T_DATE;
	}
	xarg.d = arg;
	return do_setarg (ev, &(ev->targetarg), xarg, pos, 1, flags);
}

int
ev_setarg_t (ev, arg, pos, flags)
	struct event	*ev;
	tmo_t				arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	if (flags & EVP_F_NANO) {
		xarg.typ = EVP_T_NTIME;
	} else {
		xarg.typ = EVP_T_TIME;
	}
	xarg.d = arg;
	return do_setarg (ev, &(ev->arg), xarg, pos, 0, flags);
}

int
ev_settarg_t (ev, arg, pos, flags)
	struct event	*ev;
	tmo_t				arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	if (flags & EVP_F_NANO) {
		xarg.typ = EVP_T_NTIME;
	} else {
		xarg.typ = EVP_T_TIME;
	}
	xarg.d = arg;
	return do_setarg (ev, &(ev->targetarg), xarg, pos, 1, flags);
}

int
ev_setarg_f (ev, arg, pos, flags)
	struct event	*ev;
	double			arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	xarg.typ = EVP_T_FLOAT;
	xarg.f = arg;
	return do_setarg (ev, &(ev->arg), xarg, pos, 0, flags);
}

int
ev_settarg_f (ev, arg, pos, flags)
	struct event	*ev;
	double			arg;
	int				pos, flags;
{
	struct ev_val	xarg;

	xarg.typ = EVP_T_FLOAT;
	xarg.f = arg;
	return do_setarg (ev, &(ev->targetarg), xarg, pos, 1, flags);
}

int
ev_setarg_v (ev, arg, pos, flags)
	struct event	*ev;
	struct ev_val	arg;
	int				pos, flags;
{
	return do_setarg (ev, &(ev->arg), arg, pos, 0, flags);
}

int
ev_settarg_v (ev, arg, pos, flags)
	struct event	*ev;
	struct ev_val	arg;
	int				pos, flags;
{
	return do_setarg (ev, &(ev->targetarg), arg, pos, 1, flags);
}


int
ev_setargf (
	struct event	*ev,
	int				pos,
	int				flags,
	const char		*argfmt,
	...)
{
	va_list	ap;
	int		ret;

	va_start (ap, argfmt);
	ret = ev_vsetargf (ev, pos, flags, argfmt, ap);
	va_end (ap);
	return ret;
}


int
ev_vsetargf (ev, pos, flags, argfmt, ap)
	struct event	*ev;
	const char		*argfmt;
	int				flags, pos;
	va_list			ap;
{
	char	*str;
	int	ret;

	str = vasprtf (argfmt, ap);
	if (!str) return RERR_NOMEM;
	flags &= ~EVP_F_CPY;
	ret = ev_setarg_s (ev, str, pos, flags);
	if (!RERR_ISOK(ret)) {
		free (str);
		return ret;
	}
	return RERR_OK;
}

int
ev_settargf (
	struct event	*ev,
	int				pos,
	int				flags,
	const char		*argfmt,
	...)
{
	va_list	ap;
	int		ret;

	va_start (ap, argfmt);
	ret = ev_vsetargf (ev, pos, flags, argfmt, ap);
	va_end (ap);
	return ret;
}


int
ev_vsettargf (ev, pos, flags, argfmt, ap)
	struct event	*ev;
	const char		*argfmt;
	int				flags, pos;
	va_list			ap;
{
	char	*str;
	int	ret;

	str = vasprtf (argfmt, ap);
	if (!str) return RERR_NOMEM;
	flags &= ~EVP_F_CPY;
	ret = ev_settarg_s (ev, str, pos, flags);
	if (!RERR_ISOK(ret)) {
		free (str);
		return ret;
	}
	return RERR_OK;
}



int
ev_rmarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	*arg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = TLST_GETPTR (arg, ev->arg, pos);
	if (RERR_ISOK(ret)) do_mayfreearg (ev, arg);
	return TLST_REMOVE (ev->arg, pos, TLST_F_SHIFT);
}

int
ev_rmtarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	*arg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = TLST_GETPTR (arg, ev->targetarg, pos);
	if (RERR_ISOK(ret)) do_mayfreearg (ev, arg);
	return TLST_REMOVE (ev->targetarg, pos, TLST_F_SHIFT);
}

int
ev_rmattr (ev, var)
	struct event	*ev;
	const char		*var;
{
	int	pos;

	if (!ev || !var || !*var) return RERR_PARAM;
	pos = TLST_SEARCH (ev->attr, var, tlst_cmpistr);
	if (!RERR_ISOK(pos)) return pos;
	return ev_rmattrpos (ev, pos);
}

int
ev_rmattrpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_attr	*attr;

	if (!ev || pos < 0) return RERR_PARAM;
	ret = TLST_GETPTR (attr, ev->attr, pos);
	if (RERR_ISOK(ret)) do_mayfreeattr (ev, attr);
	return TLST_REMOVE (ev->attr,pos,TLST_F_SHIFT);
}

int
ev_rmvar (ev, var, idx1, idx2)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				pos;
	struct ev_var	avar;

	if (!ev || !var || !*var) return RERR_PARAM;
	bzero (&avar, sizeof (struct ev_var));
	avar.var = (char*)var;
	avar.idx1 = idx1;
	avar.idx2 = idx2;
	pos = TLST_SEARCH (ev->var, avar, do_evvar_cmp);
	if (!RERR_ISOK(pos)) return pos;
	return ev_rmvarpos (ev, pos);
}

int
ev_rmvarpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_var	*var;

	if (!ev || pos < 0) return RERR_PARAM;
	ret = TLST_GETPTR (var, ev->var, pos);
	if (RERR_ISOK(ret)) do_mayfreevar (ev, var);
	return TLST_REMOVE (ev->var,pos,TLST_F_SHIFT);
}

int
ev_getname (out, ev)
	const char		**out;
	struct event	*ev;
{
	if (!ev || !out) return RERR_PARAM;
	*out = ev->name;
	return RERR_OK;
}

int
ev_gettname (out, ev)
	const char		**out;
	struct event	*ev;
{
	if (!ev || !out) return RERR_PARAM;
	*out = ev->targetname;
	return RERR_OK;
}


int
ev_getarg_s (out, ev, pos)
	const char		**out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xarg.arg.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.s;
	return RERR_OK;
}

int
ev_gettarg_s (out, ev, pos)
	const char		**out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xarg.arg.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.s;
	return RERR_OK;
}

int
ev_getarg_i (out, ev, pos)
	int64_t			*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	switch (xarg.arg.typ) {
	case EVP_T_INT:
		if (out) *out = xarg.arg.i;
		break;
	case EVP_T_DATE:
	case EVP_T_TIME:
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		if (out) *out = (int64_t)xarg.arg.d;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}


int
ev_gettarg_i (out, ev, pos)
	int64_t			*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	switch (xarg.arg.typ) {
	case EVP_T_INT:
		if (out) *out = xarg.arg.i;
		break;
	case EVP_T_DATE:
	case EVP_T_TIME:
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		if (out) *out = (int64_t)xarg.arg.d;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
ev_getarg_in (out, ev, pos)
	int				*out;
	struct event	*ev;
	int				pos;
{
	int64_t	x = 0;
	int		ret;

	ret = ev_getarg_i (&x, ev, pos);
	if (*out) *out = (int)x;
	return ret;
}

int
ev_gettarg_in (out, ev, pos)
	int				*out;
	struct event	*ev;
	int				pos;
{
	int64_t	x = 0;
	int		ret;

	ret = ev_gettarg_i (&x, ev, pos);
	if (*out) *out = (int)x;
	return ret;
}


int
ev_getarg_d (out, ev, pos)
	tmo_t				*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISDATE (xarg.arg.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.d;
	return RERR_OK;
}

int
ev_gettarg_d (out, ev, pos)
	tmo_t				*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISDATE (xarg.arg.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.d;
	return RERR_OK;
}

int
ev_getarg_t (out, ev, pos)
	tmo_t				*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISTIME (xarg.arg.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.d;
	return RERR_OK;
}

int
ev_gettarg_t (out, ev, pos)
	tmo_t				*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISTIME (xarg.arg.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.d;
	return RERR_OK;
}

int
ev_getarg_f (out, ev, pos)
	double			*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xarg.arg.typ != EVP_T_FLOAT) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.f;
	return RERR_OK;
}

int
ev_gettarg_f (out, ev, pos)
	double			*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xarg.arg.typ != EVP_T_FLOAT) return RERR_INVALID_TYPE;
	if (out) *out = xarg.arg.f;
	return RERR_OK;
}

int
ev_getarg_v (out, ev, pos)
	struct ev_val	*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (out) *out = xarg.arg;
	return RERR_OK;
}

int
ev_gettarg_v (out, ev, pos)
	struct ev_val	*out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (out) *out = xarg.arg;
	return RERR_OK;
}


int
ev_getarg_sr (out, ev, pos)
	char				**out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return do_mkstrep (out, xarg.arg, TOP_F_NOQUOTESIGN);
}

int
ev_gettarg_sr (out, ev, pos)
	char				**out;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_arg	xarg;

	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return do_mkstrep (out, xarg.arg, TOP_F_NOQUOTESIGN);
}


int
ev_getattrpos_s (outvar, outval, ev, pos)
	const char		**outvar, **outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattrpos_r (outvar, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.s;
	return RERR_OK;
}

int
ev_getattrpos_i (outvar, outval, ev, pos)
	const char		**outvar;
	int64_t			*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattrpos_r (outvar, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (outval) *outval = xval.i;
		break;
	case EVP_T_DATE:
	case EVP_T_TIME:
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		if (outval) *outval = (int64_t)xval.d;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
ev_getattrpos_d (outvar, outval, ev, pos)
	const char		**outvar;
	tmo_t				*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattrpos_r (outvar, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISDATE (xval.typ)) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.d;
	return RERR_OK;
}

int
ev_getattrpos_t (outvar, outval, ev, pos)
	const char		**outvar;
	tmo_t				*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattrpos_r (outvar, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISTIME (xval.typ)) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.d;
	return RERR_OK;
}

int
ev_getattrpos_f (outvar, outval, ev, pos)
	const char		**outvar;
	double			*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattrpos_r (outvar, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_FLOAT) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.f;
	return RERR_OK;
}

int
ev_getattrpos_v (outvar, outval, ev, pos)
	const char		**outvar;
	struct ev_val	*outval;
	struct event	*ev;
	int				pos;
{
	return do_getattrpos_r (outvar, outval, ev, pos);
}

int
ev_getattrpos_sr (outvar, outval, ev, pos)
	const char		**outvar;
	char				**outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattrpos_r (outvar, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return do_mkstrep (outval, xval, TOP_F_NOQUOTESIGN);
}


int
ev_getattr_s (out, ev, var)
	const char		**out;
	struct event	*ev;
	const char		*var;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattr_r (&xval, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (out) *out = xval.s;
	return RERR_OK;
}

int
ev_getattr_i (out, ev, var)
	int64_t			*out;
	struct event	*ev;
	const char		*var;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattr_r (&xval, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (out) *out = xval.i;
		break;
	case EVP_T_DATE:
	case EVP_T_TIME:
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		if (out) *out = (int64_t)xval.d;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
ev_getattr_in (out, ev, var)
	int				*out;
	struct event	*ev;
	const char		*var;
{
	int64_t	x=0;
	int		ret;

	ret = ev_getattr_i (&x, ev, var);
	if (out) *out = (int)x;
	return ret;
}


int
ev_getattr_d (out, ev, var)
	tmo_t				*out;
	struct event	*ev;
	const char		*var;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattr_r (&xval, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISDATE (xval.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xval.d;
	return RERR_OK;
}

int
ev_getattr_t (out, ev, var)
	tmo_t				*out;
	struct event	*ev;
	const char		*var;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattr_r (&xval, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISTIME (xval.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xval.d;
	return RERR_OK;
}

int
ev_getattr_f (out, ev, var)
	double			*out;
	struct event	*ev;
	const char		*var;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattr_r (&xval, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_FLOAT) return RERR_INVALID_TYPE;
	if (out) *out = xval.f;
	return RERR_OK;
}

int
ev_getattr_v (out, ev, var)
	struct ev_val	*out;
	struct event	*ev;
	const char		*var;
{
	return do_getattr_r (out, ev, var);
}

int
ev_getattr_sr (out, ev, var)
	char				**out;
	struct event	*ev;
	const char		*var;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getattr_r (&xval, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	return do_mkstrep (out, xval, TOP_F_NOQUOTESIGN);
}

int
ev_getvarptr (out, ev, var, idx1, idx2)
	struct ev_var	**out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	return do_getvarptr_r (out, ev, var, idx1, idx2, 0);
}

int
ev_getvarptr2 (out, ev, var, idx1, idx2)
	struct ev_var	**out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	return do_getvarptr_r (out, ev, var, idx1, idx2, 1);
}

int
ev_getvarptrpos (out, ev, pos)
	struct ev_var	**out;
	struct event	*ev;
	int				pos;
{
	return do_getvarptrpos_r (out, ev, pos);
}

int
ev_getvarpos_s (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar, **outval;
	int				*idx1, *idx2;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvarpos_r (outvar, idx1, idx2, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.s;
	return RERR_OK;
}

int
ev_getvarpos_i (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	int64_t			*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvarpos_r (outvar, idx1, idx2, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (outval) *outval = xval.i;
		break;
	case EVP_T_DATE:
	case EVP_T_TIME:
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		if (outval) *outval = (int64_t)xval.d;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
ev_getvarpos_in (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	int				*outval;
	struct event	*ev;
	int				pos;
{
	int64_t	x=0;
	int		ret;

	ret = ev_getvarpos_i (outvar, idx1, idx2, &x, ev, pos);
	if (outval) *outval = (int)x;
	return ret;
}


int
ev_getvarpos_d (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	tmo_t				*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvarpos_r (outvar, idx1, idx2, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISDATE (xval.typ)) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.d;
	return RERR_OK;
}

int
ev_getvarpos_t (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	tmo_t				*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvarpos_r (outvar, idx1, idx2, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISTIME (xval.typ)) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.d;
	return RERR_OK;
}

int
ev_getvarpos_f (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	double			*outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvarpos_r (outvar, idx1, idx2, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_FLOAT) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.f;
	return RERR_OK;
}

int
ev_getvarpos_v (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	struct ev_val	*outval;
	struct event	*ev;
	int				pos;
{
	return do_getvarpos_r (outvar, idx1, idx2, outval, ev, pos);
}

int
ev_getvarpos_sr (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	char				**outval;
	struct event	*ev;
	int				pos;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvarpos_r (outvar, idx1, idx2, &xval, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return do_mkstrep (outval, xval, TOP_F_NOQUOTESIGN);
}


int
ev_getvar_s (out, ev, var, idx1, idx2)
	const char		**out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvar_r (&xval, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (out) *out = xval.s;
	return RERR_OK;
}

int
ev_getvar_i (out, ev, var, idx1, idx2)
	int64_t			*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvar_r (&xval, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (out) *out = xval.i;
		break;
	case EVP_T_DATE:
	case EVP_T_TIME:
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		if (out) *out = (int64_t)xval.d;
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
ev_getvar_in (out, ev, var, idx1, idx2)
	int				*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int64_t	x=0;
	int		ret;

	ret = ev_getvar_i (&x, ev, var, idx1, idx2);
	if (out) *out = (int)x;
	return ret;
}


int
ev_getvar_d (out, ev, var, idx1, idx2)
	tmo_t				*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvar_r (&xval, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISDATE (xval.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xval.d;
	return RERR_OK;
}

int
ev_getvar_t (out, ev, var, idx1, idx2)
	tmo_t				*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvar_r (&xval, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	if (!MYT_ISTIME (xval.typ)) return RERR_INVALID_TYPE;
	if (out) *out = xval.d;
	return RERR_OK;
}

int
ev_getvar_f (out, ev, var, idx1, idx2)
	double			*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvar_r (&xval, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_FLOAT) return RERR_INVALID_TYPE;
	if (out) *out = xval.f;
	return RERR_OK;
}

int
ev_getvar_v (out, ev, var, idx1, idx2)
	struct ev_val	*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	return do_getvar_r (out, ev, var, idx1, idx2);
}

int
ev_getvar_sr (out, ev, var, idx1, idx2)
	char				**out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	struct ev_val	xval;

	ret = do_getvar_r (&xval, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	return do_mkstrep (out, xval, TOP_F_NOQUOTESIGN);
}

int
ev_hasvar (ev, var, idx1, idx2)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	struct ev_var	cmpvar;
	int				ret;

	if (!ev || !var) return RERR_PARAM;
	bzero (&cmpvar, sizeof (struct ev_var));
	cmpvar.var = (char*)var;
	cmpvar.idx1 = idx1;
	cmpvar.idx2 = idx2;
	ret = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp);
	if (ret == RERR_NOT_FOUND) {
		return RERR_FAIL;
	} else if (!RERR_ISOK(ret)) {
		return ret;
	}
	return RERR_OK;
}


int
ev_numarg (ev)
	struct event	*ev;
{
	if (!ev) return RERR_PARAM;
	return TLST_GETNUM (ev->arg);
}


int
ev_numtarg (ev)
	struct event	*ev;
{
	if (!ev) return RERR_PARAM;
	return TLST_GETNUM (ev->targetarg);
}


int
ev_numattr (ev)
	struct event	*ev;
{
	if (!ev) return RERR_PARAM;
	return TLST_GETNUM (ev->attr);
}

int
ev_numvar (ev)
	struct event	*ev;
{
	if (!ev) return RERR_PARAM;
	return TLST_GETNUM (ev->var);
}



int
ev_typeofarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	xarg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_type (xarg.arg.typ);
}

int
ev_typeoftarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	xarg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_type (xarg.arg.typ);
}

int
ev_typeofattrpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getattrpos_r (NULL, &val, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_type (val.typ);
}

int
ev_typeofattr (ev, var)
	struct event	*ev;
	const char		*var;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getattr_r (&val, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	return_type (val.typ);
}

int
ev_typeofvarpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getvarpos_r (NULL, NULL, NULL, &val, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_type (val.typ);
}

int
ev_typeofvar (ev, var, idx1, idx2)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getvar_r (&val, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	return_type (val.typ);
}

int
ev_ntypeofarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	xarg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return (xarg.arg.typ);
}

int
ev_ntypeoftarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	xarg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return (xarg.arg.typ);
}

int
ev_ntypeofattrpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getattrpos_r (NULL, &val, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return (val.typ);
}

int
ev_ntypeofattr (ev, var)
	struct event	*ev;
	const char		*var;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getattr_r (&val, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	return (val.typ);
}

int
ev_ntypeofvarpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getvarpos_r (NULL, NULL, NULL, &val, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return (val.typ);
}

int
ev_ntypeofvar (ev, var, idx1, idx2)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getvar_r (&val, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	return (val.typ);
}

int
ev_isnanoarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	xarg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_isnano (xarg.arg.typ);
}

int
ev_isnanotarg (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_arg	xarg;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_gettarg_r (&xarg, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_isnano (xarg.arg.typ);
}

int
ev_isnanoattrpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getattrpos_r (NULL, &val, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_isnano (val.typ);
}

int
ev_isnanoattr (ev, var)
	struct event	*ev;
	const char		*var;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getattr_r (&val, ev, var);
	if (!RERR_ISOK(ret)) return ret;
	return_isnano (val.typ);
}

int
ev_isnanovarpos (ev, pos)
	struct event	*ev;
	int				pos;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getvarpos_r (NULL, NULL, NULL, &val, ev, pos);
	if (!RERR_ISOK(ret)) return ret;
	return_isnano (val.typ);
}

int
ev_isnanovar (ev, var, idx1, idx2)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	struct ev_val	val;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = do_getvar_r (&val, ev, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	return_isnano (val.typ);
}



int
ev_create (out, ev, flags)
	struct event	*ev;
	char				**out;
	int				flags;
{
	char	*buf;
	int	len, ret;

	if (!ev || !out) return RERR_PARAM;
	len = ev_getoutsize (ev, flags);
	if (len < 0) return len;
	buf = malloc (len+1);
	if (!buf) return RERR_NOMEM;
	ret = ev_writeout (buf, ev, flags);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	*out = buf;
	return RERR_OK;
}

int
ev_getoutsize (ev, flags)
	struct event	*ev;
	int				flags;
{
	int				num;
	struct ev_arg	arg;
	struct ev_attr	attr;
	struct ev_var	var;
	int				hasattr;
	unsigned			i;
	char				dummy[4];

	if (!ev) return RERR_PARAM;
	num=2;
	if (ev->hastarget && !(flags & EVP_F_NOPRTTARGET)) {
		if (ev->targetname) num+=strlen (ev->targetname);
		num++;
		TLST_FOREACH2 (arg, ev->targetarg, i) {
			num += do_mkstrep_len (arg.arg, 0) + 1;
		}
		num++;
	}
	if (ev->name) num+=strlen (ev->name);
	num++;
	if (!(flags & EVP_F_NOPRTARG)) {
		TLST_FOREACH2 (arg, ev->arg, i) {
			num += do_mkstrep_len (arg.arg, 0) + 1;
		}
	}
	num+=3;
	hasattr=0;
	if (!(flags & EVP_F_NOPRTATTR)) {
		TLST_FOREACH2 (attr, ev->attr, i) {
			if (!attr.var) continue;
			num += strlen (attr.var) + 1;
			num += do_mkstrep_len (attr.val, TOP_F_FORCEQUOTE) + 2;
			hasattr=1;
		}
	}
	if (!(flags & EVP_F_NOPRTVAR)) {
		TLST_FOREACH2 (var, ev->var, i) {
			if (!var.var) continue;
			if (var.idx2 >= 0) {
				num += snprintf (	dummy, 2, "var/%s/%d/%d=", var.var, var.idx1,
										var.idx2) + 1;
				if (var.idx1 >= 0) num++;
			} else if (var.idx1 >= 0) {
				num += snprintf (dummy, 2, "var/%s/%d=", var.var, var.idx1) + 1;
			} else {
				num += strlen (var.var) + 5;
			}
			num += do_mkstrep_len (var.val, TOP_F_FORCEQUOTE) + 2;
			hasattr=1;
		}
	}
	if (!hasattr) num++;
	if (flags & EVP_F_WRNL) num++;
	return num;
}

int
ev_writeout (out, ev, flags)
	struct event	*ev;
	char				*out;
	int				flags;
{
	struct ev_arg	arg;
	struct ev_attr	attr;
	struct ev_var	var;
	int				hasattr, len;
	unsigned			i;

	if (!ev || !out) return RERR_PARAM;
	if (ev->hastarget && !(flags & EVP_F_NOPRTTARGET)) {
		if (!ev->targetname && !(flags & EVP_F_FORCE)) return RERR_INVALID_FORMAT;
		out += sprintf (out, "# %s:", ev->targetname?ev->targetname:"");
		TLST_FOREACH2 (arg, ev->targetarg, i) {
			len = do_mkstrep_write (out, arg.arg, 0);
			if (len < 0) return len;
			out += len;
			*out = ':';
			out++;
		}
		*out = ':';
		out++;
	} else {
		strcpy (out, "* ");
		out+=2;
	}
	out += sprintf (out, "%s:", ev->name?ev->name:"");
	if (!ev->name && !(flags & EVP_F_FORCE)) return RERR_INVALID_FORMAT;
	if (!(flags & EVP_F_NOPRTARG)) {
		TLST_FOREACH2 (arg, ev->arg, i) {
			len = do_mkstrep_write (out, arg.arg, 0);
			if (len < 0) return len;
			out += len;
			*out = ':';
			out++;
		}
	}
	out += sprintf (out, ":{ ");
	hasattr=0;
	if (!(flags & EVP_F_NOPRTATTR)) {
		TLST_FOREACH2 (attr, ev->attr, i) {
			if (!attr.var) continue;
			if (hasattr) {
				out += sprintf (out, ", ");
			} else {
				hasattr = 1;
			}
			out += sprintf (out, "%s=", attr.var);
			len = do_mkstrep_write (out, attr.val, TOP_F_FORCEQUOTE);
			if (len < 0) return len;
			out += len;
		}
	}
	if (!(flags & EVP_F_NOPRTVAR)) {
		TLST_FOREACH2 (var, ev->var, i) {
			if (!var.var) continue;
			if (hasattr) {
				out += sprintf (out, ", ");
			} else {
				hasattr = 1;
			}
			if (var.idx2 >= 0) {
				if (var.idx1 >= 0) {
					out += sprintf (	out, "var/%s/p%d/p%d=", var.var, var.idx1,
											var.idx2);
				} else {
					out += sprintf (	out, "var/%s/m%d/p%d=", var.var,
											(-1)*var.idx1, var.idx2);
				}
			} else if (var.idx1 >= 0) {
				out += sprintf (out, "var/%s/p%d=", var.var, var.idx1);
			} else {
				out += sprintf (out, "var/%s=", var.var);
			}
			len = do_mkstrep_write (out, var.val, TOP_F_FORCEQUOTE);
			if (len < 0) return len;
			out += len;
		}
	}
	if (hasattr) {
		out += sprintf (out, " }");
	} else {
		out += sprintf (out, "}");
	}
	if (flags & EVP_F_WRNL) out += sprintf (out, "\n");
	return RERR_OK;
}



/************************
 * print functions
 ************************/

int
ev_prtparsed (ev, fd, flags)
	struct event	*ev;
	int				fd, flags;
{
	int	ret, ret2 = RERR_OK;

	if (!ev || fd < 0) return RERR_PARAM;
	if (flags & EVP_F_PRTTIME) {
		char	tstr[16];
		ret = cjg_strftime3 (tstr, sizeof (tstr), "%T.%.3q", tmo_now());
		if (RERR_ISOK(ret)) {
			fdprtf (fd, "%s: \n", tstr);
		}
	}
	if (ev->hastarget && !(flags & EVP_F_NOPRTTARGET)) {
		ret = do_printtarget (ev, fd, flags);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	ret = do_printevent (ev, fd, flags);
	if (!RERR_ISOK(ret)) ret2 = ret;
	if (!RERR_ISOK(ret2)) return ret2;
	return RERR_OK;
}

static
int
do_printtarget (ev, fd, flags)
	struct event	*ev;
	int				fd, flags;
{
	if (!ev || fd < 0) return RERR_PARAM;
	if (flags & EVP_F_PRTTIME) fdprtf (fd, " ");
	fdprtf (fd, "target: >>%s<<\n", ev->targetname ? ev->targetname : "<NULL>");
	return do_printarg (&(ev->targetarg), fd, flags);
}


static
int
do_printevent (ev, fd, flags)
	struct event	*ev;
	int				fd, flags;
{
	int	ret, ret2 = RERR_OK;

	if (!ev || fd < 0) return RERR_PARAM;
	if (flags & EVP_F_PRTTIME) fdprtf (fd, " ");
	fdprtf (fd, "event: >>%s<<\n", ev->name ? ev->name : "<NULL>");
	if (!(flags & EVP_F_NOPRTARG)) {
		ret = do_printarg (&(ev->arg), fd, flags);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	if (!(flags & EVP_F_NOPRTATTR)) {
		if (TLST_GETNUM(ev->attr) > 0) {
			fdprtf (fd, "  attributes:\n");
		}
		ret = do_printattr (&(ev->attr), fd, flags);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	if (!(flags & EVP_F_NOPRTVAR)) {
		if (TLST_GETNUM(ev->var) > 0) {
			fdprtf (fd, "  variables:\n");
		}
		ret = do_printvar (&(ev->var), fd, flags);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	if (!RERR_ISOK(ret2)) return ret2;
	return RERR_OK;
}

static
int
do_printarg (tlst, fd, flags)
	struct tlst	*tlst;
	int			fd, flags;
{
	struct ev_arg	arg;
	unsigned			i;

	if (!tlst || fd < 0) return RERR_PARAM;
	TLST_FOREACH2 (arg, *tlst, i) {
		fdprtf (fd, "    arg[%d] = ", i);
		do_printval (arg.arg, fd, flags);
	}
	return RERR_OK;
}

static
int
do_printattr (tlst, fd, flags)
	struct tlst	*tlst;
	int			fd, flags;
{
	struct ev_attr	attr;
	unsigned			i;

	if (!tlst || fd < 0) return RERR_PARAM;
	TLST_FOREACH2 (attr, *tlst, i) {
		fdprtf (fd, "    %s = ", attr.var);
		do_printval (attr.val, fd, flags);
	}
	return RERR_OK;
}

static
int
do_printvar (tlst, fd, flags)
	struct tlst	*tlst;
	int			fd, flags;
{
	struct ev_var	var;
	unsigned			i;

	if (!tlst || fd < 0) return RERR_PARAM;
	TLST_FOREACH2 (var, *tlst, i) {
		fdprtf (fd, "    %s", var.var);
		if (var.idx1 >= 0) fdprtf (fd, ".%d", var.idx1);
		if (var.idx2 >= 0) fdprtf (fd, " [%d]", var.idx2);
		fdprtf (fd, " = ");
		do_printval (var.val, fd, flags);
	}
	return RERR_OK;
}

static
int
do_printval (val, fd, flags)
	struct ev_val	val;
	int				fd, flags;
{
	int	ret, len;
	char	buf[64], *out;

	if (flags & EVP_F_PRTSTR) {
		len = do_mkstrep_len (val, TOP_F_FORCEQUOTE);
		if (len < 0) return len;
		if (len >= (ssize_t)sizeof (buf)) {
			len = do_mkstrep (&out, val, TOP_F_FORCEQUOTE);
		} else {
			len = do_mkstrep_write (buf, val, TOP_F_FORCEQUOTE);
			out = buf;
		}
		if (len < 0) return len;
		fdprtf (fd, "%s\n", out);
		if (out != buf) free (out);
		return RERR_OK;
	}
	if (flags & EVP_F_PRTTYP) {
		switch (val.typ) {
		case EVP_T_VOID:
			fdprtf (fd, "(void)  ");
			break;
		case EVP_T_RM:
			fdprtf (fd, "(remove)");
			break;
		case EVP_T_NONE:
		case EVP_T_STR:
			fdprtf (fd, "(str)   ");
			break;
		case EVP_T_INT:
			fdprtf (fd, "(int)   ");
			break;
		case EVP_T_FLOAT:
			fdprtf (fd, "(float) ");
			break;
		case EVP_T_DATE:
			fdprtf (fd, "(date)  ");
			break;
		case EVP_T_NDATE:
			fdprtf (fd, "(ndate) ");
			break;
		case EVP_T_TIME:
			fdprtf (fd, "(time)  ");
			break;
		case EVP_T_NTIME:
			fdprtf (fd, "(ntime) ");
			break;
		default:
			fdprtf (fd, "err: %d\n", val.typ);
			return RERR_INVALID_TYPE;
		}
	}
	switch (val.typ) {
	case EVP_T_VOID:
	case EVP_T_RM:
		fdprtf (fd, "\n");
		break;
	case EVP_T_NONE:
		fdprtf (fd, "\"\"\n");
		break;
	case EVP_T_STR:
		fdprtf (fd, "\"%s\"\n", val.s?val.s:"");
		break;
	case EVP_T_INT:
		fdprtf (fd, "%lld\n", (long long) val.i);
		break;
	case EVP_T_FLOAT:
		fdprtf (fd, "%.8lg\n", val.f);
		break;
	default:
		switch (val.typ) {
		case EVP_T_DATE:
			ret = cjg_prttimestr (buf, sizeof(buf), val.d, CJG_TSTR_T_D);
			break;
		case EVP_T_TIME:
			ret = cjg_prttimestr (buf, sizeof(buf), val.d, CJG_TSTR_T_DDELTA);
			break;
		case EVP_T_NDATE:
			ret = cjg_prttimestrns (buf, sizeof(buf), val.d, CJG_TSTR_T_D);
			break;
		case EVP_T_NTIME:
			ret = cjg_prttimestrns (buf, sizeof(buf), val.d, CJG_TSTR_T_DDELTA);
			break;
		default:
			fdprtf (fd, "err: %d\n", val.typ);
			return RERR_INVALID_TYPE;
		}
		if (!RERR_ISOK(ret)) {
			fdprtf (fd, "<error %d>\n", ret);
			return ret;
		}
		fdprtf (fd, "%s\n", buf);
		break;
	}
	return RERR_OK;
}


/************************
 * static functions
 ************************/
static
int
rettype (type)
	int	type;
{
	switch (type) {
	case EVP_T_NDATE:
		return EVP_T_DATE;
	case EVP_T_NTIME:
		return EVP_T_TIME;
	default:
		return type;
	}
}

static
int
isnano (type)
	int	type;
{
	switch (type) {
	case EVP_T_NDATE:
		return 1;
	case EVP_T_NTIME:
		return 1;
	default:
		return 0;
	}
}

static
const char *
_ev_strdup (ev, str)
	struct event 	*ev;
	const char		*str;
{
	char	*s;
	int	ret;

	if (!ev || !str) return NULL;
	s = strdup (str);
	if (!s) return s;
	ret = ev_addbuf2 (ev, s, strlen (s)+1);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding buffer: %s", rerr_getstr3(ret));
		free (s);
		return NULL;
	}
	return s;
}

static
const char *
_ev_addstr (ev, str, cpy)
	struct event 	*ev;
	const char		*str;
	int				cpy;
{
	int	ret;
	if (!ev || !str) return NULL;
	if (cpy) {
		str = _ev_strdup (ev, str);
		if (!str) return NULL;
	}
	ret = bufref_ref (&ev->bufref, str);
	if (!RERR_ISOK(ret)) {
		bufref_unref (&ev->bufref, str);
		return NULL;
	}
	return str;
}

static
char *
_ev_addorrmstr (ev, str)
	struct event 	*ev;
	char				*str;
{
	int	ret;

	if (!ev || !str) return NULL;
	ret = bufref_addref (&ev->bufref, str);
	if (!RERR_ISOK(ret)) {
		free (str);
		return NULL;
	}
	return str;
}
	

static
void
_ev_rmstr (ev, str)
	struct event 	*ev;
	const char		*str;
{
	if (!ev || !str) return;
	bufref_unref (&ev->bufref, str);
}




static
int
do_addattr (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	struct ev_val	val;
	int				flags;
{
	struct ev_attr	attr;
	int				ret;

	if (!var) return RERR_PARAM;
	bzero (&attr, sizeof (struct ev_attr));
	attr.var = _ev_addstr (ev, var, flags & EVP_F_CPYVAR);
	attr.val = val;
	attr.flags = flags;
	if (flags & EVP_F_OVERWRITE) {
		ev_rmattr (ev, var);
	}
	ret = TLST_INSERT (ev->attr, attr, tlst_cmpistr);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, attr.var);
		return ret;
	}
	return RERR_OK;
}

static
int
do_addattr_s (ev, var, val, flags)
	struct event	*ev;
	const char		*var;
	const char		*val;
	int				flags;
{
	struct ev_val	aval;
	int				ret;

	if (!ev || !var || !val) return RERR_PARAM;
	val = _ev_addstr (ev, val, flags & EVP_F_CPY);
	if (!val) return RERR_NOMEM;
	aval.typ = EVP_T_STR;
	aval.s = val;
	ret = do_addattr (ev, var, aval, flags);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, val);
		return ret;
	}
	return RERR_OK;
}

static
int
do_addvar (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	struct ev_val	val;
	int				flags;
{
	int	ret;

	var = _ev_addstr (ev, var, flags & EVP_F_CPYVAR);
	if (!var) return RERR_NOMEM;
	if (flags & EVP_F_OVERWRITE) {
		ev_rmvar (ev, var, idx1, idx2);
	}
	ret = do_addvar2 (ev, var, idx1, idx2, val, flags);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr(ev, var);
		return ret;
	}
	return RERR_OK;
}


static
int
do_addvar2 (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
	struct ev_val	val;
	int				flags;
{
	struct ev_var	avar;

	bzero (&avar, sizeof (struct ev_var));
	if (idx1 < 0) idx1 = -1;
	if (idx2 < 0) idx2 = -1;
	avar.var = var;
	avar.idx1 = idx1;
	avar.idx2 = idx2;
	avar.val = val;
	avar.flags = flags;
	return TLST_INSERT (ev->var, avar, do_evvar_cmp);
}

static
int
do_addvar_s (ev, var, idx1, idx2, val, flags)
	struct event	*ev;
	const char		*var;
	const char		*val;
	int				idx1, idx2;
	int				flags;
{
	struct ev_val	aval;
	int				ret;

	if (!ev || !var || !val) return RERR_PARAM;
	val = _ev_addstr (ev, val, flags & EVP_F_CPY);
	if (!val) return RERR_NOMEM;
	aval.typ = EVP_T_STR;
	aval.s = val;
	ret = do_addvar (ev, var, idx1, idx2, aval, flags);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, val);
		return ret;
	}
	return RERR_OK;
}


static
int
do_setarg_s (ev, tlst, arg, pos, istarget, flags)
	struct event	*ev;
	struct tlst		*tlst;
	const char		*arg;
	int				pos, istarget, flags;
{
	int				ret;
	struct ev_val	xarg;

	if (!ev || !tlst) return RERR_PARAM;
	arg = _ev_addstr (ev, arg, flags & EVP_F_CPY);
	if (!arg) return RERR_NOMEM;
	xarg.typ = EVP_T_STR;
	xarg.s = arg;
	ret = do_setarg (ev, tlst, xarg, pos, istarget, flags);
	if (!RERR_ISOK(ret)) {
		_ev_rmstr (ev, arg);
		return ret;
	}
	return RERR_OK;
}

static
int
do_setarg (ev, tlst, arg, pos, istarget, flags)
	struct event	*ev;
	struct tlst		*tlst;
	struct ev_val	arg;
	int				pos, istarget, flags;
{
	struct ev_arg	evarg, *evarg2;
	int				ret;

	bzero(&evarg, sizeof (struct ev_arg));
	evarg.arg = arg;
	evarg.flags = flags;
	if ((flags & EVP_F_INSERT) && pos >= 0) {
		ret = arr_shift (tlst, pos);
		if (!RERR_ISOK(ret)) return ret;
	}
	if (!(flags & EVP_F_INSERT) && pos >= 0) {
		ret = TLST_GETPTR(evarg2, *tlst, pos);
		if (RERR_ISOK(ret)) do_mayfreearg (ev, evarg2);
	}
	if (pos >= 0) {
		ret = TLST_SET(*tlst, pos, evarg);
		if ((ssize_t)TLST_GETNUM(*tlst) <= pos) tlst->num=pos+1;
	} else {
		ret = TLST_ADD (*tlst,evarg);
	}
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	return RERR_OK;
}

static
int
do_getarg_r (out, ev, pos)
	struct ev_arg	*out;
	struct event	*ev;
	int				pos;
{
	if (!ev) return RERR_PARAM;
	return do_getarg_tr (out, &(ev->arg), pos);
}

static
int
do_getarg_tr (out, tlst, pos)
	struct ev_arg	*out;
	struct tlst		*tlst;
	int				pos;
{
	struct ev_arg	arg;
	int				ret;

	if (!tlst) return RERR_PARAM;
	ret = TLST_GET (arg, *tlst, pos);
	if (ret == RERR_NOT_FOUND) {
		bzero (&arg, sizeof (struct ev_arg));
		ret = RERR_OK;
	}
	if (!RERR_ISOK(ret)) return ret;
	if (arg.arg.typ == EVP_T_NONE) {
		arg.arg.typ = EVP_T_STR;
		arg.arg.s = "";
	}
	if (out) *out = arg;
	return RERR_OK;
}

static
int
do_gettarg_r (out, ev, pos)
	struct ev_arg	*out;
	struct event	*ev;
	int				pos;
{
	if (!ev) return RERR_PARAM;
	return do_getarg_tr (out, &(ev->targetarg), pos);
}

static
int
do_getattrpos_r (outvar, outval, ev, pos)
	const char		**outvar;
	struct ev_val	*outval;
	struct event	*ev;
	int				pos;
{
	struct ev_attr	attr;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = TLST_GET (attr, ev->attr, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (outvar) *outvar = attr.var;
	if (outval) *outval = attr.val;
	return RERR_OK;
}

static
int
do_getattr_r (out, ev, var)
	struct ev_val	*out;
	struct event	*ev;
	const char		*var;
{
	int	pos;

	if (!ev || !var) return RERR_PARAM;
	pos = TLST_SEARCH (ev->attr, var, tlst_cmpistr);
	if (pos < 0) return pos;
	return do_getattrpos_r (NULL, out, ev, pos);
}

static
int
do_getvarpos_r (outvar, idx1, idx2, outval, ev, pos)
	const char		**outvar;
	int				*idx1, *idx2;
	struct ev_val	*outval;
	struct event	*ev;
	int				pos;
{
	struct ev_var	var;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = TLST_GET (var, ev->var, pos);
	if (!RERR_ISOK(ret)) return ret;
	if (outvar) *outvar = var.var;
	if (outval) *outval = var.val;
	if (idx1) *idx1 = var.idx1;
	if (idx2) *idx2 = var.idx2;
	return RERR_OK;
}


static
int
do_getvarptrpos_r (out, ev, pos)
	struct ev_var	**out;
	struct event	*ev;
	int				pos;
{
	int	ret;

	if (!out || !ev) return RERR_PARAM;
	ret = TLST_GETPTR (*out, ev->var, pos);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
do_getvarptr_r (out, ev, var, idx1, idx2, anyneg)
	struct ev_var	**out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2, anyneg;
{
	int				pos, ret;
	struct ev_var	cmpvar;

	if (!ev || !var || !out) return RERR_PARAM;
	bzero (&cmpvar, sizeof (struct ev_var));
	cmpvar.var = (char*)var;
	cmpvar.idx1 = idx1;
	cmpvar.idx2 = idx2;
	if (anyneg) {
		pos = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp2);
	} else {
		pos = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp);
	}
	if (pos < 0) return pos;
	ret = TLST_GETPTR (*out, ev->var, pos);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
do_getvar_r (out, ev, var, idx1, idx2)
	struct ev_val	*out;
	struct event	*ev;
	const char		*var;
	int				idx1, idx2;
{
	int				pos;
	struct ev_var	cmpvar;

	if (!ev || !var) return RERR_PARAM;
	bzero (&cmpvar, sizeof (struct ev_var));
	cmpvar.var = (char*)var;
	cmpvar.idx1 = idx1;
	cmpvar.idx2 = idx2;
	pos = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp);
	if (pos == RERR_NOT_FOUND) {
		cmpvar.idx2 = -1;
		pos = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp);
#if 0
		if (pos == RERR_NOT_FOUND) {
			cmpvar.idx1 = -1;
			cmpvar.idx2 = idx2;
			pos = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp);
			if (pos == RERR_NOT_FOUND) {
				cmpvar.idx2 = -1;
				pos = TLST_SEARCH (ev->var, cmpvar, do_evvar_cmp);
			}
		}
#endif
	}
	if (pos < 0) return pos;
	return do_getvarpos_r (NULL, NULL, NULL, out, ev, pos);
}

static
int
do_evvar_cmp (arg1, arg2)
	void	*arg1, *arg2;
{
	struct ev_var	*var1, *var2;
	const char		*vn1, *vn2;
	int				ret;

	if (!arg1 && !arg2) return 0;
	if (!arg1) return -1;
	if (!arg2) return 1;
	var1 = (struct ev_var*)arg1;
	var2 = (struct ev_var*)arg2;
	vn1 = var1->var;
	vn2 = var2->var;
	if (!vn1 && !vn2) return 0;
	if (!vn1) return -1;
	if (!vn2) return 1;
	ret = strcasecmp (vn1, vn2);
	if (ret < 0) return -1;
	if (ret > 0) return 1;
	if (var2->idx1 >= 0 || var1->idx1 >= 0) {
		if (var1->idx1 < var2->idx1) return -1;
		if (var1->idx1 > var2->idx1) return 1;
	}
	if (var2->idx2 >= 0 || var1->idx2 >= 0) {
		if (var1->idx2 < var2->idx2) return -1;
		if (var1->idx2 > var2->idx2) return 1;
	}
	return 0;
}

static
int
do_evvar_cmp2 (arg1, arg2)
	void	*arg1, *arg2;
{
	struct ev_var	*var1, *var2;
	const char		*vn1, *vn2;
	int				ret;

	if (!arg1 && !arg2) return 0;
	if (!arg1) return -1;
	if (!arg2) return 1;
	var1 = (struct ev_var*)arg1;
	var2 = (struct ev_var*)arg2;
	vn1 = var1->var;
	vn2 = var2->var;
	if (!vn1 && !vn2) return 0;
	if (!vn1) return -1;
	if (!vn2) return 1;
	ret = strcasecmp (vn1, vn2);
	if (ret < 0) return -1;
	if (ret > 0) return 1;
	if (var2->idx1 >= 0 && var1->idx1 >= 0) {
		if (var1->idx1 < var2->idx1) return -1;
		if (var1->idx1 > var2->idx1) return 1;
	}
	if (var2->idx2 >= 0 && var1->idx2 >= 0) {
		if (var1->idx2 < var2->idx2) return -1;
		if (var1->idx2 > var2->idx2) return 1;
	}
	return 0;
}


static
int
do_mkstrep_write (out, val, flags)
	char				*out;
	struct ev_val	val;
	int				flags;
{
	char	*s;
	int	ret;

	if (!out) return RERR_PARAM;
	switch (val.typ) {
	case EVP_T_INT:
		if (flags & TOP_F_FORCEQUOTE) {
			return sprintf (out, "\"&i%llx\"", (long long unsigned)val.i);
		} else {
			return sprintf (out, "&i%llx", (long long unsigned)val.i);
		}
	case EVP_T_DATE:
		if (flags & TOP_F_FORCEQUOTE) {
			return sprintf (out, "\"&d%llx\"", (long long unsigned)val.d);
		} else {
			return sprintf (out, "&d%llx", (long long unsigned)val.d);
		}
	case EVP_T_TIME:
		if (flags & TOP_F_FORCEQUOTE) {
			return sprintf (out, "\"&t%llx\"", (long long unsigned)val.d);
		} else {
			return sprintf (out, "&t%llx", (long long unsigned)val.d);
		}
	case EVP_T_NDATE:
		if (flags & TOP_F_FORCEQUOTE) {
			return sprintf (out, "\"&nd%llx\"", (long long unsigned)val.d);
		} else {
			return sprintf (out, "&nd%llx", (long long unsigned)val.d);
		}
	case EVP_T_NTIME:
		if (flags & TOP_F_FORCEQUOTE) {
			return sprintf (out, "\"&nt%llx\"", (long long unsigned)val.d);
		} else {
			return sprintf (out, "&nt%llx", (long long unsigned)val.d);
		}
	case EVP_T_FLOAT:
		/* to detect potential errors */
		frassert (sizeof (uint64_t) == sizeof (double));
		if (sizeof (uint64_t) != sizeof (double)) return RERR_INTERNAL;
		if (flags & TOP_F_FORCEQUOTE) {
			return sprintf (out, "\"&f%llx\"", (long long unsigned)*(uint64_t*)(void*)&(val.f));
		} else {
			return sprintf (out, "&f%llx", (long long unsigned)*(uint64_t*)(void*)&(val.f));
		}
	case EVP_T_VOID:
		return sprintf (out, "\"&v\"");
	case EVP_T_RM:
		return sprintf (out, "\"&r\"");
	case EVP_T_NONE:
		val.s = "";
		/* fall thru */
	case EVP_T_STR:
		if (!val.s) val.s = "";
		if (*(val.s)=='&') {
			strcpy (out, "&s");
			s=out+2;
		} else {
			s=out;
		}
		ret = top_quotestr (s, val.s, flags|TOP_F_QUOTECOLON);
		if (!RERR_ISOK(ret)) return ret;
		if ((*s == '"') && (s>out)) {
			out[0]='"';
			out[1]='&';
			out[2]='s';
		}
		return strlen (out);
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

static
int
do_mkstrep_len (val, flags)
	struct ev_val	val;
	int				flags;
{
	char	buf[64];

	if (val.typ != EVP_T_STR) return do_mkstrep_write (buf, val, flags);
	if (*(val.s) == '&') return top_quotelen (val.s, flags|TOP_F_QUOTECOLON) + 2;
	return top_quotelen (val.s, flags|TOP_F_QUOTECOLON);
}

static
int
do_mkstrep (out, val, flags)
	char				**out;
	struct ev_val	val;
	int				flags;
{
	char	*buf;
	int	len, ret;

	if (!out) return RERR_OK;
	len = do_mkstrep_len (val, flags);
	if (len < 0) return len;
	buf = malloc (len+1);
	if (!buf) return RERR_NOMEM;
	ret = do_mkstrep_write (buf, val, flags);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	*out = buf;
	return RERR_OK;
}


static
int
arr_shift (tlst, pos)
	struct tlst	*tlst;
	int			pos;
{
	struct ev_arg	arg;
	int				i, ret;

	if (!tlst || pos < 0) return RERR_PARAM;
	if (pos >= (ssize_t)TLST_GETNUM (*tlst)) return RERR_OK;
	for (i=TLST_GETNUM(*tlst); i>pos; i--) {
		ret = TLST_GET (arg, *tlst, i-1);
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_SET (*tlst, i, arg);
		if (!RERR_ISOK(ret)) return ret;
	}
	bzero (&arg, sizeof (struct ev_arg));
	tlst->num++;
	TLST_SET (*tlst, pos, arg);
	return RERR_OK;
}
 


static
int
do_mayfreearg (ev, arg)
	struct event	*ev;
	struct ev_arg	*arg;
{
	if (!arg) return RERR_PARAM;
	if (arg->arg.typ == EVP_T_STR) 
		_ev_rmstr (ev, arg->arg.s);
	bzero (arg, sizeof (struct ev_arg));
	return RERR_OK;
}

static
int
do_mayfreeattr (ev, attr)
	struct event	*ev;
	struct ev_attr	*attr;
{
	if (!attr) return RERR_PARAM;
	_ev_rmstr (ev, attr->var);
	if (attr->val.typ == EVP_T_STR)
		_ev_rmstr (ev, attr->val.s);
	bzero (attr, sizeof (struct ev_attr));
	return RERR_OK;
}

static
int
do_mayfreevar (ev, var)
	struct event	*ev;
	struct ev_var	*var;
{
	if (!var) return RERR_PARAM;
	_ev_rmstr (ev, var->var);
	if (var->val.typ == EVP_T_STR)
		_ev_rmstr (ev, var->val.s);
	bzero (var, sizeof (struct ev_var));
	return RERR_OK;
}


static
int
do_parse (ev, buf)
	struct event	*ev;
	char				*buf;
{
	char	*s, *tn, *name;
	int	ret;

	if (!ev || !buf) return RERR_PARAM;
	s = top_skipwhite (buf);
	if (*s=='#') {
		ev->hastarget = 1;
	} else if (*s != '*') {
		return RERR_INVALID_FORMAT;
	}
	s = top_skipwhiteplus (s, "*#");
	if (ev->hastarget) {
		tn = s;
		ret = parse_arglist (ev, &(ev->targetarg), &s, ev->flags);
		if (!RERR_ISOK(ret)) return ret;
		s = top_skipwhiteplus (s, ":");
		tn = top_stripwhite (tn, 0);
		ret = adjust_name (tn, 0);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_settname (ev, tn, 0);
		if (!RERR_ISOK(ret)) return ret;
		
	}
	name = s;
	ret = parse_arglist (ev, &(ev->arg), &s, ev->flags);
	if (!RERR_ISOK(ret)) return ret;
	s = top_skipwhiteplus (s, ":");
	name = top_stripwhite (name, 0);
	ret = adjust_name (name, 0);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_setname (ev, name, 0);
	if (!RERR_ISOK(ret)) return ret;
	ret = parse_attrlist (ev, &(ev->attr), &(ev->var), s, ev->flags);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
parse_arglist (ev, tlst, argstr, flags)
	struct event	*ev;
	struct tlst		*tlst;
	char				**argstr;
	int				flags;
{
	char				*s, *end;
	int				ret;
	struct ev_arg	arg;
	uint64_t			ival;

	if (!tlst || !argstr || !*argstr) return RERR_PARAM;
	/* skip name */
	s=*argstr;
	s = index (s, ':');
	if (s) {
		*s = 0;
		s++;
	}
	bzero (&arg, sizeof (struct ev_arg));
	while (s && *s && *s!=':') {
		end = s;
		s = top_getquotedfield (&end, ":", TOP_F_STRIPMIDDLE);
		if (!s) s=(char*)"";
		if (*s=='&') {
			switch (s[1]) {
			case 'i':
				ival = (uint64_t)strtoull (s+2, NULL, 16);
				arg.arg.i = ival;
				arg.arg.typ = EVP_T_INT;
				break;
			case 'd':
				ival = (uint64_t)strtoull (s+2, NULL, 16);
				arg.arg.d = ival;
				arg.arg.typ = EVP_T_DATE;
				break;
			case 't':
				ival = (uint64_t)strtoull (s+2, NULL, 16);
				arg.arg.d = ival;
				arg.arg.typ = EVP_T_TIME;
				break;
			case 'n':
				switch (s[2]) {
				case 'd':
					ival = (uint64_t)strtoull (s+3, NULL, 16);
					arg.arg.d = ival;
					arg.arg.typ = EVP_T_NDATE;
					break;
				case 't':
					ival = (uint64_t)strtoull (s+3, NULL, 16);
					arg.arg.d = ival;
					arg.arg.typ = EVP_T_NTIME;
					break;
				default:
					/* yet we treat it as %nt - might change in future */
					ival = (uint64_t)strtoull (s+2, NULL, 16);
					arg.arg.d = ival;
					arg.arg.typ = EVP_T_NTIME;
					break;
				}
				break;
			case 'f':
				/* to detect potential errors */
				frassert (sizeof (uint64_t) == sizeof (double));
				if (sizeof (uint64_t) != sizeof (double)) return RERR_INTERNAL;
				ival = (uint64_t)strtoull (s+2, NULL, 16);
				arg.arg.f = *(double*)(void*)&ival;
				arg.arg.typ = EVP_T_FLOAT;
				break;
			case 'r':
				arg.arg.i = 0;
				arg.arg.typ = EVP_T_RM;
				break;
			case 's':
				arg.arg.s = s+2;
				arg.arg.typ = EVP_T_STR;
				break;
			default:
				arg.arg.s = s;
				arg.arg.typ = EVP_T_STR;
				break;
			}
		} else {
			arg.arg.s = s;
			arg.arg.typ = EVP_T_STR;
		}
		arg.flags = 0;
		ret = TLST_ADD (*tlst, arg);
		if (!RERR_ISOK(ret)) return ret;
		if (arg.arg.typ == EVP_T_STR) {
			bufref_ref (&ev->bufref, arg.arg.s);
		}
		s = end;
	}
	*argstr = s;
	return RERR_OK;
}


static
int
parse_attrlist (ev, tlst, varlst, attrstr, flags)
	struct event	*ev;
	struct tlst		*tlst, *varlst;
	char				*attrstr;
	int				flags;
{
	char				*s, *s2;
	char				*var, *val;
	int				ret;
	struct ev_attr	attr;
	struct ev_var	avar;
	uint64_t			ival;

	if (!tlst && !varlst) return RERR_PARAM;
	if (!attrstr || !*attrstr) return RERR_OK;
	s = top_getquotedfield (&attrstr, NULL, 0);
	bzero (&attr, sizeof (struct ev_attr));
	bzero (&avar, sizeof (struct ev_var));
	while (s && *s) {
		s = top_skipwhite (s);
		var = top_getfield (&s, "=", 0);
		val = top_getquotedfield (&s, ",", TOP_F_STRIPMIDDLE);
		if (!var || !*var) break;
		ret = adjust_name ((char*)var, 1);
		if (!RERR_ISOK(ret)) continue;
		attr.var = var;
		if (!val) val=(char*)"";
		if (*val=='&') {
			switch (val[1]) {
			case 'i':
				ival = (uint64_t)strtoull (val+2, NULL, 16);
				attr.val.i = ival;
				attr.val.typ = EVP_T_INT;
				break;
			case 'd':
				ival = (uint64_t)strtoull (val+2, NULL, 16);
				attr.val.i = ival;
				attr.val.typ = EVP_T_DATE;
				break;
			case 't':
				ival = (uint64_t)strtoull (val+2, NULL, 16);
				attr.val.i = ival;
				attr.val.typ = EVP_T_TIME;
				break;
			case 'n':
				switch (val[2]) {
				case 'd':
					ival = (uint64_t)strtoull (val+3, NULL, 16);
					attr.val.i = ival;
					attr.val.typ = EVP_T_NDATE;
					break;
				case 't':
					ival = (uint64_t)strtoull (val+3, NULL, 16);
					attr.val.i = ival;
					attr.val.typ = EVP_T_NTIME;
					break;
				default:
					/* yet we treat it as %nt - might change in future */
					ival = (uint64_t)strtoull (val+2, NULL, 16);
					attr.val.i = ival;
					attr.val.typ = EVP_T_NTIME;
					break;
				}
				break;
			case 'f':
				/* to detect potential errors */
				frassert (sizeof (uint64_t) == sizeof (double));
				if (sizeof (uint64_t) != sizeof (double)) return RERR_INTERNAL;
				ival = (uint64_t)strtoull (val+2, NULL, 16);
				attr.val.f = *(double*)(void*)&ival;
				attr.val.typ = EVP_T_FLOAT;
				break;
			case 'r':
				attr.val.i = 0;
				attr.val.typ = EVP_T_RM;
				break;
			case 's':
				attr.val.s = val+2;
				attr.val.typ = EVP_T_STR;
				break;
			default:
				attr.val.s = val;
				attr.val.typ = EVP_T_STR;
				break;
			}
		} else {
			attr.val.s = val;
			attr.val.typ = EVP_T_STR;
		}
		attr.flags = 0;
		if (!strncasecmp (attr.var, "var/", 4)) {
			avar.val = attr.val;
			avar.flags = attr.flags;
			avar.idx1 = avar.idx2 = -1;
			//var = attr.var;
			top_getfield (&var, "/", 0);
			avar.var = top_getfield (&var, "/", 0);
			if (!avar.var) continue;
			s2 = top_getfield (&var, "/", 0);
			if (s2) {
				avar.idx1 = pmatoi (s2);
				if (avar.idx1 < 0) avar.idx1 = -1;
				s2 = top_getfield (&var, "/", 0);
				if (s2) {
					avar.idx2 = pmatoi (s2);
					if (avar.idx2 < 0) avar.idx2 = -1;
				}
			}
			//ret = TLST_INSERT (*varlst, avar, do_evvar_cmp);
			ret = do_addvar (ev, avar.var, avar.idx1, avar.idx2, avar.val, 0);
		} else {
			//ret = TLST_INSERT (*tlst, attr, tlst_cmpistr);
			ret = do_addattr (ev, attr.var, attr.val, 0);
		}
		if (!RERR_ISOK(ret)) return ret;
		if (attr.val.typ == EVP_T_STR) {
			bufref_ref (&ev->bufref, attr.val.s);
		}
	}
	return RERR_OK;
}


static
int
pmatoi (str)
	char	*str;
{
	int	neg=0;
	int	val;

	if (!str) return 0;
	str = top_skipwhite (str);
	if (*str == 'm' || *str == 'n') neg=1;
	str = top_skipwhite (str+1);
	if (!*str) return 0;
	val = atoi (str);
	if (neg) {
		if (val == 0) {
			val = -1;
		} else {
			val *= -1;
		}
	}
	return val;
}

static
int
adjust_name (name, colon)
	char	*name;
	int	colon;
{
	char	*s, *s2;
	int	begin;

	if (!name || !*name) return RERR_INVALID_NAME;
	s2 = name;
	s = top_skipwhiteplus (s2, "/");
	for (begin=1; *s; s++) {
		if (*s == '_') {
			if (begin) *s2++ = *s;
		} else if (*s=='/') {
			*s2++ = '/';
			begin=2;
			s = top_skipwhiteplus (s, "/");
			s--;
		} else if (colon && *s==':') {
			*s2++ = ':';
			begin=2;
			s = top_skipwhiteplus (s, ":");
			s--;
		} else if (*s >= '0' && *s <= '9') {
			if (begin) return RERR_INVALID_NAME;
			*s2++ = *s;
		} else if (*s >= 'a' && *s <= 'z') {
			*s2++ = *s;
		} else if (*s >= 'A' && *s <= 'Z') {
			*s2++ = *s - 'A' + 'a';
		} else if (!iswhite (*s)) {
			return RERR_INVALID_NAME;
		}
		if (begin) begin--;
	}
	*s2 = 0;
	if (!*name) return RERR_INVALID_NAME;
	return RERR_OK;
}


/* **********************
 * copy functions
 * **********************/


static
int
cpyargs (dest, src, ev, dstref)
	struct tlst		*dest, *src;
	struct event	*ev;
	struct bufref	*dstref;
{
	int				ret, i;
	struct ev_arg	xarg, dval;
	int				num;

	if (!src || !dest) return RERR_PARAM;
	num = TLST_GETNUM (*src);
	for (i=0; i<num; i++) {
		ret = TLST_GET (xarg, *src, i);
		if (ret == RERR_NOT_FOUND || xarg.arg.typ == EVP_T_NONE) continue;
		bzero (&dval, sizeof (struct ev_arg));
		dval.arg = xarg.arg;
		if (dval.arg.typ == EVP_T_STR) {
			dval.arg.s = _ev_strcpy (ev, dstref, xarg.arg.s);
			if (!dval.arg.s && xarg.arg.s) return RERR_NOMEM;
		}
		ret = TLST_SET (*dest, i, dval);
		if (!RERR_ISOK(ret)) {
			if (dval.arg.typ == EVP_T_STR) _ev_rmstr (ev, dval.arg.s);
			return ret;
		}
		dest->num++;
	}
	return RERR_OK;
}

static
int
cpyattr (dest, src, ev, dstref)
	struct tlst		*dest, *src;
	struct event	*ev;
	struct bufref	*dstref;
{
	int				ret;
	unsigned			i;
	struct ev_attr	*ptr, dval;

	if (!src || !dest) return RERR_PARAM;
	TLST_FOREACHPTR2 (ptr, *src, i) {
		bzero (&dval, sizeof (struct ev_attr));
		dval.var = _ev_strcpy (ev, dstref, ptr->var);
		if (!dval.var && ptr->var) return RERR_NOMEM;
		dval.val = ptr->val;
		if (dval.val.typ == EVP_T_STR) {
			dval.val.s = _ev_strcpy (ev, dstref, ptr->val.s);
			if (!dval.val.s && ptr->val.s) return RERR_NOMEM;
		}
		ret = TLST_ADD (*dest, dval);
		if (!RERR_ISOK(ret)) {
			if (dval.val.typ == EVP_T_STR) _ev_rmstr (ev, dval.val.s);
			_ev_rmstr (ev, dval.var);
			return ret;
		}
	}
	return RERR_OK;
}


static
int
cpyvar (dest, src, ev, dstref)
	struct tlst		*dest, *src;
	struct event	*ev;
	struct bufref	*dstref;
{
	int				ret;
	unsigned			i;
	struct ev_var	*ptr, dval;

	if (!src || !dest) return RERR_PARAM;
	TLST_FOREACHPTR2 (ptr, *src, i) {
		bzero (&dval, sizeof (struct ev_attr));
		dval.var = _ev_strcpy (ev, dstref, ptr->var);
		if (!dval.var && ptr->var) return RERR_NOMEM;
		dval.idx1 = ptr->idx1;
		dval.idx2 = ptr->idx2;
		dval.val = ptr->val;
		if (dval.val.typ == EVP_T_STR) {
			dval.val.s = _ev_strcpy (ev, dstref, ptr->val.s);
			if (!dval.val.s && ptr->val.s) return RERR_NOMEM;
		}
		ret = TLST_ADD (*dest, dval);
		if (!RERR_ISOK(ret)) {
			if (dval.val.typ == EVP_T_STR) _ev_rmstr (ev, dval.val.s);
			_ev_rmstr (ev, dval.var);
			return ret;
		}
	}
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
