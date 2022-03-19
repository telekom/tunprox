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

#include <fr/base.h>

#include "parseevent.h"
#include "evbox.h"
#include "evpool.h"
#include "varlist.h"
#include "varmon.h"
#include "eddi.h"


struct varmon {
	void				*exist;
	int				id;	/* self */
	int				varlst;
	int				evbox;
	int				initbox;
	char				*name;
	int				flags;
	int				sendchgev;
	struct event	chgev;
	int				lock;
	char				*senderid;
	int				waitinit;
	tmo_t				waitinittime;
};

struct vmgrp {
	void			*exist;
	int			self;
	char			*grpname;
	char			*senderid;
	struct tlst	grp;
	struct tlst	gvl;
};

static int doinit ();
static int isinit = 0;
static int read_config ();
static int config_read = 0;

static int doevelab (struct varmon*, struct event*, int);
static int doelabget (struct varmon*, struct event*, const char*, int);
//static int callback (int, void*);
static int dopoll (struct varmon*, tmo_t, int);
static int dochgevent (struct varmon*);
static int doclose (struct varmon*);
static int donew (struct varmon*, const char*, int);
static int dogrpadd (struct vmgrp*, const char*, int);
static int doaddgrp (struct vmgrp*, char*, int);
static int dovarmonnew (const char*, char*, int);
static int dogetlist (struct varmon*);
static int dogrpgetall2 (char*, const char*);
static int dogetlist2 (const char*, const char*);

static struct tlst	vmlist;
static struct tlst	vmgrp;


#define DOINIT \
		if (!isinit) { \
			int	_ret; \
			_ret = doinit (); \
			if (!RERR_ISOK(_ret)) { \
				FRLOGF (LOG_ERR, "error initializing varmon list: %s", \
							rerr_getstr3(_ret)); \
				return _ret; \
			} \
		}



int
varmon_new (name, flags)
	const char	*name;
	int			flags;
{
	struct varmon	varmon, *ptr;
	int				ret, id;

	DOINIT;
	if (!name) {
		name = (char*)(void*)"varmon";
		flags = 0;
	}
	bzero (&varmon, sizeof (struct varmon));
	varmon.exist = &varmon;	/* any but NULL */
	cf_begin_read ();
	ret = donew (&varmon, name, flags);
	cf_end_read ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating new monitor >>%s<<: %s", 
					name, rerr_getstr3(ret));
		doclose (&varmon);
		return ret;
	}
	ret = TLST_ADDINSERT (vmlist, varmon);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting varmon into list: %s", 
					rerr_getstr3(ret));
		doclose (&varmon);
		return ret;
	}
	id = ret;
	ret = TLST_GETPTR (ptr, vmlist, id);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "cannot find varmon id: %s", rerr_getstr3(ret));
		return ret;
	}
	ptr->id = id;
#if 0 	/* disable callback for now */
	if ((flags & VARM_F_CB) && /* !ispublic*/ (ptr->varlst != 0)) {
		ret = evbox_callback (ptr->evbox, callback, ptr);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error installing callback function: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
#endif
	return id;
}


int
varmon_search (name)
	const char	*name;
{
	unsigned			i;
	struct varmon	*vm;

	if (!name) return RERR_PARAM;
	TLST_FOREACHPTR2 (vm, vmlist, i) {
		if (!vm || !vm->exist) continue;
		if (strcasecmp (vm->name, name) != 0) continue;
		return vm->id;
	}
	return RERR_NOT_FOUND;
}


int
varmon_close (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	ret = evvar_writefile (varmon->varlst, NULL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error writing varlist to file: %s", 
						rerr_getstr3(ret));
	}
	doclose (varmon);
	ret = TLST_REMOVE (vmlist, id, 0);
	if (!RERR_ISOK(ret)) {
		bzero (varmon, sizeof (struct varmon));
		vmlist.num--;
	}
	return RERR_OK;
}

int
varmon_setactshadow (id, shid)
	int	id, shid;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	return evvar_setactshadow (varmon->varlst, shid);
}


int
varmon_addgrp (grpname, flags)
	const char	*grpname;
	int			flags;
{
	return varmongrp_new (grpname ? grpname : "varmon", flags | VARM_F_ADDEMPTY);
}



