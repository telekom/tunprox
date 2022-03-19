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
#include <sys/types.h>
#include <sys/ipc.h>
#ifndef __USE_GNU
# define __USE_GNU
#endif
#include <sys/sem.h>
#include <errno.h>
#include <sys/stat.h>
#include <signal.h>
#include <sys/time.h>


#include "mutex.h"
#include "errors.h"
#include "tmo.h"


static int mysemtimedop (int, struct sembuf*, unsigned, tmo_t);

static
mode_t
mygetumask()
{
	mode_t mask = umask( 0 );
	umask(mask);
	return mask;
}


int
mutex_create (key, flags)
	int	key, flags;
{
	int				id, ret;
	unsigned short	arr[3];
	mode_t			mod, mask;

	if (key < 0) return RERR_PARAM;
	if (key == 0) key = IPC_PRIVATE;
	mod = flags & MUTEX_M_MODMASK;
	if (mod == 0) mod = 0666;
	mask = mygetumask ();
	mod &= ~mask;
	id = semget (key, 3, mod | IPC_CREAT);
	if (id < 0) return RERR_SYSTEM;
	arr[0] = 1;	/* write lock */
	arr[1] = 0;	/* check write lock */
	arr[2] = 0;	/* read lock */
	ret = semctl (id, 0, SETALL, arr);
	if (ret < 0) {
		if (key == IPC_PRIVATE) semctl (id, 0, IPC_RMID, 0);
		return RERR_SYSTEM;
	}
	return id;
}



int
mutex_destroy (id)
	int	id;
{
	int	ret;

	if (id < 0) return RERR_PARAM;
	ret = semctl (id, 0, IPC_RMID, 0);
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}



int
mutex_rdlock (id, tmout, flags)
	int	id, flags;
	tmo_t	tmout;
{
	struct sembuf	sbuf[2];
	int				ret;
	int				sflags=0;

	if (id < 0) return RERR_PARAM;
	bzero (sbuf, sizeof (struct sembuf) * 2);
	if (!(flags & MUTEX_F_NOUNDO)) sflags |= SEM_UNDO;
	/* acquire read and wait for check write lock to be 0 */
	sbuf[0].sem_num = 1;	/* check write lock */
	sbuf[0].sem_op = 0;
	sbuf[0].sem_flg = 0 | ((tmout == 0) ? IPC_NOWAIT : 0);
	sbuf[1].sem_num = 2;	/* read lock */
	sbuf[1].sem_op = 1;
	sbuf[1].sem_flg = sflags;
	ret = mysemtimedop (id, sbuf, 2, tmout);
	if (!RERR_ISOK (ret)) return ret;
	return RERR_OK;
}


int
mutex_wrlock (id, tmout, flags)
	int	id, flags;
	tmo_t	tmout;
{
	struct sembuf	sbuf[3];
	int				ret, sflags=0;

	if (id < 0) return RERR_PARAM;
	bzero (sbuf, sizeof (struct sembuf) * 3);
	if (!(flags & MUTEX_F_NOUNDO)) sflags |= SEM_UNDO;
	/* acquire write lock and wait for read lock */
	sbuf[0].sem_num = 0;
	sbuf[0].sem_op = -1;	/* acquire write lock */
	sbuf[0].sem_flg = sflags | ((tmout == 0) ? IPC_NOWAIT : 0);
	sbuf[1].sem_num = 1;
	sbuf[1].sem_op = 1;	/* set check write lock */
	sbuf[1].sem_flg = sflags;
	sbuf[2].sem_num = 2;
	sbuf[2].sem_op = 0;	/* wait for read lock */
	sbuf[2].sem_flg = 0 | ((tmout == 0) ? IPC_NOWAIT : 0);
	ret = mysemtimedop (id, sbuf, 3, tmout);
	if (!RERR_ISOK (ret)) return ret;
	return RERR_OK;
}


int
mutex_unlock (id, flags)
	int	id, flags;
{
	unsigned short		arr[3];
	struct sembuf		sbuf[2];
	int					ret, num, sflags=0;
	
	if (id < 0) return RERR_PARAM;
	/* find out, wether we have read or write lock */
	bzero (arr, sizeof (short)*3);
	ret = semctl (id, 0, GETALL, arr);
	if (ret < 0) return RERR_SYSTEM;
	if (arr[0] == 1 && arr[2] == 0) {
		/* there is no lock */
		return RERR_OK;
	}
	bzero (sbuf, sizeof (struct sembuf) * 2);
	if (!(flags & MUTEX_F_NOUNDO)) sflags |= SEM_UNDO;
	if (arr[2] > 0) {
		/* we have a read lock */
		sbuf[0].sem_num = 2;
		sbuf[0].sem_op = -1;	/* release read lock */
		sbuf[0].sem_flg = sflags | IPC_NOWAIT;
		num = 1;
	} else {
		/* we have a write lock */
		sbuf[0].sem_num = 0;
		sbuf[0].sem_op = 1;	/* release write lock */
		sbuf[0].sem_flg = sflags;
		sbuf[1].sem_num = 1;
		sbuf[1].sem_op = -1;	/* unset check write lock */
		sbuf[1].sem_flg = sflags | IPC_NOWAIT;
		num = 2;
	}
	ret = semop (id, sbuf, num);
	if (ret < 0 && errno != EAGAIN) return RERR_SYSTEM;
	return RERR_OK;
}


