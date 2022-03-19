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
 * Portions created by the Initial Developer are Copyright (C) 1999-2014
 * by the Initial Developer. All Rights Reserved.
 *
 * ***** END LICENSE BLOCK ***** */



#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <strings.h>


#include "errors.h"
#include "buffer.h"
#include "buffer_types.h"


int
bufvnprintf (buf, num, fmt, ap)
	BUFFER		buf;
	uint32_t		num;
	const char	*fmt;
	va_list		ap;
{
	char	*s;
	char	s2[1025];
	int	ret;

	if (!buf) return RERR_PARAM;
	if (num + 2 < sizeof(s2)) {
		s = s2;
	} else {
		s=malloc (num + 2);
		if (!s) return RERR_NOMEM;
	}
	vsnprintf (s, num+1, fmt, ap);
	s[num] = 0;
	if (s!=s2 && (bufgettype (buf) == BUF_TYPE_MEM)) {
		ret = bufputstr (buf, s);
		if (!RERR_ISOK(ret)) {
			free (s);
			return ret;
		}
	} else {
		ret = bufprint (buf, s);
		if (s!=s2) free (s);
		if (!RERR_ISOK(ret)) return ret;
	}
	return RERR_OK;
}




int
bufvprintf (buf, fmt, ap)
	BUFFER		buf;
	const char	*fmt;
	va_list		ap;
{
	struct buffer	*p;
	int				ret, num;
	FILE				*f;
	va_list			ap2;

	if (!buf) return RERR_PARAM;
	p = (struct buffer*)buf;
	if (p->type == BUF_TYPE_FILE) {
		if (!p->dat.file.f) {
			ret = buffileopen (buf);
			if (!RERR_ISOK(ret)) return ret;
		}
		num = vfprintf (p->dat.file.f, fmt, ap);
		buffileclose (buf, 0);
		if (num < 0) return RERR_SYSTEM;
		return bufevallen (buf);
	}
	f = fopen ("/dev/null", "w");
	if (!f) return RERR_SYSTEM;
#ifdef va_copy
	va_copy (ap2, ap);
#else
	ap2 = ap;
#endif
	num = vfprintf (f, fmt, ap2);
	fclose (f);
	if (num < 0) return RERR_SYSTEM;
	return bufvnprintf (buf, num, fmt, ap);
}


int
bufvtprintf (buf, fmt, ap)
	BUFFER		buf;
	const char	*fmt;
	va_list		ap;
{
	if (bufgettype (buf) == BUF_TYPE_FILE) {
		return bufvprintf (buf, fmt, ap);
	}
	return bufvnprintf (buf, 1024, fmt, ap);
}




int
bufnprintf (
	BUFFER		buf,
	uint32_t		num,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!buf) return RERR_PARAM;
	va_start (ap, fmt);
	ret = bufvnprintf (buf, num, fmt, ap);
	va_end (ap);
	return ret;
}



int
bufprintf (
	BUFFER		buf,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!buf) return RERR_PARAM;
	va_start (ap, fmt);
	ret = bufvprintf (buf, fmt, ap);
	va_end (ap);
	return ret;
}



int
buftprintf (
	BUFFER		buf,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!buf) return RERR_PARAM;
	va_start (ap, fmt);
	ret = bufvtprintf (buf, fmt, ap);
	va_end (ap);
	return ret;
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
