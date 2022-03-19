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
#include <stdarg.h>

#include "kstring.h"



using namespace std;

const kstring KSTRING_NULL("");

size_t
kstring::size () const
{
	if (!data) return 0;
	return strlen (data);
}

const char *
kstring::c_str () const
{
	if (!data) return "";
	return data;
}

char *
kstring::str () const
{
	if (!data) return strdup ("");
	return strdup (data);
}

kstring& 
kstring::asign (
	const kstring	&str)
{
	return asign (str.data);
}

kstring& 
kstring::asign (
	const kstring	&str,
	size_t			len)
{
	return asign (str.data, len);
}

kstring&
kstring::asign (
	const char	*str)
{
	if (!str) {
		if (data) {
			*data = 0;
		}
	} else {
		if (!data) {
			data = strdup (str);
			buflen = strlen (data);
		} else {
			if (buflen > strlen (str)) {
				strcpy (data, str);
			} else {
				buflen = strlen (str) + 1;
				data = (char *)realloc (data, buflen);
				if (data) strcpy (data, str);
			}
		}
	}
	if (!data) buflen = 0;
	return *this;
}


kstring&
kstring::asign (
	const char	*str,
	size_t		len)
{
	if (!str || len == 0) {
		if (data) {
			*data = 0;
		}
	} else if (strlen (str) <= len) {
		return asign (str);
	} else {
		if (!data) {
			buflen = len + 1;
			data = (char *)malloc (buflen);
			if (data) {
				strncpy (data, str, len);
				data[len] = 0;
			}
		} else {
			if (buflen > len) {
				strncpy (data, str, len);
				data[len] = 0;
			} else {
				buflen = len + 1;
				data = (char *)realloc (data, buflen);
				if (data) {
					strncpy (data, str, len);
					data[len] = 0;
				}
			}
		}
	}
	if (!data) buflen = 0;
	return *this;
}

kstring&
kstring::asign (
	char	c)
{
	return asign ((const char *) &c, 1);
}

kstring&
kstring::asign (
	int	i)
{
	char	buf[32];

	sprintf (buf, "%d", i);
	return asign (buf);
}

kstring&
kstring::asign (
	double	f)
{
	char	buf[64];

	sprintf (buf, "%g", f);
	return asign (buf);
}


kstring&
kstring::cat (
	const kstring	&str)
{
	return cat (str.data);
}

kstring& 
kstring::cat (
	const kstring	&str,
	size_t			len)
{
	return cat (str.data, len);
}

kstring&
kstring::cat (
	const char	*str)
{
	if (!str || !*str) return *this;
	if (!data || !*data) return asign (str);
	return cat (str, strlen (str));
}

kstring& 
kstring::cat (
	const char	*str,
	size_t		len)
{
	if (!str || !*str) return *this;
	if (!data || !*data) return asign (str, len);
	int len2 = strlen (data);
	if (buflen <= len2 + len) {
		buflen = len2 + len + 1 + 16;
		data = (char *)realloc (data, buflen);
		if (!data) { buflen = 0; return *this; }
	}
	strncpy (data+len2, str, len);
	data[len + len2] = 0;
	return *this;
}

kstring&
kstring::cat (
	char	c)
{
	return cat ((const char *)&c, 1);
}

kstring&
kstring::cat (
	int	i)
{
	char	buf[32];

	sprintf (buf, "%d", i);
	return cat (buf);
}

kstring&
kstring::cat (
	double	f)
{
	char	buf[64];

	sprintf (buf, "%g", f);
	return cat (buf);
}


int
kstring::cmp (
	const char	*str)
	const
{
	if ((!str || !*str) && (!data || !*data)) return 0;
	if (!str) return strcmp (data, "");
	if (!data) return strcmp ("", str);
	return strcmp (data, str);
}

int
kstring::cmp (
	const char	*str,
	size_t		len)
	const
{
	if ((!str || !*str) && (!data || !*data)) return 0;
	if (!str) return strncmp (data, "", len);
	if (!data) return strncmp ("", str, len);
	return strncmp (data, str, len);
}

int
kstring::icmp (
	const char	*str)
	const
{
	if ((!str || !*str) && (!data || !*data)) return 0;
	if (!str) return strcasecmp (data, "");
	if (!data) return strcasecmp ("", str);
	return strcasecmp (data, str);
}

int
kstring::icmp (
	const char	* str,
	size_t		len)
	const
{
	if ((!str || !*str) && (!data || !*data)) return 0;
	if (!str) return strncasecmp (data, "", len);
	if (!data) return strncasecmp ("", str, len);
	return strncasecmp (data, str, len);
}

kstring&
kstring::printf (
	const char	*fmt,
	...)
{
	va_list	ap;

	va_start (ap, fmt);
	this->vprintf (fmt, ap);
	va_end (ap);

	return *this;
}

kstring&
kstring::nprintf (
	size_t		n,
	const char	*fmt,
	...)
{
	va_list	ap;

	va_start (ap, fmt);
	this->vnprintf (n, fmt, ap);
	va_end (ap);

	return *this;
}


kstring&
kstring::vprintf (
	const char	*fmt, 
	va_list		ap)
{
	FILE	*f;
	int	n;

	f = fopen ("/dev/null", "w+");
	if (!f) return *this;
	n = vfprintf (f, fmt, ap);
	fclose (f);

	return this->vnprintf (n+1, fmt, ap);
}

kstring&
kstring::vnprintf (
	size_t		n,
	const char	*fmt, 
	va_list		ap)
{
#ifdef Linux
	n++;
#endif
	if (!data || buflen <= n) {
		buflen = n+1;
		data = (char *)realloc (data, buflen);
		if (!data) {
			buflen = 0;
			return *this;
		}
	}
	vsnprintf (data, n, fmt, ap);
	data[n] = 0;

	return *this;
}




kstring&
kstring::catprintf (
	const char	*fmt,
	...)
{
	va_list	ap;

	va_start (ap, fmt);
	this->catvprintf (fmt, ap);
	va_end (ap);

	return *this;
}


kstring&
kstring::catnprintf (
	size_t		n,
	const char	* fmt, ...)
{
	va_list	ap;

	va_start (ap, fmt);
	this->catvnprintf (n, fmt, ap);
	va_end (ap);

	return *this;
}


kstring&
kstring::catvprintf (
	const char	* fmt, 
	va_list		ap)
{
	FILE	*f;
	int	n;

	f = fopen ("/dev/null", "w");
	if (!f) return *this;
	n = vfprintf (f, fmt, ap);
	fclose (f);

	return this->catvnprintf (n+1, fmt, ap);
}


kstring&
kstring::catvnprintf (
	size_t		n,
	const char	*fmt, 
	va_list		ap)
{
	int	size = data ? strlen (data): 0;

#ifdef Linux
	n++;
#endif
	if (!data || buflen <= n + size) {
		buflen = n+1+size+16;	/* the 16 is to avoid an realloc too often */
		data = (char *)realloc (data, buflen);
		if (!data) {
			buflen = 0;
			return *this;
		}
	}
	vsnprintf (data+size, n, fmt, ap);
	data[n+size] = 0;

	return *this;
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
