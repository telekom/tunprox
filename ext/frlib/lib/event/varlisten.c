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
#include <errno.h>
#include <sys/types.h>
#include <signal.h>

#include <fr/base.h>
#include <fr/thread.h>

#include "varmon.h"
#include "varlisten.h"


static int varmon_loop ();
static int read_config ();


static tmo_t	c_timeout = 100000LL;
static int		config_read = 0;
static int		vlthr_id = -1;

static struct frthr vlthr = {
	.name = "varlisten",
	.main = varmon_loop,
};


FRTHR_COND_T (int, waitpoll)	cond = FRTHR_COND_INIT;

int
varlisten_start ()
{
	int	ret;

	ret = frthr_start (&vlthr, 1000000LL);
	if (!RERR_ISOK(ret)) {
		FRLOGF (LOG_ERR, "error starting worker thread: %s", rerr_getstr3(ret));
		return ret;
	}
	vlthr_id = ret;
	return RERR_OK;
}


int
varlisten_stop ()
{
	int	ret;

	if (vlthr_id > 0) {
		ret = frthr_destroy (vlthr_id);
		if (!RERR_ISOK(ret)) return ret;
	}
	vlthr_id = -1;
	return RERR_OK;
}




int
varlisten_actualize ()
{
	return varlisten_actualize2 (500000LL);
}

int
varlisten_actualize2 (tout)
	tmo_t	tout;
{
	FRTHR_COND_SET(cond, cond.waitpoll=1);
	FRTHR_COND_WAIT(cond, 500000LL, cond.waitpoll);
	return RERR_OK;
}

static
int
varmon_loop ()
{
	int	ret;
	int	lasterr=0;
	int	waitpoll=0;

	while (1) {
		ret = varmon_pollall (c_timeout);
		FRTHR_COND_GET(cond, waitpoll = cond.waitpoll);
		if (waitpoll) {
			varmon_pollall (0);
			FRTHR_COND_SET(cond,cond.waitpoll = 0);
			FRTHR_COND_SIGNAL(cond);
		}
		if (!RERR_ISOK(ret) && ret != RERR_TIMEDOUT) {
			if (ret != lasterr) {
				FRLOGF (LOG_WARN, "error in poll: %s", rerr_getstr3(ret));
			}
			tmo_sleep (c_timeout/2);
			lasterr = ret;
		} else {
			lasterr = 0;
		}
	}
	return RERR_OK;	/* should never reach here */
}








static
int
read_config ()
{
	cf_begin_read ();
	c_timeout = cf_atotm (cf_getarr2 ("timeout", "varlistener", "0.1"));
	config_read = 1;
	cf_end_read_cb (&read_config);
	return 0;
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
