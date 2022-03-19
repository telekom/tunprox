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
 * Portions created by the Initial Developer are Copyright (C) 2003-2019
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */


#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <pthread.h>
#include <errno.h>
#include <fcntl.h>          
#include <unistd.h>

#include <fr/base.h>
#include <fr/thread.h>

#include "evbox.h"
#include "parseevent.h"
#include "evpool.h"
#include "eddi.h"
#include "evfilter.h"



struct eventry {
	struct event	*event;
	pthread_t		tid;
};


struct evbox {
	char							*name;
	int							noglobtrigger;
	int							id;
	struct eventry				*list;
	int							listsz;
	int							first, next;
	int							(*trig_cb) (int, void*, int);
	void							*trig_parg;
	int							trig_iarg;
	int							trig_fd[2];
	pthread_mutex_t			mutex;
	FRTHR_COND_T(int,trig)	cond;
};

#define EVBOX_INIT ((struct evbox) { \
	.id = -1, \
	.mutex = PTHREAD_MUTEX_INITIALIZER, \
	.cond = FRTHR_COND_INIT, \
})

struct searchev {
	const char		*name;
	struct evbox	*box;
};

struct boxlist {
	struct tlst					boxlist;			/* id <-> box mapping */
	struct tlst					searchlist;		/* search for box name */
	struct evbox_eddinfo		eddinfo;
	pthread_mutex_t			mutex;
	FRTHR_COND_T(int,trig)	cond;
	FRTHR_COND_T(int,ref)	ref;				/* refcounter */
	int							tobereleased;
	int							(*trig_cb) (int, void*, int);
	void							*trig_parg;
	int							trig_iarg;
	int							trig_fd[2];
	struct boxlist				*next, *prev;
};

#define BOXLIST_INIT	((struct boxlist) { \
	.boxlist = TLST_SINIT_T (struct evbox*), \
	.searchlist = TLST_SINIT_T (struct searchev), \
	.mutex = PTHREAD_MUTEX_INITIALIZER, \
	.cond = FRTHR_COND_INIT, \
	.ref = FRTHR_COND_INIT, \
	.eddinfo = { \
			.filterid = -1, \
			.distgrp = TLST_SINIT_T(char*) \
	}, \
})


static __thread struct boxlist *thr_boxlist = NULL;
static struct boxlist *boxlist_head = NULL;
static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;
#define G_LOCK	do { pthread_mutex_lock (&mutex); } while (0)
#define G_UNLOCK do { pthread_mutex_unlock (&mutex); } while (0)

static
struct boxlist *
_getthrboxlist() { return thr_boxlist; }


static int	c_szevqueue = 50;
static int	c_maxevqsz = 50;
static int	config_read = 0;
static int read_config ();



static int boxlist_add (struct evbox*);
static int boxlist_rm (int);
static int boxlist_get (struct evbox**, int);
static int boxlist_finish();
static int searchlist_add (struct evbox*);
static int searchlist_search (struct evbox**, const char *);
static int searchlist_searchlock (struct evbox**, const char *, struct boxlist*);
static int searchlist_rm (char*);
static int box_get (const char*);
static int box_init (struct evbox*);
static int box_destroy (struct evbox*);
static int box_insert (struct evbox*);
static int doinsert (struct event*, const char *boxname, struct boxlist*);
static int doboxinsert (struct evbox*, void*);
static int dobox_getnum (struct evbox*);
static int box_lock (struct evbox*);
static int box_unlock (struct evbox*);
static int box_pop (struct event**, pthread_t *, struct evbox*);
static int box_popstr (char **, struct evbox*);
static int dofilterinsert (struct event*, struct boxlist*);
static struct boxlist *boxlist_getreceiver (pthread_t);
static void boxlist_releasereceiver (struct boxlist*);


#define INITME	do { if (!_getthrboxlist()) evbox_init(0); } while (0)
//#define INITME	
#define AM_I_EDDI (_getthrboxlist() && (_getthrboxlist()->eddinfo.flags & EVBOX_F_IAMEDDI))


