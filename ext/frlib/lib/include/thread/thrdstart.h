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

#ifndef _R__FRLIB_THREAD_THRDSTART_H
#define _R__FRLIB_THREAD_THRDSTART_H



#ifdef __cplusplus
extern "C" {
#endif

#include <pthread.h>
#include <fr/base/tmo.h>

struct frthr {
	/* the following shall be set by the caller */
	int			(*init) ();
	int			(*main) ();
	int 			(*cleanup) ();
	const char	*name;
	int			flags;

	/* the following are parameters that can be passed to init, main and cleanup */
	int			iarg[3];
	void			*parg[3];

	/* the following are set automatically - do only read them */
	int			id;		/* thread id (internally to frlib) */
	int			fid;		/* thread id of father */

	/* the following is used internally only - don't touch nor use it */
	void			*priv;
};


extern __thread struct frthr	*frthr_self;

int frthr_init ();
int frthr_isinit ();
int frthr_start (const struct frthr*, tmo_t);
int frthr_destroy (int tid);
int frthr_exit (int ret);
int frthr_addme (const char *name);

int frthr_id2tid (pthread_t *tid, int id);
int frthr_getftid (pthread_t *tid);

struct frthr *frthr_getself();


#ifdef __cplusplus
}	/* extern "C" */
#endif




#endif	/* _R__FRLIB_THREAD_THRDSTART_H */

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
