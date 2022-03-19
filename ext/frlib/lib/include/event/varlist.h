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

#ifndef _R__FRLIB_LIB_EVENT_VARLIST_H
#define _R__FRLIB_LIB_EVENT_VARLIST_H

#include <stdint.h>
#include <fr/event/parseevent.h>

#ifdef __cplusplus
extern "C" {
#endif


#define EVVAR_F_SILENT	0x10000000
#define EVVAR_F_VOID		0x20000000

/* flags for evvar_newlist */
#define EVVAR_F_PROTECT	0x40000000

/* flags for evvar_actshadow */
#define EVVAR_F_FORCE	0x01


int evvar_newlist ();
int evvar_newlist2 (int flags);
int evvar_rmlist (int id);
int evvar_clearlist (int id);
int evvar_setvar_s (int id, const char *var, int idx1, int idx2, const char *val, int flags);
int evvar_setvar_i (int id, const char *var, int idx1, int idx2, int64_t val, int flags);
int evvar_setvar_d (int id, const char *var, int idx1, int idx2, tmo_t val, int flags);
int evvar_setvar_t (int id, const char *var, int idx1, int idx2, tmo_t val, int flags);
int evvar_setvar_f (int id, const char *var, int idx1, int idx2, double val, int flags);
int evvar_setvar_v (int id, const char *var, int idx1, int idx2, struct ev_val val, int flags);
int evvar_setevent (int id, struct event *ev, int flags);
int evvar_setevstr (int id, char *evstr, int flags);
int evvar_addbuf (int id, char *buf, size_t buflen);
int evvar_getsinglevar_s (const char **outval, int id, const char *var, int idx1, int idx2);
int evvar_getsinglevar_i (int64_t *outval, int id, const char *var, int idx1, int idx2);
int evvar_getsinglevar_d (tmo_t *outval, int id, const char *var, int idx1, int idx2);
int evvar_getsinglevar_t (tmo_t *outval, int id, const char *var, int idx1, int idx2);
int evvar_getsinglevar_f (double *outval, int id, const char *var, int idx1, int idx2);
int evvar_getsinglevar_v (struct ev_val *outval, int id, const char *var, int idx1, int idx2);
int evvar_getvarpos_v (const char **var, int *idx1, int *idx2, struct ev_val *outval, int id, int pos);
int evvar_getnum (int id);
int evvar_getvar (struct event *ev, int id, const char *var, int idx1, int idx2, int flags);
int evvar_getallvar (struct event *ev, int id, int flags);
int evvar_getvarlist (struct event *oev, int id, struct event *ev, int flags);
int evvar_getvarstr (char **evstr, int id, const char *var, int idx1, int idx2, const char *evname);
int evvar_getallvarstr (char **evstr, int id, const char *evname);
int evvar_typeof (int id, const char *var, int idx1, int idx2);
int evvar_ntypeof (int id, const char *var, int idx1, int idx2);
int evvar_isnano (int id, const char *var, int idx1, int idx2);
int evvar_rmvar (int id, const char *var, int idx1, int idx2);
int evvar_rmvarlist (int id, struct event*);
int evvar_readfile (int id, const char *fname, int flags);
int evvar_writefile (int id, const char *fname);
int evvar_setautowrite (int id, const char *fname, int flags);

/* shadow list */
int evvar_getuniqid ();
int evvar_actshadow (int id, int shid, int flags);
int evvar_setactshadow (int id, int shid);
int evvar_rmshadow (int id, int shid);
int evvar_rmshadowall (int id);
int evvar_getshadowvar_s (const char **outval, int id, int shid, const char *var, int idx1, int idx2);
int evvar_getshadowvar_i (int64_t *outval, int id, int shid, const char *var, int idx1, int idx2);
int evvar_getshadowvar_f (double *outval, int id, int shid, const char *var, int idx1, int idx2);
int evvar_getshadowvar_d (tmo_t *outval, int id, int shid, const char *var, int idx1, int idx2);
int evvar_getshadowvar_t (tmo_t *outval, int id, int shid, const char *var, int idx1, int idx2);
int evvar_getshadowvar_v (struct ev_val *outval, int id, int shid, const char *var, int idx1, int idx2);
int evvar_getshadowvarpos_v (const char **var, int *idx1, int *idx2, struct ev_val *outval, int id, int shid, int pos);
int evvar_getshadownum (int id, int shid);

/* add / remove gvarlist id to trigger */
int evvar_addgvl (int id, int gid);
int evvar_rmgvl (int id, int gid);

/* set/get varmon name */
int evvar_setvarmon (int id, const char *vmon);
int evvar_getvarmon (const char **vmon, int id);







#ifdef __cplusplus
}	/* extern "C" */
#endif











#endif	/* _R__FRLIB_LIB_EVENT_VARLIST_H */


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