int
evbox_init (flags)
	int	flags;
{
	int	ret;
	struct boxlist	*boxlist;

	FRLOGF (LOG_DEBUG, "call evbox_init for (%s.%d)", frthr_self?frthr_self->name:"thrd",
				frthr_self?frthr_self->id:-1);
	if (thr_boxlist) return RERR_OK;
	boxlist = malloc (sizeof (struct boxlist));
	if (!boxlist) return RERR_NOMEM;
	*boxlist = BOXLIST_INIT;
	boxlist->eddinfo.flags = flags;
	boxlist->eddinfo.owner = pthread_self ();

	G_LOCK;
	if (!boxlist_head) {
		boxlist->next = boxlist->prev = boxlist;
		boxlist_head = boxlist;
	} else {
		boxlist->next = boxlist_head;
		boxlist->prev = boxlist_head->prev;
		boxlist_head->prev->next = boxlist;
		boxlist_head->prev = boxlist;
	}
	G_UNLOCK;
	thr_boxlist = boxlist;
	ret = evbox_get (NULL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error creating accept box: %s", rerr_getstr3(ret));
	}
	return RERR_OK;
}

int
evbox_finish ()
{
	int					ret;

	if (!thr_boxlist) return RERR_OK;
	/* first unlink boxlist, but we cannot if eddi is in progress */
	FRTHR_COND_SET(thr_boxlist->ref,thr_boxlist->tobereleased=1);
	FRTHR_COND_WAIT(thr_boxlist->ref,5000000LL,thr_boxlist->ref.ref > 0);
	FRTHR_COND_UNLOCK (thr_boxlist->ref);
	G_LOCK;
	/* now we can unlink */
	if (boxlist_head == thr_boxlist) {
		if (thr_boxlist->next == thr_boxlist) {
			boxlist_head = NULL;
		} else {
			boxlist_head = thr_boxlist->next;
		}
	}
	thr_boxlist->prev->next = thr_boxlist->next;
	thr_boxlist->next->prev = thr_boxlist->prev;
	thr_boxlist->next = NULL;
	thr_boxlist->prev = NULL;
	G_UNLOCK;
	ret = boxlist_finish ();
	*thr_boxlist = BOXLIST_INIT;
	free (thr_boxlist);
	thr_boxlist = NULL;
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error cleaning up boxlist: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

/* there is only one eddi thread, keeping this global speeds up
 * the access
 */
static struct boxlist	*current_ref = NULL;

struct evbox_eddinfo*
evbox_getnextthrd ()
{
	struct boxlist	*boxlist, *next;
	int				hasnext;

	if (!AM_I_EDDI) return NULL;
	G_LOCK;
	boxlist = current_ref;
	if (!boxlist) {
		next = boxlist_head;
	} else {
		next = boxlist->next;
		if (next == boxlist_head) next = NULL;
	}
	while (next) {
		FRTHR_COND_LOCK (next->ref);
		if (!next->tobereleased) {
			hasnext = ++(next->ref.ref);
		}
		FRTHR_COND_UNLOCK (next->ref);
		if (hasnext) break;
		next = next->next == boxlist_head ? NULL : next->next;
	}
	current_ref = next;
	G_UNLOCK;
	if (boxlist) {
		boxlist_releasereceiver (boxlist);
	}
	return next ? &next->eddinfo : NULL;
}

/* the following two are needed by evbox_insert */
static
struct boxlist *
boxlist_getreceiver (receiver)
	pthread_t	receiver;
{
	struct boxlist	*bl;
	int				start;

	if (AM_I_EDDI && current_ref && pthread_equal (current_ref->eddinfo.owner, receiver)) {
		FRTHR_COND_SET (current_ref->ref, current_ref->ref.ref++);
		return current_ref;
	}
	G_LOCK;
	for (	start=1, bl=boxlist_head;
			bl && (start || bl != boxlist_head);
			start=0, bl=bl->next) {
		if (pthread_equal (bl->eddinfo.owner, receiver)) {
			FRTHR_COND_LOCK (bl->ref);
			if (bl->tobereleased) {
				bl=NULL;
			} else {
				bl->ref.ref++;
			}
			FRTHR_COND_UNLOCK (bl->ref);
			G_UNLOCK;
			return bl;
		}
	}
	G_UNLOCK;
	return NULL;
}

static
void
boxlist_releasereceiver (boxlist)
	struct boxlist	*boxlist;
{
	int	need_deref=0;

	if (!boxlist) return;
	G_LOCK;
	need_deref = boxlist && boxlist->ref.ref > 0;
	G_UNLOCK;
	if (!need_deref) return;
	FRTHR_COND_SET (boxlist->ref, boxlist->ref.ref--);
	FRTHR_COND_SIGNAL (boxlist->ref);
}




int
evbox_get (name)
	const char	*name;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = searchlist_search (&box, name);
	if (RERR_ISOK(ret)) return box->id;
	if (ret != RERR_NOT_FOUND) return ret;
	return box_get (name);
}



