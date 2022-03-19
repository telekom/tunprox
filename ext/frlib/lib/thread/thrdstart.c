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

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <stdint.h>
#include <pthread.h>
#include <errno.h>
extern int errno;
#include <setjmp.h>

#include <fr/base.h>
#include "thrdstart.h"


#define FRTHR_STATE_VERGIN			0
#define FRTHR_STATE_INIT			1
#define FRTHR_STATE_CHILD_INIT	2
#define FRTHR_STATE_CLEANUP		3
#define FRTHR_STATE_DEAD			4

struct frthr_int {
	int					id;
	int					fid;
	struct frthr		frthr;
	pthread_t			tid;
	pthread_t			ftid;
	int					state;
	pthread_mutex_t	mutex;
	pthread_cond_t		cond;
	int					childret;
	jmp_buf				env;
};

static int dostart1 (struct frthr_int*, tmo_t);
static int dostart2 (struct frthr_int*, tmo_t);
static int wait_thread (struct frthr_int*, tmo_t);
static void *run_thread (void*);
static int dorun_thread (struct frthr_int*);
static void thr_cleanup (void*);
static int do_cleanup (struct frthr_int*);


static pthread_mutex_t	frthr_mutex = PTHREAD_MUTEX_INITIALIZER;
static struct tlst	frthr_list = TLST_INIT_T (void*);

extern int _frthr_isinit;

#define FRTHR_INIT do { \
	if (!_frthr_isinit) { \
		int _ret; \
		_ret = frthr_init(); \
		if (!RERR_ISOK(_ret)) return _ret; \
	} } while (0)

int
frthr_init ()
{
	int					ret;
	struct frthr_int	*thr;

	if (_frthr_isinit) return RERR_OK;
	pthread_mutex_lock (&frthr_mutex);
	if (_frthr_isinit) {
		pthread_mutex_unlock (&frthr_mutex);
		return RERR_OK;
	}
	thr = malloc (sizeof (struct frthr_int));
	if (!thr) {
		pthread_mutex_unlock (&frthr_mutex);
		return RERR_NOMEM;
	}
	bzero (thr, sizeof (struct frthr_int));
	thr->id = 0;
	thr->fid = 0;
	thr->tid = pthread_self();
	thr->ftid = pthread_self();
	thr->state = FRTHR_STATE_INIT;
	thr->frthr = (struct frthr) {
						.name = "main",
						.id = 0,
						.fid = 0,
						.priv = thr,
					};
	ret = TLST_ADD (frthr_list, thr);
	if (ret != 0) {
		free (thr);
		TLST_FREE(frthr_list);
		frthr_list = TLST_INIT_T (void*);
		pthread_mutex_unlock (&frthr_mutex);
		return ret > 0 ? RERR_INTERNAL : ret;
	}
	frthr_self = &(thr->frthr);
	_frthr_isinit = 1;
	pthread_mutex_unlock (&frthr_mutex);
	return RERR_OK;
}


int
frthr_id2tid(tid, id)
	pthread_t	*tid;
	int			id;
{
	struct frthr_int	*thr;
	int					ret;

	if (!tid) return RERR_PARAM;
	pthread_mutex_lock (&frthr_mutex);
	ret = TLST_GET (thr, frthr_list, id);
	if (!RERR_ISOK(ret)) goto errout;
	if (!thr) { ret = RERR_NOT_FOUND; goto errout; }
	*tid = thr->tid;
errout:
	pthread_mutex_unlock (&frthr_mutex);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "could not get thread id %d: %s\n", id, rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}

int
frthr_getftid(tid)
	pthread_t	*tid;
{
	if (!tid) return RERR_PARAM;
	if (!frthr_self) return RERR_NOT_FOUND;
	*tid = ((struct frthr_int*)frthr_self->priv)->ftid;
	return RERR_OK;
}

int
frthr_start (frthr, tout)
	const struct frthr	*frthr;
	tmo_t						tout;
{
	struct frthr_int	*thr;
	int					ret;

	if (!frthr) return RERR_PARAM;
	FRTHR_INIT;
	thr = malloc (sizeof (struct frthr_int));
	if (!thr) return RERR_NOMEM;
	bzero (thr, sizeof (struct frthr_int));
	thr->frthr = *frthr;
	thr->state = FRTHR_STATE_VERGIN;
	pthread_mutex_lock (&frthr_mutex);
	ret = TLST_ADD (frthr_list, thr);
	pthread_mutex_unlock (&frthr_mutex);
	if (!RERR_ISOK(ret)) {
		free (thr);
		return ret;
	}
	thr->id = ret;
	thr->fid = frthr_self->id;
	thr->frthr.id = thr->id;
	thr->frthr.fid = thr->fid;
	thr->frthr.priv = thr;
	ret = dostart1 (thr, tout);
	if (!RERR_ISOK(ret)) {
		pthread_mutex_lock (&frthr_mutex);
		TLST_REMOVE (frthr_list, thr->id, TLST_F_NONE);
		pthread_mutex_unlock (&frthr_mutex);
		free (thr);
		return ret;
	}
	return thr->id;
}

