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


#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <pthread.h>


#include <fr/base.h>
#include "evpool.h"
#include "parseevent.h"


static int config_read = 0;
static int read_config ();
static int c_szevqueue = 50;
static int c_poolsize = 100;
static int c_qgrow = 0;



struct evpool {
	struct event	event;	/* must be first */
	int				used;
};

static struct evpool		*evpool = NULL;
static int					evpoollen = 0;
static int					evpoolsz = 0;
static int					evpoolinit = 0;
static pthread_mutex_t	mutex = PTHREAD_MUTEX_INITIALIZER;

int
evpool_init ()
{
	int	i;

	CF_MAYREAD;
	if (evpoolinit) return RERR_OK;
	if (pthread_mutex_lock (&mutex) != 0) return RERR_SYSTEM;
	evpoollen = (c_poolsize > 0) ? c_poolsize : (2 * c_szevqueue);
	if (evpoollen < 10) evpoollen = 10;
	evpoolsz = sizeof (struct evpool) * evpoollen;
	evpool = malloc (evpoolsz);
	if (!evpool) {
		pthread_mutex_unlock (&mutex);
		return RERR_NOMEM;
	}
	bzero (evpool, evpoolsz);
	for (i=0; i<evpoollen; i++) {
		ev_new (&(evpool[i].event));
	}
	pthread_mutex_unlock (&mutex);
	evpoolinit = 1;
	return RERR_OK;
}


int
evpool_acquire (event)
	struct event	**event;
{
	int				ret, i;
	struct event	*ev;

	if (!event) return RERR_PARAM;
	if (!evpoolinit && evpool_init () < 0) return RERR_NOTINIT;
	if (pthread_mutex_lock (&mutex) != 0) return RERR_SYSTEM;
	for (i=0; i<evpoollen; i++) {
		if (evpool[i].used) continue;
		if (event) {
			evpool[i].used = 1;
			*event = &(evpool[i].event);
		}
		pthread_mutex_unlock (&mutex);
		return RERR_OK;
	}
	pthread_mutex_unlock (&mutex);
	if (c_qgrow) {
		ev = malloc (sizeof(struct event));
		if (!ev) return RERR_NOMEM;
		ret = ev_new (ev);
		if (!RERR_ISOK(ret)) {
			free (ev);
			return ret;
		}
		*event = ev;
		return RERR_OK;
	}
	return RERR_NOT_FOUND;
}


int
evpool_release (event)
	struct event	*event;
{
	if (!evpoolinit && evpool_init () < 0) return RERR_NOTINIT;
	if (!event) return RERR_PARAM;
	if ((char*)event >= (char*)evpool && (char*)event < (((char*)evpool) + evpoolsz)) {
		/* can be released */
		ev_clear (event);
		((struct evpool*)event)->used = 0;
	} else {
		/* need to be freed */
		ev_free (event);
		free (event);
	}
	return RERR_OK;
}

int
evpool_isin (event)
	struct event	*event;
{
	if (!event) return RERR_PARAM;
	if (!evpoolinit && evpool_init () < 0) return RERR_NOTINIT;
	if ((char*)event >= (char*)evpool && 
				(char*)event < (((char*)evpool) + evpoolsz)) {
		return RERR_OK;
	} else {
		return RERR_FAIL;
	}
}








static
int
read_config ()
{
	cf_begin_read ();
	c_szevqueue = cf_atoi (cf_getarr2 ("SizeEventQueue", fr_getprog(), "50"));
	if (c_szevqueue < 5) c_szevqueue = 5;	/* min. size */
	c_poolsize = cf_atoi (cf_getarr2 ("SizeEventPool", fr_getprog(), "-1"));
	c_qgrow = cf_isyes (cf_getarr2 ("EventPoolGrow", fr_getprog (), "yes"));
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