int
evbox_close (id)
	int	id;
{
	struct evbox	*box;
	int				ret;
	int				nofree = 0;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = searchlist_rm (box->name);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error removing box from searchlist: %s",
								rerr_getstr3 (ret));
		return ret;
	}
	ret = boxlist_rm (id);
	if (!RERR_ISOK(ret)) {	
		FRLOGF (LOG_WARN, "error removing box from boxlist: %s",
								rerr_getstr3(ret));
		nofree = 1;
	}
	ret = box_destroy (box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error destroying event box: %s",
								rerr_getstr3 (ret));
	}
	if (!nofree) free (box);
	return RERR_OK;
}

int
evbox_search (name)
	const char	*name;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = searchlist_search (&box, name);
	if (!RERR_ISOK(ret)) return ret;
	return box->id;
}

int
evbox_exist (name)
	const char	*name;
{
	INITME;
	return searchlist_search (NULL, name);
}

int
evbox_noglobtrigger (id, off)
	int	id, off;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return RERR_NOTINIT;
	box->noglobtrigger = off;
	return RERR_OK;
}

const char*
evbox_getname (id)
	int	id;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return NULL;
	return box->name;
}


int
evbox_settrigcb (id, cb, parg, iarg)
	int	id;
	int	(*cb)(int, void*, int);
	void	*parg;
	int	iarg;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	box_lock (box);
	box->trig_cb = cb;
	box->trig_parg = parg;
	box->trig_iarg = iarg;
	box_unlock (box);
	return RERR_OK;
}

int
evbox_setglobtrigcb (cb, parg, iarg)
	int	(*cb)(int, void*, int);
	void	*parg;
	int	iarg;
{
	INITME;
	if (!thr_boxlist) return RERR_NOTINIT;
	pthread_mutex_lock (&thr_boxlist->mutex);
	thr_boxlist->trig_cb = cb;
	thr_boxlist->trig_parg = parg;
	thr_boxlist->trig_iarg = iarg;
	pthread_mutex_unlock (&thr_boxlist->mutex);
	return RERR_OK;
}

int
evbox_getglobtrigfd ()
{
	int	ret, fd;

	INITME;
	if (!thr_boxlist) return RERR_NOTINIT;
	pthread_mutex_lock (&thr_boxlist->mutex);
	if (thr_boxlist->trig_fd[0] > 0) {
		fd = thr_boxlist->trig_fd[0];
		pthread_mutex_unlock (&thr_boxlist->mutex);
		return fd;
	}
	ret = pipe2 (thr_boxlist->trig_fd, O_DIRECT|O_CLOEXEC|O_NONBLOCK);
	if (ret < 0) ret = pipe2 (thr_boxlist->trig_fd, O_CLOEXEC|O_NONBLOCK);
	if (ret < 0) thr_boxlist->trig_fd[0] = -1;
	fd = thr_boxlist->trig_fd[0];
	pthread_mutex_unlock (&thr_boxlist->mutex);
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error creating pipe: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return fd;
}

int
evbox_gettrigfd (id)
	int	id;
{
	struct evbox	*box;
	int				ret, fd;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	box_lock (box);
	if (box->trig_fd[0] > 0) {
		fd = box->trig_fd[0];
		box_unlock (box);
		return fd;
	}
	ret = pipe2 (box->trig_fd, O_DIRECT|O_CLOEXEC|O_NONBLOCK);
	if (ret < 0) ret = pipe2 (box->trig_fd, O_CLOEXEC|O_NONBLOCK);
	if (ret < 0) box->trig_fd[0] = -1;
	fd = box->trig_fd[0];
	box_unlock (box);
	if (ret < 0) {
		FRLOGF (LOG_ERR, "error creating pipe: %s", rerr_getstr3(RERR_SYSTEM));
		return RERR_SYSTEM;
	}
	return fd;
}


int
evbox_insert (event, boxname, receiver)
	struct event	*event;
	const char		*boxname;
	pthread_t		receiver;
{
	struct boxlist	*boxlist;
	int				ret;

	INITME;
	boxlist = boxlist_getreceiver (receiver);
	if (!boxlist) return RERR_NOT_FOUND;
	ret = doinsert (event, boxname, boxlist);
	boxlist_releasereceiver (boxlist);
	return ret;
}