int
frthr_exit (ret)
	int	ret;
{
	struct frthr_int	*thr;

	if (!frthr_self) return RERR_INTERNAL;
	thr = frthr_self->priv;
	if (!thr) return RERR_INTERNAL;
	if (ret > 0) ret++;
	longjmp (thr->env, ret);
	/* should not reach here */
	return RERR_INTERNAL;
}

int
frthr_addme (name)
	const char	*name;
{
	struct frthr_int	*thr;
	int					ret;

	FRTHR_INIT;
	if (frthr_self) return RERR_ALREADY_EXIST;
	thr = malloc (sizeof (struct frthr_int));
	if (!thr) return RERR_NOMEM;
	bzero (thr, sizeof (struct frthr_int));
	thr->state = FRTHR_STATE_VERGIN;
	pthread_mutex_lock (&frthr_mutex);
	ret = TLST_ADD (frthr_list, thr);
	pthread_mutex_unlock (&frthr_mutex);
	if (!RERR_ISOK(ret)) {
		free (thr);
		return ret;
	}
	thr->id = ret;
	thr->fid = -1;	/* should be changed */
	thr->frthr = (struct frthr) {
						.name = name,
						.id = thr->id,
						.fid = thr->fid,
						.priv = thr,
					};
	thr->mutex = (pthread_mutex_t) PTHREAD_MUTEX_INITIALIZER;
	thr->cond = (pthread_cond_t)PTHREAD_COND_INITIALIZER;
	thr->tid = pthread_self();
	thr->frthr.name = name;
	thr->state = FRTHR_STATE_INIT;
	return RERR_OK;
}

struct frthr *
__attribute__ ((noinline))
frthr_getself()
{
	return frthr_self;
}

static
int
dostart1 (thr, tout)
	struct frthr_int	*thr;
	tmo_t					tout;
{
	int	ret;

	if (!thr) return RERR_PARAM;
	ret = pthread_mutex_init (&(thr->mutex), NULL);
	if (ret != 0) {
		errno = ret;
		return RERR_SYSTEM;
	}
	ret = pthread_cond_init (&(thr->cond), NULL);
	if (ret != 0) {
		pthread_mutex_destroy (&(thr->mutex));
		errno = ret;
		return RERR_SYSTEM;
	}
	thr->ftid = pthread_self();
	ret = dostart2 (thr, tout);
	if (!RERR_ISOK(ret)) {
		pthread_mutex_destroy (&(thr->mutex));
		pthread_cond_destroy (&(thr->cond));
		FRLOGF (LOG_ERR, "error starting thread(): %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
}


static
int
dostart2 (thr, tout)
	struct frthr_int	*thr;
	tmo_t					tout;
{
	int				ret;
	pthread_attr_t	attr;

	if (!thr) return RERR_PARAM;
	thr->childret = RERR_OK;
	ret = pthread_attr_init (&attr);
	if (ret != 0) {
		errno = ret;
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error creating thread attributes: %s",
					rerr_getstr3 (ret));
		return ret;
	}
	pthread_attr_setdetachstate (&attr, PTHREAD_CREATE_DETACHED);
	ret = pthread_create (&(thr->tid), &attr, run_thread, thr);
	pthread_attr_destroy (&attr);
	if (ret != 0) {
		errno = ret;
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "error initializing thread: %s", rerr_getstr3 (ret));
		return ret;
	}
	ret = wait_thread (thr, tout);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error waiting for child thread: %s", 
						rerr_getstr3 (ret));
		return ret;
	}
	return RERR_OK;
}



static
int
wait_thread (thr, tout)
	struct frthr_int	*thr;
	tmo_t					tout;
{
	struct timespec	tval;
	tmo_t					start, now;

	if (!thr) return RERR_PARAM;
	TMO_START (start, tout);
	pthread_mutex_lock(&(thr->mutex)); 
	while (thr->state < FRTHR_STATE_CHILD_INIT) {
		if (tout < 0) {
			pthread_cond_wait(&(thr->cond), &(thr->mutex)); 
		} else {
			now = tout - tmo_now();
			if (now < 2) now = 2;
			TMO_TOTSPEC (tval, now);
			pthread_cond_timedwait(&(thr->cond), &(thr->mutex), &tval); 
			if (!TMO_CHECK (start,tout)) {
				pthread_mutex_unlock(&(thr->mutex));
				return RERR_TIMEDOUT;
			}
		}
	}
	pthread_mutex_unlock(&(thr->mutex));
	return thr->childret;
}



