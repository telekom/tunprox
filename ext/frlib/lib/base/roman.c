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
#include <errno.h>
#include <getopt.h>

#include "errors.h"
#include "textop.h"

#include "roman.h"

struct romtable_t {
	int	factor;
	char	minuscola;
	char	maiuscola;
	int 	sub;
};
static const struct romtable_t subtable[] = {
			{1000, 'm', 'M', 1000},
			{ 999, 'i', 'I',   -1},
			{ 990, 'x', 'X',  -10},
			{ 900, 'c', 'C', -100},
			{ 500, 'd', 'D',  500},
			{ 499, 'i', 'I',   -1},
			{ 490, 'x', 'X',  -10},
			{ 400, 'c', 'C', -100},
			{ 100, 'c', 'C',  100},
			{  99, 'i', 'I',   -1},
			{  90, 'x', 'X',  -10},
			{  50, 'l', 'L',   50},
			{  49, 'i', 'I',   -1},
			{  40, 'x', 'X',  -10},
			{  10, 'x', 'X',   10},
			{   9, 'i', 'I',   -1},
			{   5, 'v', 'V',    5},
			{   4, 'i', 'I',   -1},
			{   1, 'i', 'I',    1}};
static const struct romtable_t addtable[] = {
			{1000, 'm', 'M', 1000},
			{ 500, 'd', 'D',  500},
			{ 100, 'c', 'C',  100},
			{  50, 'l', 'L',   50},
			{  10, 'x', 'X',   10},
			{   5, 'v', 'V',    5},
			{   1, 'i', 'I',    1}};

static int _putc (char**, int*, char);
static int _adjust (char**, int*, int);
static int _getval (char);


int
num2roman (buf, len, n, flags)
	int	n, flags, len;
	char	*buf;
{
	int					wlen=0, nonul, rem, ret;
	struct romtable_t	*rt,*rt2;

	if ((!buf && len > 0) || len < 0) return RERR_PARAM;
	if (len > 0) {
		len--;
		nonul=0;
	} else {
		nonul=1;
	}
	if (n < 0) {
		if (flags & ROM_NEG) {
			ret = _putc (&buf, &len, '-');
			if (ret < 0) return ret;
			wlen++;
			n *= -1;
		} else {
			return RERR_OUTOFRANGE;
		}
	}
	if (n == 0) {
		if (flags & ROM_LCASE) {
			ret = _putc (&buf, &len, 'n');
		} else {
			ret = _putc (&buf, &len, 'N');
		}
		wlen++;
		if (ret < 0) return ret;
		if (!nonul && buf) {
			*buf = 0;
		}
		return wlen;
	}
	if (flags & ROM_ADD) {
		rt2 = (struct romtable_t*)addtable;
	} else {
		rt2 = (struct romtable_t*)subtable;
	}
	if ((flags & ROM_CIFRAO) || (flags & ROM_DOT)) {
		rem = n / 1000;
		n %= 1000;
		if (rem) {
			ret = num2roman (buf, len, rem, flags);
			if (!RERR_ISOK(ret)) return ret;
			wlen += ret;
			_adjust (&buf, &len, ret);
			if (flags & ROM_CIFRAO) {
				_putc (&buf, &len, '$');
			} else {
				_putc (&buf, &len, '.');
			}
			wlen++;
		}
	} else if (n > 3999) {
		rem = n / 1000;
		n %= 1000;
		ret = num2roman (buf, len, rem, flags);
		if (!RERR_ISOK(ret)) return ret;
		wlen += ret;
		_adjust (&buf, &len, ret);
		_putc (&buf, &len, '*');
		if (flags & ROM_LCASE) {
			_putc (&buf, &len, 'm');
		} else {
			_putc (&buf, &len, 'M');
		}
		wlen+=2;
	}
	while (n) {
		for (rt=rt2; n < rt->factor; rt++);
		if (flags & ROM_LCASE) {
			_putc (&buf, &len, rt->minuscola);
		} else {
			_putc (&buf, &len, rt->maiuscola);
		}
		wlen++;
		n -= rt->sub;
	}
	if (!nonul && buf) {
		*buf = 0;
	}
	return wlen;
}

static
int
_putc (buf, len, c)
	char	**buf, c;
	int	*len;
{
	if (buf && *buf && len && *len > 0) {
		**buf=c;
		(*buf)++;
		(*len)--;
	}
	return 1;
}

static
int
_adjust (buf, len, num)
	char	**buf;
	int	*len, num;
{
	int	n;

	if (buf && *buf && len && *len > 0) {
		n = num > *len ? *len : num;
		*buf+=n;
		*len-=n;
	}
	return n;
}


int
roman2num (num, rom, flags)
	int	*num, flags;
	char	*rom;
{
	char	*s;
	int	n2, n=0, neg=0;
	int	last=0, lastsum=0;

	if (!num || !rom) return RERR_PARAM;
	s = top_skipwhite (rom);
	if (*s == '-') {
		neg=1;
		s++;
	} else if (*s == 'n' || *s == 'N') {
		*num = 0;
		return 1;
	}
	for (; *s; s++) {
		if (*s==' ') continue;
		switch (*s) {
		case '*':
			s = top_skipwhiteplus (s+1, "mM");
			s--;
			/* fall thru */
		case '.':
		case '$':
			n *= 1000;
			last = lastsum = 0;
			break;
		default:
			n2 = _getval (*s);
			if (n2 == 0) goto out;
			n += n2;
			if (last) {
				if (last < n2) {
					n-=2*lastsum;
					last = lastsum = 0;
				} else if (last > n2) {
					lastsum = n2;
					last = n2;
				} else {
					lastsum += n2;
				}
			} else {
				last = lastsum = n2;
			}
			break;
		}
	}
out:
	if (neg) n *= -1;
	*num = n;
	return s-rom;
}

static
int
_getval (c)
	char	c;
{
	switch (c) {
	case 'm': case 'M':
		return 1000;
	case 'd': case 'D':
		return 500;
	case 'c': case 'C':
		return 100;
	case 'l': case 'L':
		return 50;
	case 'x': case 'X':
		return 10;
	case 'v': case 'V':
	case 'u': case 'U':
		return 5;
	case 'i': case 'I':
	case 'j': case 'J':
		return 1;
	}
	return 0;
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