int
evbox_filterinsert (event, receiver)
	struct event	*event;
	pthread_t		receiver;
{
	struct boxlist	*boxlist;
	int				ret;

	INITME;
	boxlist = boxlist_getreceiver (receiver);
	if (!boxlist) return RERR_NOT_FOUND;
	ret = dofilterinsert (event, boxlist);
	boxlist_releasereceiver (boxlist);
	return ret;
}

int
evbox_insertstr (evstr, flags, boxname, receiver)
	const char	*evstr;
	int			flags;
	const char	*boxname;
	pthread_t	receiver;
{
	struct event	*event;
	int				ret;

	INITME;
	if (!evstr) return RERR_PARAM;
	ret = evpool_acquire (&event);
	if (ret == RERR_NOT_FOUND) return RERR_FULL;
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting free event structure: %s",
								rerr_getstr3(ret));
		return ret;
	}
	ret = ev_cparse (event, evstr, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing event string: %s",
								rerr_getstr3 (ret));
		evpool_release (event);
		return ret;
	}
	ret = evbox_insert (event, boxname, receiver);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event: %s", rerr_getstr3 (ret));
		evpool_release (event);
		return ret;
	}
	return RERR_OK;
}

int
evbox_filterinsertstr (evstr, flags, receiver)
	const char	*evstr;
	int			flags;
	pthread_t	receiver;
{
	struct event	*event;
	int				ret;

	INITME;
	if (!evstr) return RERR_PARAM;
	ret = evpool_acquire (&event);
	if (ret == RERR_NOT_FOUND) return RERR_FULL;
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error getting free event structure: %s",
								rerr_getstr3(ret));
		return ret;
	}
	ret = ev_cparse (event, evstr, flags);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error parsing event string: %s",
								rerr_getstr3 (ret));
		evpool_release (event);
		return ret;
	}
	ret = evbox_filterinsert (event, receiver);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event: %s", rerr_getstr3 (ret));
		evpool_release (event);
		return ret;
	}
	return RERR_OK;
}


int
evbox_release (event)
	struct event	*event;
{
	return evpool_release (event);
}


int
evbox_numev (id)
	int	id;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	return dobox_getnum (box);
}


int
evbox_pop (oev, id)
	struct event	**oev;
	int				id;
{
	return evbox_pop2 (oev, NULL, id);
}

int
evbox_pop2 (oev, tid, id)
	struct event	**oev;
	pthread_t		*tid;
	int				id;
{
	struct evbox	*box;
	int				ret;

	INITME;

#if 0
	if(evbox_numev(id) > 0){
	   FRLOGF (LOG_DEBUG, "fetching next event from eventbox '%i' (Backlog: %i events)",
	    		              id, evbox_numev(id));
	}
#endif
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	ret = box_pop (oev, tid, box);
	return ret;
}


int
evbox_popstr (ostr, id)
	char	**ostr;
	int	id;
{
	struct evbox	*box;
	int				ret;

	INITME;
	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	box_lock (box);
	ret = box_popstr (ostr, box);
	box_unlock (box);
	return ret;
}



int
evbox_wait (id, timeout)
	int	id;
	tmo_t	timeout;
{
	struct evbox	*box;
	int				ret;

	INITME;

	ret = boxlist_get (&box, id);
	if (!RERR_ISOK(ret)) return ret;
	FRTHR_COND_WAIT (box->cond, timeout, box->cond.trig <= 0);
	return RERR_OK;
}

int
evbox_waitany (timeout)
	tmo_t	timeout;
{
	INITME;

	if (!thr_boxlist) return RERR_NOTINIT;
	FRTHR_COND_WAIT (thr_boxlist->cond, timeout, thr_boxlist->cond.trig <= 0);
	return RERR_OK;
}

int
evbox_addfilter (fid)
	int	fid;
{
	int	ret;

	INITME;

	if (!thr_boxlist) return RERR_NOTINIT;
	if (fid < 0) return RERR_PARAM;
	if (fid > 0) {
		ret = evf_reference (fid);
		if (!RERR_ISOK(ret)) return ret;
		ret = evf_protect (fid);
		if (!RERR_ISOK(ret)) {
			evf_release (fid);
			return ret;
		}
	}
	if (thr_boxlist->eddinfo.filterid > 0) {
		evf_unprotect (thr_boxlist->eddinfo.filterid);
		evf_release (thr_boxlist->eddinfo.filterid);
	}
	thr_boxlist->eddinfo.filterid = fid;
	return RERR_OK;
}


