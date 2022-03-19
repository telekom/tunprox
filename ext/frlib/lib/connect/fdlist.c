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

#define _GNU_SOURCE
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>
#include <poll.h>
#include <sys/poll.h>
#include <sys/types.h>

#include <fr/base/errors.h>
#include <fr/base/tlst.h>
#include <fr/base/tmo.h>

#include "fdlist.h"

static int fdlist_getfd (unsigned*, struct fdentry*, struct fdlist*, int);


int
fdlist_new (list)
	struct fdlist	*list;
{
	if (!list) return RERR_PARAM;
	bzero (list, sizeof (struct fdlist));
	TLST_NEW (list->allfd, struct fdentry);
	TLST_NEW (list->foundfd, struct fdentry);
	return RERR_OK;
}

int
fdlist_free (list)
	struct fdlist	*list;
{
	if (!list) return RERR_PARAM;
	TLST_FREE (list->allfd);
	TLST_FREE (list->foundfd);
	if (list->plist) free (list->plist);
	bzero (list, sizeof (struct fdlist));
	return RERR_OK;
}


int
fdlist_add (list, fd, events, argi, argp)
	struct fdlist	*list;
	int	fd, events, argi;
	void	*argp;
{
	struct fdentry	fdentry;
	unsigned			idx;
	int				ret;

	if (!list || fd < 0) return RERR_PARAM;
	fdentry.fd = fd;
	fdentry.events = events;
	fdentry.argi = argi;
	fdentry.argp = argp;
	if (RERR_ISOK(fdlist_getfd (&idx, NULL, list, fd))) {
		return TLST_SET (list->allfd, idx, fdentry);
	} else {
		ret = TLST_ADD (list->allfd, fdentry);
		if (!RERR_ISOK(ret)) return ret;
		return RERR_OK;
	}
}

int
fdlist_remove (list, fd)
	struct fdlist	*list;
	int				fd;
{
	unsigned	idx;

	if (!list || fd < 0) return RERR_PARAM;
	if (!RERR_ISOK(fdlist_getfd (&idx, NULL, list, fd))) return RERR_OK;
	return TLST_REMOVE (list->allfd, idx, TLST_F_CPYLAST);

}

int
fdlist_wait (list, events, timeout)
	struct fdlist	*list;
	int				events;
	tmo_t				timeout;
{
	int					putall, num, i, ret;
	struct fdentry		entry;
	struct pollfd		*p;
#ifdef Linux
	struct timespec	ts;
#endif

	if (!list) return RERR_PARAM;
	ret = TLST_RESET (list->foundfd);
	if (!RERR_ISOK(ret)) return ret;
	putall = events & FDLIST_F_ALLFD ? 1 : 0;
	events &= FDLIST_F_EVLIST_MASK;
	num = TLST_GETNUM (list->allfd);
	if (num > list->plistlen) {
		p = realloc (list->plist, (num + 16) * sizeof (struct pollfd));
		if (!p) return RERR_NOMEM;
		list->plist = p;
		list->plistlen = num + 16;
	}
	num=0;
	TLST_FOREACH (entry, list->allfd) {
		if (!putall && !(entry.events & events)) continue;
		list->plist[num].fd = entry.fd;
		list->plist[num].events = putall ? events : (entry.events & events);
		list->plist[num].revents = 0;
		num++;
	}
#ifdef Linux
	ts.tv_sec = timeout / 1000000LL;
	ts.tv_nsec = (timeout % 1000000LL) * 1000LL;
	ret = ppoll (list->plist, num, timeout >= 0 ? &ts : NULL, NULL);
#else
	/* TODO: we should use pselect here, to be compatible */
	ret = poll (list->plist, num, timeout/1000LL);
#endif
	if (ret < 0) return RERR_SYSTEM;
	if (ret == 0) return 0;
	for (i=0; i<num; i++) {
		if (list->plist[i].revents == 0) continue;
		ret = fdlist_getfd (NULL, &entry, list, list->plist[i].fd);
		if (!RERR_ISOK(ret)) return ret;
		entry.events = list->plist[i].revents;
		ret = TLST_ADD (list->foundfd, entry);
		if (!RERR_ISOK(ret)) return ret;
	}
	return TLST_GETNUM (list->foundfd);
}




static
int
fdlist_getfd (idx, entry, list, fd)
	unsigned			*idx;
	struct fdentry	*entry;
	struct fdlist	*list;
	int				fd;
{
#if 0
	unsigned			i;
	struct fdentry	data;

	if (!list || fd < 0) return RERR_PARAM;
	TLST_FOREACH2 (data, list->allfd, i) {
		if (fd == data.fd) break;
	}
	if (i>=TLST_GETNUM(list->allfd)) return RERR_NOT_FOUND;
	if (idx) *idx = i;
	if (entry) *entry = data;
	return RERR_OK;
#endif
	return tlst_find (idx, entry, &(list->allfd), &tlst_cmpint, &fd);
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
