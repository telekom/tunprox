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
#include <stdint.h>
#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>
#include <errno.h>
#include <pthread.h>
#include <math.h>
#include <float.h>


#include <fr/base.h>
#include "parseevent.h"
#include "varlist.h"
#include "gvarlist.h"


struct xbufref {
	char		*buf;
	size_t	buflen;
	int		refcnt;
};

struct shadow {
	struct event	*ev;
	tmo_t				lastupdate;
	int				needact;
};

struct mylst {
	void					*exist;	/* for existance check */
	struct event		lst;
	struct tlst			bufs;
	struct tlst			shadowlst;
	struct tlst			shadownums;
	char					*fname;
	int					flags;
	int					self;
	const char			*vmon;
	struct tlst			trglst;
	int					protect;
	tmo_t					lastchange;
	pthread_mutex_t	mutex;
};

static struct tlst	listlist;
static int 				isinit = 0;
static int				config_read = 0;

static int doinit ();
static int read_config ();
static int dosetvar (struct mylst*, const char*, int, int, struct ev_val, int);
static int doaddvar (struct mylst*, const char*, int, int, struct ev_val, int);
static int dosetevent (struct mylst*, struct event*, int);
static int dosetevent2 (struct mylst*, struct event*, int);
static int dosetevstr (struct mylst*, char*, int);
static int dormvar (struct mylst*, const char*, int, int);
static int dormvar2 (struct mylst*, const char*, int, int);
static int dormvarlist (struct mylst*, struct event*);
static int addbuf (struct mylst*, char*, size_t);
static int dowriteout (struct mylst*, const char*);
static int dowriteout2 (struct mylst*, const char*);
static int maywrite (struct mylst*);
static int doreadfile (struct mylst*, const char*, int);
static int dogetvar (struct event*, struct mylst*, const char*, int, int, int);
static int dogetallvar (struct event*, struct mylst*, int);
static int dogetvarlist (struct event*, struct mylst*, struct event*, int);
static int dogetvarstr (char**, struct mylst*, const char*, int, int, const char*);
static int dogetallvarstr (char**, struct mylst*, const char*);
static int dolock (struct mylst*);
static int dounlock (struct mylst*);
static int globlock ();
static int globunlock ();
static int getmylst (struct mylst**, int);
static int releasemylst (struct mylst*);
static int elabrmvar (struct mylst*, struct event*, int);
static int doactshadow (struct mylst*, int, int);
static int dormshadow (struct mylst*, int);
static int dormshadowall (struct mylst*);
static int float_almost_eq (double, double);

#define TRG_C_ADD	1
#define TRG_C_RM	2
static int triggergvar (struct mylst*, const char*, int);
static int triggergvar2 (int, int, const char*, int);
static int triggerall (struct mylst*, int);


#define DOINIT \
		if (!isinit) { \
			int	_ret; \
			globlock (); \
			_ret = doinit (); \
			globunlock (); \
			if (!RERR_ISOK(_ret)) { \
				FRLOGF (LOG_ERR, "error initializing var list: %s", \
							rerr_getstr3(_ret)); \
				return _ret; \
			} \
		}


static int g_uniqid = 0;

int
evvar_getuniqid ()
{
	int	ret;
	int	num;

	ret = globlock ();
	num = ++g_uniqid;
	if (RERR_ISOK(ret)) globunlock ();
	return num;
}


int
evvar_newlist ()
{
	return evvar_newlist2 (EVVAR_F_PROTECT);
}

int
evvar_newlist2 (flags)
	int	flags;
{
	struct mylst	lst, *ptr;
	int				ret, id;

	DOINIT;
	bzero (&lst, sizeof (struct mylst));
	ret = TLST_NEW (lst.bufs, struct xbufref);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (lst.trglst, int);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (lst.shadowlst, struct shadow);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (lst.shadownums, int);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_new (&(lst.lst));
	if (!RERR_ISOK(ret)) return ret;
	if (flags & EVVAR_F_PROTECT) {
		lst.protect = 1;
	}
	lst.flags = flags;
	if (lst.protect) {
		ret = pthread_mutex_init (&(lst.mutex), NULL);
		if (ret < 0) {
			ev_free (&(lst.lst));
			return RERR_SYSTEM;
		}
	}
	lst.exist = &lst;	/* any value but NULL */
	globlock ();
	id = ret = TLST_ADDINSERT (listlist, lst);
	globunlock ();
	if (!RERR_ISOK(ret)) {
		if (lst.protect) pthread_mutex_destroy (&(lst.mutex));
		ev_free (&(lst.lst));
		return ret;
	}
	globlock ();
	ret = TLST_GETPTR (ptr, listlist, id);
	globunlock ();
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_INTERNAL;
	ptr->self = id;
	return id;
}

int
evvar_clearlist (id)
	int	id;
{
	struct mylst	*lst;
	int				ret;
	struct xbufref	buf;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	TLST_FOREACH (buf, lst->bufs) {
		if (buf.buf) free (buf.buf);
		bzero (&buf, sizeof (struct xbufref));
	}
	TLST_RESET (lst->bufs);
	ev_clear (&(lst->lst));
	dormshadowall (lst);
	TLST_RESET (lst->shadownums);
	TLST_RESET (lst->shadowlst);
	releasemylst (lst);
	return RERR_OK;
}

int
evvar_rmlist (id)
	int	id;
{
	struct mylst	lst;
	int				ret, ret2=RERR_OK;
	struct xbufref	buf;

	DOINIT;
	ret = globlock ();
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GET (lst, listlist, id);
	if (RERR_ISOK(ret) && lst.exist) once {
		/* this might take longer, but is needed here, to avoid a race condition */
		ret = dolock (&lst);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error locking list - give up: %s", rerr_getstr3(ret));
			break;
		}
		/* now remove from list */
		ret = TLST_REMOVE (listlist, id, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error removing list %d: %s", id, rerr_getstr3(ret));
		}
		ret2 = dounlock (&lst);
		if (!RERR_ISOK(ret2)) {
			FRLOGF (LOG_WARN, "cannot unlock list - therefore we cannot destroy "
						"it: %s", rerr_getstr3(ret));
		}
	}
	globunlock ();
	if (!RERR_ISOK(ret)) return ret;
	if (!lst.exist) return RERR_NOT_FOUND;

	/* now we can free structure */
	TLST_FOREACH (buf, lst.bufs) {
		if (buf.buf) free (buf.buf);
		bzero (&buf, sizeof (struct xbufref));
	}
	TLST_FREE (lst.bufs);
	ev_free (&(lst.lst));
	if (lst.fname && (lst.flags & EVP_F_FREE)) free (lst.fname);
	ret = dormshadowall (&lst);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error removing shadowlists: %s", rerr_getstr3(ret));
	}
	TLST_FREE (lst.shadowlst);
	TLST_FREE (lst.shadownums);
	if (lst.protect && RERR_ISOK(ret2)) {
		pthread_mutex_destroy (&(lst.mutex));
	}
	bzero (&lst, sizeof (struct mylst));
	return RERR_OK;
}