int
evbox_enable ()
{
	INITME;
	if (!thr_boxlist) return RERR_NOTINIT;
	thr_boxlist->eddinfo.flags &= ~EVBOX_F_DISABLED;
	return RERR_OK;
}

int
evbox_disable ()
{
	INITME;
	if (!thr_boxlist) return RERR_NOTINIT;
	thr_boxlist->eddinfo.flags |= EVBOX_F_DISABLED;
	return RERR_OK;
}




/*********************************
 * static functions
 *********************************/

static
int
boxlist_finish ()
{
	int					ret, num, id;
	struct searchev	sev;

	if (!thr_boxlist) return RERR_OK;
	/* lock is not necc. we are the only one */
	while ((num=TLST_GETNUM (thr_boxlist->searchlist))) {
		/* get last one */
		ret = TLST_GET (sev, thr_boxlist->searchlist, num-1);
		if (!RERR_ISOK(ret)) continue;	/* should not happen */
		id = sev.box->id;
		/* HACK: we must be able to close accept box as well, so rename it */
		if (!sev.box->name) {
			sev.box->name = strdup ("dead");
		} else if (!strcasecmp (sev.box->name, "accept")) {
			strcpy (sev.box->name, "dead");
		}
		ret = evbox_close (id);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error closeing box %d: %s", id, rerr_getstr3(ret));
			continue;
		}
	}
	TLST_FREE (thr_boxlist->boxlist);
	TLST_FREE (thr_boxlist->searchlist);
	TLST_FREE (thr_boxlist->eddinfo.distgrp);
	evf_unprotect (thr_boxlist->eddinfo.filterid);
	evf_release (thr_boxlist->eddinfo.filterid);
	if (thr_boxlist->trig_fd[0] > 2) {
		close (thr_boxlist->trig_fd[0]);
		close (thr_boxlist->trig_fd[1]);
	}
	return RERR_OK;
}


static
int
box_get (name)
	const char	*name;
{
	struct evbox	*box;
	int				ret;

	box = malloc (sizeof (struct evbox));
	if (!box) return RERR_NOMEM;
	bzero (box, sizeof (struct evbox));
	ret = box_init (box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing event box: %s", rerr_getstr3 (ret));
		free (box);
		return ret;
	}
	if (!name) name = "accept";
	box->name = strdup (name);
	if (!box->name) {
		free (box);
		return RERR_NOMEM;
	}
	ret = box_insert (box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting box: %s", rerr_getstr3 (ret));
		box_destroy (box);
		free (box);
		return ret;
	}
	return box->id;
}

static
int
box_popstr (ostr, box)
	char				**ostr;
	struct evbox	*box;
{
	int				ret, ret2;
	struct event	*ev;

	if (!ostr || !box) return RERR_PARAM;
	ret = box_pop (&ev, NULL, box);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_create (ostr, ev, 0);
	ret2 = evpool_release (ev);
	if (!RERR_ISOK(ret2)) {
		FRLOGF (LOG_WARN, "cannot release event: %s", rerr_getstr3(ret2));
	}
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error creating event: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
box_pop (oev, tid, box)
	struct event	**oev;
	pthread_t		*tid;
	struct evbox	*box;
{
	if (!oev || !box) return RERR_PARAM;
	box_lock (box);
	if (box->first == box->next) {
		box_unlock(box);
		return RERR_NOT_FOUND;
	}
	*oev = box->list[box->first].event;
	if (tid) *tid = box->list[box->first].tid;
	box->list[box->first].event = NULL;
	box->first = (box->first + 1) % box->listsz;
	box_unlock (box);
	FRTHR_COND_SET (box->cond, box->cond.trig--);
	FRTHR_COND_SET (thr_boxlist->cond, thr_boxlist->cond.trig--);
	return RERR_OK;
}

static
int
box_lock (box)
	struct evbox	*box;
{
	int	ret;

	if (!box) return RERR_PARAM;
	ret = pthread_mutex_lock (&(box->mutex));
	if (ret != 0) {
		errno = ret;
		return RERR_SYSTEM;
	}
	return RERR_OK;
}

static
int
box_unlock (box)
	struct evbox	*box;
{
	int	ret;

