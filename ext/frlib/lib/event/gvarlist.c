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
#include <regex.h>



#include <fr/base.h>
#include <fr/event.h>
#include "gvarlist.h"



struct rootpath {
	char	*rpath;
	int	flags;
	int	len;
};


#define CMD_ADD	1
#define CMD_RM		2
#define CMD_RPATH	3	/* yet not scheduled */

struct spoolcmd {
	struct gvar			gvar;		/* must be at the beginning and not inside a union */
	struct rootpath	rpath;
	int					cmd;
};


struct gvarlst {
	void			*exist;
	int			self;
	struct tlst	list;
	int			flags;
	struct tlst	rpathlst;
	int			lock;			/* if locked cmds are spooled */
	int			spoollock;	/* if spool is locked spool2 is used */
	struct tlst	spool;
	struct tlst	spool2;		/* 2 spools are needed to avoid race cond. */
	int			hasspool;
	int			hasspool2;
};



static struct tlst	gvarlst;
static int				isinit = 0;
static int				config_read = 0;


#define PURIFY_F_HAVENS			0x01
#define PURIFY_F_FIRSTCOLON	0x02

static int dormgvar (struct gvarlst*, struct gvar*, int);
static int dormgvar2 (struct gvarlst*, struct gvar*);
static int gvarfree (struct gvar*);
static int dosearch (struct gvl_result*, struct gvarlst*, char*, int);
static int dosearch2 (struct gvl_result*, struct gvarlst*, char*, int);
static int dosearch3 (struct gvl_result*, struct gvarlst*, int);
static int dosearchglob (struct gvl_result*, struct gvarlst*, char*, int);
static int dosearchregex2 (struct gvl_result*, struct gvarlst*, char*, int);
static int dosearchregex3 (struct gvl_result*, struct gvarlst*, int);
static int rescleanup (struct gvl_result*, struct gvarlst*);
static int gvarcmp (void*, void*);
static int gvarcmp2 (void*, void*);
#if 0
static int gvarcmpfull (void*, void*);
#endif
static int dosetrootpath (struct gvarlst*, char*, int);
static int dosetrootpath2 (struct gvarlst*, char*, int);
static int freerpath (struct gvarlst*);
static int spoolcmd (struct gvarlst*, struct spoolcmd*);
static int unspoolcmd (struct gvarlst*);
static int docmd (struct gvarlst*, struct spoolcmd*);
#if 0
static int freedupvar (struct gvar*, struct gvar*);
#endif
static int creatgvar (struct gvarlst*, struct gvar*, int*);
static int creatgvar2 (struct gvarlst*, struct gvar*);
static int doaddgvar (struct gvarlst*, struct gvar*);
static int doaddgvar2 (struct gvarlst*, struct gvar*);
static int purifyvar (char*, int);
static int rmrpath (struct gvarlst*, struct gvar*, int);
static int doinit ();
static int read_config ();


#define DOINIT \
		if (!isinit) { \
			int	_ret; \
			_ret = doinit (); \
			if (!RERR_ISOK(_ret)) { \
				FRLOGF (LOG_ERR, "error initializing var list: %s", \
							rerr_getstr3(_ret)); \
				return _ret; \
			} \
		}




int
gvl_new ()
{
	int				ret, id;
	struct gvarlst	vl, *ptr;

	DOINIT;
	bzero (&vl, sizeof (struct gvarlst));
	vl.exist = &vl;	/* any but NULL */
	ret = TLST_NEW ((vl.list), struct gvar);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (vl.rpathlst, struct rootpath);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (vl.spool, struct spoolcmd);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (vl.spool2, struct spoolcmd);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_ADDINSERT (gvarlst, vl);
	if (!RERR_ISOK(ret)) return ret;
	id = ret;
	ret = TLST_GETPTR (ptr, gvarlst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_INTERNAL;
	ptr->self = id;
	return id;
}

int
gvl_free (id)
	int	id;
{
	int				ret, i;
	struct gvarlst	gvl, *ptr;
	struct gvar		*gvar;

	DOINIT;
	if (id < 0) return RERR_PARAM;
	if (id == 0) return RERR_FORBIDDEN;
	ret = TLST_GET (gvl, gvarlst, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_REMOVE (gvarlst, id, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "cannot remove varlist (%d): %s", id, rerr_getstr3(ret));
		return ret;
	}
	ptr = &gvl;
	ret = unspoolcmd (ptr);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error unspooling spooled commands: %s", 
					rerr_getstr3(ret));
	}
	TLST_FREE (ptr->spool);
	TLST_FREE (ptr->spool2);
	if (ptr->flags & GVL_F_FREE) {
		TLST_FOREACHPTR2 (gvar, ptr->list, i) {
			if (!gvar) continue;
			gvarfree (gvar);
		}
	}
	ret = TLST_FREE ((ptr->list));
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "cannot free varlist: %s", rerr_getstr3(ret));
	}
	ret = freerpath (ptr);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "cannot clean root path list: %s", rerr_getstr3(ret));
	}
	ret = TLST_FREE (ptr->rpathlst);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "cannot free root path list: %s", rerr_getstr3(ret));
	}
	bzero (&gvl, sizeof (struct gvarlst));
	return RERR_OK;
}


/* *******************
 * set root path
 * *******************/