int
varmon_closeall ()
{
	struct varmon	*ptr;
	int				i;
	int				ret, ret2=RERR_OK;

	DOINIT;
	varmongrp_closeall ();
	TLST_FOREACHPTR2 (ptr, vmlist, i) {
		if (!(ptr && ptr->exist)) continue;
		ret = varmon_close (i);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}

int
varmon_evelab (id, ev, flags)
	int				id, flags;
	struct event	*ev;
{
	struct varmon	*varmon;
	int				ret;

	if (!ev) return RERR_PARAM;
	DOINIT;
	FRLOGF (LOG_DEBUG, "elab event (name==>>%s<<", ev->name?ev->name:"<NULL>");
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	return doevelab (varmon, ev, flags);
}


int
varmon_getlist (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	return varmon->varlst;
}

int
varmon_isinit (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	return !varmon->waitinit;
}

int
varmon_poll (id, tout)
	int	id;
	tmo_t	tout;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	return dopoll (varmon, tout, 0);
}

int
varmon_pollall (tout)
	tmo_t	tout;
{
	struct varmon	*ptr;
	int				i;
	int				ret, ret2=RERR_OK;
	tmo_t				now, start;

	DOINIT;
	start = tmo_now ();
	TLST_FOREACHPTR2 (ptr, vmlist, i) {
		if (!(ptr && ptr->exist)) continue;
		ret = varmon_poll (i, tout);
		if (!RERR_ISOK(ret)) ret2 = ret;
		if (tout > 0) {
			now = tmo_now ();
			tout -= (now - start);
			if (tout < 0) tout = 0;
			start = now;
		}
	}
	return ret2;
}

#if 0
int
varmon_instcb (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	ret = evbox_callback (varmon->evbox, callback, varmon);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error installing callback function: %s",
					rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

int
varmon_rmcb (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!(varmon->exist)) return RERR_NOT_FOUND;
	return evbox_callback (varmon->evbox, NULL, NULL);
}
#endif

int
varmon_addgvl (id, gid)
	int	id, gid;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!varmon || !(varmon->exist)) return RERR_NOT_FOUND;
	return evvar_addgvl (varmon->varlst, gid);
}

int
varmon_rmgvl (id, gid)
	int	id, gid;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!varmon || !(varmon->exist)) return RERR_NOT_FOUND;
	return evvar_rmgvl (varmon->varlst, gid);
}

int
varmon_lock (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!varmon || !(varmon->exist)) return RERR_NOT_FOUND;
	varmon->lock = 1;
	return RERR_OK;
}

int
varmon_unlock (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!varmon || !(varmon->exist)) return RERR_NOT_FOUND;
	varmon->lock = 0;
	return RERR_OK;
}

int
varmon_getall (id)
	int	id;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!varmon || !(varmon->exist)) return RERR_NOT_FOUND;
	return dogetlist (varmon);
}

int
varmon_getall2 (name, requester)
	const char	*name, *requester;
{
	return dogetlist2 (name, requester);
}

int
varmon_setsenderid (id, senderid)
	int			id;
	const char	*senderid;
{
	struct varmon	*varmon;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (varmon, vmlist, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!varmon || !(varmon->exist)) return RERR_NOT_FOUND;
	if (varmon->senderid) free (varmon->senderid);
	varmon->senderid = NULL;
	if (senderid) {
		varmon->senderid = strdup (senderid);
		if (!varmon->senderid) return RERR_NOMEM;
	}
	return RERR_OK;
}



/* ***********************
 * varmon group functions
 * ***********************/

int
varmongrp_new (grpname, flags)
	const char	*grpname;
	int			flags;
{
	int			ret;
	const char	*cgrp;
	char			*grp=NULL;

	DOINIT;
	if (grpname) {
		cf_begin_read ();
		cgrp = cf_getarr ("varmongroup", grpname);
		if (cgrp) cgrp = top_skipwhiteplus (grp, ",");
		if (cgrp && !*cgrp) cgrp=NULL;
		if (cgrp) {
			grp = strdup (cgrp);
			if (!grp) {
				cf_end_read ();
				return RERR_NOMEM;
			}
		}
		cf_end_read ();
	}
	ret = dovarmonnew (grpname, grp, flags);
	if (grp) free (grp);
	return ret;
}