	if (!box) return RERR_PARAM;
	ret = pthread_mutex_unlock (&(box->mutex));
	if (ret != 0) {
		errno = ret;
		return RERR_SYSTEM;
	}
	return RERR_OK;
}


static
int
dobox_getnum (box)
	struct evbox	*box;
{
	int	num;
	if (!box) return 0;
	box_lock (box);
	if (box->next >= box->first) {
		num = box->next - box->first;
	} else {
		num = box->next + box->listsz - box->first;
	}
	box_unlock(box);
	return num;
}

static
int
doinsert (event, boxname, boxlist)
	struct event	*event;
	const char		*boxname;
	struct boxlist	*boxlist;
{
	struct evbox	*box;
	int				ret, ret2;
	int				noglobtrigger;

	if (!event || !boxlist) return RERR_PARAM;
	if (!boxname) boxname = event->hastarget ? event->targetname : NULL;
	FRLOGF (LOG_VERBOSE, "got event for box %s", boxname);
	if ((boxlist->eddinfo.flags & EVBOX_F_DISABLED) 
					&& !strncasecmp (boxname, "reject", 6)) {
		/* drop event */
		FRLOGF (LOG_VERBOSE, "event boxen disabled");
		evpool_release (event);
		return RERR_OK;
	}
	ret = searchlist_searchlock (&box, boxname, boxlist);
	if (boxname && ret == RERR_NOT_FOUND) {
		FRLOGF (LOG_VERBOSE, "cannot find box >>%s<<", boxname);
		if (!strncasecmp (boxname, "reject", 6)) {
			FRLOGF (LOG_VERBOSE, "insert into reject");
			if (boxname[6] == '_') ret = searchlist_searchlock (&box, "reject", boxlist);
			if (ret == RERR_NOT_FOUND) {
				evpool_release (event);
				return RERR_OK;
			}
		} else if (strcasecmp (boxname, "accept") != 0) {
			FRLOGF (LOG_VERBOSE, "insert into accept");
			ret = searchlist_searchlock (&box, NULL, boxlist);
		}
	}
	if (!RERR_ISOK(ret)) return ret;
	ret = doboxinsert (box, event);
	if (RERR_ISOK(ret)) {
		noglobtrigger = box->noglobtrigger;
		if (box->trig_cb) {
			/* trigger box callback */
			ret2 = box->trig_cb (box->id, box->trig_parg, box->trig_iarg);
			if (!RERR_ISOK(ret2)) {
				FRLOGF (LOG_NOTICE, "error in trigger callback (box %d:%s): %s", 
								box->id, box->name, rerr_getstr3 (ret2));
			}
		}
		if (box->trig_fd[0] > 0) {
			ret2 = write (box->trig_fd[1], &(box->id), sizeof (int));
			if (ret2 < 0) {
				FRLOGF (LOG_NOTICE, "error writing trigger to fd (box %d:%s): %s",
								box->id, box->name, rerr_getstr3 (RERR_SYSTEM));
			}
		}
	}
	box_unlock (box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event into box: %s", rerr_getstr3(ret));
		return ret;
	}
	/* trigger boxlist callback */
	if (boxlist->trig_cb && !noglobtrigger) {
		ret2 = boxlist->trig_cb (box->id, boxlist->trig_parg, boxlist->trig_iarg);
		if (!RERR_ISOK(ret2)) {
			FRLOGF (LOG_NOTICE, "error in global trigger callback "
							"(box %d:%s): %s", 
							box->id, box->name, rerr_getstr3 (ret2));
		}
	}
	if (boxlist->trig_fd[0] > 0 && !noglobtrigger) {
		ret2 = write (boxlist->trig_fd[1], &(box->id), sizeof (int));
		if (ret2 < 0) {
			FRLOGF (LOG_NOTICE, "error writing trigger to global fd "
							"(box %d:%s): %s",
							box->id, box->name, rerr_getstr3 (RERR_SYSTEM));
		}
	}
	/* increment counter */
	FRTHR_COND_SET (box->cond, box->cond.trig++);
	FRTHR_COND_SET (boxlist->cond, boxlist->cond.trig++);
	/* trigger waiting thread */
	FRTHR_COND_SIGNAL (box->cond);
	FRTHR_COND_SIGNAL (boxlist->cond);
	return RERR_OK;
}


static
int
dofilterinsert (event, boxlist)
	struct event	*event;
	struct boxlist	*boxlist;
{
	int				ret, ret2=RERR_OK;
	struct evlist	evlist, *ptr;