/* force unlock - hence reset mutex to (1,0,0) */
int
mutex_funlock (id)
	int	id;
{
	unsigned short		arr[3];
	int					ret;
	
	if (id < 0) return RERR_PARAM;
	arr[0] = 1;
	arr[1] = 0;
	arr[2] = 0;
	ret = semctl (id, 0, SETALL, arr);
	if (ret < 0) return RERR_SYSTEM;
	return RERR_OK;
}


int
mutex_islocked (id)
	int	id;
{
	unsigned short		arr[3];
	int					ret;
	
	if (id < 0) return RERR_PARAM;
	ret = semctl (id, 0, GETALL, arr);
	if (ret < 0) return RERR_SYSTEM;
	return (!((arr[0] == 1) && (arr[2] == 0)));
}









#ifdef Linux

static
int
mysemtimedop (id, sbuf, num, tmout)
	int				id;
	struct sembuf	*sbuf;
	unsigned			num;
	tmo_t				tmout;
{
	struct timespec	tspec;
	int					ret;
	tmo_t					now, now2;

	if (id < 0 || !sbuf) return RERR_PARAM;
	now = tmo_now ();
	while (1) {
		tspec.tv_sec = tmout / 1000000LL;
		tspec.tv_nsec = (tmout % 1000000LL) * 1000LL;
		ret = semtimedop (id, sbuf, num, (tmout > 0)?&tspec:NULL);
		if (ret < 0) {
			if (errno == EAGAIN) return RERR_TIMEDOUT;
			if (errno == EINTR) {
				now2 = tmo_now ();
				if (tmout > 0) {
					tmout -= now2 - now;
					if (tmout <= 0) return RERR_TIMEDOUT;
				}
				now = now2;
				continue;
			}
			return RERR_SYSTEM;
		}
		return RERR_OK;
	}
	return RERR_INTERNAL;	/* never reaches here */
}

#else	/* not Linux */

static
void
dummy (sig)
	int	sig;
{
	return;
}


static
int
mysemtimedop (id, sbuf, num, tmout)
	int				id;
	struct sembuf	*sbuf;
	unsigned			num;
	tmo_t				tmout;
{
	struct itimerval	ival, oval;
	tmo_t					otmr, now, now2;
	struct sigaction	nact, oact;
	int					ret;

	if (id < 0 || !sbuf) return RERR_PARAM;
	if (tmout <= 0) {
		while (1) {
			ret = semop (id, sbuf, num);
			if (ret < 0) {
				if (errno == EINTR) continue;
				return RERR_SYSTEM;
			}
			return RERR_OK;
		}
	}

	while (1) {
		now = tmo_now ();
		if (setitimer (ITIMER_REAL, &ival, &oval) < 0) return RERR_SYSTEM;
		otmr = TMO_FROMTVAL (oval.it_value);
		if (otmr > 0 && otmr < tmout) {
			setitimer (ITIMER_REAL, &oval, NULL);
		} else {
			nact.sa_handler = dummy;
			nact.sa_flags = SA_NOCLDSTOP;
			nact.sa_restorer = NULL;
			sigemptyset (&nact.sa_mask);
			if (sigaction (SIGALRM, &nact, &oact) < 0) {
				setitimer (ITIMER_REAL, &oval, NULL);
				return RERR_SYSTEM;
			}
		}
		ret = semop (id, sbuf, num);
		sigaction (SIGALRM, &oact, NULL);
		if (ret < 0) {
			if (errno == EINTR) {
				now2 = tmo_now();
				if ((tmout > 0) && (now2 - now > tmout)) return RERR_TIMEDOUT;
				tmout -= now2 - now;
				now = now2;
				continue;
			} else {
				ret = RERR_SYSTEM;
				break;
			}
		} else {
			ret = RERR_OK;
			break;
		}
	}
	if (otmr > 0) {
		now2 = tmo_now();
		otmr -= now2 - now;
		if (otmr > 0) {
			TMO_TOTVAL (oval.it_value, otmr);
		}
	}
	return ret;
}

#endif




























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
