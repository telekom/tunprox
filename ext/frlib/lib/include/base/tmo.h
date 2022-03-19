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

#ifndef _R__FRLIB_LIB_BASE_TMO_H
#define _R__FRLIB_LIB_BASE_TMO_H

#include <stdint.h>
#include <inttypes.h>

typedef int64_t		tmo_t;

#define PRtmo  PRId64


#ifdef __cplusplus
extern "C" {
#endif


int tmo_msleep (unsigned long msec);
int tmo_sleep (tmo_t usec);
int tmo_sleepns (tmo_t nsec);

tmo_t tmo_now ();
tmo_t tmo_nowns ();



#define TMO_TOTVAL(tv,tmo)	{ \
		(tv).tv_sec = (tmo) / 1000000LL; \
		(tv).tv_usec = (tmo) % 1000000LL; \
	}
#define TMO_FROMTVAL(tv) \
		(1000000LL * ((tmo_t)(tv).tv_sec) + ((tmo_t)(tv).tv_usec))

#define TMO_TOTSPEC(ts,tmo)	{ \
		(ts).tv_sec = (tmo) / 1000000LL; \
		(ts).tv_nsec = ((tmo) % 1000000LL)*1000LL; \
	}


#define TMO_START(start,tout) do { if ((tout) >= 0) (start) = tmo_now(); } while (0)
#define TMO_CHECK(start,tout) (((tout) < 0) ? 1 : (((tmo_now() - (start)) > (tout)) ? 0 : 1))
#define TMO_GETTIMEOUT(start,tout,now) (((tout) <= 0) ? (tout) : ((((tout)-(((now)=tmo_now())-(start))) < 0) ? 0 : ((tout) - ((now)-(start)))))

struct tmo_tout {
	tmo_t	tout, start, now;
};
typedef struct tmo_tout	tmo_tout;

#define TOUT_INIT(tout)	((struct tmo_tout) { \
		(tout), \
		(((tout) >= 0) ? tmo_now() : 0), \
		0 })
#define TOUT_CHECK(tos) TMO_CHECK((tos).start, (tos).tout)
#define TOUT_GET(tos) TMO_GETTIMEOUT((tos).start, (tos).tout, (tos).now)

#define TOUT_WHILE(tos) for ((tos).now=1; (tos).now || TOUT_CHECK(tos); (tos).now=0)




#ifdef __cplusplus
}	/* extern "C" */
#endif






#endif	/* _R__FRLIB_LIB_BASE_TMO_H */

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
