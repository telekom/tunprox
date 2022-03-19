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

#ifndef _R__FRLIB_LIB_EVENT_EVBOX_H
#define _R__FRLIB_LIB_EVENT_EVBOX_H


#ifdef __cplusplus
extern "C" {
#endif

struct event;

#include <pthread.h>
#include <fr/base/tmo.h>
#include <fr/base/tlst.h>

#define EVBOX_F_GETGLOBAL		0x01	/* receive events from outside world */
#define EVBOX_F_FILTERGLOBAL	0x02	/* filter events with thread filter, too */
#define EVBOX_F_DISABLED		0x04	/* this eventbox doesn't receive events */
#define EVBOX_F_IAMEDDI			0x08	/* MUST only be used by EDDI thread */


int evbox_init (int flags);
int evbox_finish ();

int evbox_get (const char *name);
int evbox_exist (const char *name);
int evbox_search (const char *name);
int evbox_close (int id);

const char	* evbox_getname (int id);

int evbox_settrigcb (int id, int (*cb)(int, void*, int), void *parg, int iarg);
int evbox_setglobtrigcb (int (*cb)(int, void*, int), void *parg, int iarg);
int evbox_gettrigfd (int id);
int evbox_getglobtrigfd ();
int evbox_noglobtrigger (int id, int off);

int evbox_wait (int id, tmo_t timeout);
int evbox_waitany (tmo_t timeout);
int evbox_numev (int id);

int evbox_pop (struct event **oevent, int id);
int evbox_pop2 (struct event **oevent, pthread_t *tid, int id);
int evbox_popstr (char **ostr, int id);
int evbox_release (struct event*);

int evbox_insert (struct event*, const char *boxname, pthread_t receiver);
int evbox_insertstr (const char *evstr, int flags, const char *boxname, pthread_t receiver);
int evbox_filterinsert (struct event*, pthread_t receiver);
int evbox_filterinsertstr (const char *evstr, int flags, pthread_t receiver);

int evbox_addfilter (int fid);
int evbox_enable ();
int evbox_disable ();
int evbox_adddistgrp (const char*);
int evbox_rmdistgrp (const char*);



/* the following is used internally by the eddi subsystem
   don't use it outside this scope
 */

struct evbox_eddinfo {
	pthread_t	owner;
	struct tlst	distgrp;
	int			filterid;
	int			flags;
};

struct evbox_eddinfo *evbox_getnextthrd ();








#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_EVENT_EVBOX_H */

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
