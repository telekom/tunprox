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
 * Portions created by the Initial Developer are Copyright (C) 2003-2017
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#ifndef _R__FRLIB_THREAD_THRCOND_H
#define _R__FRLIB_THREAD_THRCOND_H


#ifdef __cplusplus
extern "C" {
#endif


#include <pthread.h>


#define FRTHR_COND_T(type,var) \
struct { \
	pthread_mutex_t	mutex; \
	pthread_cond_t		cond; \
	type					var; \
}

#define FRTHR_COND_INIT { \
		.mutex = PTHREAD_MUTEX_INITIALIZER, \
		.cond = PTHREAD_COND_INITIALIZER, \
}

#define FRTHR_COND_LOCK(cond) \
	do { pthread_mutex_lock (&((cond).mutex)); } while (0)

#define FRTHR_COND_UNLOCK(cond) \
	do { pthread_mutex_unlock (&((cond).mutex)); } while (0)

#define FRTHR_COND_SET(cond,cmd) \
	do { FRTHR_COND_LOCK(cond); cmd; FRTHR_COND_UNLOCK(cond); } while (0)

#define FRTHR_COND_GET(cond,cmd) FRTHR_COND_SET(cond,cmd)


#define FRTHR_COND_WAIT_CMD(_cond,tout,condcheck,cmd) do { \
	tmo_t	start; \
	TMO_START(start, tout); \
	FRTHR_COND_LOCK(_cond); \
	while ((condcheck)) { \
		if ((tout) < 0) { \
			pthread_cond_wait(&((_cond).cond), &((_cond).mutex));  \
		} else { \
			struct timespec	tval; \
			tmo_t					now; \
			now = start + (tout) + 2; \
			TMO_TOTSPEC (tval, now); \
			pthread_cond_timedwait(&((_cond).cond), &((_cond).mutex), &tval);  \
			if (!TMO_CHECK (start,(tout))) { \
				FRTHR_COND_UNLOCK(_cond); \
				return RERR_TIMEDOUT; \
			} \
		} \
	} \
	cmd; \
	FRTHR_COND_UNLOCK(_cond); \
} while(0)


#define FRTHR_COND_WAIT(_cond,tout,condcheck)  FRTHR_COND_WAIT_CMD(_cond,tout,condcheck,)


#define FRTHR_COND_SIGNAL(_cond) pthread_cond_signal (&((_cond).cond))
#define FRTHR_COND_BROADCAST(_cond) pthread_cond_broadcast (&((_cond).cond))


#define FRTHR_COND_DESTROY(_cond) do { \
		pthread_mutex_destroy (&((_cond).mutex)); \
		pthread_cond_destroy (&((_cond).cond)); \
	} while (0)






#ifdef __cplusplus
}	/* extern "C" */
#endif








#endif	/* _R__FRLIB_THREAD_THRCOND_H */

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
