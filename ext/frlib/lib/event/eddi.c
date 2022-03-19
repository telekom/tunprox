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
#include <pthread.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>


#include <fr/base.h>
#include <fr/connect.h>
#include <fr/thread.h>

#include "eddi.h"
#include "eddilisten.h"
#include "evbox.h"
#include "parseevent.h"
#include "evpool.h"
#include "evfilter.h"


static int read_config ();
static int config_read = 0;


static const char *c_filterid = NULL;
static int			c_dropIsFatal = 0;
static int			c_distout = 0;
static int			c_debugev = 0;
static int			c_debugev2stdout = 0;
static int 			eddi_started = 0;
static int 			filter_id = 0;
static int 			outbox = -1;
static int			distbox = -1;
static int			inbox = -1;


static int thrd_init ();
static int thrd_main ();
static int thrd_cleanup ();

static struct frthr	eddithr = {
	.name = "eddi",
	.init = thrd_init,
	.main = thrd_main,
	.cleanup = thrd_cleanup,
};
static int			eddithr_id = -1;
static pthread_t	eddithr_ptid;


static int loadautofilter ();
static int outpoll ();
static int elab_outevent (struct event *, pthread_t);
static int distpoll ();
static int elab_distevent (struct event *, pthread_t);
static int distinsert (struct event *, pthread_t);
static int distout (struct event*);
static int inpoll ();
static int elab_inevent (struct event*);
static int evinsert (struct event*, int);
static int evinsert2 (struct event*, struct evbox_eddinfo*, int);
static int _printevstr (const char *str);
static int _printev (struct event *event);



int
eddi_start ()
{
	int	ret;

	if (eddi_started) return RERR_OK;
	CF_MAYREAD;
	ret = eddilisten_start ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting listening thread: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = evpool_init ();
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing event pool: %s", rerr_getstr3(ret));
		return ret;
	}
	ret = frthr_start (&eddithr, 500000000LL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting eddi worker thread: %s", rerr_getstr3 (ret));
		return ret;
	}
	eddithr_id = ret;
	eddi_started = 1;
	return RERR_OK;
}

int
eddi_finish ()
{
	int	ret;

	ret = frthr_destroy (eddithr_id);
	if (!RERR_ISOK(ret)) return ret;
	eddithr_id = -1;
	eddi_started = 0;
	return eddilisten_finish ();
}

int
eddi_isstarted ()
{
	return eddi_started;
}

int
eddi_gettid (tid)
	pthread_t	*tid;
{
	if (!eddi_started) return RERR_NOTINIT;
	*tid = eddithr_ptid;
	return RERR_OK;
}

int
eddi_sendev (event)
	struct event	*event;
{
	int	ret;

	CF_MAYREAD;
	if (!eddi_started) {
		ret = eddi_start ();
	 	if (!RERR_ISOK(ret)) {
	 	  	FRLOGF (LOG_ERR, "error starting eddi subsystem: %s", rerr_getstr3(ret));
	 	  	return ret;
	 	}
	}
	if (c_debugev)
		_printev (event);
	return evbox_insert (event, "out", eddithr_ptid);
}


int
eddi_sendstr (evstr, flags)
	char	*evstr;
	int	flags;
{
	int	ret;

	if (!eddi_started) {
		ret = eddi_start ();
	 	if (!RERR_ISOK(ret)) {
	 	  	FRLOGF (LOG_ERR, "error starting eddi subsystem: %s", rerr_getstr3(ret));
	 	  	return ret;
	 	}
	}
	if (c_debugev)
		_printevstr (evstr);
	return evbox_insertstr (evstr, flags, "out", eddithr_ptid);
}

int
eddi_distev (event)
	struct event	*event;
{
	int	ret;

	CF_MAYREAD;
	if (!eddi_started) {
		ret = eddi_start ();
	 	if (!RERR_ISOK(ret)) {
	 	  	FRLOGF (LOG_ERR, "error starting eddi subsystem: %s", rerr_getstr3(ret));
	 	  	return ret;
	 	}
	}
	if (c_debugev)
		_printev (event);
	return evbox_insert (event, "dist", eddithr_ptid);
}


int
eddi_diststr (evstr, flags)
	char	*evstr;
	int	flags;
{
	int	ret;

	CF_MAYREAD;
	if (!eddi_started) {
		ret = eddi_start ();
	 	if (!RERR_ISOK(ret)) {
	 	  	FRLOGF (LOG_ERR, "error starting eddi subsystem: %s", rerr_getstr3(ret));
	 	  	return ret;
	 	}
	}
	if (c_debugev)
		_printevstr (evstr);
	return evbox_insertstr (evstr, flags, "dist", eddithr_ptid);
}

int
eddi_debugevent (event)
	struct event	*event;
{
	int	ret;