int
varmongrp_getall2 (grpname, requester)
	const char	*grpname, *requester;
{
	int			ret;
	const char	*cgrp;
	char			*grp;

	DOINIT;
	if (!grpname) return RERR_PARAM;
	cf_begin_read ();
	cgrp = cf_getarr ("varmongroup", grpname);
	if (!cgrp) {
		cf_end_read ();
		return RERR_NOT_FOUND;
	}
	cgrp = top_skipwhiteplus (cgrp, ",");
	if (!cgrp || !*cgrp) {
		cf_end_read ();
		return RERR_OK;
	}
	grp = strdup (cgrp);
	if (!grp) {
		cf_end_read ();
		return RERR_NOMEM;
	}
	cf_end_read ();
	ret = dogrpgetall2 (grp, requester);
	free (grp);
	return ret;
}



int
varmongrp_close (id)
	int	id;
{
	struct vmgrp	grp;
	int				ret, vmid, i;

	DOINIT;
	ret = TLST_GET (grp, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!grp.exist) return RERR_NOT_FOUND;
	ret = TLST_REMOVE (vmgrp, id, 0);
	if (!RERR_ISOK(ret)) return ret;
	TLST_FOREACH2 (vmid, grp.grp, i) {
		ret = varmon_close (vmid);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "cannot remove varmon (%d): %s", vmid, 
					rerr_getstr3(ret));
		}
	}
	TLST_FREE (grp.grp);
	if (grp.grpname) free (grp.grpname);
	return RERR_OK;
}

int
varmongrp_closeall ()
{
	struct vmgrp	*ptr;
	int				i;
	int				ret, ret2=RERR_OK;

	DOINIT;
	TLST_FOREACHPTR2 (ptr, vmgrp, i) {
		if (!ptr->exist) continue;
		ret = varmongrp_close (ptr->self);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "cannot close varmon group %d: %s", ptr->self, 
						rerr_getstr3(ret));
			ret2 = ret;
		}
	}
	return ret2;
}

int
varmongrp_setactshadow (id, shid)
	int	id, shid;
{
	struct vmgrp	*ptr;
	int				i, ret, ret2=RERR_OK;
	int				vmid;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_setactshadow (vmid, shid);
		if (!RERR_ISOK(ret) && ret != RERR_NOT_FOUND) ret2 = ret;
	}
	return ret2;
}

int
varmongrp_isinit (id)
	int	id;
{
	struct vmgrp	*ptr;
	int				i, ret;
	int				vmid;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_isinit (vmid);
		if (!RERR_ISOK(ret)) return ret;
		if (!ret) return 0;
	}
	return 1;
}

int
varmongrp_addgvl (id, gid)
	int	id, gid;
{
	struct vmgrp	*ptr;
	int				i, ret, ret2=RERR_OK;
	int				vmid;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	ret = TLST_HASINT (ptr->gvl, gid);
	if (ret != RERR_NOT_FOUND) return ret;
	ret = TLST_ADD (ptr->gvl, gid);
	if (!RERR_ISOK(ret)) return ret;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_addgvl (vmid, gid);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error adding gvl (%d) to varmon %d: %s",
						gid, vmid, rerr_getstr3(ret));
			ret2 = ret;
		}
	}
	return ret2;
}

int
varmongrp_rmgvl (id, gid)
	int	id, gid;
{
	struct vmgrp	*ptr;
	int				i, ret, ret2=RERR_OK;
	int				vmid;
	unsigned			idx;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	ret = TLST_FINDINT (idx, ptr->gvl, gid);
	if (ret > 0) {
		ret = TLST_REMOVE (ptr->gvl, idx, TLST_F_CPYLAST);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_rmgvl (vmid, gid);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error removing gvl (%d) from varmon %d: %s",
						gid, vmid, rerr_getstr3(ret));
			ret2 = ret;
		}
	}
	return ret2;
}

int
varmongrp_add (id, vmon, flags)
	int			id, flags;
	const char	*vmon;
{
	struct vmgrp	*ptr;
	int				ret;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	return dogrpadd (ptr, vmon, flags);
}

int
varmongrp_list2name (name, id, lstid)
	char	**name;
	int	id, lstid;
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	struct varmon	*vmon;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = TLST_GETPTR (vmon, vmlist, vmid);
		if (!RERR_ISOK(ret) || !vmon || !(vmon->exist)) continue;
		if (vmon->varlst == lstid) {
			if (name) *name = vmon->name;
			return RERR_OK;
		}
	}
	return RERR_NOT_FOUND;
}

