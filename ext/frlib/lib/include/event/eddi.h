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

#ifndef _R__FRLIB_LIB_EVENT_EDDI_H
#define _R__FRLIB_LIB_EVENT_EDDI_H


#include <pthread.h>

#include <fr/base/tmo.h>

#ifdef __cplusplus
extern "C" {
#endif


struct event;


int eddi_start ();
int eddi_finish ();
int eddi_isstarted ();
int eddi_gettid (pthread_t *tid);

int eddi_sendev (struct event*);
int eddi_sendstr (char *evstr, int flags);
int eddi_distev (struct event*);
int eddi_diststr (char *evstr, int flags);

int eddi_addfilter (int filterid);
int eddi_addfilterid (const char *filter, int flags);

int eddi_debugevent (struct event *event);




/*
 * Sends a trigger event to anyone who cares (in our case, the RTF does).
 * This is a fire-and-forget function, there is no need for any
 * prior eddi-initialization.
 *
 * \param errorname An errorname a participant of the eventsystem reacts upon.
 */
int eddi_sendfatalerror(const char *errorname);













#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_EVENT_EDDI_H */


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