	if (boxlist->eddinfo.filterid < 0) {
		return doinsert (event, "accept", boxlist);
	}
	ret = evf_apply (&evlist, boxlist->eddinfo.filterid, event, EVF_F_IN | EVF_F_COPY);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_DEBUG, "error applying filter (id=%d): %s", 
								boxlist->eddinfo.filterid, rerr_getstr3(ret));
		return ret;
	}
	for (ptr=&evlist; ptr; ptr=ptr->next) {
		switch (ptr->target) {
		case EVF_T_DROP:
			evpool_release(ptr->event);
			continue;
		case EVF_T_ACCEPT:
			ret = doinsert (ptr->event, NULL, boxlist);
			break;
		case EVF_T_REJECT:
			ret = doinsert (ptr->event, "reject_in", boxlist);
			break;
		default:
			ret = RERR_NOT_SUPPORTED;
			break;
		}
		if (!RERR_ISOK(ret)) {
			char	*wrout = NULL;
			ret2=ret;
			ret = ev_create (&wrout, ptr->event, EVP_F_NOPRTVAR);
			if (!RERR_ISOK(ret)) wrout = NULL;
			evpool_release(ptr->event);
			FRLOGF (LOG_WARN, "error inserting event >>%s<< into eventbox: %s", 
						(wrout?wrout:"???"), rerr_getstr3(ret2));
			if (wrout) free (wrout);
		}
	}
	evf_evlistfree (&evlist, EVF_F_NOEVFREE);
	if (!RERR_ISOK(ret2)) return ret2;
	evpool_release (event);
	return RERR_OK;
}



static
int
doboxinsert (box, ev)
	struct evbox	*box;
	void				*ev;
{
	struct eventry	*ptr;
	ssize_t			len, num;

	/* lock is already held */
	if (!box || !ev) return RERR_PARAM;
	FRLOGF (LOG_VERBOSE, "do insert");
	if (frthr_self && !AM_I_EDDI) {
		ev_addattr_i (ev, "_thrsenderid", frthr_self->id, EVP_F_OVERWRITE);
		if (frthr_self->name) {
			ev_addattr_s (ev, "_thrsender", (char*)frthr_self->name, EVP_F_CPY|EVP_F_OVERWRITE);
		}
	}
	if (((box->next + 1) % box->listsz) == box->first) {
		if (c_maxevqsz >= 0 && box->listsz >= c_maxevqsz) return RERR_FULL;
		len = box->listsz + c_szevqueue;
		if (c_maxevqsz >= 0 && len > c_maxevqsz) len = c_maxevqsz;
		ptr = realloc (box->list, (len + 1) * sizeof (struct eventry));
		if (!ptr) return RERR_NOMEM;
		bzero (ptr+box->listsz, (len+1-box->listsz)*sizeof(struct eventry));
		if (box->next < box->first) {
			num = box->next <= c_szevqueue ? box->next : c_szevqueue;
			memcpy (ptr+box->listsz, ptr, num*sizeof(struct eventry));
			if (num < box->next) {
				memmove (ptr, ptr+num, (box->next-num)*sizeof(struct eventry));
				box->next -= num;
			} else {
				box->next += box->listsz;
			}
		}
		box->listsz += c_szevqueue;
		box->list = ptr;
	}
	box->list[box->next].event = ev;
	box->list[box->next].tid = pthread_self ();
	box->next = (box->next + 1) % box->listsz;
	return RERR_OK;
}


static
int
box_init (box)
	struct evbox	*box;
{
	CF_MAYREAD;
	*box = EVBOX_INIT;
	box->list = malloc (sizeof (struct eventry) * (c_szevqueue+1));
	if (!box->list) return RERR_NOMEM;
	bzero (box->list, sizeof (struct eventry) * (c_szevqueue+1));
	box->listsz = c_szevqueue+1;
	box->first = box->next = 0;
	return RERR_OK;
}


static
int
box_destroy (box)
	struct evbox	*box;
{
	int	i, ret=RERR_OK;

	if (!box) return RERR_PARAM;
	box_lock(box);
	if (box->name) free (box->name);
	if (box->list) {
		for (i=box->first; i!=box->next; i=(i+1)%box->listsz) {
			if (!box->list[i].event) continue;
			evbox_release (box->list[i].event);
			box->list[i].event = NULL;
		}
		free (box->list);
	}
	if (box->trig_fd[0] > 2) {
		close (box->trig_fd[0]);
		close (box->trig_fd[1]);
	}
	box_unlock (box);
	bzero (box, sizeof (struct evbox));
	return ret;
}


static
int
box_insert (box)
	struct evbox	*box;
{
	int	ret;