int
varmongrp_name2list (listid, id, name)
	int			*listid;
	int			id;
	const char	*name;
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	struct varmon	*vmon;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = TLST_GETPTR (vmon, vmlist, vmid);
		if (!RERR_ISOK(ret) || !vmon || !(vmon->exist)) continue;
		if (!strcasecmp (vmon->name, name)) {
			if (listid) *listid = vmon->varlst;
			return RERR_OK;
		}
	}
	return RERR_NOT_FOUND;
}

int
varmongrp_getvmonid (
	int			id,
	const char	*name)
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	struct varmon	*vmon;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = TLST_GETPTR (vmon, vmlist, vmid);
		if (!RERR_ISOK(ret) || !vmon || !(vmon->exist)) continue;
		if (!strcasecmp (vmon->name, name)) {
			return vmid;
		}
	}
	return RERR_NOT_FOUND;
}


int
varmongrp_lock (id)
	int	id;
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	int				ret2 = RERR_OK;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_lock (vmid);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}


int
varmongrp_unlock (id)
	int	id;
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	int				ret2 = RERR_OK;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_unlock (vmid);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}

int
varmongrp_getall (id)
	int	id;
{
	return varmongrp_getlist (id);
}

int
varmongrp_getlist (id)
	int	id;
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	int				ret2 = RERR_OK;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_getall (vmid);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}

int
varmongrp_setsenderid (id, senderid)
	int			id;
	const char	*senderid;
{
	struct vmgrp	*ptr;
	int				ret, vmid, i;
	int				ret2 = RERR_OK;

	DOINIT;
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) return ret;
	if (!ptr || !ptr->exist) return RERR_NOT_FOUND;
	if (ptr->senderid) free (ptr->senderid);
	ptr->senderid = NULL;
	if (senderid) {
		ptr->senderid = strdup (senderid);
		if (!ptr->senderid) return RERR_NOMEM;
	}
	TLST_FOREACH2 (vmid, ptr->grp, i) {
		ret = varmon_getlist (vmid);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}


/* ********************
 * static functions
 * ********************/

static
int
dogrpgetall2 (grp, requester)
	char			*grp;
	const char	*requester;
{
	char	*name;
	int	ret, ret2=RERR_OK, num=0;

	if (!grp) return RERR_PARAM;
	while ((name=top_getfield (&grp, ",", 0))) {
		ret = dogetlist2 (name, requester);
		if (!RERR_ISOK(ret)) {
			ret2 = ret;
		} else {
			num++;
		}
	}
	if (!RERR_ISOK(ret2)) return ret2;
	return num;
}

static
int
dogetlist (vmon)
	struct varmon	*vmon;
{
	int	ret;

	if (!vmon || !vmon->exist) return RERR_PARAM;
	vmon->waitinit = 0;
	ret = dogetlist2 (vmon->name, vmon->senderid);
	if (!RERR_ISOK(ret)) return ret;
	vmon->waitinit = 1;
	vmon->waitinittime = tmo_now();
	return RERR_OK;
}

static
int
dogetlist2 (name, requester)
	const char	*name, *requester;
{
	struct event	*ev;
	int				ret;

	if (!name) return RERR_PARAM;
	ret = evpool_acquire (&ev);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_setnamef (ev, "varmon/%s", name);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev);
		return ret;
	}
	ret = ev_setarg_s (ev, "getall", 0, 0);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev);
		return ret;
	}
	if (requester) {
		ret = ev_addattr_s (ev, "requester", (char*)(void*)requester, 0);
		if (!RERR_ISOK(ret)) return ret;
	}
	ret = eddi_sendev (ev);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev);
		FRLOGF (LOG_WARN, "error sending getall event for varmon >>%s<<: %s",
						name, rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



static
int
dovarmonnew (grpname, grpstr, flags)
	const char	*grpname;
	char			*grpstr;
	int			flags;
{
	int				ret, id;
	struct vmgrp	vgrp, *ptr;

	DOINIT;
	bzero (&vgrp, sizeof (struct vmgrp));
	vgrp.exist = &vgrp;	/* any but null */
	ret = TLST_NEW (vgrp.grp, int);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (vgrp.gvl, int);
	if (!RERR_ISOK(ret)) return ret;
	if (grpname) {
		vgrp.grpname = strdup (grpname);
		if (!vgrp.grpname) {
			return RERR_NOMEM;
		}
	}
	id = ret = TLST_ADDINSERT (vmgrp, vgrp);
	if (!RERR_ISOK(ret)) {
		if (vgrp.grpname) free (vgrp.grpname);
		return ret;
	}
	ret = TLST_GETPTR (ptr, vmgrp, id);
	if (!RERR_ISOK(ret)) {
		return ret;
	}
	ptr->self = id;
	ret = doaddgrp (ptr, grpstr, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error adding group of varmons: %s", rerr_getstr3(ret));
		return ret;
	}
	return id;
}

static
int
doaddgrp (grp, grpstr, flags)
	struct vmgrp	*grp;
	char				*grpstr;
	int				flags;
{
	char	*field;
	int	ret, ret2=RERR_OK;

	if (!grp) return RERR_PARAM;
	if (!grpstr && (flags & VARM_F_ADDEMPTY)) return dogrpadd (grp, NULL, flags);
	while ((field = top_getfield (&grpstr, ",", 0))) {
		ret = dogrpadd (grp, field, flags);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error creating varmon >>%s<<: %s", field,
						rerr_getstr3(ret));
			ret2 = ret;
		}
	}
	return ret2;
}

