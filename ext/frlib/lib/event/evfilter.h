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

#ifndef _R__FRLIB_LIB_EVENT_EVFILTER_H
#define _R__FRLIB_LIB_EVENT_EVFILTER_H



#ifdef __cplusplus
extern "C" {
#endif

#define EVF_T_ACCEPT	1
#define EVF_T_REJECT	2
#define EVF_T_DROP	3
/* other targets do return EVT_T_ACCEPT */

struct event;
/* the main branch will be in the first evlist, all other branches 
 * - if any - follow in a linked list accessible with next.
 * the return value of evf_apply on success is the target field
 * of the main branch.
 * the event in the first evlist structure is the event passed
 * to evf_apply, or a copy of it if EVF_F_COPY was used.
 * When there are no branches and EVF_F_COPY is not given the
 * evlist parametr can miss.
 * Note: EVF_F_COPY and branches are not supported yet.
 */
struct evlist {
	struct event	*event;
	int				target;	/* one of EVF_T_... */
	int				flags;	/* for internal use only */
	struct evlist	*next;
};

#define EVF_F_NONE		0x0000
#define EVF_F_IN			0x0001
#define EVF_F_OUT			0x0002
#define EVF_F_BOTH		(EVF_F_IN | EVF_F_OUT)
#define EVF_F_COPY		0x0004
#define EVF_F_FREE		0x0008	/* compile only */
#define EVF_F_STRICT		0x0010	/* compile only */
#define EVF_F_VERBOSE	0x0020
#define EVF_F_QUIET		0x0040
#define EVF_F_NOCPY		0x0080
/* note: using EVF_F_NOCPY can speed up event filter processing, but
 *       it must be guaranteed, that the event on which the filter
 *       is applied is not accessed after the event filter was deleted.
 */
#define EVF_F_NOBRANCH	0x0100	/* apply only */
#define EVF_F_OUTTARGET	0x0200	/* apply only */
#define EVF_F_USEPOOL	0x0400	/* apply only */
#define EVF_F_NOPOOL		0x0800	/* apply only */
#define EVF_F_NOEVFREE	0x1000	/* evlist free only */


int evf_new ();
int evf_release (int id);
int evf_reference (int id);

int evf_protect (int id);
int evf_unprotect (int id);

int evf_addfilter (int id, const char *name, int flags);
int evf_addfile (int id, const char *fname, int flags);
int evf_addbuf (int id, char *buf, int flags);
int evf_addconstbuf (int id, const char *buf, int flags);

int evf_apply (struct evlist*, int id, struct event*, int flags);
int evf_evlistfree (struct evlist*, int flags);










#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_EVENT_EVFILTER_H */

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
