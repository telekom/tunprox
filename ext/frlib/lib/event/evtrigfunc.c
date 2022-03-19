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

#include <fr/base/errors.h>
#include "evtrigfunc.h"

int
evbox_trigterm (data, dlen, tlen)
   char  *data;
   int   dlen, *tlen;
{
    if (!data || dlen < (ssize_t)sizeof (int)) return 0;
    if (tlen) *tlen = 0;
    return sizeof (int);
}



ssize_t
evbox_recv (fd, buf, len, flags, arg)
	int		fd;
	void		*buf;
	size_t	len;
	int		flags;
	void		*arg;
{
	int	ret;

	if (fd < 0 || !buf) return RERR_PARAM;
	if (len < sizeof (int)) return 0;
	ret = read (fd, buf, sizeof (int));
	if (ret < 0) return RERR_SYSTEM;
	return ret;
}


ssize_t
evbox_send (fd, buf, len, flags, arg)
	int			fd;
	const void	*buf;
	size_t		len;
	int			flags;
	void			*arg;
{
	/* it is not possible to send, so this function is a noop, but we
   	need it for SCONS
 	*/
	return len;
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