int
evvar_actshadow (id, shid, flags)
	int	id, shid, flags;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = doactshadow (lst, shid, flags);
	releasemylst (lst);
	return ret;
}

int
evvar_setactshadow (id, shid)
	int	id, shid;
{
	struct mylst	*lst;
	int				ret;
	struct shadow	*shp;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (shp, lst->shadowlst, shid);
	if (RERR_ISOK(ret) && shp) shp->needact = 1;
	releasemylst (lst);
	return ret;
}

int
evvar_rmshadow (id, shid)
	int	id, shid;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dormshadow (lst, shid);
	releasemylst (lst);
	return ret;
}

int
evvar_rmshadowall (id)
	int	id;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dormshadowall (lst);
	releasemylst (lst);
	return ret;
}

int
evvar_setvarmon (id, vmon)
	int			id;
	const char	*vmon;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	lst->vmon = vmon;
	releasemylst (lst);
	return RERR_OK;
}

int
evvar_getvarmon (vmon, id)
	int			id;
	const char	**vmon;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (vmon) *vmon = lst->vmon;
	releasemylst (lst);
	return RERR_OK;
}

int
evvar_addgvl (id, gid)
	int	id, gid;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	once {
		ret = TLST_HASINT (lst->trglst, gid);
		if (ret != RERR_NOT_FOUND) break;
		ret = TLST_ADD (lst->trglst, gid);
		if (!RERR_ISOK(ret)) break;
		ret = triggerall (lst, gid);
		if (!RERR_ISOK(ret)) break;
	}
	releasemylst (lst);
	return ret;
}

int
evvar_rmgvl (id, gid)
	int	id, gid;
{
	struct mylst	*lst;
	int				ret;
	unsigned			idx;

	ret = getmylst (&lst, id);
	once {
		if (!RERR_ISOK(ret)) break;
		ret = TLST_FINDINT (idx, lst->trglst, gid);
		if (!RERR_ISOK(ret)) break;
		ret = TLST_REMOVE (lst->trglst, idx, TLST_F_CPYLAST);
		if (!RERR_ISOK(ret)) break;
	}
	releasemylst (lst);
	return ret;
}

int
evvar_setvar_v (id, var, idx1, idx2, val, flags)
	int				id, idx1, idx2, flags;
	const char		*var;
	struct ev_val	val;
{
	struct mylst	*lst;
	int				ret;

	if (!var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	once {
		ret = dosetvar (lst, var, idx1, idx2, val, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error inserting variable: %s", rerr_getstr3(ret));
			break;
		}
		if (ret) {
			ret = maywrite (lst);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_WARN, "error writing varlist: %s", rerr_getstr3(ret));
			}
			ret = RERR_OK;
		}
	}
	releasemylst (lst);
	return ret;
}


int
evvar_setvar_s (id, var, idx1, idx2, val, flags)
	int			id, idx1, idx2, flags;
	const char	*var;
	const char	*val;
{
	struct ev_val	eval;

	eval.s = (char*)val;
	eval.typ = EVP_T_STR;
	return evvar_setvar_v (id, var, idx1, idx2, eval, flags);
}


int
evvar_setvar_i (id, var, idx1, idx2, val, flags)
	int			id, idx1, idx2, flags;
	const char	*var;
	int64_t		val;
{
	struct ev_val	eval;

	eval.i = val;
	eval.typ = EVP_T_INT;
	return evvar_setvar_v (id, var, idx1, idx2, eval, flags);
}


int
evvar_setvar_d (id, var, idx1, idx2, val, flags)
	int			id, idx1, idx2, flags;
	const char	*var;
	tmo_t			val;
{
	struct ev_val	eval;

	eval.d = val;
	if (flags & EVP_F_NANO) {
		eval.typ = EVP_T_NDATE;
	} else {
		eval.typ = EVP_T_DATE;
	}
	return evvar_setvar_v (id, var, idx1, idx2, eval, flags);
}


int
evvar_setvar_t (id, var, idx1, idx2, val, flags)
	int			id, idx1, idx2, flags;
	const char	*var;
	tmo_t			val;
{
	struct ev_val	eval;

	eval.d = val;
	if (flags & EVP_F_NANO) {
		eval.typ = EVP_T_NTIME;
	} else {
		eval.typ = EVP_T_TIME;
	}
	return evvar_setvar_v (id, var, idx1, idx2, eval, flags);
}


int
evvar_setvar_f (id, var, idx1, idx2, val, flags)
	int			id, idx1, idx2, flags;
	const char	*var;
	double		val;
{
	struct ev_val	eval;

	eval.f = val;
	eval.typ = EVP_T_FLOAT;
	return evvar_setvar_v (id, var, idx1, idx2, eval, flags);
}

int
evvar_setevent (id, ev, flags)
	int				id, flags;
	struct event	*ev;
{
	struct mylst	*lst;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dosetevent (lst, ev, flags);
	releasemylst (lst);
	return ret;
}

int
evvar_setevstr (id, evstr, flags)
	int	id, flags;
	char	*evstr;
{
	struct mylst	*lst;
	int				ret;

	if (!evstr) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dosetevstr (lst, evstr, flags);
	releasemylst (lst);
	return ret;
}