	if (!event) {
		FRLOGF (LOG_WARN, "debugging NULL event");
	}
	ret = _printev (event);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error printing event: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

static
int
_printev (event)
	struct event	*event;
{
	int			ret, ret2;
	const char	*name;
	char			*str;

	CF_MAYREAD;
	if (!c_debugev) return RERR_OK;
	if (c_debugev2stdout) {
		ret = ev_prtparsed (event, 1, 0);
		if (!RERR_ISOK(ret)) {
			printf ("error in ev_prtparsed(): %d\n", ret);
		}
	}
	ret = ev_create (&str, event, 0);
	if (!RERR_ISOK(ret)) {
		ret2 = ev_getname (&name, event);
		if (!RERR_ISOK(ret2)) name="<unknown>";
		FRLOGF (LOG_ERR, "error creating event >>%s<<: %s", name, rerr_getstr3(ret));
		return ret;
	}
	FRLOGF (LOG_DEBUG, "event to be sent: %s", str);
	if (str) free (str);
	return RERR_OK;
}

static
int
_printevstr (str)
	const char	*str;
{
	if (!str) str = "no event given";
	FRLOGF (LOG_DEBUG, "event to be sent: %s", str);
	if (c_debugev2stdout) {
		printf ("%s\n", str);
	}
	return RERR_OK;
}



int
eddi_addfilter (fid)
	int	fid;
{
	int	ret;

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
	if (filter_id > 0) {
		evf_unprotect (filter_id);
		evf_release (filter_id);
	}
	filter_id = fid;
	return RERR_OK;
}

int
eddi_addfilterid (filter, flags)
	const char	*filter;
	int			flags;
{
	int	fid;
	int	ret;

	fid = evf_new ();
	if (!RERR_ISOK(fid)) return fid;
	ret = evf_addfilter (fid, filter, flags);
	if (!RERR_ISOK(ret)) {
		evf_release (fid);
		return ret;
	}
	ret = eddi_addfilter (fid);
	if (!RERR_ISOK(ret)) {
		evf_release (fid);
		return ret;
	}
	return RERR_OK;
}

int
eddi_sendfatalerror (errorname)
	const char *errorname;
{
	int				ret;
	struct event	ev;
	
	FRLOGF (LOG_ERR, "fatal error in child. Preparing event '%s'", errorname);

	/* create the event manually, not from the event pool */
	ret = ev_new(&ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "Error creating event: %s", rerr_getstr3(ret));
		return ret;
	}
	
	ret = ev_setname (&ev, "fatalerror", 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error setting event name: %s", rerr_getstr3(ret));
		ev_free(&ev);
		return ret;
	}
	ret = ev_addattr_s (&ev, "error", (char*)errorname, EVP_F_CPY);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error setting event variable: %s", rerr_getstr3(ret));
		ev_free(&ev);
		return ret;
	}
	ret = eddi_sendev(&ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error sending event: %s", rerr_getstr3(ret));
		ev_free(&ev);
		return ret;
	}
	FRLOGF (LOG_ERR, "sent triggerevent '%s'", errorname);
	
	return RERR_OK;
}


/************************
 * static functions 
 ************************/

static
int
loadautofilter ()
{
	CF_MAYREAD;
	if (c_filterid) {
		return eddi_addfilterid (c_filterid, 0);
	}
	return RERR_OK;
}



/********************* 
 * thread functions 
 *********************/




static
int
thrd_cleanup ()
{
	return evbox_finish();
}



static
int
thrd_init ()
{
	int	ret;

	ret = evbox_init (EVBOX_F_IAMEDDI);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing event box: %s", rerr_getstr3 (ret));
		return ret;
	}
	if (filter_id <= 0) {
		ret = loadautofilter ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error loading auto filter: %s", rerr_getstr3(ret));
			filter_id = 0;
		}
	}
	outbox = evbox_get ("out");
	if (!RERR_ISOK(outbox)) {
		FRLOGF (LOG_ERR, "error getting out box: %s", rerr_getstr3 (outbox));
		return outbox;
	}
	inbox = evbox_get ("in");
	if (!RERR_ISOK(outbox)) {
		FRLOGF (LOG_ERR, "error getting in box: %s", rerr_getstr3 (inbox));
		return outbox;
	}
	distbox = evbox_get ("dist");
	if (!RERR_ISOK(distbox)) {
		FRLOGF (LOG_ERR, "error getting dist box: %s", rerr_getstr3 (distbox));
		return outbox;
	}
	eddithr_ptid = pthread_self();

	return RERR_OK;
}

int eddiout_maysend();
static
int
thrd_main()
{
	while (1) {
		evbox_waitany (1500000LL /* 1s */);
		distpoll ();
		outpoll ();
		inpoll ();
		eddiout_maysend ();
	}
	return RERR_INTERNAL;	/* should never reach here */
}