static
int
dogrpadd (grp, vmname, flags)	
	struct vmgrp	*grp;
	const char		*vmname;
	int				flags;
{
	int	id, ret, gid, i;

	if (!grp) return RERR_PARAM;
	id = ret = varmon_new (vmname, flags);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_ADD (grp->grp, id);
	if (!RERR_ISOK(ret)) return ret;
	TLST_FOREACH2 (gid, grp->gvl, i) {
		ret = varmon_addgvl (id, gid);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "cannot set gvl (%d) to varmon (%d): %s", 
						gid, id, rerr_getstr3(ret));
		}
	}
	return id;
}


static
int
donew (varmon, name, flags)
	struct varmon	*varmon;
	const char		*name;
	int				flags;
{
	int			ret;
	const char	*fname, *chgev;
	char			*schgev;
	int			ispublic, i;
	char			*field;
	char			*buf, _buf[256];

	if (!varmon || !name) return RERR_PARAM;
	varmon->name = strdup (name);
	if (!varmon->name) return RERR_NOMEM;
	varmon->flags = flags;
	if (flags & VARM_F_LOCKED) varmon->lock = 1;
	varmon->evbox = -1;
	varmon->initbox = -1;
	ispublic = cf_isyes (cf_getvarf2 ("no", "varmon[%s]/public", name));
	if (!(flags & VARM_F_LISTEN)) {
		fname = cf_getvarf ("varmon[%s]/savefile", name);
	} else {
		fname = NULL;
	}
	chgev = cf_getvarf ("varmon[%s]/changeevent", name);
	if (!chgev) chgev = cf_getvarf ("varmon[%s]/changevent", name);
	if (!chgev) chgev = cf_getvarf ("varmon[%s]/chgev", name);
	if (chgev && !(flags & VARM_F_LISTEN)) {
		ret = ev_new (&(varmon->chgev));
		if (!RERR_ISOK(ret)) return ret;
		varmon->sendchgev = 1;
	} else {
		varmon->sendchgev = 0;
	}
	if (ispublic) {
		varmon->varlst = 0;
	} else {
		ret = evvar_newlist ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error creating new variable list: %s",
								rerr_getstr3(ret));
			return ret;
		}
		varmon->varlst = ret;
	}
	if (fname) {
		ret = evvar_readfile (varmon->varlst, fname, EVVAR_F_SILENT);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "error reading file (%s): %s", fname, 
								rerr_getstr3(ret));
		}
		ret = evvar_setautowrite (varmon->varlst, fname, EVP_F_CPY);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error setting autowrite: %s", rerr_getstr3(ret));
			return ret;
		}
	}
	ret = evbox_init (EVBOX_F_GETGLOBAL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARNING, "cannot initialize event box: %s",
							rerr_getstr3(ret));
	}
	ret = evbox_get ((char*)(void*)name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting event box: %s", rerr_getstr3(ret));
		return ret;
	}
	varmon->evbox = ret;
	buf = a2sprtf (_buf, sizeof (_buf), "%s_init", name);
	if (!buf) return RERR_PARAM;
	ret = evbox_get (buf);
	if (buf != _buf) free (buf);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting init event box: %s", rerr_getstr3(ret));
		return ret;
	}
	varmon->initbox = ret;

	if (chgev) once {
		chgev = top_skipwhiteplus (chgev, ":");
		if (!chgev || !*chgev) break;
		schgev = strdup (chgev);
		if (!schgev) return RERR_NOMEM;
		field = top_getfield (&schgev, ":", 0);
		ret = ev_setname (&(varmon->chgev), field, EVP_F_FREE);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error creating change event: %s",
									rerr_getstr3(ret));
			free (schgev);
			return ret;
		}
		/* NOTE: the name must not be changed any more, without deleting 
			the arguments */
		i=0;
		while ((field = top_getfield (&schgev, ":", 0))) {
			ret = ev_setarg (&(varmon->chgev), field, i++, 0);
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error creating change event: %s",
										rerr_getstr3(ret));
				return ret;
			}
		}
	}
	ret = evvar_setvarmon (varmon->varlst, varmon->name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error setting varmon name to varlist: %s",
						rerr_getstr3(ret));
	}
