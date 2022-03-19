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

#ifndef _R__FRLIB_LIB_EVENT_VARMON_H
#define _R__FRLIB_LIB_EVENT_VARMON_H


#include <fr/base/tmo.h>
#include <fr/event/parseevent.h>


#ifdef __cplusplus
extern "C" {
#endif



#define VARM_F_CB			0x01	/* use callback mech - do not use! */
#define VARM_F_LISTEN	0x02	/* listen only */
#define VARM_F_ADDEMPTY	0x04
#define VARM_F_LOCKED	0x08	/* start in locked state */


/* varmon functions */

int varmon_new (const char *name, int flags);
int varmon_search (const char *name);
int varmon_addgrp (const char *grpname, int flags);
int varmon_close (int id);
int varmon_closeall ();
int varmon_getlist (int id);
int varmon_setactshadow (int id, int shid);
int varmon_isinit (int id);

int varmon_poll (int id, tmo_t tout);
int varmon_pollall (tmo_t tout);
int varmon_evelab (int id, struct event *ev, int flags);

#if 0
int varmon_instcb (int id);
int varmon_rmcb (int id);
#endif

int varmon_addgvl (int id, int gid);
int varmon_rmgvl (int id, int gid);

int varmon_lock (int id);
int varmon_unlock (int id);
int varmon_getall (int id);
int varmon_getall2 (const char *name, const char *requester);
int varmon_setsenderid (int id, const char *senderid);


/* varmon group functions */

int varmongrp_new (const char *grpname, int flags);
int varmongrp_close (int id);
int varmongrp_closeall ();
int varmongrp_setactshadow (int id, int shid);
int varmongrp_isinit (int id);

int varmongrp_add (int id, const char *vmon, int flags);
int varmongrp_addgvl (int id, int gid);
int varmongrp_rmgvl (int id, int gid);

int varmongrp_list2name (char **name, int id, int listid);
int varmongrp_name2list (int *listid, int id, const char *name);
int varmongrp_getvmonid (int id, const char *name);

int varmongrp_lock (int id);
int varmongrp_unlock (int id);
int varmongrp_getlist (int id);
int varmongrp_getall (int id);
int varmongrp_getall2 (const char *grpname, const char *requester);
int varmongrp_setsenderid (int id, const char *senderid);


#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_EVENT_VARMON_H */

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