static
int
outpoll ()
{
	struct event	*ev;
	int				ret;
	pthread_t		ptid;

	while (evbox_numev (outbox) > 0) {
		ret = evbox_pop2 (&ev, &ptid, outbox);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_NOT_FOUND) {
				FRLOGF (LOG_WARN, "error popping event: %s", rerr_getstr3(ret));
				tmo_sleep (10000LL);	/* 10 ms */
			}
			return ret;
		}
		ret = elab_outevent (ev, ptid);
		evpool_release (ev);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error elaborating outgoing event: %s", rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}


static
int
elab_outevent (ev, ptid)
	struct event	*ev;
	pthread_t		ptid;
{
	int				ret, ret2=RERR_OK;
	struct evlist	evlist, *ptr;

	if (!ev) return RERR_PARAM;
	ev_addattr_d (ev, "_date", tmo_now(), EVP_F_OVERWRITE);	/* ignore errors */
	ev_addattr_i (ev, "_sender", getpid(), EVP_F_OVERWRITE);
	
	if (filter_id < 0) {
		ret = eddiout_sendev (ev);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_ERR, "error sending event: %s", rerr_getstr3(ret));
			return ret;
		}
	}
	ret = evf_apply (&evlist, filter_id, ev, EVF_F_OUT | EVF_F_OUTTARGET);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_INFO, "error applying filter: %s", rerr_getstr3(ret));
		return ret;
	}
	for (ptr=&evlist; ptr; ptr=ptr->next) {
		switch (ptr->target) {
		case EVF_T_DROP:
			continue;
		case EVF_T_REJECT:
			ret = evbox_insert (ptr->event, "reject_out", ptid);
			if (!RERR_ISOK(ret)) {
				ret2=ret;
				FRLOGF (LOG_WARN, "error inserting event into rejectbox: %s", 
							rerr_getstr3(ret));
				break;
			}
			ptr->event = NULL;
			break;
		case EVF_T_ACCEPT:
			ret = eddiout_sendev (ptr->event);
			if (!RERR_ISOK(ret)) {
				ret2=ret;
				FRLOGF (LOG_ERR, "error sending event: %s", rerr_getstr3(ret));
			}
			ptr->event = NULL;
			break;
		default:
			return RERR_NOT_SUPPORTED;
		}
	}
	ret = evf_evlistfree (&evlist, 0);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_WARN, "error freeing event list: %s", rerr_getstr3(ret));
		if (!RERR_ISOK(ret2)) return ret2;
		return ret;
	}
	if (!RERR_ISOK(ret2)) return ret2;
	return RERR_OK;
}


static
int
distpoll ()
{
	struct event	*ev;
	int				ret;
	pthread_t		ptid;

	while (evbox_numev (distbox) > 0) {
		ret = evbox_pop2 (&ev, &ptid, distbox);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_NOT_FOUND) {
				FRLOGF (LOG_WARN, "error popping event: %s", rerr_getstr3(ret));
				tmo_sleep (10000LL);	/* 10 ms */
			}
			return ret;
		}
		ret = elab_distevent (ev, ptid);
		evpool_release (ev);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error distributing event: %s", rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}


static
int
elab_distevent (ev, ptid)
	struct event	*ev;
	pthread_t		ptid;
{
	struct evbox_eddinfo	*edi;
	int						ret, ret2=RERR_OK;

	if (!ev) return RERR_PARAM;
	/* add extra info */
	ev_addattr_d (ev, "_date", tmo_now(), 0);	/* ignore errors */
	ev_addattr_s (ev, "_origin", "internal", EVP_F_OVERWRITE);
	if (c_distout) {
		distout (ev);	/* ignore errors */
	}
	while ((edi=evbox_getnextthrd())) {
		if (pthread_equal (ptid, edi->owner)) continue;
		if (pthread_equal (edi->owner, eddithr_ptid)) continue;
		if ((edi->flags & EVBOX_F_DISABLED)) continue;
		ret = distinsert (ev, edi->owner);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;
}

static
int
distinsert (ev, ptid)
	struct event	*ev;
	pthread_t		ptid;
{
	struct event	*ev2;
	int				ret;

	ret = evpool_acquire (&ev2);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_copy (ev2, ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error copying event: %s", rerr_getstr3(ret));
		evpool_release (ev2);
		return ret;
	}
	ret = evbox_filterinsert (ev2, ptid);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error inserting event: %s", rerr_getstr3(ret));
		evpool_release (ev2);
		return ret;
	}
	return RERR_OK;
}


static
int
distout (ev)
	struct event	*ev;
{
	struct event	*ev2;
	int				ret;