static
void*
run_thread (thr)
	void	*thr;
{
	int	ret;

	ret = dorun_thread (thr);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error executing thread: %s", rerr_getstr3 (ret));
	}
	return NULL;
}

static
int
dorun_thread (thr)
	struct frthr_int	*thr;
{
	int	ret = RERR_OK;

	if (!thr) return RERR_PARAM;

	/* initialize thread */
	thr->tid = pthread_self();
	frthr_self = &(thr->frthr);
	if (frthr_self->init) {
		ret = frthr_self->init ();
	}
	pthread_mutex_lock(&(thr->mutex)); 
	thr->childret = ret;
	thr->state = RERR_ISOK(ret) ? FRTHR_STATE_CHILD_INIT : FRTHR_STATE_DEAD;
	pthread_mutex_unlock(&(thr->mutex)); 
	pthread_cond_signal(&(thr->cond));
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error initializing thread: %s" ,rerr_getstr3(ret));
		return ret;
	}

	pthread_cleanup_push (thr_cleanup, thr);

	if (!(ret = setjmp(thr->env))) {
		/* now run thread */
		if (frthr_self->main) {
			ret = frthr_self->main ();
			if (!RERR_ISOK(ret)) {
				FRLOGF (LOG_ERR, "error running thread: %s", rerr_getstr3(ret));
			}
		}
	} else {
		if (ret > 0) ret--;
	}

	pthread_cleanup_pop (1);

	return ret;
}



static
void
thr_cleanup (arg)
	void	*arg;
{
	int	ret;

	ret = do_cleanup(arg);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error in cleanup: %s", rerr_getstr3(ret));
	}
}

static
int
do_cleanup (thr)
	struct frthr_int	*thr;
{
	int	ret = RERR_OK;

	if (!thr) return RERR_PARAM;

	pthread_mutex_lock(&(thr->mutex)); 
	thr->state = FRTHR_STATE_CLEANUP;
	pthread_mutex_unlock(&(thr->mutex)); 

	/* cleanup thread */
	if (frthr_self->cleanup) {
		ret = frthr_self->cleanup ();
		if (!RERR_ISOK(ret)) {
			FRLOGF (LOG_WARN, "error cleaning up thread: %s", rerr_getstr3(ret));
		}
	}
	pthread_mutex_lock(&(thr->mutex)); 
	thr->childret = ret;
	thr->state = FRTHR_STATE_DEAD;
	pthread_mutex_unlock(&(thr->mutex)); 

	/* cleanup frthr_int structure */
	pthread_mutex_lock (&frthr_mutex);
	ret = TLST_REMOVE (frthr_list, thr->id, TLST_F_NONE);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error removing thread struct from list: %s",
					rerr_getstr3(ret));
		return ret;
	}
	frthr_self = NULL;
	pthread_mutex_unlock (&(frthr_mutex));
	pthread_mutex_destroy (&(thr->mutex));
	pthread_cond_destroy (&(thr->cond));
	free (thr);
	return RERR_OK;
}


int
frthr_destroy (id)
	int	id;
{
	struct frthr_int	*thr;
	int					ret;

	if (id < 0) return RERR_OK;	/* ignore */
	if (id == 0) return RERR_FORBIDDEN;	/* main thread cannot be destroyed */
	if (id == frthr_self->id) {	/* we are destroying ourself */
		pthread_exit (NULL);
		return RERR_OK;
	}
	pthread_mutex_lock(&frthr_mutex);
	ret = TLST_GET (thr, frthr_list, id);
	if (!RERR_ISOK(ret)) goto err_out;
	if (!thr) { 
		ret = RERR_NOT_FOUND;
		goto err_out;
	}
	ret = pthread_cancel (thr->tid);
	if (ret != 0) {
		errno = ret;
		ret = RERR_SYSTEM;
		FRLOGF (LOG_ERR, "could not cancel thread: %s", rerr_getstr3(ret));
		goto err_out;
	}
err_out:
	pthread_mutex_unlock (&frthr_mutex);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error destroying thread: %s", rerr_getstr3(ret));
		return ret;
	}
	return RERR_OK;
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
