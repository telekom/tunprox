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
 * Portions created by the Initial Developer are Copyright (C) 2003-2021
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#define _FILE_OFFSET_BITS 64
#include <stdlib.h>
#include <stdio.h>
#include <unistd.h>
#include <errno.h>

#include "errors.h"
#include "slog.h"


extern int errno;


ssize_t fdwrite (int fd, const void *buf, size_t count, int flags);
ssize_t fdread (int fd, void *buf, size_t count, int flags);
ssize_t fdwritep (int fd, const void *buf, size_t count, off_t pos, int flags);
ssize_t fdreadp (int fd, void *buf, size_t count, off_t pos, int flags);

ssize_t
fdwrite (fd, buf, count, flags)
	int			fd, flags;
	const void	*buf;
	size_t		count;
{
	ssize_t	num, num2=0;

	if (!buf || fd < 0) return RERR_PARAM;
	if (count == 0) return 0;
restart:
	for (num=1; num > 0 && (size_t)num2 < count; 
			num2 += num = write (fd, (const char*)buf+num2, count-num2));
	if (num < 0) {
		int myerr = errno;
		if (myerr == EINTR) goto restart;
		FRLOGF (LOG_ERR, "error writing to fd %d: %s",
				fd, rerr_getstr3(RERR_SYSTEM));
		errno = myerr;
		return RERR_SYSTEM;
	}
	if (num == 0) {
		FRLOGF (LOG_ERR, "zero write to fd %d", fd);
		return RERR_INVALID_LEN;
	}
	return (size_t) num2 == count ? (ssize_t)count : RERR_INVALID_LEN;
}


ssize_t
fdread (fd, buf, count, flags)
	int		fd, flags;
	void		*buf;
	size_t	count;
{
	ssize_t	num, num2=0;

	if (!buf || fd < 0) return RERR_PARAM;
restart:
	for (num=1; num > 0 && (size_t)num2 < count; 
			num2 += num = read (fd, (char*)buf+num2, count-num2));
	if (num < 0) {
		int myerr = errno;
		if (myerr == EINTR) goto restart;
		FRLOGF (LOG_ERR, "error reading from fd %d: %s",
				fd, rerr_getstr3(RERR_SYSTEM));
		errno = myerr;
		return RERR_SYSTEM;
	}
	if (num == 0) return num2;
	return (size_t) num2 == count ? (ssize_t)count : RERR_INVALID_LEN;
}



ssize_t
fdwritep (fd, buf, count, pos, flags)
	int			fd, flags;
	const void	*buf;
	off_t			pos;
	size_t		count;
{
	ssize_t	num, num2=0;

	if (!buf || fd < 0) return RERR_PARAM;
	if (pos == (off_t)-1) return fdwrite (fd, buf, count, flags);
	if (count == 0) return 0;
restart:
	for (num=1; num > 0 && (size_t)num2 < count; 
			num2 += num = pwrite (fd, (const char*)buf+num2, count-num2, pos+num2));
	if (num < 0) {
		int myerr = errno;
		if (myerr == EINTR) goto restart;
		FRLOGF (LOG_ERR, "error writing to fd %d: %s",
				fd, rerr_getstr3(RERR_SYSTEM));
		errno = myerr;
		return RERR_SYSTEM;
	}
	if (num == 0) {
		FRLOGF (LOG_ERR, "zero write to fd %d", fd);
		return RERR_INVALID_LEN;
	}
	return (size_t) num2 == count ? (ssize_t)count : RERR_INVALID_LEN;
}


ssize_t
fdreadp (fd, buf, count, pos, flags)
	int		fd, flags;
	void		*buf;
	off_t		pos;
	size_t	count;
{
	ssize_t	num, num2=0;

	if (!buf || fd < 0) return RERR_PARAM;
	if (pos == (off_t)-1) return fdread (fd, buf, count, flags);
restart:
	for (num=1; num > 0 && (size_t)num2 < count; 
			num2 += num = pread (fd, (char*)buf+num2, count-num2, pos+num2));
	if (num < 0) {
		int myerr = errno;
		if (myerr == EINTR) goto restart;
		FRLOGF (LOG_ERR, "error reading from fd %d (buf=%p, count=%lld, "
				"pos=%lld, num2=%lld): %s", fd, buf, (long long) count,
				(long long) pos, (long long) num2, rerr_getstr3(RERR_SYSTEM));
		errno = myerr;
		return RERR_SYSTEM;
	}
	if (num == 0) return num2;
	return (size_t) num2 == count ? (ssize_t)count : RERR_INVALID_LEN;
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