	if (!box) return RERR_PARAM;
	ret = boxlist_add (box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event box into "
								"boxlist: %s", rerr_getstr3 (ret));
		return ret;
	}
	box->id = ret;
	ret = searchlist_add (box);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event box into "
								"searchlist: %s", rerr_getstr3 (ret));
		boxlist_rm (box->id);
		return ret;
	}
	return RERR_OK;
}




/***********************
 * searchlist functions
 ***********************/


static
int
searchlist_add (box)
	struct evbox	*box;
{
	struct searchev	sev;
	int					ret;
	const char			*name;

	if (!box) return RERR_PARAM;
	name = box->name ? box->name : "accept";
	ret = searchlist_search (NULL, box->name);
	if (RERR_ISOK(ret)) return RERR_ALREADY_EXIST;
	if (ret != RERR_NOT_FOUND) return ret;
	sev = (struct searchev) { .name = name, .box = box };
	pthread_mutex_lock (&thr_boxlist->mutex);
	ret = TLST_INSERT (thr_boxlist->searchlist, sev, tlst_cmpistr);
	pthread_mutex_unlock (&thr_boxlist->mutex);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting box into searchlist: %s",
						rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}

static
int
searchlist_search (obox, name)
	struct evbox	**obox;
	const char		*name;
{
	struct searchev	sev;
	int					ret, num;

	if (!name) name = "accept";
	ret = TLST_SEARCH (thr_boxlist->searchlist, name, tlst_cmpistr);
	if (!RERR_ISOK(ret)) return ret;
	if (!obox) return RERR_OK;
	num = ret;
	ret = TLST_GET (sev, thr_boxlist->searchlist, num);
	if (!RERR_ISOK(ret)) return ret;
	*obox = sev.box;
	return RERR_OK;
}

static
int
searchlist_searchlock (obox, name, boxlist)
	struct evbox	**obox;
	const char		*name;
	struct boxlist	*boxlist;
{
	struct searchev	sev;
	int					ret, num;

	if (!boxlist) return RERR_PARAM;
	pthread_mutex_lock (&boxlist->mutex);
	if (!name) name = "accept";
	ret = TLST_SEARCH (boxlist->searchlist, name, tlst_cmpistr);
	if (!RERR_ISOK(ret)) goto out;
	if (!obox) goto out;
	num = ret;
	ret = TLST_GET (sev, boxlist->searchlist, num);
	if (!RERR_ISOK(ret)) goto out;
	*obox = sev.box;
out:
	if (RERR_ISOK(ret) && obox && *obox) {
		box_lock (*obox);
	}
	pthread_mutex_unlock (&boxlist->mutex);
	return ret;
}


static
int
searchlist_rm (name)
	char	*name;
{
	int	ret, num;

	if (!name || !strcasecmp (name, "accept")) {
		FRLOGF (LOG_ERR, "the default box cannot be removed");
		return RERR_PARAM;
	}
	ret = TLST_SEARCH (thr_boxlist->searchlist, name, tlst_cmpistr);
	if (!RERR_ISOK(ret)) return ret;
	num = ret;
	pthread_mutex_lock (&thr_boxlist->mutex);
	ret = TLST_REMOVE (thr_boxlist->searchlist, num, TLST_F_SHIFT);
	pthread_mutex_unlock (&thr_boxlist->mutex);
	if (!RERR_ISOK(ret)) return ret;
	return RERR_OK;
}





/***********************
 * boxlist functions 
 ***********************/



static
int
boxlist_add (box)
	struct evbox	*box;
{
	return TLST_ADDINSERT (thr_boxlist->boxlist, box);
}



static
int
boxlist_rm (nbox)
	int	nbox;
{
	return TLST_REMOVE (thr_boxlist->boxlist, nbox, TLST_F_NONE);
}


static
int
boxlist_get (box, nbox)
	struct evbox	**box;
	int				nbox;
{
	return TLST_GET (*box, thr_boxlist->boxlist, nbox);
}























static
int
read_config ()
{
	cf_begin_read ();
	c_szevqueue = cf_atoi (cf_getarr2 ("SizeEventQueue", fr_getprog(), "50"));
	if (c_szevqueue < 5) c_szevqueue = 5;	/* min. size */
	c_maxevqsz = cf_atoi (cf_getarr2 ("MaxSizeEventQueue", fr_getprog(), "50"));
	if (c_maxevqsz >= 0 && c_maxevqsz < c_szevqueue) c_maxevqsz = c_szevqueue;
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 1;
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