	if (!ev) return RERR_PARAM;
	ret = evpool_acquire (&ev2);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_copy (ev2, ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error copying event: %s", rerr_getstr3(ret));
		evpool_release (ev2);
		return ret;
	}
	ev_addattr_i (ev2, "_sender", getpid(), EVP_F_OVERWRITE);	/* ignore errors */
	ret = eddiout_sendev (ev2);
	if (!RERR_ISOK(ret)) {
		evpool_release (ev2);
		FRLOGF (LOG_ERR, "error sending event: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


static
int
inpoll ()
{
	struct event	*ev;
	int				ret;

	while (evbox_numev (inbox) > 0) {
		ret = evbox_pop (&ev, inbox);
		if (!RERR_ISOK(ret)) {
			if (ret != RERR_NOT_FOUND) {
				FRLOGF (LOG_WARN, "error popping event: %s", rerr_getstr3(ret));
				tmo_sleep (10000LL);	/* 10 ms */
			}
			return ret;
		}
		ret = elab_inevent (ev);
		evpool_release (ev);
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error elaborating incomming event: %s", rerr_getstr3(ret));
		}
	}
	return RERR_OK;
}

static
int
elab_inevent (ev)
	struct event	*ev;
{
	int				ret, ret2=RERR_OK;
	struct evlist	evlist, *ptr;

	if (!ev) return RERR_PARAM;
	ev_addattr_s (ev, "_origin", "external", EVP_F_OVERWRITE);
	ret = evf_apply (&evlist, filter_id, ev, EVF_F_IN | EVF_F_COPY);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_DEBUG, "error applying filter (id=%d): %s", filter_id,
								rerr_getstr3(ret));
		return ret;
	}
	for (ptr=&evlist; ptr; ptr=ptr->next) {
		switch (ptr->target) {
		case EVF_T_DROP:
			evpool_release(ptr->event);
			continue;
		case EVF_T_ACCEPT:
			ret = evinsert (ptr->event, 0);
			break;
		case EVF_T_REJECT:
			ret = evinsert (ptr->event, 1);
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
	//evf_evlistfree (&evlist, EVF_F_NOEVFREE);
	evf_evlistfree (&evlist, 0);
	if (!RERR_ISOK(ret2)) return ret2;
	return RERR_OK;
}

static
int
evinsert (ev, isrej)
	struct event	*ev;
	int				isrej;
{
	struct evbox_eddinfo	*edi;
	int						ret, ret2=RERR_OK;

	if (!ev) return RERR_PARAM;
	while ((edi=evbox_getnextthrd())) {
		if (!(edi->flags & EVBOX_F_GETGLOBAL)) continue;
		if ((edi->flags & EVBOX_F_DISABLED)) continue;
		ret = evinsert2 (ev, edi, isrej);
		if (!RERR_ISOK(ret)) ret2 = ret;
	}
	return ret2;

}

static
int 
evinsert2 (ev, edi, isrej)
	struct event			*ev;
	struct evbox_eddinfo	*edi;
	int						isrej;
{
	struct event	*ev2;
	int				ret, i;

	if (!ev || !edi) return RERR_PARAM;
	ret = evpool_acquire (&ev2);
	if (!RERR_ISOK(ret)) return ret;
	ret = ev_copy (ev2, ev);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error copying event: %s", rerr_getstr3(ret));
		evpool_release (ev2);
		return ret;
	}
	if (isrej) {
		ret = evbox_insert (ev, "reject_in", edi->owner);
		if (!RERR_ISOK(ret)) {
			evpool_release (ev2);
			FRLOGF (LOG_ERR, "error inserting rejected event: %s", rerr_getstr3(ret));
		}
		return ret;
	}
	for (i=0; i<3; i++) {
		if (edi->flags & EVBOX_F_FILTERGLOBAL) {
			ret = evbox_filterinsert (ev2, edi->owner);
		} else {
			ret = evbox_insert (ev2, NULL, edi->owner);
		}
		if (ret != RERR_FULL) {
			if (!RERR_ISOK(ret)) {
				evpool_release (ev2);
				FRLOGF (LOG_ERR, "error inserting event: %s", rerr_getstr3(ret));
			}
			return ret;
		}
		tmo_sleep (10000LL);	/* 10 ms */
		/* and try again */
	}
	FRLOGF (LOG_ERR, "dropping event - eventbox is full");
	if (c_dropIsFatal) {
		eddi_sendfatalerror ("dropping event");
	}
	return RERR_FULL;
}





static
int
read_config ()
{
	cf_begin_read ();
	c_filterid = cf_getarr ("autofilter", fr_getprog());
	c_distout = cf_isyes (cf_getval2 ("SendInternalEvents", "no"));
	c_debugev = cf_isyes (cf_getval2 ("DebugEvents", "no"));
	c_debugev2stdout = cf_isyes (cf_getval2 ("DebugEventsToStdout", "no"));
	c_dropIsFatal = cf_isyes (cf_getval2 ("DroppingEventsIsFatalError", "no"));
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