int
evvar_rmvar (id, var, idx1, idx2)
	int			id, idx1, idx2;
	const char	*var;
{
	struct mylst	*lst;
	int				ret;

	if (!var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dormvar (lst, var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_rmvarlist (id, ev)
	int				id;
	struct event	*ev;
{
	struct mylst	*lst;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dormvarlist (lst, ev);
	releasemylst (lst);
	return ret;
}

int
evvar_addbuf (id, buf, buflen)
	int		id;
	char		*buf;
	size_t	buflen;
{
	struct mylst	*lst;
	int				ret;

	if (!buf) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = addbuf (lst, buf, buflen);
	releasemylst (lst);
	return ret;
}

int
evvar_writefile (id, fname)
	int			id;
	const char	*fname;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dowriteout (lst, fname);
	releasemylst (lst);
	return ret;
}

int
evvar_readfile (id, fname, flags)
	int			id, flags;
	const char	*fname;
{
	struct mylst	*lst;
	int				ret;

	if (!fname) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = doreadfile (lst, fname, (flags & EVVAR_F_SILENT)?1:0);
	releasemylst (lst);
	return ret;
}

int
evvar_setautowrite (id, fname, flags)
	int			id, flags;
	const char	*fname;
{
	struct mylst	*lst;
	int				ret;

	if (!fname) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	once {
		if (lst->fname && lst->flags & EVP_F_FREE) {
			free (lst->fname);
			lst->fname = NULL;
			lst->flags &= ~EVP_F_FREE;
		}
		lst->fname = (flags & EVP_F_CPY) ? strdup (fname) : (char*)fname;
		if (!lst->fname) { ret = RERR_NOMEM; break; }
		if (flags & EVP_F_CPY) flags |= EVP_F_FREE;
		lst->flags = flags;
	}
	releasemylst (lst);
	return ret;
}


int
evvar_getsinglevar_s (outval, id, var, idx1, idx2)
	const char	**outval, *var;
	int			id, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvar_s (outval, &(lst->lst), var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_getsinglevar_f (outval, id, var, idx1, idx2)
	double		*outval;
	const char	*var;
	int			id, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvar_f (outval, &(lst->lst), var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_getsinglevar_i (outval, id, var, idx1, idx2)
	int64_t		*outval;
	const char	*var;
	int			id, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvar_i (outval, &(lst->lst), var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_getsinglevar_d (outval, id, var, idx1, idx2)
	tmo_t			*outval;
	const char	*var;
	int			id, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvar_d (outval, &(lst->lst), var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_getsinglevar_t (outval, id, var, idx1, idx2)
	tmo_t			*outval;
	const char	*var;
	int			id, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvar_t (outval, &(lst->lst), var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_getsinglevar_v (outval, id, var, idx1, idx2)
	struct ev_val	*outval;
	const char		*var;
	int				id, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvar_v (outval, &(lst->lst), var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_getshadowvar_s (outval, id, shid, var, idx1, idx2)
	const char	**outval;
	const char	*var;
	int			id, shid, idx1, idx2;
{
	struct ev_val	xval;
	int				ret;

	ret = evvar_getshadowvar_v (&xval, id, shid, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	if (xval.typ != EVP_T_STR) return RERR_INVALID_TYPE;
	if (outval) *outval = xval.s;
	return RERR_OK;
}

int
evvar_getshadowvar_i (outval, id, shid, var, idx1, idx2)
	int64_t		*outval;
	const char	*var;
	int			id, shid, idx1, idx2;
{
	struct ev_val	xval;
	int				ret;

	ret = evvar_getshadowvar_v (&xval, id, shid, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (outval) *outval = xval.i;
		break;
	case EVP_T_FLOAT:
		if (outval) *outval = (int64_t)(xval.f);
		break;
	case EVP_T_DATE:
	case EVP_T_NDATE:
	case EVP_T_TIME:
	case EVP_T_NTIME:
		if (outval) *outval = (int64_t)(xval.d);
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
evvar_getshadowvar_f (outval, id, shid, var, idx1, idx2)
	double		*outval;
	const char	*var;
	int			id, shid, idx1, idx2;
{
	struct ev_val	xval;
	int				ret;

	ret = evvar_getshadowvar_v (&xval, id, shid, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (outval) *outval = (double)(xval.i);
		break;
	case EVP_T_FLOAT:
		if (outval) *outval = (xval.f);
		break;
	case EVP_T_DATE:
	case EVP_T_NDATE:
	case EVP_T_TIME:
	case EVP_T_NTIME:
		if (outval) *outval = (double)(xval.d);
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
evvar_getshadowvar_d (outval, id, shid, var, idx1, idx2)
	tmo_t			*outval;
	const char	*var;
	int			id, shid, idx1, idx2;
{
	struct ev_val	xval;
	int				ret;

	ret = evvar_getshadowvar_v (&xval, id, shid, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	switch (xval.typ) {
	case EVP_T_INT:
		if (outval) *outval = (tmo_t)(xval.i);
		break;
	case EVP_T_FLOAT:
		if (outval) *outval = (tmo_t)(xval.f);
		break;
	case EVP_T_DATE:
	case EVP_T_NDATE:
	case EVP_T_TIME:
	case EVP_T_NTIME:
		if (outval) *outval = (xval.d);
		break;
	default:
		return RERR_INVALID_TYPE;
	}
	return RERR_OK;
}

int
evvar_getshadowvar_t (outval, id, shid, var, idx1, idx2)
	tmo_t			*outval;
	const char	*var;
	int			id, shid, idx1, idx2;
{
	return evvar_getshadowvar_d (outval, id, shid, var, idx1, idx2);
}

int
evvar_getshadowvar_v (outval, id, shid, var, idx1, idx2)
	struct ev_val	*outval;
	const char		*var;
	int				id, shid, idx1, idx2;
{
	struct mylst	*lst;
	int				ret;
	struct shadow	*shp;
	struct event	*ev;

	if (!outval || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (shid == 0) {
		ev = &(lst->lst);
	} else {
		ret = TLST_GETPTR (shp, lst->shadowlst, shid);
		if (!RERR_ISOK(ret)) {
			ev = NULL;
		} else {
			if (!shp || !shp->ev) {
				ret = RERR_NOT_FOUND;
				ev = NULL;
			} else {
				ev = shp->ev;
			}
		}
	}
	if (ev) ret = ev_getvar_v (outval, ev, var, idx1, idx2);
	releasemylst (lst);
	return ret;
}

int
evvar_typeof (id, var, idx1, idx2)
	int			id, idx1, idx2;
	const char	*var;
{
	struct ev_val	val;
	int				ret;

	ret = evvar_getsinglevar_v (&val, id, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	switch (val.typ) {
	case EVP_T_NDATE:
		return EVP_T_DATE;
	case EVP_T_NTIME:
		return EVP_T_TIME;
	default:
		return val.typ;
	}
}

int
evvar_isnano (id, var, idx1, idx2)
	int			id, idx1, idx2;
	const char	*var;
{
	struct ev_val	val;
	int				ret;

	ret = evvar_getsinglevar_v (&val, id, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	switch (val.typ) {
	case EVP_T_NDATE:
	case EVP_T_NTIME:
		return 1;
	default:
		return 0;
	}
}

int
evvar_getvar (ev, id, var, idx1, idx2, flags)
	struct event	*ev;
	const char		*var;
	int				id, idx1, idx2, flags;
{
	struct mylst	*lst;
	int				ret;

	if (!ev || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dogetvar (ev, lst, var, idx1, idx2, flags);
	releasemylst (lst);
	return ret;
}

int
evvar_getallvar (ev, id, flags)
	struct event	*ev;
	int				id, flags;
{
	struct mylst	*lst;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dogetallvar (ev, lst, flags);
	releasemylst (lst);
	return ret;
}

int
evvar_getvarlist (oev, id, ev, flags)
	struct event	*oev, *ev;
	int				id, flags;
{
	struct mylst	*lst;
	int				ret;

	if (!ev || !oev) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dogetvarlist (oev, lst, ev, flags);
	releasemylst (lst);
	return ret;
}


int
evvar_getvarstr (evstr, id, var, idx1, idx2, evname)
	char			**evstr;
	const char	*var;
	int			id, idx1, idx2;
	const char	*evname;
{
	struct mylst	*lst;
	int				ret;

	if (!evstr || !var) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dogetvarstr (evstr, lst, var, idx1, idx2, evname);
	releasemylst (lst);
	return ret;
}

int
evvar_getallvarstr (evstr, id, evname)
	char			**evstr;
	int			id;
	const char	*evname;
{
	struct mylst	*lst;
	int				ret;

	if (!evstr) return RERR_PARAM;
	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = dogetallvarstr (evstr, lst, evname);
	releasemylst (lst);
	return ret;
}

int
evvar_getvarpos_v (var, idx1, idx2, outval, id, pos)
	struct ev_val	*outval;
	const char		**var;
	int				*idx1, *idx2;
	int				id, pos;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_getvarpos_v (var, idx1, idx2, outval, &(lst->lst), pos);
	releasemylst (lst);
	return ret;
}

int
evvar_getnum (id)
	int	id;
{
	struct mylst	*lst;
	int				ret;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_numvar (&(lst->lst));
	releasemylst (lst);
	return ret;
}


int
evvar_getshadowvarpos_v (var, idx1, idx2, outval, id, shid, pos)
	struct ev_val	*outval;
	const char		**var;
	int				*idx1, *idx2;
	int				id, pos, shid;
{
	struct mylst	*lst;
	int				ret;
	struct shadow	*shp;
	struct event	*ev;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (shid == 0) {
		ev = &(lst->lst);
	} else {
		ret = TLST_GETPTR (shp, lst->shadowlst, shid);
		if (!RERR_ISOK(ret)) {
			ev = NULL;
		} else {
			if (!shp || !shp->ev) {
				ret = RERR_NOT_FOUND;
				ev = NULL;
			} else {
				ev = shp->ev;
			}
		}
	}
	if (ev) ret = ev_getvarpos_v (var, idx1, idx2, outval, ev, pos);
	releasemylst (lst);
	return ret;
}

int
evvar_getshadownum (id, shid)
	int	id, shid;
{
	struct mylst	*lst;
	int				ret;
	struct shadow	*shp;
	struct event	*ev;

	ret = getmylst (&lst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (shid == 0) {
		ev = &(lst->lst);
	} else {
		ret = TLST_GETPTR (shp, lst->shadowlst, shid);
		if (!RERR_ISOK(ret)) {
			ev = NULL;
		} else {
			if (!shp || !shp->ev) {
				ret = RERR_NOT_FOUND;
				ev = NULL;
			} else {
				ev = shp->ev;
			}
		}
	}
	if (ev) ret = ev_numvar (ev);
	releasemylst (lst);
	return ret;
}



/* ********************
 * static functions
 * ********************/

static
int
getmylst (lst, id)
	struct mylst	**lst;
	int				id;
{
	int	ret;

	if (!lst) return RERR_PARAM;
	DOINIT;
	ret = globlock ();
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_GETPTR (*lst, listlist, id);
	if (RERR_ISOK(ret)) {
		if (!*lst) {
			ret = RERR_INTERNAL;
		} else if (!((*lst)->exist)) {
			ret = RERR_NOT_FOUND;
		}
	}
	globunlock ();
	if (!RERR_ISOK(ret)) return ret;
	ret = dolock (*lst);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

static
int
releasemylst (lst)
	struct mylst	*lst;
{
	if (!lst) return RERR_PARAM;
	return dounlock (lst);
}


static
int
doactshadow (lst, shid, flags)
	struct mylst	*lst;
	int				shid, flags;
{
	struct event	*ev;
	int				ret, num, i, idx1, idx2, chg;
	const char		*var;
	struct ev_val	val;
	struct ev_var	*wvar;
	struct shadow	shadow, *shp;

	if (!lst || shid < 0) return RERR_PARAM;
	if (shid == 0) return RERR_OK;
	ret = TLST_GETPTR (shp, lst->shadowlst, shid);
	if ((ret == RERR_NOT_FOUND) || !shp->ev) {
		bzero (&shadow, sizeof (struct shadow));
		ev = shadow.ev = malloc (sizeof (struct event));
		if (!ev) return RERR_NOMEM;
		shadow.needact = 1;
		ret = ev_new (ev);
		if (!RERR_ISOK(ret)) {
			free (ev);
			return ret;
		}
		ret = TLST_SET (lst->shadowlst, shid, shadow);
		if (!RERR_ISOK(ret)) {
			free (ev);
			return ret;
		}
		ret = TLST_ADD (lst->shadownums, shid);
		if (!RERR_ISOK(ret)) return ret;
		ret = TLST_GETPTR (shp, lst->shadowlst, shid);
		if (!RERR_ISOK(ret)) return ret;
	} else if (!RERR_ISOK(ret)) {
		return ret;
	}
	if (!shp || !shp->ev) return RERR_INTERNAL;
	ev = shp->ev;

	/* check for changes */
	if (!(shp->needact || (flags & EVVAR_F_FORCE))) return RERR_OK;
	if (!((shp->lastupdate == 0) || (lst->lastchange == 0) || 
						(lst->lastchange > shp->lastupdate))) {
		/* nothing to be done */
		shp->needact = 0;
		return RERR_OK;
	}

	/* first remove all vars to be removed */
	ret = num = ev_numvar (ev);
	if (!RERR_ISOK(ret)) return ret;
	for (i=0; i<num; i++) {
		ret = ev_getvarpos_v (&var, &idx1, &idx2, &val, ev, i);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_hasvar (&(lst->lst), var, idx1, idx2);
		if (ret == RERR_FAIL) {
			ret = ev_rmvarpos (ev, i);
			if (!RERR_ISOK(ret)) return ret;
			i--;
			num--;
		} else if (!RERR_ISOK(ret)) {
			return ret;
		}
	}

	/* now add vars or change its content */
	ret = num = ev_numvar (&(lst->lst));
	if (!RERR_ISOK(ret)) return ret;
	for (i=0; i<num; i++) {
		ret = ev_getvarpos_v (&var, &idx1, &idx2, &val, &(lst->lst), i);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_getvarptr2 (&wvar, ev, var, idx1, idx2);
		if (ret == RERR_NOT_FOUND) {
			/* the following should be optimized, using references instead of
				copies. it does make no difference in speed, but in memory
				usage.
			 */
			ret = ev_addvar_v (ev, var, idx1, idx2, val, EVP_F_CPY);	
			if (!RERR_ISOK(ret)) return ret;
		} else if (!RERR_ISOK(ret)) {
			return ret;
		} else {
			chg=0;
			if (wvar->val.typ == EVP_T_STR) {
				if (val.typ != EVP_T_STR || strcmp (val.s, wvar->val.s) != 0) {
					if (wvar->flags & EVP_F_FREEVAL) {
						if (wvar->val.s) free (wvar->val.s);
						wvar->val.s = NULL;
					}
					chg = 1;
				}
			}
			if (val.typ == EVP_T_STR) {
				if (chg) {
					wvar->val.typ = val.typ;
					if (val.s) {
						wvar->val.s = strdup (val.s);
						if (!wvar->val.s) return RERR_NOMEM;
						wvar->flags |= EVP_F_FREEVAL;
					}
				}
			} else {
				wvar->val = val;
				wvar->flags &= ~EVP_F_FREEVAL;
			}
		}
	}
	shp->lastupdate = tmo_now ();
	shp->needact = 0;

	return RERR_OK;
}

static
int
dormshadow (lst, shid)
	struct mylst	*lst;
	int				shid;
{
	struct shadow	*shp;
	int				ret;
	unsigned			idx;

	if (!lst || shid < 0) return RERR_PARAM;
	if (shid == 0) return RERR_OK;
	ret = TLST_GETPTR (shp, lst->shadowlst, shid);
	if ((ret == RERR_NOT_FOUND) || !shp->ev) {
		return RERR_OK;
	} else if (!RERR_ISOK(ret)) {
		return ret;
	}
	ev_free (shp->ev);
	bzero (shp, sizeof (struct shadow));
	ret = TLST_FINDINT (idx, lst->shadownums, shid);
	if (ret == RERR_NOT_FOUND) {
		return RERR_OK;
	} else if (!RERR_ISOK(ret)) {
		return ret;
	}
	return TLST_REMOVE (lst->shadownums, idx, TLST_F_CPYLAST);
}

static
int
dormshadowall (lst)
	struct mylst	*lst;
{
	int	ret, num, ret2=RERR_OK, i;

	if (!lst) return RERR_PARAM;
	i=0;
	while ((ssize_t)TLST_GETNUM (lst->shadownums) > i) {
		ret = TLST_GET (num, lst->shadownums, i);
		if (!RERR_ISOK(ret)) {
			ret2 = ret;
			i++;
			continue;
		}
		ret = dormshadow (lst, num);
		if (!RERR_ISOK(ret)) {
			ret2 = ret;
			i++;
			continue;
		}
	}
	return ret2;
}

static
int
dogetvar (ev, lst, var, idx1, idx2, flags)
	struct event	*ev;
	struct mylst	*lst;
	const char		*var;
	int				idx1, idx2, flags;
{
	int				ret, num, i;
	struct ev_var	*ptr;
	struct ev_val	val;

	if (!ev || !lst || !var) return RERR_PARAM;
	flags |= EVP_F_CPY;
	if (idx1 >= 0 && idx2 >= 0) {
		ret = ev_getvar_v (&val, &(lst->lst), var, idx1, idx2);
		if ((ret == RERR_NOT_FOUND) && (flags & EVVAR_F_VOID)) {
			val.typ = EVP_T_VOID;
		} else if (!RERR_ISOK(ret)) {
			return ret;
		}
		ret = ev_addvar_v (ev, var, idx1, idx2, val, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	num = ev_numvar (&(lst->lst));
	for (i=0; i<num; i++) {
		ret = ev_getvarptrpos (&ptr, &(lst->lst), i);
		if (!RERR_ISOK(ret)) return ret;
		if (strcasecmp (var, ptr->var) != 0) continue;
		if (idx1 >= 0 && idx1 != ptr->idx1) continue;
		if (idx2 >= 0 && idx2 != ptr->idx2) continue;
		ret = ev_addvar_v (ev, ptr->var, ptr->idx1, ptr->idx2, ptr->val, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}


static
int
dogetallvar (ev, lst, flags)
	struct event	*ev;
	struct mylst	*lst;
	int				flags;
{
	int				ret, num, i;
	struct ev_var	*ptr;

	if (!ev || !lst) return RERR_PARAM;
	num = ev_numvar (&(lst->lst));
	flags &= EVP_F_CPY;
	for (i=0; i<num; i++) {
		ret = ev_getvarptrpos (&ptr, &(lst->lst), i);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_addvar_v (ev, ptr->var, ptr->idx1, ptr->idx2, ptr->val, flags);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}

static
int
dogetvarlist (oev, lst, ev, flags)
	struct event	*oev, *ev;
	struct mylst	*lst;
	int				flags;
{
	struct ev_var	*ptr;
	int				ret, num, i;
	int				ret2 = RERR_OK;

	if (!oev || !ev) return RERR_PARAM;
	num = ev_numvar (ev);
	for (i=0; i<num; i++) {
		ret = ev_getvarptrpos (&ptr, ev, i);
		if (!RERR_ISOK(ret)) {
			ret2 = ret;
			continue;
		}
		ret = dogetvar (oev, lst, ptr->var, ptr->idx1, ptr->idx2, flags | EVVAR_F_VOID);
		if (!RERR_ISOK(ret)) {
			ret2 = ret;
			continue;
		}
	}
	if (ret2 < 0) return ret2;
	return RERR_OK;
}

static
int
dogetvarstr (evstr, lst, var, idx1, idx2, evname)
	char				**evstr;
	struct mylst	*lst;
	const char		*var;
	int				idx1, idx2;
	const char		*evname;
{
	int				ret;
	struct event	ev;

	if (!evstr || !lst || !var) return RERR_PARAM;
	ret = ev_new (&ev);
	if (!RERR_ISOK(ret)) return ret;
	ev.name = (char*)(void*)evname;
	ret = dogetvar (&ev, lst, var, idx1, idx2, 0);
	if (!RERR_ISOK(ret)) {
		ev_free (&ev);
		return ret;
	}
	ret = ev_create (evstr, &ev, 0);
	ev_free (&ev);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

static
int
dogetallvarstr (evstr, lst, evname)
	char				**evstr;
	struct mylst	*lst;
	const char		*evname;
{
	if (!evstr || !lst) return RERR_PARAM;
	lst->lst.name = (char*)(void*)evname;
	return ev_create (evstr, &(lst->lst), 0);
}

static
int
maywrite (lst)
	struct mylst	*lst;
{
	if (!lst) return RERR_PARAM;
	if (!(lst->fname)) return RERR_OK;
	return dowriteout (lst, lst->fname);
}

static
int
dowriteout (lst, fname)
	struct mylst	*lst;
	const char		*fname;
{
	int	ret, needback=0;
	char	*fname2=NULL;

	if (!lst) return RERR_PARAM;
	if (!fname) fname = lst->fname;
	if (!fname) return RERR_OK;
	/* if exist, make backup first */
	ret = fop_exist (fname);
	if (!RERR_ISOK(ret)) return ret;
	if (ret) once {
		needback = 1;
		/* backup it */
		fname2 = malloc (strlen (fname) + 6);
		if (!fname2) break;
		strcpy (fname2, fname);
		strcat (fname2, ".bak");
		if (fop_exist (fname2)) {
			if (unlink (fname2) < 0) {
				free (fname2);
				fname2=NULL;
				break;
			}
		}
		if (link (fname, fname2) < 0) {
			free (fname2);
			fname2=NULL;
			break;
		}
		if (unlink (fname) < 0) {
			unlink (fname2);
			free (fname2);
			fname2=NULL;
			break;
		}
	}
	if (needback && !fname2) {
		FRLOGF (LOG_NOTICE, "could not create backup of >>%s<<: %s", fname, 
					rerr_getstr3(RERR_SYSTEM));
	}
	ret = dowriteout2 (lst, fname);
	if (!RERR_ISOK(ret)) {
		/* restore backup */
		if (needback && !fname2) {
			FRLOGF (LOG_WARN, "write of >>%s<< failed, and no backup available",
						fname);
		}
		if (fname2) once {
			if (fop_exist (fname)) {
				if (unlink (fname) < 0) break;
			}
			if (link (fname2, fname) < 0) break;
			unlink (fname2);
			free (fname2);
			fname2 = NULL;
		}
		if (fname2) {
			FRLOGF (LOG_WARN, "cannot restore backup, backup available as "
									">>%s<<: %s", fname2, rerr_getstr3(RERR_SYSTEM));
			free (fname2);
			fname2 = NULL;
		}
		return ret;
	} else if (fname2) {
		/* destroy backup */
		unlink (fname2);
		free (fname2);
		fname2 = NULL;
	}
	return RERR_OK;
}

static
int
dowriteout2 (lst, fname)
	struct mylst	*lst;
	const char		*fname;
{
	int		ret, fd;
	char		*buf;
	ssize_t	len, len2;

	if (!lst || !fname) return RERR_PARAM;
	lst->lst.name = (char*)"varmon/save";
	ret = ev_create (&buf, &(lst->lst), 0);
	if (!RERR_ISOK(ret)) return ret;
	fd = open (fname, O_WRONLY | O_CREAT | O_TRUNC, 0660);
	if (fd < 0) {	
		FRLOGF (LOG_ERR, "cannot open file >>%s<< for writing: %s", fname,
							rerr_getstr3 (RERR_SYSTEM));
		free (buf);
		return RERR_SYSTEM;
	}
	len = strlen (buf);
	len2 = write (fd, buf, len);
	close (fd);
	free (buf);
	if (len2 < 0) {
		FRLOGF (LOG_ERR, "error writing file >>%s<<: %s", fname,
							rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	if (len2 < len) {
		FRLOGF (LOG_WARN, "write truncated");
		return RERR_SYSTEM;
	}
	return RERR_OK;
}

static
int
doreadfile (lst, fname, silent)
	struct mylst	*lst;
	const char		*fname;
	int				silent;
{
	char	*buf;
	int	ret;

	if (!lst || !fname) return RERR_PARAM;
	errno=0;
	buf = fop_read_fn (fname);
	if (!buf) {
		if (silent && errno==ENOENT) {
			return RERR_OK;
		}
		FRLOGF (LOG_ERR, "cannot read file >>%s<<: %s", fname,
							rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	ret = dosetevstr (lst, buf, EVVAR_F_SILENT);
	if (!RERR_ISOK(ret)) {
		free (buf);
		return ret;
	}
	return RERR_OK;
}

static
int
dormvarlist (lst, ev)
	struct mylst	*lst;
	struct event	*ev;
{
	struct ev_var	*ptr;
	int				ret, num, i, chg = 0;
	int				ret2 = RERR_OK;

	if (!lst || !ev) return RERR_PARAM;
	num = ev_numvar (ev);
	for (i=0; i<num; i++) {
		ret = ev_getvarptrpos (&ptr, ev, i);
		if (!RERR_ISOK(ret)) return ret;
		ret = dormvar2 (lst, ptr->var, ptr->idx1, ptr->idx2);
		if (ret == RERR_NOT_FOUND) {
			ret2 = ret;
		} else if (!RERR_ISOK(ret)) {
			return ret;
		} else {
			chg = 1;
		}
	}
	if (chg) {
		ret = maywrite (lst);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error writing varlist: %s", rerr_getstr3(ret));
		}
	}
	if (!RERR_ISOK(ret2) && !chg) return ret2;
	return RERR_OK;
}

static
int
dormvar (lst, var, idx1, idx2)
	struct mylst	*lst;
	const char		*var;
	int				idx1, idx2;
{
	int	ret;

	ret = dormvar2 (lst, var, idx1, idx2);
	if (!RERR_ISOK(ret)) return ret;
	ret = maywrite (lst);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error writing varlist: %s", rerr_getstr3(ret));
	}
	return RERR_OK;
}


static
int
dormvar2 (lst, var, idx1, idx2)
	struct mylst	*lst;
	const char		*var;
	int				idx1, idx2;
{
	int				ret;
	unsigned			i;
	struct ev_var	*ptr;
	struct xbufref	*buf;
	int				has=0;

	if (!lst || !var) return RERR_PARAM;
	if (idx1 >= 0 && idx2 >= 0) {
		FRLOGF (LOG_VERB, "removing var >>%s.%d[%d]<<", var, idx1, idx2);
	} else if (idx1 >= 0) {
		FRLOGF (LOG_VERB, "removing var >>%s.%d<<", var, idx1);
	} else if (idx2 >= 0) {
		FRLOGF (LOG_VERB, "removing var >>%s[%d]<<", var, idx2);
	} else {
		FRLOGF (LOG_VERB, "removing var >>%s<<", var);
	}
	while (1) {
		ret = ev_getvarptr2 (&ptr, &(lst->lst), var, idx1, idx2);
		if (ret == RERR_NOT_FOUND) break;
		if (!RERR_ISOK(ret)) return ret;
		has=1;
		ret = ev_rmvar (&(lst->lst), ptr->var, ptr->idx1, ptr->idx2);
		if (!RERR_ISOK(ret)) return ret;
		TLST_FOREACHPTR2 (buf, lst->bufs, i) {
			if (ptr->var >= buf->buf && ptr->var < buf->buf + buf->buflen) {
				buf->refcnt--;
				if (buf->refcnt <= 0) {
					TLST_REMOVE (lst->bufs, i, TLST_F_CPYLAST);
				}
				break;
			}
		}
		if (ptr->val.typ == EVP_T_STR) {
			TLST_FOREACHPTR2 (buf, lst->bufs, i) {
					if (ptr->val.s >= buf->buf && ptr->val.s < buf->buf + buf->buflen) {
					buf->refcnt--;
					if (buf->refcnt <= 0) {
						TLST_REMOVE (lst->bufs, i, TLST_F_CPYLAST);
					}
					break;
				}
			}
		}
	}
	if (has) {
		lst->lastchange = tmo_now ();
		/* gvar list has ref-counter */
		triggergvar (lst, var, TRG_C_RM);
	}
	return RERR_OK;
}


static
int
addbuf (lst, buf, buflen)
	struct mylst	*lst;
	char				*buf;
	size_t			buflen;
{
	struct xbufref	*ptr;
	int				ret, num;
	unsigned			i;
	struct xbufref	bref;
	struct ev_var	*var;

	if (!lst || !buf) return RERR_PARAM;
	if (buflen == 0) buflen = strlen (buf);
	/* check for overlap */
	TLST_FOREACHPTR2 (ptr, lst->bufs, i) {
		if ((buf >= ptr->buf && buf < ptr->buf + ptr->buflen) ||
				(ptr->buf >= buf && ptr->buf < buf + buflen)) {
			/* buffer do overlap */
			FRLOGF (LOG_WARN, "buffer do overlap with already available buffer");
			return RERR_PARAM;
		}
	}
	bzero (&bref, sizeof (struct xbufref));
	bref.buf = buf;
	bref.buflen = buflen;
	bref.refcnt = 0;
	/* check refcnt */
	num = ev_numvar (&(lst->lst));
	for (i=0; i<(unsigned)num; i++) {
		ret = ev_getvarptrpos (&var, &(lst->lst), i);
		if (!RERR_ISOK(ret)) return ret;
		if (var->var >= buf && var->var < buf + buflen) {
			bref.refcnt++;
		}
		if (var->val.typ == EVP_T_STR && var->val.s >= buf && 
						var->val.s < buf + buflen) {
			bref.refcnt++;
		}
	}
	/* insert into list */
	ret = TLST_ADD (lst->bufs, bref);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
dosetevstr (lst, evstr, flags)
	struct mylst	*lst;
	char				*evstr;
	int				flags;
{
	struct event	ev;
	int				ret;

	if (!lst || !evstr) return RERR_PARAM;
	ret = ev_new (&ev);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_parse (&ev, evstr, flags);
	if (!RERR_ISOK(ret)) {
		ev_free (&ev);
		return ret;
	}
	ret = dosetevent (lst, &ev, flags);
	if (!RERR_ISOK(ret)) {
		if (!(flags & EVP_F_CPY)) ev.buf = NULL;
		ev_free (&ev);
		return ret;
	}
	ev_free (&ev);
	return ret;
}

static
int
dosetevent (lst, ev, flags)
	struct mylst	*lst;
	struct event	*ev;
	int				flags;
{
	int				ret;
	struct event	nev;

	if (!lst || !ev) return RERR_PARAM;
	if (flags & EVP_F_CPY) {
		ret = ev_new (&nev);
		if (!RERR_ISOK(ret)) return ret;
		ret = ev_copy (&nev, ev);
		if (!RERR_ISOK(ret)) {
			ev_free (&nev);
			return ret;
		}
		ev = &nev;
	}
	ret = elabrmvar (lst, ev, flags);
	if (!RERR_ISOK(ret)) {
		if (flags & EVP_F_CPY) {
			ev_free (&nev);
		}
		FRLOGF (LOG_ERR, "error elaborating remove vars: %s",
					rerr_getstr3(ret));
		return ret;
	}
	ret = dosetevent2 (lst, ev, flags);
	if (flags & EVP_F_CPY) {
		ev_free (&nev);
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event: %s", rerr_getstr3(ret));
		return ret;
	}
	return ret;
}

static
int
elabrmvar (lst, ev, flags)
	struct mylst	*lst;
	struct event	*ev;
	int				flags;
{
	int				num, i, ret, chg=0;
	struct ev_var	*var;

	num = ev_numvar (ev);
	for (i=0; i<num; i++) {
		ret = ev_getvarptrpos (&var, ev, i);
		if (!RERR_ISOK(ret)) return ret;
		if (var->val.typ == EVP_T_RM) {
			ret = dormvar (lst, var->var, var->idx1, var->idx2);
			if (!RERR_ISOK(ret)) return ret;
			ret = ev_rmvarpos (ev, i);
			if (!RERR_ISOK(ret)) return ret;
			i--;
			num--;
			chg = 1;
		}
	}
	if (chg) lst->lastchange = tmo_now ();
	return RERR_OK;
}

static
int
dosetevent2 (lst, ev, flags)
	struct mylst	*lst;
	struct event	*ev;
	int				flags;
{
	int				ret, num, i;
	struct ev_var	*var;
	int				chg, f;

	if (!lst || !ev) return RERR_PARAM;
	flags &= ~(EVP_F_CPY | EVP_F_FREE | EVP_F_CPYVAL | EVP_F_FREEVAL);
	num = ev_numvar (ev);
	if (!RERR_ISOK(num)) return num;
	ret = addbuf (lst, ev->buf, ev->buflen);
	if (!RERR_ISOK(ret)) return ret;
	ev->buf = NULL;
	ev->buflen = 0;
	chg = 0;
	for (i=0; i<num; i++) {
		ret = ev_getvarptrpos (&var, ev, i);
		if (!RERR_ISOK(ret)) return ret;
		f = 0;
		if (var->flags & EVP_F_FREEVAL) f |= EVP_F_FREEVAL;
		if (var->flags & EVP_F_FREE) f |= EVP_F_FREE;
		ret = dosetvar (lst, var->var, var->idx1, var->idx2, var->val, f);
		if (!RERR_ISOK(ret)) return ret;
		if (ret) chg=1;
		var->flags &= ~(EVP_F_FREE | EVP_F_FREEVAL);
	}
	if (flags & EVVAR_F_SILENT) chg=0;
	if (chg) {
		ret = maywrite (lst);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error writing varlist: %s", rerr_getstr3(ret));
		}
	}
	return (flags & EVVAR_F_SILENT) ? 0 : chg;
}

static
int
dosetvar (lst, var, idx1, idx2, val, flags)
	struct mylst	*lst;
	int				idx1, idx2, flags;
	const char		*var;
	struct ev_val	val;
{
	struct ev_var	*ptr;
	struct xbufref	*buf;
	int				ret, chg;
	unsigned			i;

	if (!lst) return RERR_PARAM;
	ret = ev_getvarptr (&ptr, &(lst->lst), var, idx1, idx2);
	if (ret == RERR_NOT_FOUND) {
		return doaddvar (lst, var, idx1, idx2, val, flags);
	} else if (!RERR_ISOK(ret)) {
		return ret;
	}
	chg=1;
	if (ptr->val.typ == val.typ) {
		switch (val.typ) {
		case EVP_T_INT:
			chg = !(ptr->val.i == val.i);
			break;
		case EVP_T_FLOAT:
			chg = !float_almost_eq (ptr->val.f, val.f);
			if (chg && isnan(ptr->val.f) && isnan (val.f)) chg = 0;
			break;
		case EVP_T_TIME:
		case EVP_T_NTIME:
		case EVP_T_DATE:
		case EVP_T_NDATE:
			chg = !(ptr->val.d == val.d);
			break;
		case EVP_T_STR:
			chg = strcmp (ptr->val.s, val.s) ? 1 : 0;
			break;
		}
	}
	if (!chg) return 0;
	if (ptr->val.typ == EVP_T_STR) {
		TLST_FOREACHPTR2 (buf, lst->bufs, i) {
			if (ptr->val.s >= buf->buf && 
							ptr->val.s < buf->buf + buf->buflen) {
				buf->refcnt--;
				if (buf->refcnt <= 0) {
					TLST_REMOVE (lst->bufs, i, TLST_F_CPYLAST);
				}
				break;
			}
		}
		ptr->flags &= ~(EVP_F_CPYVAL | EVP_F_FREEVAL);
	}
	ptr->val.typ = val.typ;
	switch (val.typ) {
	case EVP_T_INT:
		ptr->val.i = val.i;
		break;
	case EVP_T_FLOAT:
		ptr->val.f = val.f;
		break;
	case EVP_T_TIME:
	case EVP_T_NTIME:
	case EVP_T_DATE:
	case EVP_T_NDATE:
		ptr->val.d = val.d;
		break;
	case EVP_T_STR:
		if ((flags & EVP_F_CPY) || (flags & EVP_F_CPYVAL)) {
			if (!val.s) {
				ptr->val.s = NULL;
			} else {
				ptr->val.s = strdup (val.s);
				if (!ptr->val.s) return RERR_NOMEM;
				ptr->flags |= EVP_F_CPYVAL | EVP_F_FREEVAL;
			}
		} else {
			ptr->val.s = val.s;
			TLST_FOREACHPTR2 (buf, lst->bufs, i) {
				if (val.s >= buf->buf && val.s < buf->buf + buf->buflen) {
					buf->refcnt++;
					break;
				}
			}
		}
		break;
	}
	if (chg) lst->lastchange = tmo_now ();
	return chg;
}

static
int
doaddvar (lst, var, idx1, idx2, val, flags)
	struct mylst	*lst;
	int				idx1, idx2, flags;
	const char		*var;
	struct ev_val	val;
{
	struct xbufref	*buf;
	unsigned			i;
	int				ret;

	if (!lst) return RERR_PARAM;
	ret = ev_addvar_v (&(lst->lst), var, idx1, idx2, val, flags);
	if (!RERR_ISOK(ret)) return ret;
	if (!(flags & EVP_F_CPY)) {
		TLST_FOREACHPTR2 (buf, lst->bufs, i) {
			if (var >= buf->buf && var < buf->buf + buf->buflen) {
				buf->refcnt++;
				break;
			}
		}
		if ((val.typ == EVP_T_STR) && !(flags & EVP_F_CPYVAL)) {
			TLST_FOREACHPTR2 (buf, lst->bufs, i) {
				if (val.s >= buf->buf && val.s < buf->buf + buf->buflen) {
					buf->refcnt++;
					break;
				}
			}
		}
	}
	lst->lastchange = tmo_now ();
	triggergvar (lst, var, TRG_C_ADD);
	return 1;
}

static
int
triggerall (lst, gid)
	struct mylst	*lst;
	int				gid;
{
	int			num, i;
	const char	*var;
	int			ret, ret2 = RERR_OK;

	if (!lst || gid < 0) return RERR_PARAM;
	num = ev_numvar (&(lst->lst));
	if (!RERR_ISOK(num)) return num;
	for (i=0; i<num; i++) {
		ret = ev_getvarpos_v (&var, NULL, NULL, NULL, &(lst->lst), i);
		if (!RERR_ISOK(ret)) ret2 = ret;
		ret = triggergvar2 (gid, lst->self, var, TRG_C_ADD);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}


static
int
triggergvar (lst, var, cmd)
	struct mylst	*lst;
	const char		*var;
	int				cmd;
{
	unsigned	i;
	int		ret, gid;
	int		ret2 = RERR_OK;

	if (!lst || !var) return RERR_PARAM;
	TLST_FOREACH2 (gid, lst->trglst, i) {
		ret = triggergvar2 (gid, lst->self, var, cmd);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}

static
int
triggergvar2 (gid, id, var, cmd)
	int			gid, id, cmd;
	const char	*var;
{
	struct gvar	gvar;
	int			ret = RERR_OK;

	if (!var) return RERR_PARAM;
	once {
		bzero (&gvar, sizeof (struct gvar));
		gvar.fullname = strdup (var);
		if (!gvar.fullname) {
			ret = RERR_NOMEM;
			break;
		}
		gvar.typ = GVAR_T_EVVAR;
		gvar.id = id;
		gvar.flags = GVAR_F_FREEFULLNAME;
		switch (cmd) {
		case TRG_C_ADD:
			ret = gvl_addgvar (gid, &gvar);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error adding var >>%s<< to gvar list (%d) - "
								"variable won't be found: %s", var, gid,
								rerr_getstr3(ret));
			}
			break;
		case TRG_C_RM:
			ret = gvl_rmgvar (gid, &gvar, 0);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error removing var >>%s<< from gvar list (%d) - "
								"variable still will be found: %s", var, gid,
								rerr_getstr3(ret));
			}
			break;
		default:
			ret = RERR_NOT_SUPPORTED;
			break;
		}
		if (!RERR_ISOK(ret)) {
			gvl_gvarfree (&gvar);
		}
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error %sing var >>%s<<: %s", (cmd == TRG_C_ADD ? "add" :
							(cmd == TRG_C_RM ? "remove" : "???")), var, 
							rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



static
int
doinit ()
{
	struct mylst	lst;
	int				ret;

	CF_MAYREAD;
	if (isinit) return RERR_OK;
	ret = TLST_NEW (listlist, struct mylst);
	if (!RERR_ISOK(ret)) return ret;
	bzero (&lst, sizeof (struct mylst));
	ret = TLST_NEW (lst.bufs, struct xbufref);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (lst.shadowlst, struct shadow);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (lst.shadownums, int);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_new (&(lst.lst));
	if (!RERR_ISOK(ret)) return ret;
	lst.exist = &lst;	/* must have any value but NULL */
	ret = TLST_SET (listlist, 0, lst);
	if (!RERR_ISOK(ret)) {
		ev_free (&(lst.lst));
		TLST_FREE (listlist);
		return ret;
	}
	listlist.num = 1;
	isinit = 1;
	return RERR_OK;
}

static
int
dolock (lst)
	struct mylst	*lst;
{
	int	ret;

	if (!lst) return RERR_PARAM;
	if (!lst->protect) return RERR_OK;
	ret = pthread_mutex_lock (&(lst->mutex));
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}

static
int
dounlock (lst)
	struct mylst	*lst;
{
	int	ret;

	if (!lst) return RERR_PARAM;
	if (!lst->protect) return RERR_OK;
	ret = pthread_mutex_unlock (&(lst->mutex));
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}

static
pthread_mutex_t	globmutex = PTHREAD_MUTEX_INITIALIZER;

static
int
globlock ()
{
	int	ret;

	ret = pthread_mutex_lock (&(globmutex));
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}

static
int
globunlock ()
{
	int	ret;

	ret = pthread_mutex_unlock (&(globmutex));
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}


static
int
read_config ()
{
	cf_begin_read ();
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 0;
}

static
int
float_almost_eq (a, b)
	double	a, b;
{
	double	diff, l;

	diff = fabs (a - b);

	a = fabs (a);
	b = fabs (b); 
	l = (b > a) ? b : a;
	if (diff <= l * DBL_EPSILON) {
		return 1;
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