#if 0	/* this is evil, because the varmon pointer is only temporary here */
	if ((flags & VARM_F_CB) && !ispublic) {
		ret = evbox_callback (varmon->evbox, callback, varmon);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error installing callback function: %s",
						rerr_getstr3(ret));
			return ret;
		}
	}
#endif
	return RERR_OK;
}

static
int
doclose (varmon)
	struct varmon	*varmon;
{
	if (!varmon) return RERR_PARAM;
	if (varmon->varlst > 0) evvar_rmlist (varmon->varlst);
	if (varmon->evbox > 0) evbox_close (varmon->evbox);
	if (varmon->initbox > 0) evbox_close (varmon->initbox);
	if (varmon->name) free (varmon->name);
	varmon->name = NULL;
	if (varmon->sendchgev) ev_free (&(varmon->chgev));
	bzero (varmon, sizeof (struct varmon));
	varmon->exist = varmon;
	return RERR_OK;
}


static
int
dopoll (varmon, tout, nowait)
	struct varmon	*varmon;
	tmo_t				tout;
	int				nowait;
{
	int				ret;
	struct event	*ev;
	int				evbox;
	tmo_t				now;

	if (!varmon) return RERR_PARAM;
	if (varmon->waitinit < 0) {
		return dogetlist (varmon);
	} 
	if (varmon->waitinit) {
		evbox = varmon->initbox;
	} else {
		while (RERR_ISOK(evbox_pop (&ev, varmon->initbox))) {
			evpool_release (ev);
		}
		evbox = varmon->evbox;
	}
	if (!nowait) {
		ret = evbox_wait (evbox, tout);
		if (!RERR_ISOK(ret)) return ret;
	}
	while (1) {
		ret = evbox_pop (&ev, evbox);
		if (ret == RERR_NOT_FOUND) break;
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error popping event: %s", rerr_getstr3(ret));
			return ret;
		}
		ret = doevelab (varmon, ev, 0);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "error elaborating event: %s", rerr_getstr3(ret));
		}
		ret = evpool_release (ev);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARNING, "error releasing event: %s", rerr_getstr3(ret));
		}
	}
	if (varmon->waitinit) {
		now = tmo_now ();
		if (now - varmon->waitinittime > 1000000LL) {
			return dogetlist (varmon);
		}
	}
	return RERR_OK;
}


#if 0
static
int
callback (box, ptr)
	int	box;
	void	*ptr;
{
	struct varmon	*varmon;

	if (!ptr) return RERR_PARAM;
	varmon = (struct varmon*)ptr;
	if (box != varmon->evbox) return RERR_PARAM;
	return dopoll (varmon, 0, 1);
}
#endif