int
gvl_setrootpath (id, rpath, flags)
	int	id, flags;
	char	*rpath;
{
	int				ret;
	struct gvarlst	*ptr;
	DOINIT;
	if (id < 0) return RERR_PARAM;
	ret = TLST_GETPTR (ptr, gvarlst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_INTERNAL;
	return dosetrootpath (ptr, rpath, flags);
}




/* *******************
 * add var functions
 * *******************/


int
gvl_addgvar (id, gvar)
	int			id;
	struct gvar	*gvar;
{
	int				ret;
	struct gvarlst	*ptr;

	DOINIT;
	if (id < 0) return RERR_PARAM;
	ret = TLST_GETPTR (ptr, gvarlst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_INTERNAL;
	ret = doaddgvar (ptr, gvar);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting var: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



int
gvl_addvar (id, var, typ, arg1, arg2, flags)
	int	id, typ, arg1, flags;
	char	*var;
	void	*arg2;
{
	int			needfree=0;
	int			ret;
	struct gvar	gvar;

	if (!var || !*var) return RERR_PARAM;
	if (id < 0) return RERR_PARAM;
	if (flags & GVL_F_CPY) {
		var = strdup (var);
		if (!var) return RERR_NOMEM;
		flags &= ~GVL_F_CPY;
		needfree=1;
	}
	bzero (&gvar, sizeof (struct gvar));
	gvar.fullname = var;
	gvar.typ = typ;
	gvar.id = arg1;
	gvar.arg = arg2;
	if (needfree || (flags & GVL_F_FREE)) gvar.flags = GVAR_F_FREEVAR;
	if (flags & GVL_F_INTEXT) gvar.flags |= GVAR_F_INTEXT;
	ret = gvl_addgvar (id, &gvar);
	if (!RERR_ISOK(ret)) {
		if (needfree) free (var);
		return ret;
	}
	return RERR_OK;
}

int
gvl_addvarns (id, var, ns, typ, arg1, arg2, flags)
	int	id, typ, arg1, flags;
	char	*var, *ns;
	void	*arg2;
{
	struct gvar	gvar;
	int			needfree=0;
	int			ret;

	if (!var || !*var) return RERR_PARAM;
	if (id < 0) return RERR_PARAM;
	bzero (&gvar, sizeof (struct gvar));
	if (flags & GVL_F_CPY) {
		var = strdup (var);
		if (!var) return RERR_NOMEM;
		if (ns) {
			ns = strdup (ns);
			if (!ns) {
				free (var);
				return RERR_NOMEM;
			}
		}
		needfree = 1;
	}
	gvar.var = var;
	gvar.ns = ns;
	if (needfree || (flags & GVL_F_FREE)) {
		gvar.flags = GVAR_F_FREEVAR;
		if (ns) gvar.flags |= GVAR_F_FREENS;
	}
	gvar.typ = typ;
	gvar.id = arg1;
	gvar.arg = arg2;
	if (flags & GVL_F_INTEXT) gvar.flags |= GVAR_F_INTEXT;
	ret = gvl_addgvar (id, &gvar);
	if (!RERR_ISOK(ret)) {
		if (needfree) {
			free (var);
			if (ns) free (ns);
		}
		return ret;
	}
	return RERR_OK;
}

int
gvl_rmvar (id, var, flags)
	int			id, flags;
	const char	*var;
{
	struct gvar	gvar;

	bzero (&gvar, sizeof (struct gvar));
	gvar.var = (char*)(void*)var;
	return gvl_rmgvar (id, &gvar, flags);
}

int
gvl_rmgvar (id, gvar, flags)
	int			id, flags;
	struct gvar	*gvar;
{
	int				ret;
	struct gvarlst	*ptr;

	DOINIT;
	if (id < 0) return RERR_PARAM;
	ret = TLST_GETPTR (ptr, gvarlst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_INTERNAL;
	ret = dormgvar (ptr, gvar, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting var: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


/* *****************
 * search functions
 * *****************/


int
gvl_search (res, id, var, flags)
	struct gvl_result	*res;
	int					id, flags;
	char					*var;
{
	int				ret;
	struct gvarlst	*ptr;

	DOINIT;
	if (id < 0) return RERR_PARAM;
	ret = TLST_GETPTR (ptr, gvarlst, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr) return RERR_INTERNAL;
	if (flags & GVL_SEARCH_F_CLEANUP) {
		return rescleanup (res, ptr);
	}
	if (!(flags & GVL_SEARCH_F_MATCHNEXT)) {
		if (ptr->lock) rescleanup (NULL, ptr);
		bzero (res, sizeof (struct gvl_result));
	}
	ptr->lock = 1;
	if (flags & GVL_SEARCH_F_GLOB) {
		ret = dosearchglob (res, ptr, var, flags);
	} else {
		ret = dosearch (res, ptr, var, flags);
	}
	if (!RERR_ISOK(ret)) {
		rescleanup (res, ptr);
		if (ret != RERR_NOT_FOUND) {
			FRLOGF (LOG_DEBUG, "error in search: %s", rerr_getstr3(ret));
		}
		return ret;
	}
	if (!(flags & GVL_SEARCH_F_MATCHMORE)) {
		rescleanup (res, ptr);
	}
	return RERR_OK;
}


int
gvl_search_cleanup (res, id)
	struct gvl_result	*res;
	int					id;
{
	return gvl_search (res, id, NULL, GVL_SEARCH_F_CLEANUP);
}


int
gvl_gvarfree (gvar)
	struct gvar	*gvar;
{
	return gvarfree (gvar);
}




/* *******************
 * static functions
 * *******************/


static
int
dormgvar (gvl, gvar, flags)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
	int				flags;
{
	struct spoolcmd	cmd;
	int					saveflags, ret;
	struct gvar			gvar2;

	if (!gvl || !gvar) return RERR_PARAM;
	gvar2 = *gvar;
	ret = creatgvar (gvl, gvar, &saveflags);
	if (!RERR_ISOK(ret)) return ret;
	if (gvl->lock) {
		bzero (&cmd, sizeof (struct spoolcmd));
		cmd.cmd = CMD_RM;
		cmd.gvar = *gvar;
		return spoolcmd (gvl, &cmd);
	}
	ret = dormgvar2 (gvl, gvar);
	gvar->flags = saveflags;
	gvarfree (gvar);
	*gvar = gvar2;
	if (!RERR_ISOK(ret)) {
		if (gvl->lock) {
			FRLOGF (LOG_ERR, "cannot spool command: %s", rerr_getstr3(ret));
		} else {
			FRLOGF (LOG_ERR, "error removing var: %s", rerr_getstr3(ret));
		}
		return ret;
	}
	return RERR_OK;
}

static
int
dormgvar2 (gvl, gvar)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
{
	struct gvar	*ptr, gvar2;
	int			ret;
	unsigned		id;

	if (!gvl || !gvar) return RERR_PARAM;
	/* check wether we have it already and decrement refcnt */
	ret = TLST_FIND (id, gvl->list, &gvarcmp, *gvar);	/* we cannot use search here */
	if (ret >= 0) {
		ret = TLST_GETPTR (ptr, gvl->list, id);
		if (!RERR_ISOK(ret)) ptr = NULL;
	} else {
		ptr = NULL;
	}
	if (!ptr) return RERR_OK;
	ptr->refcnt--;
	if (ptr->refcnt > 0) return RERR_OK;
	gvar2 = *ptr;
	ret = TLST_REMOVE (gvl->list, id, TLST_F_SHIFT);
	if (!RERR_ISOK(ret)) {
		ptr->refcnt++;
		return ret;
	}
	gvarfree (&gvar2);
	return RERR_OK;
}


static
int
gvarfree (gvar)
	struct gvar	*gvar;
{
	if (!gvar) return RERR_PARAM;
	if ((gvar->flags & GVAR_F_FREEVAR) && (gvar->var)) free (gvar->var);
	if ((gvar->flags & GVAR_F_FREENS) && (gvar->ns)) free (gvar->ns);
	if ((gvar->flags & GVAR_F_FREEFULLNAME) && gvar->fullname) {
		free (gvar->fullname);
	}
	bzero (gvar, sizeof (struct gvar));
	return RERR_OK;
}


static
int
dosearch (res, gvl, var, flags)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
	char					*var;
	int					flags;
{
	char		_buf[1024];
	char		*buf;
	size_t	len;
	int		ret, needfree=0;

	if (!gvl || !var) return RERR_PARAM;
	if (!(flags & GVL_SEARCH_F_MAYMOD) && !(flags & GVL_SEARCH_F_MATCHNEXT)) {
		len = strlen (var);
		if (res) {
			if (len < sizeof (res->sbuf)) {
				buf = res->sbuf;
			} else {
				buf = res->abuf = malloc (len+1);
			}
		} else {
			if (len < sizeof (_buf)) {
				buf = _buf;
			} else {
				buf = malloc (len+1);
				needfree = 1;
			}
		}
		if (!buf) return RERR_NOMEM;
		strcpy (buf, var);
		var = buf;
	}
	ret = dosearch2 (res, gvl, var, flags);
	if (needfree) free (buf);
	return ret;
}


static
int
dosearch2 (res, gvl, var, flags)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
	char					*var;
	int					flags;
{
	struct gvar			gvar;
	int					ret;
	char					*s, *s2;
	int					abs=0;
	struct gvl_result	_res;

	if (!gvl || !var) return RERR_PARAM;
	if (flags & GVL_SEARCH_F_MATCHNEXT) {
		if (!res) return RERR_PARAM;
		return dosearch3 (res, gvl, flags);
	}
	bzero (&gvar, sizeof (struct gvar));
	s2 = top_skipwhiteplus (var, ":");
	s = rindex (s2, ':');
	if (s) {
		*s=0; s++;
		gvar.ns = var;
		gvar.var = s;
	} else {
		gvar.var = var;
		gvar.ns = NULL;
	}
	ret = purifyvar (gvar.var, PURIFY_F_FIRSTCOLON);
	if (!RERR_ISOK(ret)) return ret;
	if (!*(gvar.var)) return RERR_INVALID_VAR;
	if (gvar.ns) {
		ret = purifyvar (gvar.ns, PURIFY_F_HAVENS);
		if (!RERR_ISOK(ret)) return ret;
		if (!*(gvar.ns)) gvar.ns = NULL;
	}
	if (*(gvar.var) == ':') {
		if (gvar.ns) return RERR_INTERNAL;
		abs = 1;
		gvar.var++;	/* at most 1 colon */
	}
	if (!res) {
		res = &_res;
	}
	if (flags & GVL_SEARCH_F_ABS) abs = 1;
	res->abs = abs;
	res->varbuf = gvar;
	return dosearch3 (res, gvl, flags);
}

static
int
dosearch3 (res, gvl, flags)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
	int					flags;
{
	struct gvar	*ptr, *ptr2;
	struct gvar	*gvar;
	int			abs, id;
	int			ret;

	if (!res || !gvl) return RERR_PARAM;
	abs = res->abs;
	gvar = &(res->varbuf);
	if (flags & GVL_SEARCH_F_MATCHNEXT) {
		if (!res) return RERR_PARAM;
		id = res->idx+1;
		ret = TLST_GETPTR (ptr, gvl->list, id);
		if (!RERR_ISOK(ret)) return ret;
		if (abs || gvar->ns) {
			if (gvarcmp (ptr, gvar) != 0) return RERR_NOT_FOUND;
		} else {
			if (gvarcmp2 (ptr, gvar) != 0) return RERR_NOT_FOUND;
		}
	} else {
		ret = TLST_SEARCH (gvl->list, *gvar, gvarcmp);
		if (ret == RERR_NOT_FOUND) {
			if (abs || gvar->ns) return ret;
			ret = TLST_SEARCH (gvl->list, *gvar, gvarcmp2);
			if (!RERR_ISOK(ret)) return ret;
		} else if (!RERR_ISOK(ret)) {
			return ret;
		}
		id = ret;
		ret = TLST_GETPTR (ptr, gvl->list, id);
		if (!RERR_ISOK(ret)) return ret;
		if (flags & GVL_SEARCH_F_UNIQ) {
			ret = TLST_GETPTR (ptr2, gvl->list, (id+1));
			if (RERR_ISOK(ret)) {
				if (!gvarcmp2 (ptr2, gvar)) {
					return RERR_NOT_UNIQ;
				}
			}
		}
	}
	res->idx = id;
	if (flags & GVL_SEARCH_F_CHECKONLY) return RERR_OK;
	res->gvar = *ptr;
	return RERR_OK;
}

int
gvl_isglob (str)
	const char	*str;
{
	const char	*s;

	if (!str) return RERR_PARAM;
	for (s=str; *s; s++) {
		switch (*s) {
		case '\\':
			if (s[1]) s++;
			break;
		case '*':
		case '?':
		case '[':
			return RERR_OK;
		}
	}
	return RERR_FAIL;
}

static
int
dosearchglob (res, gvl, var, flags)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
	char					*var;
	int					flags;
{
	size_t	len;
	char		*s, *so, *s2;
	char		_buf[1024];
	char		*buf;
	int		ret, needfree=0;

	if (!gvl || !var) return RERR_PARAM;
	if (!res && (!(flags & GVL_SEARCH_F_CHECKONLY) || 
				(flags & GVL_SEARCH_F_MATCHNEXT))) {
		return RERR_PARAM;
	}
	if (flags & GVL_SEARCH_F_MATCHNEXT) {
		return dosearchregex3 (res, gvl, flags);
	}
	/* calculate regex len */
	for (s=var,len=0; *s; s++) {
		switch (*s) {
		case '\\':
			if (s[1]) {
				s++;
			} else {
				len++;
			}
			break;
		case '*':
		case '.':
		case '+':
		case '(':
		case ')':
		case '^':
		case '$':
		case '|':
		case '[':
			len++;
			break;
		}
	}
	len += s-var;
	if (len == 0) return RERR_PARAM;
	len += 2;
	/* alloc buf */
	if (flags & GVL_SEARCH_F_MATCHMORE) {
		if (!res) return RERR_PARAM;
		if (len < sizeof (res->sbuf)) {
			buf = res->sbuf;
		} else {
			buf = res->abuf = malloc (len+1);
		}
	} else {
		if (len < sizeof (_buf)) {
			buf = _buf;
		} else {
			buf = malloc (len+1);
			needfree = 1;
		}
	}
	/* copy and convert to regex */
	so = buf;
	*so = '^';
	so++;
	for (s=var; *s; s++, so++) {
		switch (*s) {
		case '\\':
			*so='\\';
			s++; so++;
			if (*s) {
				*so = *s;
			} else {
				/* allow ending on backslash */
				s--;
				*so = '\\';
			}
			break;
		case '*':
			*so = '.';
			so++;
			*so = '*';
			break;
		case '?':
			*so = '.';
			break;
		case '.':
		case '+':
		case '(':
		case ')':
		case '^':
		case '$':
		case '|':
			*so = '\\';
			so++;
			*so = *s;
			break;
		case '[':
			*so = *s;
			if (!s[1]) {
				*so = '\\';
				so++;
				*so = '[';
				break;
			}
			so++; 
			s++;
			if (*s == '!') {
				*so = '^';
				if (!s[1]) {
					so++;
					*so = ']';
					break;
				}
				s++; so++;
			}
			s2 = s;
			if (*s == ']') s++;
			for (; *s; s++) {
				if ((*s == ']') && !((s[-1] == '.') || (s[-1] == '=') || 
										(s[-1] == ':'))) {
					break;
				}
			}
			strncpy (so, s2, (s-s2));
			so += s-s2;
			*so = ']';
			if (!*s) s--;
			break;
		default:
			*so = *s;
		}
	}
	*so = '$';
	so++;
	*so = 0;
	ret = dosearchregex2 (res, gvl, buf, flags);
	if (needfree) free (buf);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}

static
int
dosearchregex2 (res, gvl, var, flags)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
	char					*var;
	int					flags;
{
	struct gvl_result	_res;
	int					ret;
	char					errstr[96];

	if (!gvl || !var) return RERR_PARAM;
	if (!res) {
		res = &_res;
	}
	bzero (&_res, sizeof (struct gvl_result));
	res->isglob = 1;
	ret = regcomp (&(res->reg), var, REG_EXTENDED|REG_ICASE|REG_NOSUB);
	if (ret < 0) {
		regerror (ret, &(res->reg), errstr, sizeof (errstr));
		FRLOGF (LOG_WARN, "error compiling regural expression: %s", errstr);
		return RERR_SYSTEM;
	}
	res->regneedfree = 1;
	return dosearchregex3 (res, gvl, flags);
}


static
int
dosearchregex3 (res, gvl, flags)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
	int					flags;
{
	int			id, ret, i;
	struct gvar	*ptr;
	char			*var;

	if (!res || !gvl) return RERR_PARAM;
	if (flags & GVL_SEARCH_F_MATCHNEXT) {
		id = res->idx+1;
	} else {
		id = 0;
	}
	for (i=id; i<(ssize_t)TLST_GETNUM(gvl->list); i++) {
		ret = TLST_GETPTR (ptr, gvl->list, i);
		if (!RERR_ISOK(ret)) return ret;
		var = ptr->noroot ? ptr->noroot : (ptr->fullname ? ptr->fullname : 
																		ptr->var);
		ret = regexec (&(res->reg), var, 0, NULL, 0);
		if (ret == 0) {
			res->idx = i;
			if (!(flags & GVL_SEARCH_F_CHECKONLY)) {
				res->gvar = *ptr;
			}
			return RERR_OK;
		} else if (ret != REG_NOMATCH) {
			return RERR_SYSTEM;
		}
	}
	return RERR_NOT_FOUND;
}


static
int
rescleanup (res, gvl)
	struct gvl_result	*res;
	struct gvarlst		*gvl;
{
	if (res) {
		if (res->abuf) free (res->abuf);
		if (res->isglob && res->regneedfree) {
			regfree (&(res->reg));
		}
	}
	if (gvl) {
		unspoolcmd (gvl);
	}
	return RERR_OK;
}


static
int
gvarcmp (ptr1, ptr2)
	void	*ptr1, *ptr2;
{
	struct gvar	*gvar1, *gvar2;
	int			ret;

	if (!ptr1 && !ptr2) return 0;
	if (!ptr1) return 1;
	if (!ptr2) return -1;
	gvar1 = ptr1;
	gvar2 = ptr2;
	if (!gvar1->var && !gvar2->var) return 0;
	if (!gvar1->var) return 1;
	if (!gvar2->var) return -1;
	ret = strcasecmp (gvar2->var, gvar1->var);
	if (ret != 0) return ret;
	if (!gvar1->ns && !gvar2->ns) return 0;
	if (!gvar1->ns) return 1;
	if (!gvar2->ns) return -1;
	return strcasecmp (gvar2->ns, gvar1->ns);
}

#if 0
static
int
gvarcmpfull (ptr1, ptr2)
	void	*ptr1, *ptr2;
{
	int			ret;
	struct gvar	*gvar1, *gvar2;

	ret = gvarcmp (ptr1, ptr2);
	if (ret != 0) return ret;
	gvar1 = ptr1;
	gvar2 = ptr2;
	if ((gvar1->typ == gvar2->typ) && (gvar1->id == gvar2->id)
				&& (gvar1->arg == gvar2->arg)) {
		return 0;
	}
	return 1;	/* continue search on the right */
}
#endif

static
int
gvarcmp2 (ptr1, ptr2)
	void	*ptr1, *ptr2;
{
	struct gvar	*gvar1, *gvar2;

	if (!ptr1 && !ptr2) return 0;
	if (!ptr1) return 1;
	if (!ptr2) return -1;
	gvar1 = ptr1;
	gvar2 = ptr2;
	if (!gvar1->var && !gvar2->var) return 0;
	if (!gvar1->var) return 1;
	if (!gvar2->var) return -1;
	return strcasecmp (gvar2->var, gvar1->var);
}

static
int
dosetrootpath (gvl, rpath, flags)
	struct gvarlst	*gvl;
	char				*rpath;
	int				flags;
{
	int	needfree = 0;
	int	ret;

	if (!gvl) return RERR_PARAM;
	if (!(flags & GVL_F_APPEND)) {
		freerpath (gvl);
	}
	if (!rpath || !*rpath) return RERR_OK;
	if (flags & GVL_F_CPY) {
		rpath = strdup (rpath);
		if (!rpath) return RERR_NOMEM;
		flags &= ~GVL_F_CPY;
		flags |= GVL_F_FREE;
		needfree = 1;
	}
	ret = dosetrootpath2 (gvl, rpath, flags);
	if (!RERR_ISOK(ret)) {
		if (needfree) free (rpath);
		FRLOGF (LOG_ERR, "error setting root path: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
dosetrootpath2 (gvl, rpath, flags)
	struct gvarlst	*gvl;
	char				*rpath;
	int				flags;
{
	struct rootpath	rp, *ptr;
	char					*field;
	int					i, ret;
	char					*s;

	if (!gvl || !rpath) return RERR_PARAM;
	s = top_skipwhiteplus (rpath, ",;");
	if (!s || !*s) return RERR_INVALID_PATH;
	if (s > rpath) {
		/* this guarantees, that the first field is ident. to rpath */
		memmove (rpath, s, strlen (s));
	}
	while ((field = top_getfield (&rpath, ",;", 0))) {
		bzero (&rp, sizeof (struct rootpath));
		rp.rpath = field;
		rp.len = strlen (field);
		if (field == rpath) rp.flags = flags;
		ret = TLST_ADD (gvl->rpathlst, rp);
		if (!RERR_ISOK(ret)) {
			TLST_FOREACHPTR2 (ptr, gvl->rpathlst, i) {
				if (ptr->rpath == rpath) ptr->flags = 0;
			}
			return ret;
		}
	}
	return RERR_OK;
}


static
int
freerpath (gvl)
	struct gvarlst	*gvl;
{
	struct rootpath	*ptr;
	int					i;

	if (!gvl) return RERR_PARAM;
	TLST_FOREACHPTR2 (ptr, gvl->rpathlst, i) {
		if (ptr->rpath && (ptr->flags & GVL_F_FREE)) {
			free (ptr->rpath);
		}
	}
	TLST_RESET (gvl->rpathlst);
	return RERR_OK;
}

static
int
spoolcmd (gvl, cmd)
	struct gvarlst		*gvl;
	struct spoolcmd	*cmd;
{
	int	ret;
	
	if (!gvl || !cmd) return RERR_PARAM;
	if (gvl->spoollock) {
		ret = TLST_ADD (gvl->spool2, *cmd);
		if (!RERR_ISOK(ret)) return ret;
		gvl->hasspool2=1;
	} else {
		ret = TLST_ADD (gvl->spool, *cmd);
		if (!RERR_ISOK(ret)) return ret;
		gvl->hasspool=1;
	}
	return RERR_OK;
}

static
int
unspoolcmd (gvl)
	struct gvarlst	*gvl;
{
	struct spoolcmd	*ptr;
	int					i;

	if (!gvl) return RERR_PARAM;
	while (1) {
		gvl->lock = 1;
		while (1) {
			if (!gvl->hasspool) break;
			gvl->spoollock = 1;
			TLST_FOREACHPTR2 (ptr, gvl->spool, i) {
				docmd (gvl, ptr);
			}
			TLST_RESET (gvl->spool);
			gvl->hasspool=0;
			gvl->spoollock = 0;
			if (!gvl->hasspool2) break;
			TLST_FOREACHPTR2 (ptr, gvl->spool2, i) {
				docmd (gvl, ptr);
			}
			TLST_RESET (gvl->spool2);
		}
		gvl->lock = 0;
		if (!gvl->hasspool && !gvl->hasspool2) break;
	}
	return RERR_OK;
}

static
int
docmd (gvl, cmd)
	struct gvarlst		*gvl;
	struct spoolcmd	*cmd;
{
	int	ret;

	if (!gvl || !cmd) return RERR_PARAM;
	switch (cmd->cmd) {
	case CMD_ADD:
		ret = doaddgvar2 (gvl, &(cmd->gvar));
		if (!RERR_ISOK(ret)) {
			/* drop gvar */
			gvarfree (&(cmd->gvar));
			FRLOGF (LOG_ERR, "error inserting var - drop it: %s",
										rerr_getstr3(ret));
			return ret;
		}
		break;
	case CMD_RM:
		ret = dormgvar2 (gvl, &(cmd->gvar));
		gvarfree (&(cmd->gvar));
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error removing var: %s", rerr_getstr3(ret));
			return ret;
		}
		break;
	case CMD_RPATH:
		/* won't be spooled */
		break;
	}
	return RERR_OK;
}



static
int
creatgvar (gvl, gvar, saveflags)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
	int				*saveflags;
{
	struct gvar	gvar2;
	int			sflags;
	int			ret;

	if (!gvl || !gvar) return RERR_PARAM;
	gvar2 = *gvar;
	sflags = gvar->flags;
#if 0
	gvar->flags = 0;
#endif
	ret = creatgvar2 (gvl, gvar);
	if (!RERR_ISOK(ret)) {
		gvarfree (gvar);
		return ret;
	}
	if (saveflags) *saveflags = gvar->flags;
	if ((sflags & GVAR_F_FREENS) && (gvar->ns == gvar2.ns)) {
		gvar->flags |= GVAR_F_FREENS;
	}
	if ((gvar->flags & GVAR_F_FREEVAR) && (gvar->var == gvar2.var)) {
		gvar->flags |= GVAR_F_FREEVAR;
	}
	if ((gvar->flags & GVAR_F_FREEFULLNAME)
					&& (gvar->fullname == gvar2.fullname)) {
		gvar->flags |= GVAR_F_FREEFULLNAME;
	}
	return RERR_OK;
}

#if 0
static
int
freedupvar (origvar, gvar)
	struct gvar	*origvar, *gvar;
{
	int	flags;

	if (!origvar || !gvar) return RERR_PARAM;
	flags = origvar->flags;
	if ((flags & GVAR_F_FREENS) && origvar->ns != gvar->ns && gvar->ns &&
				origvar->ns) {
		free (origvar->ns);
	}
	if ((flags & GVAR_F_FREEVAR) && origvar->var != gvar->var && gvar->var &&
				origvar->var) {
		free (origvar->var);
	}
	if ((flags & GVAR_F_FREEFULLNAME) && origvar->fullname != gvar->fullname &&
				gvar->fullname && origvar->fullname) {
		free (origvar->fullname);
	}
	return RERR_OK;
}
#endif


static
int
creatgvar2 (gvl, gvar)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
{
	int		ret;
	char		*s, *s2;
	size_t	len;

	if (!gvl || !gvar) return RERR_PARAM;
	if ((gvar->fullname) && !*(gvar->fullname)) {
		if (gvar->flags & GVAR_F_FREEFULLNAME) free (gvar->fullname);
		gvar->fullname = NULL;
		gvar->flags &= ~GVAR_F_FREEFULLNAME;
	}
	if (gvar->ns && !*(gvar->ns)) {
		if (gvar->flags & GVAR_F_FREENS) free (gvar->ns);
		gvar->ns = NULL;
		gvar->flags &= ~GVAR_F_FREENS;
	}
	if (gvar->var && !*(gvar->var)) {
		if (gvar->flags & GVAR_F_FREEVAR) free (gvar->var);
		gvar->var = NULL;
		gvar->flags &= ~GVAR_F_FREEVAR;
	}
	if (!(gvar->fullname)) {
		if (!gvar->var) return RERR_PARAM;
		ret = purifyvar (gvar->var, 0);
		if (!RERR_ISOK(ret)) return ret;
		if (!gvar->ns) {
			gvar->fullname = gvar->var;
			gvar->flags &= ~GVAR_F_FREEFULLNAME;
		} else {
			ret = purifyvar (gvar->ns, PURIFY_F_HAVENS);
			if (!RERR_ISOK(ret)) return ret;
			gvar->fullname = malloc (strlen (gvar->var) + strlen (gvar->ns) + 2);
			if (!gvar->fullname) return RERR_NOMEM;
			sprintf (gvar->fullname, "%s:%s", gvar->ns, gvar->var);
			gvar->flags |= GVAR_F_FREEFULLNAME;
		}
	} else {
		ret = purifyvar (gvar->fullname, PURIFY_F_HAVENS);
		if (!RERR_ISOK(ret)) return ret;
		if (gvar->var) {
			ret = purifyvar (gvar->var, 0);
			if (!RERR_ISOK(ret)) return ret;
		}
		if (gvar->ns) {
			ret = purifyvar (gvar->ns, PURIFY_F_HAVENS);
			if (!RERR_ISOK(ret)) return ret;
		}
	}
	ret = rmrpath (gvl, gvar, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "cannot remove root path from var: %s", 
						rerr_getstr3 (ret));
		return ret;
	}
	s = rindex (gvar->fullname, ':');
	if (s) {
		s++;
		if (!*s || s<gvar->noroot) return RERR_INTERNAL;
	}
	if (!gvar->var) {
		if (s) {
			gvar->var = s;
		} else {
			gvar->var = gvar->fullname;
		}
	}
	if ((!s || s==gvar->noroot)) {
		if (gvar->ns) {
			if (gvar->flags & GVAR_F_FREENS) free (gvar->ns);
			gvar->ns = NULL;
			gvar->flags &= ~GVAR_F_FREENS;
		}
	} else {
		if (!gvar->ns) {
			len = s - gvar->noroot - 1;
			gvar->ns = malloc (len+1);
			if (!gvar->ns) return RERR_NOMEM;
			strncpy (gvar->ns, gvar->noroot, len);
			gvar->ns[len] = 0;
			gvar->flags |= GVAR_F_FREENS;
		} else {
			s2 = gvar->ns + (gvar->noroot - gvar->fullname);
			if (s2 > gvar->ns) {
				if (s2 - gvar->ns > (ssize_t)strlen (gvar->ns)) return RERR_PARAM;
				memmove (gvar->ns, s2, strlen(s2) + 1);
			}
		}
	}
	gvar->refcnt = 1;
	return RERR_OK;
}


static
int
doaddgvar (gvl, gvar)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
{
	int					ret, saveflags;
	struct spoolcmd	cmd;

	if (!gvl || !gvar) return RERR_PARAM;
	ret = creatgvar (gvl, gvar, &saveflags);
	if (!RERR_ISOK(ret)) return ret;
	/* check for lock */
	if (gvl->lock) {
		bzero (&cmd, sizeof (struct spoolcmd));
		cmd.cmd = CMD_ADD;
		cmd.gvar = *gvar;
		ret = spoolcmd (gvl, &cmd);
	} else {
		ret = doaddgvar2 (gvl, gvar);
	}
	if (!RERR_ISOK(ret)) {
		if (gvl->lock) {
			FRLOGF (LOG_ERR, "cannot spool command: %s", rerr_getstr3(ret));
		} else {
			FRLOGF (LOG_ERR, "error inserting var: %s", rerr_getstr3(ret));
		}
		gvar->flags = saveflags;
		gvarfree (gvar);
		return ret;
	}
	return RERR_OK;
}


static
int
doaddgvar2 (gvl, gvar)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
{
	struct gvar	*ptr;
	int			ret;
	unsigned		id;

	/* check wether we have it already and increment refcnt */
	ret = TLST_FIND (id, gvl->list, &gvarcmp, *gvar);	/* we cannot use search here */
	if (ret >= 0) {
		ret = TLST_GETPTR (ptr, gvl->list, id);
		if (!RERR_ISOK(ret)) ptr = NULL;
	} else {
		ptr = NULL;
	}
	if (ptr) {
		ptr->refcnt++;
		if (ptr->typ == GVAR_T_EVVAR && gvar->typ == GVAR_T_EXT
								&& (gvar->flags & GVAR_F_INTEXT)) {
			ptr->arg = gvar->arg;
			ptr->typ |= GVAR_T_EXT;
		} else if (ptr->typ == GVAR_T_EXT && gvar->typ == GVAR_T_EVVAR 
								&& (ptr->flags & GVAR_F_INTEXT)) {
			ptr->id = gvar->id;
			ptr->typ |= GVAR_T_EVVAR;
		}
		gvarfree (gvar);
		return RERR_OK;
	}
	ret = TLST_INSERT (gvl->list, *gvar, &gvarcmp);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}


static
int
purifyvar (var, flags)
	char	*var;
	int	flags;
{
	char	*s, *s2;

	if (!var) return RERR_PARAM;
	if (flags & PURIFY_F_FIRSTCOLON) {
		s = top_skipwhite (var);
		if (*s == ':') {
			s2 = top_skipwhiteplus (s+1, ":");
			if (!index (s2, ':')) {
				*var = ':';
				s = var+1;
			} else {
				s = var;
			}
		} else {
			s2 = s;
			s = var;
		}
	} else {
		s2 = top_skipwhiteplus (var, ":");
		s = var;
	}
	if (s2 > s) {
		memmove (s, s2, strlen(s2)+1);
	}
	/* first char is special */
	if (*s >= 'A' && *s <= 'Z') {
		*s += 'a' - 'A';
	} else if (!((*s >= 'a' && *s <= 'z') || *s == '_')) {
		return RERR_INVALID_VAR;
	}
	for (s2 = ++s; *s2; s++, s2++) {
		if (iswhite (*s2)) {
			for (; iswhite (*s2); s2++);
			if (*s2) return RERR_INVALID_VAR;
			break;
		} else if (*s2 >= 'A' && *s2 <= 'Z') {
			*s = *s2 - 'A' + 'a';
		} else if (*s2 == '_') {
			for (s2=s; *s=='_'; s2++);
			s--; s2--;
		} else if ((*s2 >= '0' && *s2 <= '9') || (*s2 >= 'a' && *s2 <= 'z')) {
			*s = *s2;
		} else if (*s2 == ':') {
			if (!(flags & PURIFY_F_HAVENS)) return RERR_INVALID_VAR;
			*s = *s2;
			for (; *s2 == ':'; s2++);
			s++;
			/* first char after colon is special */
			if (*s2 >= 'A' && *s2 <= 'Z') {
				*s = *s2 - 'A' + 'a';
			} else if ((*s2 >= 'a' && *s2 <= 'z') || *s2 == '_') {
				*s = *s2;
			} else if (iswhite (*s2)) {
				s2 = top_skipwhite (s2);
				if (!s2 || !*s2) break;
				return RERR_INVALID_VAR;
			} else if (!*s2) {
				break;
			} else {
				return RERR_INVALID_VAR;
			}
		} else {
			return RERR_INVALID_VAR;
		}
	}
	*s = 0;
	if (!*var) return RERR_INVALID_VAR;
	return RERR_OK;
}

static
int
rmrpath (gvl, gvar, flags)
	struct gvarlst	*gvl;
	struct gvar		*gvar;
	int				flags;
{
	struct rootpath	*rpath;
	int					i;
	char					*s;

	if (!gvl || !gvar || !gvar->fullname) return RERR_PARAM;
	gvar->noroot = NULL;
	TLST_FOREACHPTR2 (rpath, gvl->rpathlst, i) {
		if (!strncasecmp (gvar->fullname, rpath->rpath, rpath->len)) {
			s = gvar->fullname+rpath->len;
			if (*s == ':') s++;	/* due to purify, there can be at most one : */
			if (!*s) continue;
			gvar->noroot = s;
			break;
		}
	}
	if (!gvar->noroot) gvar->noroot = gvar->fullname;
	return RERR_OK;
}

static
int
doinit ()
{
	int	ret;

	CF_MAYREAD;
	if (isinit) return RERR_OK;
	ret = TLST_NEW (gvarlst, struct gvarlst);
	if (!RERR_ISOK(ret)) return ret;
	isinit = 1;		/* hack to avoid loop */
	ret = gvl_new ();
	isinit = 0;
	if (!RERR_ISOK(ret)) {
		TLST_FREE (gvarlst);
		return ret;
	}
	if (ret > 0) {
		gvl_free (ret);
		FRLOGF (LOG_ERR, "created id %d (should be zero)", ret);
		TLST_FREE (gvarlst);
		return RERR_INTERNAL;
	}
	isinit = 1;
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
