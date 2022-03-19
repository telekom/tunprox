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
 * Portions created by the Initial Developer are Copyright (C) 2016
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <unistd.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <errno.h>
#include <sys/types.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/msg.h>


#ifdef __ANDROID__

/* most of the following functions are just stubs, to avoid undefined
   references when linking against androids NON posix ndk.
   these stubs do not provide the functionallity requested (apart of
   index and rindex). so some stuff in the frlib does NOT work
   as expected on android!!! unfortunately android is NOT posix
   compliant and thus there is no easy way to shift around those
   restrictions. 
 */


int getdomainname(char *name, size_t len)
{
	if (!name || len == 0) return 0;
	snprintf (name, len, "fuck.android");
	return 0;
}

char *getpass( const char *prompt)
{
	printf ("%s: unfortunately android does not support reading passwords\n", prompt);
	errno = ENOTSUP;
	return NULL;
}

char *index(const char *s, int c)
{
	return strchr (s, c);
}

char *rindex(const char *s, int c)
{
	return strrchr (s, c);
}

int msgctl(int msqid, int cmd, struct msqid_ds *buf)
{
	errno = ENOTSUP;
	return -1;
}

int msgget(key_t key, int msgflg)
{
	errno = ENOTSUP;
	return -1;
}

int msgsnd(int msqid, const void *msgp, size_t msgsz, int msgflg)
{
	errno = ENOTSUP;
	return -1;
}

ssize_t msgrcv(int msqid, void *msgp, size_t msgsz, long msgtyp,
               int msgflg)
{
	errno = ENOTSUP;
	return -1;
}

int semctl(int semid, int semnum, int cmd, ...)
{
	errno = ENOTSUP;
	return -1;
}

int semget(key_t key, int nsems, int semflg)
{
	errno = ENOTSUP;
	return -1;
}


int semop(int semid, struct sembuf *sops, size_t nsops)
{
	errno = ENOTSUP;
	return -1;
}

int semtimedop(int semid, struct sembuf *sops, size_t nsops,
               const struct timespec *timeout)
{
	errno = ENOTSUP;
	return -1;
}

int on_exit(void (*function)(int , void *), void *arg)
{
	/* ignore */
	return 0;
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