static
int
doevelab (varmon, ev, flags)
	struct varmon	*varmon;
	struct event	*ev;
	int				flags;
{
	int	ret, chg=0;
	char	*cmd;
#ifdef DEBUG
	char	*wrout = NULL;
#endif

	if (!ev || !varmon) return RERR_PARAM;
	ret = ev_getarg (&cmd, ev, 0);
	if (ret == RERR_NOT_FOUND) return RERR_OK;
	if (!RERR_ISOK(ret)) return ret;
	if (!cmd || !*cmd) return RERR_OK;
	if (varmon->lock) return RERR_OK;
#ifdef DEBUG
	ret = ev_create (&wrout, ev, EVP_F_NOPRTVAR);
	if (RERR_ISOK(ret) && wrout) {
		FRLOGF (LOG_DEBUG, "elab event: >>%s<<", wrout);
		free (wrout);
	} else {
		FRLOGF (LOG_DEBUG, "elab command >>%s<<", cmd);
	}
#else
	FRLOGF (LOG_DEBUG, "elab command >>%s<<", cmd);
#endif
	sswitch (cmd) {
	sincase ("get")
		ret = doelabget (varmon, ev, cmd, flags);
		break;
	sicase ("allvars")
		if (!(varmon->flags & VARM_F_LISTEN)) break;
		varmon->waitinit = 0;
		/* fall thru */
	sicase ("set")
	sincase ("setvar")
	sicase ("publish")
		ret = evvar_setevent (varmon->varlst, ev, flags);
		if (!RERR_ISOK(ret) && !strcasecmp (cmd, "allvars")) {
			varmon->waitinit = ret;
		}
		if (ret > 0) chg=1;
		break;
	sincase ("rmvar")
	sicase ("remove")
		ret = evvar_rmvarlist (varmon->varlst, ev);
		break;
	sicase ("clearlist")
		ret = evvar_clearlist (varmon->varlst);
		break;
	sdefault
		return RERR_OK;
	} esac;
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error executing command >>%s<<: %s", cmd,
						rerr_getstr3(ret));
		return ret;
	}
	if (chg) {
		FRLOGF (LOG_DEBUG, "variables have changed");
		ret = dochgevent (varmon);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_NOTICE, "could not send change event: %s",
						rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}


static
int
doelabget (varmon, ev, cmd, flags)
	struct varmon	*varmon;
	struct event	*ev;
	const char		*cmd;
	int				flags;
{
	int				ret;
	struct event	*oev;
	const char		*req;

	if (!varmon || !ev || !cmd) return RERR_PARAM;
	if (varmon->flags & VARM_F_LISTEN) return RERR_OK;
	ret = evpool_acquire (&oev);
	if (!RERR_ISOK(ret)) return ret;
	ev_clear (oev);
	ret = ev_getattr_s (&req, ev, "requester");
	if (RERR_ISOK(ret)) {
		ret = ev_addattr_s (oev, "requester", req, EVP_F_CPYVAL);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error adding requester to out event: %s",
						rerr_getstr3(ret));
		}
	}
	once {
		ret = ev_addattr_s (oev, "varmon", varmon->name, 0);
		if (!RERR_ISOK(ret)) break;
		ret = ev_setnamef (oev, "varmon/%s", varmon->name);
		if (!RERR_ISOK(ret)) break;
		sswitch (cmd) {
		sincase ("getvar")
		sicase ("get")
			ret = ev_setarg_s (oev, "vars", 0, 0);
			if (!RERR_ISOK(ret)) break;
			ret = evvar_getvarlist (oev, varmon->varlst, ev, flags);
			break;
		sincase ("getall")
			ret = ev_setarg_s (oev, "allvars", 0, 0);
			if (!RERR_ISOK(ret)) break;
			ret = evvar_getallvar (oev, varmon->varlst, flags);
			break;
		sdefault
			evpool_release (oev);
			return RERR_OK;
		} esac;
	}
	if (!RERR_ISOK(ret)) {
		evpool_release (oev);
		FRLOGF (LOG_ERR, "error creating answer event: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = eddi_sendev (oev);
	if (!RERR_ISOK(ret)) {
		evpool_release (oev);
		FRLOGF (LOG_ERR, "error sending answer event: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}



static
int
dochgevent (varmon)
	struct varmon	*varmon;
{
	struct event	*ev;
	int				ret;

	if (!varmon) return RERR_PARAM;
	if (!varmon->sendchgev) return RERR_OK;
	ret = evpool_acquire (&ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "cannot acquire event from pool: %s", 
							rerr_getstr3(ret));
		return ret;
	}
	ret = ev_copy (ev, &(varmon->chgev));
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error copying event: %s", rerr_getstr3(ret));
		evpool_release (ev);
		return ret;
	}
	ret = eddi_sendev (ev);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev);
		FRLOGF (LOG_ERR, "error sending change event: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}







static
int
doinit ()
{
	int	ret;

	CF_MAYREAD;
	if (isinit) return RERR_OK;
	ret = TLST_NEW (vmlist, struct varmon);
	if (!RERR_ISOK(ret)) return ret;
	ret = TLST_NEW (vmgrp, struct vmgrp);
	if (!RERR_ISOK(ret)) return ret;
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
