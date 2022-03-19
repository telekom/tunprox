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
 * Portions created by the Initial Developer are Copyright (C) 2003-2015
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */

#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>
#include <unistd.h>

#include <fr/base.h>
#include "frmail.h"



int
frmail_parse (mail, msg, flags)
	struct frmail	*mail;
	const char		*msg;
	int				flags;
{

}



static
int
hdrline (hdr, val, next)
	char	**hdr, **val, **next;
{
	char	*s, *s2;

	if (!hdr || !val || !next || !*next) return RERR_PARAM;
	s = *next;
	if (!*s || *s == '\n' || (*s == '\r' && s[1] == '\n')) {
		return RERR_EOT;
	}

	/* skip beginning */
	while (iswhite (*s)) s++;
	if (!strncasecmp (s, "From ", 5)) {
		s = index (s, '\n');
	}
	if (!s) return RERR_EOT;
	while (iswhite (*s)) s++;

	/* find header colon */
	*hdr = s;
	while (1) {
		s = index (s, '\n');
		s2 = index (*hdr, ':');
		if (!s2) return RERR_EOT;
		if (s && s2 > s) {
			/* invalid header line - skip it */
			*hdr = ++s;
		} else {
			/* find header field */
			for (s3=s2-1; iswhite (*s3) && s3 >= *hdr; s3--); s3++;
			if (s3 == *hdr) {
				*hdr = ++s
				continue;
			}
			*s3 = 0;
			break;
		}
	}

	/* find end of header line */
	while (s && iswhite (s[1] && !(s[1] == '\n' || s[1] == '\r')) {
		s = index (s+1, '\n');
	}
	if (s) {
		if (s[-1] == '\r') {
			s[-1] = 0;
		} else {
			*s = 0;
		}
		s++;
	}
	*next = s;

	/* find beginning of header value (might be empty) */
	for (s2++; iswhite (*s2); s2++);
	*val = s2;

	return RERR_OK;
}


#define FRMAIL_F_COPY	0x01

static
int
hdrdecode (obuf, ibuf, flags)
	char			**obuf;
	const char	*ibuf;
	int			flags;
{
	char	*s, *si, *so;

	if (!ibuf) return RERR_PARAM;
	if (flags & FRMAIL_F_COPY) {
		if (!obuf) return RERR_PARAM;
		s = strdup (ibuf);
		if (!s) return RERR_NOMEM;
		*obuf = s;
	} else {
		s = (char*)ibuf;
		if (obuf) *obuf = s;
	}
	for (si=s; *si; si++) {
		
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
