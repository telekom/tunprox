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

#ifndef _R__FRLIB_LIB_CONNECT_FDLIST_H
#define _R__FRLIB_LIB_CONNECT_FDLIST_H

#define _GNU_SOURCE
#include <stdlib.h>
#include <fr/base/tlst.h>
#include <fr/base/tmo.h>
#include <poll.h>

#ifdef __cplusplus
extern "C" {
#endif

#define FDLIST_F_EVLIST_MASK	0xffff
#define FDLIST_EVENT_ANY		0xffff
#define FDLIST_F_ALLFD			(1<<16)

struct fdentry {
	int	fd;
	int	events;
	int	argi;
	void	*argp;
};

struct fdlist {
	struct tlst		allfd;
	struct pollfd	*plist;
	int				plistlen;
	struct tlst		foundfd;
	struct fdentry	dummy;
	int				f_events;
	int				f_argi;
	void				*f_argp;
};

#define FDLIST_SINIT	{ \
	.allfd = TLST_SINIT_T (struct fdentry), \
	.foundfd = TLST_SINIT_T (struct fdentry), \
}
#define FDLIST_INIT ((struct fdlist) FDLIST_SINIT)



int fdlist_new (struct fdlist*);
int fdlist_free (struct fdlist*);
int fdlist_add (struct fdlist*, int fd, int events, int argi, void* argp);
int fdlist_remove (struct fdlist*, int fd);
int fdlist_wait (struct fdlist*, int events, tmo_t timeout);


#define FDLIST_NEW(list)							fdlist_new (&(list))
#define FDLIST_FREE(list)							fdlist_free (&(list))
#define FDLIST_RESET(list)							tlst_reset (&((list).allfd));
#define FDLIST_ADD(list,fd,events,argi,argp)	fdlist_add(&(list),(fd),(events),(argi),(argp))
#define FDLIST_ADDL(list,fd,argi)				FDLIST_ADD((list),(fd),POLLIN,(argi),NULL)
#define FDLIST_REMOVE(list,fd)					fdlist_remove(&(list),(fd))
#define FDLIST_WAIT(list,events,timeout)		fdlist_wait (&(list),(events),(timeout))
#define FDLIST_WAITANY(list,timeout)			FDLIST_WAIT ((list),FDLIST_EVENT_ANY,(timeout))
#define FDLIST_WAITALL(list,timeout)			FDLIST_WAIT ((list),FDLIST_EVENT_ANY|FDLIST_F_ALLFD,(timeout))

#define FDLIST_FOREACH(entry,list)	\
			TLST_FOREACH((entry),(list).foundfd)
#define FDLIST_FOREACHIDX(entry,list,idx)	\
			TLST_FOREACH2((entry),(list).foundfd,idx)

#define FDLIST_FOREACH2b(fd,events,argi,argp,list,e,i)	\
				for ((i)=0; (i) < (list).allfd.num && \
						TLST_GET((e),(list).alfd,(i)) == RERR_OK \
						&& ((fd=(e).fd) ? 1 : 1) \
						&& ((events=(e).events) ? 1 : 1) \
						&& ((argi=(e).argi) ? 1 : 1) \
						&& ((argp=(e).argp) ? 1 : 1); \
						(i)++)

#define FDLIST_FOREACH2(fd,events,argi,argp,list)	\
				FDLIST_FOREACH2b ((fd),(events),(argi),(argp),(list),\
										(list).dummy,(list).allfd.cnt)
#define FDLIST_FOREACH3(fd,argi,list) \
				FDLIST_FOREACH2((fd),(list).f_events,(argi),(list).f_argp)
#define FDLIST_FOREACH4(fd,list) \
				FDLIST_FOREACH3 ((fd),(list).f_argi,(list))









#ifdef __cplusplus
}	/* extern "C" */
#endif



















#endif	/* _R__FRLIB_LIB_CONNECT_FDLIST_H */

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
