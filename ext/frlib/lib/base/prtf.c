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
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdarg.h>
#include <time.h>
#include <unistd.h>



#include "slog.h"
#include "prtf.h"
#include "errors.h"
#include "misc.h"
#include "frassert.h"


int
fdprtf (
	int			fd,
	const char	*fmt,
	...)
{
	va_list	ap;
	int		ret;

	if (!fmt || !*fmt) {
		errno=EFAULT;
		return -1;
	}
	if (fd < 0) {
		errno=EBADF;
		return -1;
	}

	va_start (ap, fmt);
	ret = vfdprtf (fd, (char*)fmt, ap);
	va_end (ap);
	return ret;
}



#if 1 || !defined Linux || defined CYGWIN

int
vfdprtf (fd, fmt, ap)
	int			fd;
	const char	*fmt;
	va_list		ap;
{
	FILE		*f;
	int		num, myerrno, num2;
	char		*str;
	char		buf[1024];
	va_list	ap2;

	f=fopen ("/dev/null", "w");
	if (!f) {
#if 0	/* this function is used by frlogf, so it could be recursive */
		FRLOGF (LOG_ERR, "error opening /dev/null for writing: %s", 
								rerr_getstr3 (RERR_SYSTEM));
#endif
		return -1;
	}
#if 0		/* do we really should need it */
#ifdef va_copy
	va_copy (ap2, ap);
#else
	memcpy (&ap2, &ap, sizeof (ap));
#endif
#endif
	frvacopy (ap2, ap);
	num = vfprintf (f, fmt, ap2);
	frvacopyend (ap2);
#if 0
#ifdef va_copy
	va_end (ap2);
#endif
#endif
	myerrno=errno;
	fclose (f);
	if (num<0) {
		errno=myerrno;
		return -1;
	}
	if (num==0) return 0;
	if (num<1023) {
		str=buf;
	} else {
		str = malloc (num+2);
		if (!str) {
			errno=ENOMEM;
			return -1;
		}
	}
	num2=vsnprintf (str, num+1, fmt, ap);
	if (num2<0) {
		if (str!=buf) free (str);
		return -1;
	}
	num=num2;	/* could be less */
	num2 = write (fd, str, num);
	if (str!=buf) free (str);
	return num2;
}


#else	/* !defined Linux || defined CYGWIN */

int
vfdprtf (fd, fmt, ap)
	int			fd;
	const char	*fmt;
	va_list		ap;
{
	return vdprintf (fd, fmt, ap);
}

#endif	/* !defined Linux || defined CYGWIN */


char*
asprtf (
	const char	*fmt,
	...)
{
	va_list	ap;
	char		*str;

	if (!fmt || !*fmt) {
		errno=EFAULT;
		return NULL;
	}

	va_start (ap, fmt);
	str = vasprtf (fmt, ap);
	va_end (ap);
	return str;
}

char*
vasprtf (fmt, ap)
	const char	*fmt;
	va_list		ap;
{
	FILE		*f;
	int		num, myerrno, num2;
	char		*str;
	va_list	ap2;

	f=fopen ("/dev/null", "w");
	if (!f) {
#if 0	/* this function is used by frlogf, so it could be recursive */
		FRLOGF (LOG_ERR, "vasprtf(): error opening /dev/null for writing: %s", 
						rerr_getstr3 (RERR_SYSTEM));
#endif
		return NULL;
	}
	frvacopy (ap2, ap);
	num = vfprintf (f, fmt, ap2);
	frvacopyend (ap2);
	myerrno=errno;
	fclose (f);
	if (num<0) {
		errno=myerrno;
		return NULL;
	}
	if (num==0) return 0;
	str = malloc (num+2);
	if (!str) {
		errno=ENOMEM;
		return NULL;
	}
	num2=vsnprintf (str, num+1, fmt, ap);
	str[num]=0;
	if (num2<0) {
		free (str);
		return NULL;
	}
	return str;
}


char*
sasprtf (
	const char	*fmt,
	...)
{
	va_list	ap;
	char	*str;

	if (!fmt || !*fmt) {
		errno=EFAULT;
		return NULL;
	}

	va_start (ap, fmt);
	str = vsasprtf ((char*)fmt, ap);
	va_end (ap);
	return str;
}

char*
vsasprtf (fmt, ap)
	const char	*fmt;
	va_list		ap;
{
	FILE			*f;
	int			num, myerrno, num2;
	static char	*str;
	static char	*_str = NULL;
	static char	buf[1024];
	va_list		ap2;

	str = NULL;
	if (_str) {
		free (_str);
		_str = NULL;
	}
	f=fopen ("/dev/null", "w");
	if (!f) {
#if 0	/* this function is used by frlogf, so it could be recursive */
		FRLOGF (LOG_ERR, "error opening /dev/null for writing: %s", 
							rerr_getstr3 (RERR_SYSTEM));
#endif
		return NULL;
	}
	frvacopy (ap2, ap);
	num = vfprintf (f, fmt, ap2);
	frvacopyend (ap2);
	myerrno=errno;
	fclose (f);
	if (num<0) {
		errno=myerrno;
		return NULL;
	}
	if (num==0) return 0;
	if (num<1023) {
		str=buf;
	} else {
		str = _str = malloc (num+2);
		if (!str) {
			errno=ENOMEM;
			return NULL;
		}
	}
	num2=vsnprintf (str, num+1, fmt, ap);
	if (num2<0) {
		if (_str) {
			free (_str);
			_str = str = NULL;
		}
		return NULL;
	}
	return str;
}


char*
a2sprtf (
	char			*buf,
	size_t		bufsz,
	const char	*fmt,
	...)
{
	va_list	ap;
	char		*str;

	if (!fmt || !*fmt) {
		errno=EFAULT;
		return NULL;
	}

	va_start (ap, fmt);
	str = va2sprtf (buf, bufsz, fmt, ap);
	va_end (ap);
	return str;
}

char*
va2sprtf (buf, bufsz, fmt, ap)
	char			*buf;
	size_t		bufsz;
	const char	*fmt;
	va_list		ap;
{
	FILE		*f;
	int		num, myerrno, num2;
	char		*str;
	va_list	ap2;

	if (!buf || bufsz == 0) return vasprtf (fmt, ap);
	f=fopen ("/dev/null", "w");
	if (!f) {
#if 0	/* this function is used by frlogf, so it could be recursive */
		FRLOGF (LOG_ERR, "error opening /dev/null for writing: %s", 
						rerr_getstr3 (RERR_SYSTEM));
#endif
		return NULL;
	}
	frvacopy (ap2, ap);
	num = vfprintf (f, fmt, ap2);
	frvacopyend (ap2);
	myerrno=errno;
	fclose (f);
	if (num<0) {
		errno=myerrno;
		return NULL;
	}
	if (num==0) return 0;
	if (num+1 <= (ssize_t)bufsz) {
		str = buf;
	} else {
		str = malloc (num+2);
		if (!str) {
			errno=ENOMEM;
			return NULL;
		}
	}
	num2=vsnprintf (str, num+1, fmt, ap);
	if (num2<0) {
		if (str != buf) free (str);
		return NULL;
	}
	return str;
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
