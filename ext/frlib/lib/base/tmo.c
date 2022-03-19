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
//#define _XOPEN_SOURCE
#define __USE_XOPEN
#include <time.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <ctype.h>
#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>
#include <stdint.h>



#include "tmo.h"
#include "errors.h"
#include "dlrt.h"
#include "config.h"


static int	c_thack32 = 1;
static int	config_read = 0;
static int read_config ();




#ifdef SunOS
extern int usleep (useconds_t);
#endif

int
tmo_sleep (usec)
	tmo_t	usec;
{
	struct timespec	ts;
	int					ret;

	ts.tv_sec = usec / 1000000LL;
	ts.tv_nsec = (usec % 1000000LL) * 1000LL;

	ret = EXTCALL(nanosleep) (&ts, NULL);
	if (ret == -2) {
		/* we cannot find nanosleep, use usleep */
		ret = usleep ((useconds_t) usec);
	}
	return ret;
}

/* sleeps msec milliseconds */
int
tmo_msleep (msec)
	unsigned long	msec;
{
	return tmo_sleep (((tmo_t)msec)*1000LL);
}


int
tmo_sleepns (nsec)
	tmo_t	nsec;
{
	struct timespec	ts;
	int					ret;

	ts.tv_sec = nsec / 1000000000LL;
	ts.tv_nsec = (nsec % 1000000000LL);

	ret = EXTCALL(nanosleep) (&ts, NULL);
	if (ret == -2) return RERR_NOT_FOUND;
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}



tmo_t
tmo_now ()
{
	struct timeval	tv;
	tmo_t				now;

	CF_MAYREAD
	if (gettimeofday (&tv, NULL) < 0) return -1;
	if (sizeof(time_t)==4 && c_thack32) {
		now = (uint32_t)(tv.tv_sec);
	} else {
		now = tv.tv_sec;
	}
	now *= 1000000;
	now += tv.tv_usec;
	return now;
}


tmo_t
tmo_nowns ()
{
	struct timespec	ts;
	tmo_t					now;
	int					ret;

	CF_MAYREAD
	ret = EXTCALL(clock_gettime) (CLOCK_REALTIME, &ts);
	if (ret < 0) {
		now = tmo_now ();
		if (now != -1) now *= 1000LL;
		return now;
	}
	if (sizeof(time_t)==4 && c_thack32) {
		now = (uint32_t)(ts.tv_sec);
	} else {
		now = ts.tv_sec;
	}
	now *= 1000000000LL;
	now += ts.tv_nsec;
	return now;
}





static
int
read_config ()
{
	cf_begin_read ();
	c_thack32 = cf_isyes(cf_getval2 ("TimeHackOn32bit", "yes"));
	config_read = 1;
	cf_end_read_cb (&read_config);
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
