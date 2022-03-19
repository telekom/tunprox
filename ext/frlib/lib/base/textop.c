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
#include <ctype.h>
#include <stdint.h>
#include <errno.h>
#ifdef SunOS
extern int errno;
#endif



#include "textop.h"
#include "errors.h"
#include "config.h"
#include "utf8.h"


int
top_delcr (str)
	char	*str;
{
	char	*s, *s2;

	if (!str) return 0;
	s = index (str, '\r');
	if (!s) return 1;
	s2 = s;
	while (*s) {
		s++;
		for (; *s && *s != '\r'; s++, s2++) {
			*s2 = *s;
		}
	}
	*s2 = 0;
	return 1;
}




static char	base64map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklm"
							"nopqrstuvwxyz0123456789+/";

int
top_base64encode_short (in, inlen, out)
	const char	*in;
	char			*out;
	int			inlen;
{
	char	*s, *s2;
	int	i;

	if (!out) return 0;
	*out = 0;
	if (!in || inlen <= 0) return 0;
	for (i=0, s=(char*)in, s2=out; i<inlen/3; i++, s+=3, s2+=4) {
		s2[0] = base64map[(s[0]&0xfc)>>2];
		s2[1] = base64map[((s[0]&0x03)<<4)|((s[1]&0xf0)>>4)];
		s2[2] = base64map[((s[1]&0x0f)<<2)|((s[2]&0xc0)>>6)];
		s2[3] = base64map[s[2]&0x3f];
	}
	*s2=0;
	i=inlen%3;
	if (i==0) return 1;
	s2[0] = base64map[(s[0]&0xfc)>>2];
	if (i==1) {
		s2[1] = base64map[(s[0]&0x03)<<4];
		s2[2] = '=';
		s2[3] = '=';
	} else {	/* i==2 */
		s2[1] = base64map[((s[0]&0x03)<<4)|((s[1]&0xf0)>>4)];
		s2[2] = base64map[(s[1]&0x0f)<<2];
		s2[3] = '=';
	}
	s2[4] = 0;

	return 1;
}



int
top_base64encode (in, inlen, out)
	const char	*in;
	char			**out;
	int			inlen;
{
	char	*obuf;
	int	ret;

	if (!in || !out || inlen < 0) return RERR_PARAM;
	obuf = malloc (inlen * 3 / 2 + 3);
	if (!obuf) return RERR_NOMEM;

	ret = top_base64encode_buf (in, inlen, obuf);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		return ret;
	}
	*out = obuf;
	return RERR_OK;
}


int
top_base64encode_buf (in, inlen, out)
	const char	*in;
	char			*out;
	int			inlen;
{
	const char	*s;
	char			*s2;
	int			i;

	if (!in || !out || inlen < 0) return RERR_PARAM;

	for (i=0, s=in, s2=out; i<inlen/3; i++, s+=3, s2+=4) {
		if (i!=0 && i%18==0) {
			*s2 = '\n';
			s2++;
		}
		s2[0] = base64map[(s[0]&0xfc)>>2];
		s2[1] = base64map[((s[0]&0x03)<<4)|((s[1]&0xf0)>>4)];
		s2[2] = base64map[((s[1]&0x0f)<<2)|((s[2]&0xc0)>>6)];
		s2[3] = base64map[s[2]&0x3f];
	}
	if ((inlen%3)==0) {
		s2[0] = '\n';
		s2[1] = 0;
		return RERR_OK;
	}
	if (i!=0 && i%18==0) {
		*s2 = '\n';
		s2++;
	}
	i=inlen%3;
	s2[0] = base64map[(s[0]&0xfc)>>2];
	if (i==1) {
		s2[1] = base64map[(s[0]&0x03)<<4];
		s2[2] = '=';
		s2[3] = '=';
	} else {	/* i==2 */
		s2[1] = base64map[((s[0]&0x03)<<4)|((s[1]&0xf0)>>4)];
		s2[2] = base64map[(s[1]&0x0f)<<2];
		s2[3] = '=';
	}
	s2[4] = '\n';
	s2[5] = 0;

	return RERR_OK;
}

static const unsigned   rb64table[] = {/*43*/ 62, 0, 0, 0, /*47*/ 63, 52, 53,
							54, 55, 56, 57, 58, 59, 60, 61, 0, 0, 0, 0, 0, 0,
							0, /*A*/ 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11,
							12, 13, 14, 15, 16, 17, 18, 19, 20, 21, 22, 23,
							24, 25, 0, 0, 0, 0, 0, 0, /*97*/ 26, 27, 28, 29,
							30, 31, 32, 33, 34, 35, 36, 37, 38, 39, 40, 41,
							42, 43, 44, 45, 46, 47, 48, 49, 50, 51};
#define RB64TAB(c)	(rb64table[(c)-43])
#define VALID_B64(c) (((c)>='A'&&(c)<='Z')||((c)>='a'&&(c)<='z')||\
						((c)>='0'&&(c)<='9')||(c)=='+'||(c)=='/'||(c)=='=')
#define VALID_B64STR(s) (VALID_B64((s)[0])&&VALID_B64((s)[1])&&\
						VALID_B64((s)[2])&&VALID_B64((s)[3]))

int
top_base64decode (in, out, outlen)
	const char	*in;
	char			**out;
	int			*outlen;
{
	if (!in || !out) return RERR_PARAM;
	return top_base64decode2 (in, strlen (in), out, outlen);
}

int
top_base64decode_buf (in, out, outlen)
	const char	*in;
	char			*out;
	int			*outlen;
{
	if (!in || !out) return RERR_PARAM;
	return top_base64decode2_buf (in, strlen (in), out, outlen);
}

int
top_base64decode2 (in, inlen, out, outlen)
	const char	*in;
	int			inlen;
	char			**out;
	int			*outlen;
{
	char	*obuf;
	int	ret;

	if (!in || !out) return RERR_PARAM;
	obuf = malloc (inlen * 3 / 4 + 2);
	if (!obuf) return RERR_NOMEM;
	ret = top_base64decode2_buf (in, inlen, obuf, outlen);
	if (!RERR_ISOK(ret)) {
		free (obuf);
		return ret;
	}
	*out = obuf;
	return RERR_OK;
}

int
top_base64decode2_buf (in, inlen, out, outlen)
	const char	*in;
	int			inlen;
	char			*out;
	int			*outlen;
{
	char			*s2;
	const char	*s;
	int			i;

	if (!in || !out) return RERR_PARAM;

	for (i=0,s=in,s2=out; *s && s-in < inlen; i+=3,s+=4,s2+=3) {
		s = top_skipwhite (s);
		if (!*s) break;
		if (!VALID_B64STR(s)) {
			free (out);
			return RERR_NOT_BASE64;
		}
		s2[0] = (RB64TAB(s[0]) << 2) | (RB64TAB(s[1]) >> 4);
		if (s[2] != '=') {
			s2[1] = ((RB64TAB(s[1]) & 0x0f) << 4) | (RB64TAB(s[2]) >> 2);
		} else {
			i++; s2++;
			break;
		}
		if (s[3] != '=') {
			s2[2] = ((RB64TAB(s[2]) & 0x03) << 6) | RB64TAB(s[3]);
		} else {
			i+=2; s2+=2;
			break;
		}
	}
	*s2 = 0;
	if (outlen) *outlen = i;

	return RERR_OK;
}


static char base32map[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ012345";

int
top_base32encode_short (in, inlen, out)
	const char	*in;
	char			*out;
	int			inlen;
{
	int	i;
	char	*s, *so;

	if (!in || !out || inlen < 0) return RERR_PARAM;
	if (inlen == 0) {
		*out = 0;
		return RERR_OK;
	}
	for (i=0,s=(char*)in,so=out; i < inlen/5; i++, s+=5, so+=8) {
		so[0] = base32map[(s[0]&0xf8) >> 3];
		so[1] = base32map[((s[0]&0x07) << 2) | ((s[1]&0xc0) >> 6)];
		so[2] = base32map[(s[1]&0x3e) >> 1];
		so[3] = base32map[((s[1]&0x01) << 4) | ((s[2]&0xf0) >> 4)];
		so[4] = base32map[((s[2]&0x0f) << 1) | ((s[3]&0x80) >> 7)];
		so[5] = base32map[(s[3]&0x7c) >> 2];
		so[6] = base32map[((s[3]&0x02) << 3) | ((s[4]&0xe0) >> 5)];
		so[7] = base32map[s[4] & 0x1f];
	}
	*so = 0;
	i = inlen%5;
	if (i==0) return RERR_OK;
	so[0] = base32map[(s[0]&0xf8) >> 3];
	if (i==1) {
		so[1] = base32map[s[0] & 0x07];
		so++;
		goto finish;
	}
	so[1] = base32map[((s[0]&0x07) << 2) | ((s[1]&0xc0) >> 6)];
	so[2] = base32map[(s[1]&0x3e) >> 1];
	if (i==2) {
		so[3] = base32map[s[1] & 0x01];
		so+=3;
		goto finish;
	}
	so[3] = base32map[((s[1]&0x01) << 4) | ((s[2]&0xf0) >> 4)];
	if (i==3) {
		so[4] = base32map[s[2] & 0x0f];
		so+=4;
		goto finish;
	}
	so[4] = base32map[((s[2]&0x0f) << 1) | ((s[3]&0x80) >> 7)];
	so[5] = base32map[(s[3]&0x7c) >> 2];
	so[6] = base32map[s[3] & 0x02];
	so+=6;
finish:
	so[0] = '9';
	so[1] = '0' + i;
	so[2] = 0;

	return RERR_OK;
}




int
top_unix2dos (ibuf, obuf)
	const char	*ibuf;
	char			**obuf;
{
	char	*buf;
	int	numnl;
	char	*s, *s2, *so;

	if (!ibuf || !obuf) return RERR_PARAM;
	*obuf=NULL;
	s = (char*)ibuf-1;
	numnl=0;
	while ((s=index (s+1, '\n'))) numnl++;
	buf = malloc (strlen (ibuf)+numnl+1);
	if (!buf) return RERR_NOMEM;
	s2=(char*)ibuf; so=buf;
	while ((s=index (s2, '\n'))) {
		if (s!=s2 && *(s2-1) == '\r') continue;
		if (s!=s2) {
			strncpy (so, s2, (s-s2));
			so+=(s-s2);
		}
		*(so++)='\r';
		*(so++)='\n';
		s2=s+1;
	}
	*so=0;
	*obuf = buf;
	return RERR_OK;
}

int
top_dos2unix (ibuf, obuf)
	const char	*ibuf;
	char			**obuf;
{
	char	*buf;
	char	*s, *s2, *so;

	if (!ibuf || !obuf) return RERR_PARAM;
	*obuf = NULL;
	buf = malloc (strlen (ibuf) + 1);
	if (!buf) return RERR_NOMEM;
	s2=(char*)ibuf; so=buf;
	while ((s=index (s2, '\r'))) {
		strncpy (so, s2, (s-s2));
		so+=(s-s2);
		if (*(s+1) != '\n' && *(s+1) != '\r') *(so++)='\n';
		s2=s+1;
	}
	*so=0;
	*obuf = buf;
	return RERR_OK;
}



char *
top_getline (ptr, flags)
	char	**ptr;
	int	flags;
{
	return top_getline2 (ptr, flags, NULL);
}

char *
top_getline2 (ptr, flags, cmtstr)
	char			**ptr;
	int			flags;
	const char	*cmtstr;
{
	char	*line, *s;
	int	haslinesplice = 0;

	if (!ptr || !*ptr) return NULL;
	s = line = *ptr;
	while (1) {
		if (flags & TOP_F_NOCR) {
			for (; *s && *s!='\n'; s++);
		} else {
			for (; *s && *s!='\n' && *s!='\r'; s++);
		}
		if (!(flags & TOP_F_LINESPLICE)) break;
		if (s>line && s[-1] == '\\') {
			haslinesplice=1;
			if (*s == '\r' && s[1] == '\n') s++;
			s++;
			continue;
		}
		break;
	}
	if (*s) {
		*ptr = s+1;
		if (!(flags & TOP_F_NOCR)) {
			if (*s == '\r' && s[1] == '\n') {
				(*ptr)++;
			}
		}
		*s = 0;
	} else {
		*ptr = NULL;
	}
	if (haslinesplice) {
		line = top_striplinesplice (line, flags);
	}
	line = top_stripwhite (line, flags);
	line = top_striplncomment (line, cmtstr?cmtstr:"#", flags);
	if (!*line && !(flags & TOP_F_NOSKIPBLANK)) 
		return top_getline2 (ptr,flags,cmtstr);
	return line;
}

char*
top_getcsvline (ptr)
	char	**ptr;
{
	char	*line, *s;
	int	isquote=0, begfield=1;

	if (!ptr || !*ptr) return NULL;
	line = *ptr;
	for (s=line; *s; s++) {
		if (begfield) {
			if (*s==' ') continue;
			begfield = 0;
			if (*s=='"') {
				isquote = 1;
				continue;
			}
		}
		if (isquote) {
			if (*s == '"') {
				if (s[1] == '"') {
					s++;
				} else {
					isquote = 0;
				}
			}
		} else if (*s == ',') {
			begfield = 1;
		} else if (*s == '\n') {
			*s = 0;
			s++;
			*ptr = s;
			return line;
		}
	}
	*ptr = NULL;
	return line;
}


char*
top_getfield (ptr, sep, flags)
	char			**ptr;
	const char	*sep;
	int			flags;
{
	return top_getfield2 (ptr, sep, NULL, flags);
}

char*
top_getfield2 (ptr, sep, osep, flags)
	char			**ptr;
	const char	*sep;
	char			*osep;	/* return value */
	int			flags;
{
	char	*field, *s;

	if (!ptr || !*ptr) return NULL;
	field = *ptr;
	s = top_findchar (*ptr, sep, flags);
	if (osep) *osep = s ? *s : 0;
	if (s&&*s) {
		*s = 0;
		*ptr = s+1;
	} else {
		*ptr = NULL;
	}
	if (*field) {
		field = top_stripwhite (field, flags);
	}
	if (!(flags & TOP_F_NOSKIPCOMMENT)) {
		field = top_striplncomment (field, "#", flags);
	}
	if ((!field || !*field) && !(flags & TOP_F_NOSKIPBLANK)) {
		return top_getfield (ptr, sep, flags);
	}
	return field;
}

#define _NOQUOTE	1
#define _DQUOTE	2
#define _SQUOTE	3
#define _CSVQUOTE	4
#define _DNQUOTE	5

char *
top_findchar (ptr, pivot, flags)
	const char	*ptr, *pivot;
	int			flags;
{
	int	mode = (flags & TOP_F_DNQUOTE) ? _DNQUOTE : _NOQUOTE;

	if (!ptr || !pivot) return NULL;
	for (; *ptr; ptr++) {
		switch (mode) {
		case _NOQUOTE:
			if (index (pivot, *ptr)) goto out;
			switch (*ptr) {
			case '"':
				if (flags & TOP_F_DQUOTE) {
					mode = _DQUOTE;
				} else if (flags & TOP_F_CSVQUOTE) {
					mode = _CSVQUOTE;
				}
				break;
			case '\'':
				if (flags & TOP_F_SQUOTE) mode = _SQUOTE;
				break;
			}
			break;
		case _DQUOTE:
			if (*ptr=='\\' && ptr[1]) {
				ptr++;
			} else if (*ptr=='"') {
				mode = _NOQUOTE;
			}
			break;
		case _SQUOTE:
			if (*ptr=='\\' && ptr[1]) {
				ptr++;
			} else if (*ptr=='\'') {
				mode = _NOQUOTE;
			}
			break;
		case _CSVQUOTE:
			if (*ptr=='"') {
				if (ptr[1] == '"') {
					ptr++;
				} else {
					mode = _NOQUOTE;
				}
			}
			break;
		case _DNQUOTE:
			if (*ptr=='\\') {
				if (ishex(ptr[1]) && ishex(ptr[2])) {
					ptr+=2;
				//} else if (index (",+\"\\<>;= #", ptr[1])) {
				} else {
					ptr++;
				}
			} else if (index (pivot, *ptr)) {
				goto out;
			}
			break;
		}
	}
out:
	return (char*)(*ptr?ptr:NULL);
}

char*
top_findstr (ptr, needle, flags)
	const char	*ptr, *needle;
	int			flags;
{
	char	pivot[2];
	char	*s;
	int	n;

	if (!ptr || !needle || !*needle) return NULL;
	pivot[0] = *needle;
	pivot[1] = 0;
	n = strlen (needle);
	/* the following function terminates if the searched string (needle)
	   is found, or the string reaches an end.
     */
	while (1) {
		s = top_findchar (ptr, pivot, flags);
		if (!s || !*s) return NULL;
		if (n==1 || !strncmp (s, needle, n)) return s;
		ptr = s+1;
	}
	return NULL;	/* to make compiler happy - never reaches here */
}

char*
top_striplncomment (ptr, cmtstr, flags)
	char			*ptr;
	const char	*cmtstr;
	int			flags;
{
	char	*s;

	if (flags & TOP_F_NOSKIPCOMMENT) return ptr;
	s = top_findstr (ptr, cmtstr, flags);
	if (s) *s=0;
	return ptr;
}

char*
top_skipwhite (ptr)
	const char	*ptr;
{
	if (!ptr) return NULL;
	for (; *ptr; ptr++) if (!iswhite (*ptr)) return (char*)ptr;
	return (char*)ptr;
}

char*
top_skipwhiteplus (str, toskip)
	const char	*str, *toskip;
{
	if (!str) return NULL;
	if (!toskip) toskip = "";
	for (; *str && (iswhite (*str) || index (toskip, *str)); str++);
	return (char*)str;
}

char*
top_stripwhite (str, flags)
	char	*str;
	int	flags;
{
	char	*s, *s2;

	if (!str) return NULL;
	if (!(flags & TOP_F_NOSKIPWHITE) || !(flags & TOP_F_NOSTRIPWHITE)) {
		str = top_skipwhite (str);
	}
	if (flags & TOP_F_NOSTRIPWHITE) return str;
	if (flags & TOP_F_STRIPMIDDLE) {
		for (s2=s=str; *s; s++) {
			if (iswhite (*s)) {
				for (; *s && iswhite (*s); s++);
				if (!*s) break;
				*s2 = ' '; s2++;
			}
			*s2=*s; s2++;
		}
		*s2 = 0;
	} else {
		for (s2=s=str; *s; s++) {
			if (!iswhite (*s)) s2=s+1;
		}
		*s2 = 0;
	}
	return str;
}

char*
top_striplinesplice (str, flags)
	char	*str;
	int	flags;
{
	char	*s, *s2;

	if (!str) return NULL;
	for (s=str; *s && !(*s=='\\' && (s[1]=='\n' || s[1]=='\r')); s++);
	for (s2=s; *s; s++, s2++) {
		if (*s=='\\') {
			if (s[1]=='\n') {
				s+=2;
			} else if (!(flags & TOP_F_NOCR) && (s[1]=='\r')) {
				s+=2;
				if (*s=='\n') s++;
			}
		}
		*s2=*s;
	}
	*s2 = 0;
	return str;
}

char*
top_getquotedfield (next, delim, flags)
	char			**next;
	const char	*delim;
	int			flags;
{
	char	*s, *end;
	int	ret, type;

	if (!next || !*next || !**next) return NULL;
	s = top_skipwhite (*next);
	switch (*s) {
	case '"':
		type = CFPS_T_DOUBLE;
		s++;
		break;
	case '\'':
		type = CFPS_T_SINGLE;
		s++;
		break;
	case '{':
		type = CFPS_T_BBRACE;
		s++;
		break;
	default:
		type = -1;
		break;
	}
	if (type == -1) {
		if (delim) {
			for (end=s; *end && !index (delim, *end); end++);
		} else {
			end=NULL;
		}
	} else {
		ret = cf_find_endquote (&end, s, type);
		if (RERR_ISOK(ret)) {
			*end = 0;
			if (delim) {
				for (end++; *end && !index (delim, *end); end++);
			} else {
				end=NULL;
			}
		} else if (ret == RERR_NOT_FOUND) {
			end = NULL;
		} else {
			return NULL;
		}
	}
	if (end) {
		if (*end) {
			*end = 0;
			end++;
		} else {
			end=NULL;
		}
	}
	if (type == -1) {
		s = top_stripwhite (s, flags);
	} else {
		ret = cf_parse_string2 (&s, s, type);
		if (!RERR_ISOK(ret)) return NULL;
	}
	*next = end;
	return s;
}

char*
top_unquote (str, flags)
	char	*str;
	int	flags;
{
	int	type, ret;
	char	*end;

	if (flags & TOP_F_UNQUOTE_DOUBLE) {
		type = CFPS_T_DOUBLE;
	} else if (flags & TOP_F_UNQUOTE_SINGLE) {
		type = CFPS_T_SINGLE;
	} else if (flags & TOP_F_UNQUOTE_BRACE) {
		type = CFPS_T_BBRACE;
	} else {
		str = top_skipwhite (str);
		switch (*str) {
		case '"':
			type = CFPS_T_DOUBLE;
			str++;
			break;
		case '\'':
			type = CFPS_T_SINGLE;
			str++;
			break;
		case '{':
			type = CFPS_T_BBRACE;
			str++;
			break;
		default:
			type = -1;
			break;
		}
		if (type == -1) {
			return top_stripwhite (str, flags);
		}
		ret = cf_find_endquote (&end, str, type);
		if (RERR_ISOK (ret)) {
			*end = 0;
		} else if (ret != RERR_NOT_FOUND) {
			return NULL;
		}
	}
	ret = cf_parse_string2 (&str, str, type);
	if (!RERR_ISOK (ret)) return NULL;
	return str;
}

int
top_csvunquote (str)
	char	*str;
{
	char	*s;
	int	isquote=0;

	if (!str) return RERR_PARAM;
	for (s=str; *s; s++) {
		if (*s != '"') {
			*str++ = *s;
		} else if (!isquote) {
			isquote = 1;
		} else if (s[1] == '"') {
			*str++ = '"';
			s++;
		} else {
			isquote = 0;
		}
	}
	*str = 0;
	return RERR_OK;
}

char *
top_csvquote (buf, blen, str)
	char	*str, *buf;
	int	blen;
{
	char	*s, *so;
	int	len, need, num, nlen;

	if (!str) return NULL;
	for (s=str, need=num=0; *s; s++) {
		if (*s == ',') need = 1;
		if (*s == '"') num++;
	}
	len = s-str;
	if (need || num || !len) {
		nlen = need = len + num + 2;
	} else {
		nlen = len;
	}
	if (len >= blen || !buf) {
		buf = malloc (nlen+1);
		if (!buf) return NULL;
	}
	if (!need) {
		strcpy (buf, str);
		return buf;
	}
	so = buf;
	*so++ = '"';
	for (s=str; *s; s++) {
		if (*s == '"') *so++ = '"';
		*so++ = *s;
	}
	*so++ = '"';
	*so = 0;
	return buf;
}

char *
top_dnunquote (str)
	char	*str;
{
	char	*s, *so;

	if (!str) return NULL;
	for (s=so=str; *s; s++, so++) {
		if (*s=='\\') {
			if (ishex (s[1]) && ishex (s[2])) {
				*so=(HEX2NUM(s[1]) << 4) | HEX2NUM(s[2]);
				s+=2;
			} else {
				*so=s[1];
				s++;
			}
		} else {
			*so=*s;
		}
	}
	*so = 0;
	return str;
}

int
top_dnquote (buf, blen, str)
	char			*buf;
	int			blen;
	const char	*str;
{
	char			*so;
	const char	*s;

	if (!str || !buf || blen < 1) return RERR_PARAM;
	blen--;
	for (s=str, so=buf; *s && blen; s++, so++, blen--) {
		if (index (",+\"\\<>;=#", *s)) {
			if (blen < 2) break;
			*so = '\\'; so++; *so=*s;
			blen--;
		} else if (*s <= 32 || *s >= 127) {
			if (blen < 3) break;
			*so = '\\';
			so++; *so = NUM2HEX(((unsigned char)*s) >> 4);
			so++; *so = NUM2HEX(*s & 0x0f);
		} else {
			*so = *s;
		}
	}
	*so = 0;
	return *s ? RERR_INVALID_LEN : RERR_OK;
}

int
top_dnquotelen (str)
	const char	*str;
{
	const char	*s;
	int			len;

	if (!str) return RERR_PARAM;
	for (s=str, len=1; *s; s++, len++) {
		if (index (",+\"\\<>;=#", *s)) {
			len++;
		} else if (*s <= 32 || *s >= 127) {
			len+=2;
		}
	}
	return len;
}


int
top_gethexnum (str)
	const char	*str;
{
	unsigned	result;

	if (!str || !*str) return -1;
	if ((*str == '0' || *str =='\\') && (str[1] == 'x' || str[1] == 'X')) {
		str+=2;
	} else if (*str == 'x' || *str == 'X') {
		str++;
	}
	result = (unsigned) -1;
	sscanf (str, "%x", &result);
	return (int) result;
}




int
utf2wchar_char (wch, ptr)
	wchar_t		*wch;
	const char	**ptr;
{
	switch (sizeof (wchar_t)) {
	case 2:
		return utf8ucs2_char ((uint16_t*)wch, ptr);
	case 4:
		return utf8ucs4_char ((uint32_t*)wch, ptr);
	}
	return RERR_INTERNAL;
}

int
utf2wchar_str (wch_str, str)
	wchar_t		**wch_str;
	const char	*str;
{
	switch (sizeof (wch_str)) {
	case 2:
		return utf8ucs2 ((uint16_t**)wch_str, str);
	case 4:
		return utf8ucs4 ((uint32_t**)wch_str, str);
	}
	return RERR_INTERNAL;
}



int
utf8ucs2_char (ucs, utf)
	uint16_t		*ucs;
	const char	**utf;
{
	const char	*s;
	uint16_t		c;
	int			num;

	if (!utf || !*utf) return RERR_PARAM;
	s=*utf;
	if (!(*s&0x80)) {
		c = *s;
		num=0;
	} else if ((*s&0xc0) == 0x80) {
		for (s++; (*s&0xc0) == 0x80; s++);
		*utf = s;
		return RERR_UTF8;
	} else if ((*s&0xe0) == 0xc0) {
		c = *s & 0x1f;
		num=1;
	} else if ((*s&0xf0) == 0xe0) {
		c = *s & 0x0f;
		num=2;
	} else if ((*s&0xfe) == 0xfe) {
		for (s++; (*s&0xc0) == 0x80; s++);
		*utf = s;
		return RERR_UTF8;
	} else {
		/* we translate all chars larger than 0xffff as 0xffff */
		c=0xffff;
		num=0;
		for (s++; (*s&0xc0) == 0x80; s++);
		*utf = s;
	}
	for (s++; num>0; num--, s++) {
		if ((*s&0xc0) != 0x80) {
			*utf = s;
			return RERR_UTF8;
		}
		c<<=6;
		c|=*s&0x3f;
	}
	if (ucs) *ucs = c;
	*utf = s;
	return RERR_OK;
}


int
utf8ucs4_char (ucs, utf)
	uint32_t		*ucs;
	const char	**utf;
{
	const char	*s;
	uint32_t		c;
	int			num;

	if (!utf || !*utf) return RERR_PARAM;
	s=*utf;
	if (!(*s&0x80)) {
		c = *s;
		num=0;
	} else if ((*s&0xc0) == 0x80) {
		for (s++; (*s&0xc0) == 0x80; s++);
		*utf = s;
		return RERR_UTF8;
	} else if ((*s&0xe0) == 0xc0) {
		c = *s & 0x1f;
		num=1;
	} else if ((*s&0xf0) == 0xe0) {
		c = *s & 0x0f;
		num=2;
	} else if ((*s&0xf8) == 0xf0) {
		c = *s & 0x07;
		num=3;
	} else if ((*s&0xfc) == 0xf8) {
		c = *s & 0x03;
		num=4;
	} else if ((*s&0xfe) == 0xfc) {
		c = *s & 0x01;
		num=5;
	} else {
		for (s++; (*s&0xc0) == 0x80; s++);
		*utf = s;
		return RERR_UTF8;
	}
	for (s++; num>0; num--, s++) {
		if ((*s&0xc0) != 0x80) {
			*utf = s;
			return RERR_UTF8;
		}
		c<<=6;
		c|=*s&0x3f;
	}
	if (ucs) *ucs = c;
	*utf = s;
	return RERR_OK;
}


int
utf8ucs2 (ucs, utf)
	uint16_t		**ucs;
	const char	*utf;
{
	int			len;
	const char	*s;
	uint16_t		*ws;

	if (!ucs) return RERR_PARAM;
	if (!utf) {
		*ucs=NULL;
		return RERR_OK;
	}
	len = utf8clen (utf);
	ws = *ucs = malloc ((len+1) * sizeof (uint16_t));
	if (!ws) return RERR_NOMEM;
	for (s=utf; *s; ) {
		if (RERR_ISOK (utf8ucs2_char (ws, &s))) ws++;
	}
	*ws = 0;
	return RERR_OK;
}

int
utf8ucs4 (ucs, utf)
	uint32_t		**ucs;
	const char	*utf;
{
	int			len;
	const char	*s;
	uint32_t		*ws;

	if (!ucs) return RERR_PARAM;
	if (!utf) {
		*ucs=NULL;
		return RERR_OK;
	}
	len = utf8clen (utf);
	ws = *ucs = malloc ((len+1) * sizeof (uint32_t));
	if (!ws) return RERR_NOMEM;
	for (s=utf; *s; ) {
		if (RERR_ISOK (utf8ucs4_char (ws, &s))) ws++;
	}
	*ws = 0;
	return RERR_OK;
}



#if 0
#define BIT8(i)	(((uint8_t)1) << (i))
#define BIT16(i)	(((uint16_t)1) << (i))
#define BIT32(i)	(((uint32_t)1) << (i))
#define LT8(n,i)	(((uint8_t)n) < (BIT8(i)))
#define LT16(n,i)	(((uint16_t)n) < (BIT16(i)))
#define LT32(n,i)	(((uint32_t)n) < (BIT32(i)))
#endif
#if 0
#define BIT64(i)	(((uint64_t)1) << (i))
#define LT64(n,i)	(((uint64_t)n) < (BIT64(i)))
#endif

int
wchar2utf_charlen (wch)
	const wchar_t	wch;
{
	switch (sizeof (wchar_t)) {
	case 2:
		return ucs2utf8_charlen ((uint16_t)wch);
	case 4:
		return ucs4utf8_charlen ((uint32_t)wch);
	}
	return 0;
}


int
wchar2utf_len (wstr)
	const wchar_t	*wstr;
{
	switch (sizeof (wchar_t)) {
	case 2:
		return ucs2utf8_len ((uint16_t*)wstr);
	case 4:
		return ucs4utf8_len ((uint32_t*)wstr);
	}
	return 0;
}


int
wchar2utf_char (ustr, wch)
	char				**ustr;
	const wchar_t	wch;
{
	switch (sizeof (wchar_t)) {
	case 2:
		return ucs2utf8_char (ustr, (uint16_t)wch);
	case 4:
		return ucs4utf8_char (ustr, (uint32_t)wch);
	}
	return RERR_INTERNAL;
}


int
wchar2utf_str (ustr, wstr)
	char				**ustr;
	const wchar_t	*wstr;
{
	switch (sizeof (wchar_t)) {
	case 2:
		return ucs2utf8 (ustr, (uint16_t*)wstr);
	case 4:
		return ucs4utf8 (ustr, (uint32_t*)wstr);
	}
	return RERR_INTERNAL;
}


int
ucs2utf8_charlen (ucs)
	const uint16_t	ucs;
{
	if (ucs < 128) return 1;
	if (ucs < 2048) return 2;
	return 3;
}


int
ucs4utf8_charlen (ucs)
	const uint32_t	ucs;
{
	if (ucs < 128) return 1;
	if (ucs < 2048) return 2;
	if (ucs < ((uint32_t)1<<16)) return 3;
	if (ucs < ((uint32_t)1<<21)) return 4;
	if (ucs < ((uint32_t)1<<26)) return 5;
	if (ucs < ((uint32_t)1<<31)) return 6;
	return 0;
}


int
ucs2utf8_len (ucs)
	const uint16_t	*ucs;
{
	int				len;
	const uint16_t	*s;

	if (!ucs) return 0;
	for (s=ucs,len=0; *s; s++) {
		len += ucs2utf8_charlen (*s);
	}
	return len;
}


int
ucs4utf8_len (ucs)
	const uint32_t	*ucs;
{
	int				len;
	const uint32_t	*s;

	if (!ucs) return 0;
	for (s=ucs,len=0; *s; s++) {
		len += ucs4utf8_charlen (*s);
	}
	return len;
}



int
ucs2utf8_char (utf, ucs)
	char				**utf;
	const uint16_t	ucs;
{
	int		num;

	if (!utf || !*utf) return RERR_PARAM;
	if (ucs < 128) {
		**utf = (char) ucs;
		num=0;
	} else if (ucs < 2048) {
		**utf = (((0x1f << 6) & ucs) >> 6) | 0xc0;
		num=1;
	} else {
		**utf = (((0x0f << 12) & ucs) >> 12) | 0xe0;
		num=2;
	}
	(*utf)++;
	while (num>0) {
		num--;
		**utf = (((0x3f << num) & ucs) >> num) | 0x80;
		(*utf)++;
	}
	return RERR_OK;
}




int
ucs4utf8_char (utf, ucs)
	char				**utf;
	const uint32_t	ucs;
{
	int	num;

	if (!utf || !*utf) return RERR_PARAM;
	if (ucs < 128) {
		**utf = (char) ucs;
		num=0;
	} else if (ucs < 2048) {
		**utf = (((0x1f << 6) & ucs) >> 6) | 0xc0;
		num=1;
	} else if (ucs < ((uint32_t)1<<16)) {
		**utf = (((0x0f << 12) & ucs) >> 12) | 0xe0;
		num=2;
	} else if (ucs < ((uint32_t)1<<21)) {
		**utf = (((0x07 << 18) & ucs) >> 18) | 0xf0;
		num=3;
	} else if (ucs < ((uint32_t)1<<26)) {
		**utf = (((0x03 << 24) & ucs) >> 24) | 0xf8;
		num=4;
	} else if (ucs < ((uint32_t)1<<31)) {
		**utf = (((0x01 << 30) & ucs) >> 30) | 0xfc;
		num=5;
	} else {
		return RERR_UTF8;
	}
	(*utf)++;
	while (num>0) {
		num--;
		**utf = (((0x3f << num) & ucs) >> num) | 0x80;
		(*utf)++;
	}
	return RERR_OK;
}



int
ucs2utf8 (utf, ucs)
	char				**utf;
	const uint16_t	*ucs;
{
	int				len;
	char				*str;
	const uint16_t	*s;

	if (!utf || !ucs) return RERR_PARAM;
	len = ucs2utf8_len (ucs);
	str = malloc (len+1);
	if (!str) return RERR_NOMEM;
	*utf = str;
	for (s = ucs; *s; s++) {
		ucs2utf8_char (&str, *s);
	}
	*str = 0;
	return RERR_OK;
}


int
ucs4utf8 (utf, ucs)
	char				**utf;
	const uint32_t	*ucs;
{
	int				len;
	char				*str;
	const uint32_t	*s;

	if (!utf || !ucs) return RERR_PARAM;
	len = ucs4utf8_len (ucs);
	str = malloc (len+1);
	if (!str) return RERR_NOMEM;
	*utf = str;
	for (s = ucs; *s; s++) {
		ucs4utf8_char (&str, *s);
	}
	*str = 0;
	return RERR_OK;
}




size_t
utf8blen (utf)
	const char	*utf;
{
	size_t	len = 0;

	if (!utf) return 0;
	for (;*utf;utf++) len++;
	return len;
}


size_t
utf8clen (utf)
	const char	*utf;
{
	size_t	len = 0;

	if (!utf) return 0;
	for (;*utf;utf++) {
		if (!(((*utf & 0xc0) == 0x80) || ((*utf & 0xfe) == 0xfe))) len++;
	}
	return len;
}

size_t
utf8tlen (utf)
	const char	*utf;
{
	/* to be done... - for now return char len */
	return utf8clen (utf);
}


int
utf8bcmp (str1, str2)
	const char	*str1, *str2;
{
	if (!str1 || !str2) {
		if (!str1 && !str2) return 0;
		if (!str1) return -1;
		return 1;
	}
	for (; *str1 && *str2; str1++, str2++) {
		if (*str1 < *str2) return -1;
		if (*str2 > *str1) return 1;
	}
	if (!*str1 && !*str2) return 0;
	if (!*str1) return -1;
	return 1;
}

int
utf8ccmp (str1, str2)
	const char	*str1, *str2;
{
	uint32_t	c1, c2;
	int		ret1, ret2;

	if (!str1 || !str2) {
		if (!str1 && !str2) return 0;
		if (!str1) return -1;
		return 1;
	}
	for (; *str1 && *str2; ) {
		ret1 = utf8ucs4_char (&c1, &str1);
		ret2 = utf8ucs4_char (&c2, &str2);
		if (!RERR_ISOK(ret1) || !RERR_ISOK(ret2)) {
			/* QUESTION: can the following line lead to a security risk,
			             and how can it made better? */
			if (!RERR_ISOK(ret1) && !RERR_ISOK(ret2)) continue;
			if (!RERR_ISOK(ret1)) return -2;
			return 2;
		}
		if (c1 < c2) return -1;
		if (c1 > c2) return 1;
	}
	if (!*str1 && !*str2) return 0;
	if (!*str1) return -1;
	return 1;
}


int
utf8tcmp (str1, str2)
	const char	*str1, *str2;
{
	/* TODO: currently we return a char-compare */
	return utf8ccmp (str1, str2);
}



int
utf8lcmp (str1, str2)
	const char	*str1, *str2;
{
	/* TODO: currently we return a text-compare */
	return utf8tcmp (str1, str2);
}



/*!\brief converts latin1 strings to utf8 */
int
top_lat1utf8 (dest, src, max)
	char			*dest;
	const char	*src;
	int			max;
{
	unsigned char	*s2, *s;

	if (!dest || !src) return RERR_PARAM;
	if (max<0) max=0;
	for (s2=(unsigned char*)dest, s=(unsigned char*)src; *s && max != 1; s++, s2++) {
		if (*s<128) {
			*s2=*s;
		} else if (max > 2 || max == 0) {
			*s2 = 0xc0 | ((*s & 0xc0) >> 6);
			s2++;
			if (max) max--;
			*s2 = 0x80 | (*s & 0x3f);
		} else {
			break;
		}
		if (max) max--;
	}
	*s2=0;
	return s2-(unsigned char*)dest;
}

/*!\brief converts utf8 strings to latin1 */
int
top_utf8lat1 (dest, src, max)
	char			*dest;
	const char	*src;
	int			max;
{
	uint32_t	ucs;
	char		*s;
	int		ret;

	if (!dest || !src) return RERR_PARAM;
	if (max<0) max=0;
	for (s=dest; *src && max!=1; s++) {
		ret = utf8ucs4_char (&ucs, &src);
		if (!RERR_ISOK(ret)) return ret;
		if (ucs>255) {
			*s=255;
		} else {
			*s=(unsigned char)ucs;
		}
	}
	*s=0;
	return s-dest;
}

int
top_utf8to16 (dest, src)
	uint16_t		**dest;
	const char	*src;
{
	int				clen;
	uint16_t			*d;
	const uint8_t	*s;
	uint32_t			t;

	if (!dest || !src) return RERR_PARAM;
	clen = fu8_clen (src);
	if (!RERR_ISOK(clen)) return clen;
	/* at most 4 Byte per char */
	*dest = malloc (4*clen+2);
	if (!dest) return RERR_NOMEM;
	d = *dest;
	s = (const uint8_t*) src;
	while (*s) {
		if ((*s & 0x80) == 0) {
			*d = (uint16_t) *s;
			d++;
			s++;
		} else if ((*s & 0xe0) == 0xc0) {
			if (!s[1] || (s[1] & 0xc0) != 0x80) {
				/* invalid char - skip */
				s++;
				continue;
			}
			*d = (((uint16_t) (*s & 0x1f)) << 6) | 
					((uint16_t) (s[1] & 0x3f));
			d++;
			s+=2;
		} else if ((*s & 0xf0) == 0xe0) {
			if (!s[1] || (s[1] & 0xc0) != 0x80 || !s[2] || (s[2] & 0xc0) != 0x80) {
				/* invalid char - skip */
				s++;
				continue;
			}
			*d = (((uint16_t) (*s & 0x0f)) << 12) | 
					(((uint16_t) (s[1] & 0x3f)) << 6) |
					((uint16_t) (s[2] & 0x3f));
			d++;
			s+=3;
		} else if ((*s & 0xf8) == 0xf0) {
			if (!s[1] || (s[1] & 0xc0) != 0x80 || !s[2] || (s[2] & 0xc0) != 0x80 
						|| !s[3] || (s[3] & 0xc0) != 0x80) {
				/* invalid char - skip */
				s++;
				continue;
			}
			/* more complicated - we need surrogate chars */
			/* first convert to utf32 */
			t = (((uint32_t) (*s & 0x07)) << 18) | 
					(((uint32_t) (s[1] & 0x3f)) << 12) |
					(((uint32_t) (s[2] & 0x3f)) << 6) |
					((uint32_t) (s[3] & 0x3f));
			s += 4;
			/* subtract bmp */
			t -= 0x10000;
			/* check for range - value must fit into 20 bit */
			if ((t & 0xfffff) != t) {
				/* invalid char - skip */
				continue;
			}
			d[0] = ((uint16_t)(t > 10)) | 0xd800;		/* high surrogate */
			d[1] = ((uint16_t)(t & 0x3ff)) | 0xdc00;	/* low surrogate */
			d+=2;
		} else {
			/* invalid char - skip */
			s++;
			continue;
		}
	}
	*d = 0;
	return (d - *dest);
}

int
top_utf16to8 (dest, src)
	char				**dest;
	const uint16_t	*src;
{
	uint8_t			*d;
	const uint16_t	*s;
	int				len;
	uint32_t			t;

	if (!dest || !src) return RERR_PARAM;
	/* calculate length */
	for (len=1, s=src; *s; s++) {
		if ((*s & 0xd800) == 0xd800) {
			len += 4;
			s++;
		} else if (*s < 128) {
			len++;
		} else if (*s < 2048) {
			len += 2;
		} else {
			len += 3;
		}
	}
	d = malloc (len);
	if (!d) return RERR_NOMEM;
	*dest = (char*)d;
	for (s=src; *s; s++) {
		if ((*s & 0xd800) == 0xd800) {
			if (((s[0] & 0xdc00) != 0xd800) || ((s[1] & 0xdc00) != 0xdc00)) {
				/* invalid char - skip */
				continue;
			}
			t = (((uint32_t)(s[0] & 0x3ff)) << 10) |
					((uint32_t)(s[1] & 0x3ff));
			t += 0x10000;
			d[0] = ((t & 0x1c0000) >> 18) | 0xf0;
			d[1] = ((t & 0x03f000) >> 12) | 0x80;
			d[2] = ((t & 0x000fc0) >>  6) | 0x80;
			d[3] =  (t & 0x00003f)        | 0x80;
			d+=4;
			s++;
		} else if (*s < 128) {
			*d = (uint8_t)*s;
			d++;
		} else if (*s < 2048) {
			d[0] = ((*s & 0x07c0) >> 6) | 0xc0;
			d[1] =  (*s & 0x003f)       | 0x80;
			d+=2;
		} else {
			d[0] = ((*s & 0xf000) >> 12) | 0xe0;
			d[1] = ((*s & 0x0fc0) >>  6) | 0x80;
			d[2] =  (*s & 0x003f)        | 0x80;
			d+=3;
		}
	}
	*d = 0;
	return (d - (uint8_t*)*dest);
}

int
top_utf8to16hex (dest, src, upper)
	char			**dest;
	const char	*src;
	int			upper;
{
	uint16_t		*bdest, *bd;
	char			*d, t;
	int			len;

	if (!dest || !src) return RERR_PARAM;
	len = top_utf8to16 (&bdest, src);
	if (!RERR_ISOK(len)) return len;
	d = malloc (len * 4 + 1);
	if (!d) {
		free (bdest);
		return RERR_NOMEM;
	}
	*dest = d;
	for (bd = bdest; *bd; bd++, d+=4) {
		t = ((*bd & 0xf000) >> 12);
		d[0] = upper ? NUM2HEXU (t) : NUM2HEX(t);
		t = ((*bd & 0x0f00) >> 8);
		d[1] = upper ? NUM2HEXU (t) : NUM2HEX(t);
		t = ((*bd & 0x00f0) >> 4);
		d[2] = upper ? NUM2HEXU (t) : NUM2HEX(t);
		t = (*bd & 0x000f);
		d[3] = upper ? NUM2HEXU (t) : NUM2HEX(t);
	}
	*d = 0;
	free (bdest);
	return (d - *dest);
}

int
top_utf16hexto8 (dest, src)
	char			**dest;
	const char	*src;
{
	uint16_t		*bsrc, *bs;
	const char	*s;
	int			ret;

	if (!dest || !src) return RERR_PARAM;
	bs = bsrc = malloc (strlen (src) * 2 + 4);
	if (!bsrc) return RERR_NOMEM;
	for (s = src; *s; s+=4, bs++) {
		if (!s[1] || !s[2] || !s[3]) break;
		*bs = (((uint16_t)HEX2NUM (s[0])) << 12) |
				(((uint16_t)HEX2NUM (s[1])) <<  8) |
				(((uint16_t)HEX2NUM (s[2])) <<  4) |
				 ((uint16_t)HEX2NUM (s[3]));
	}
	*bs = 0;
	ret = top_utf16to8 (dest, bsrc);
	free (bsrc);
	return ret;
}


static uint16_t	gsm_map[] = {
	'@', 0xa3, '$', 0xa5, 0xe8, 0xe9, 0xf9, 0xec, 0xf2, 0xe7, '\n', 0xd8, 0xf8,
		'\r', 0xc5, 0xe5,
	0x0394 /* delta */, '_', 0x03a6 /* phi */, 0x0393 /* gamma */,
		0x039b /* lambda */, 0x3a9 /* omega */, 0x03a0 /* pi */,
		0x03a8 /* psi */, 0x03a3 /* sigma */, 0x0398/* theta */,
		0x039e /* xi */, '\e', 0xc6, 0xe6, 0xdf, 0xc9,
	' ', '!', '"', '#', 0xa4, '%', '&', '\'', '(', ')', '*', '+', ',', '-',
		'.', '/',
	'0', '1', '2', '3', '4', '5', '6', '7', '8', '9', ':', ';', '<', '=', '>',
		'?',
	0xa1, 'A', 'B', 'C', 'D', 'E', 'F', 'G', 'H', 'I', 'J', 'K', 'L', 'M', 'N',
		'O',
	'P', 'Q', 'R', 'S', 'T', 'U', 'V', 'W', 'X', 'Y', 'Z', 0xc4, 0xd6, 0xd1,
		0xdc, 0xa7,
	0xbf, 'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h', 'i', 'j', 'k', 'l', 'm', 'n',
		'o', 
	'p', 'q', 'r', 's', 't', 'u', 'v', 'w', 'x', 'y', 'z', 0xe4, 0xf6, 0xf1,
		0xfc, 0xe0,
	
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0x0c, 0, 0, 0, 0, 0,
	0, 0, 0, 0, '^', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, '{', '}', 0, 0, 0, 0, 0, '\\',
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, '[', '~', ']', 0,
	'|', 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0x20ac, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
	0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0
};

int top_u16isgsm7_char (uint16_t c);
int top_u16isgsm7 (uint16_t *s);
int top_u8isgsm7 (const char *str);
int
top_u16isgsm7_char (c)
	uint16_t	c;
{
	int	i;

	if (c >= 32 && c < 128) return 1;
	if (c == 0x20ac) return 1;
	if (c > 0x03a9) return 0;
	if (c > 255 && c < 0x0393) return 0;
	for (i=0; i<256; i++)
		if (gsm_map[i] == c) return 1;
	return 0;
}

int
top_u16isgsm7 (s)
	uint16_t	*s;
{
	if (!s) return 0;
	for (; *s; s++) 
		if (!top_u16isgsm7_char (*s)) return 0;
	return 1;
}

int
top_u8isgsm7 (str)
	const char	*str;
{
	const uint8_t	*s;
	uint16_t			t;

	for (s = (const uint8_t*)str; *s; s++) {
		if (*s < 128) {
			if (!top_u16isgsm7_char (*s)) return 0;
		} else if ((*s & 0xe0) == 0x0c) {
			if ((*s & 0xc0) != 0x80) return 0;
			t = (((uint16_t)(s[0] & 0x1f)) << 6) |
				  ((uint16_t)(s[1] & 0x3f));
			if (!top_u16isgsm7_char (t)) return 0;
			s++;
		} else {
			return 0;
		}
	}
	return 1;
}


int64_t
top_atoi64 (text)
	const char	*text;
{
	int64_t	res;
	int		neg=0;

	if (!text) return 0;
	res = 0;
	text = top_skipwhiteplus ((char*)text, "+");
	if (*text == '-') {
		neg=1;
		text++;
	}
	while (isdigit (*text)) {
		res *= 10;
		res += *text - '0';
		text++;
	}
	if (neg) res *= -1;
	return res;
}


uint64_t
top_atou64 (text)
	const char	*text;
{
	uint64_t	res;

	if (!text) return 0;
	res = 0;
	text = top_skipwhiteplus ((char*)text, "+-");
	while (isdigit (*text)) {
		res *= 10;
		res += *text - '0';
		text++;
	}
	return res;
}

int
top_isnum (str)
	const char	*str;
{
	int	hasdigit = 0;

	if (!str || !*str) return 0;
	str = top_skipwhiteplus (str, "+-");
	while (*str) {
		if (!isdigit (*str)) return 0;
		hasdigit=1;
		str++;
	}
	return hasdigit;
}

char*
top_strcpdup (buf, blen, str)
	char			*buf;
	size_t		blen;
	const char	*str;
{
	return top_strcpdup2 (buf, blen, str, str?strlen(str):0);
}

char*
top_strcpdup2 (buf, blen, str, slen)
	char			*buf;
	size_t		blen, slen;
	const char	*str;
{
	char	*out;

	if (!buf || blen < 1 || slen >= blen) {
		out = malloc (slen+1);
		if (!out) return NULL;
	} else {
		out = buf;
	}
	strncpy (out, str, slen);
	out[slen]=0;
	return out;
}


char*
top_memcpdup (buf, blen, str, slen)
	char			*buf;
	size_t		blen, slen;
	const char	*str;
{
	char	*out;

	if (!buf || blen < 1 || slen >= blen) {
		out = malloc (slen+1);
		if (!out) return NULL;
	} else {
		out = buf;
	}
	memcpy (out, str, slen);
	out[slen]=0;
	return out;
}


char*
top_quotestrcpdup (buf, blen, str, flags)
	char			*buf;
	size_t		blen;
	const char	*str;
	int			flags;
{
	return top_quotestrcpdup2 (buf, blen, str, str?strlen(str):0, flags);
}

char*
top_quotestrcpdup2 (buf, blen, str, slen, flags)
	char			*buf;
	size_t		blen, slen;
	const char	*str;
	int			flags;
{
	char		*out;
	ssize_t	len;
	int		ret;

	len = top_quotelen2 (str, slen, flags);
	if (len < 0) return NULL;
	if (buf && (size_t)len < blen) {
		out = buf;
	} else {
		out = malloc (len+1);
		if (!out) return NULL;
	}
	ret = top_quotestr2 (out, str, slen, flags);
	if (!RERR_ISOK(ret)) {
		if (out != buf) free (out);
		return NULL;
	}
	return out;
}

char*
top_quotestrdup (str, flags)
	const char	*str;
	int			flags;
{
	return top_quotestrdup2 (str, str?strlen(str):0, flags);
}

char*
top_quotestrdup2 (str, slen, flags)
	const char	*str;
	size_t		slen;
	int			flags;
{
	char		*out;
	ssize_t	len;
	int		ret;

	len = top_quotelen2 (str, slen, flags);
	if (len < 0) return NULL;
	out = malloc (len+1);
	if (!out) return NULL;
	ret = top_quotestr2 (out, str, slen, flags);
	if (!RERR_ISOK(ret)) {
		free (out);
		return NULL;
	}
	return out;
}

#define QCHAR	"\\\"\'$\n\r\a\f\t\v"
ssize_t
top_quotelen (str, flags)
	const char	*str;
	int			flags;
{
	return top_quotelen2 (str, str ? strlen (str) : 0, flags);
}

ssize_t
top_quotelen2 (str, slen, flags)
	const char	*str;
	size_t		slen;
	int			flags;
{
	size_t	len, len2;

	if (flags & TOP_F_NOQUOTESIGN) flags |= TOP_F_NOQUOTEEMPTY;	
	if (slen==0) return (flags & TOP_F_NOQUOTEEMPTY) ? 0 : 2;
	for (len=len2=0; len2<slen; str++, len2++, len++) {
		if (!*str) {
			len++;
		} else if (index (QCHAR, *str)) {
			if (!((*str == '\n') && (flags & TOP_F_QUOTE_NONL))) {
				len++;
			}
		} else if (*str < 32 && *str > 0) {
			len+=3;
		} else if ((unsigned char)*str > 127 && (flags & TOP_F_QUOTEHIGH)) {
			len+=3;
		} else if (*str == ':' && (flags & TOP_F_QUOTECOLON)) {
			len+=3;
		}
	}
	if (!(flags & TOP_F_NOQUOTESIGN) && (len != len2 || (flags & TOP_F_FORCEQUOTE))) len+=2;
	return len;
}

int
top_quotestr (out, str, flags)
	char			*out;
	const char	*str;
	int			flags;
{
	return top_quotestr2 (out, str, strlen (str), flags);
}

int
top_quotestr2 (out, str, len, flags)
	char			*out;
	const char	*str;
	size_t		len;
	int			flags;
{
	size_t	i;

	if (!out) return RERR_PARAM;
	if (!str || len == 0) {
		if ((flags & TOP_F_NOQUOTESIGN) || (flags & TOP_F_NOQUOTEEMPTY)) {
			*out = 0;
		} else {
			strcpy (out, "\"\"");
		}
		return RERR_OK;
	}
	if (top_quotelen2 (str, len, flags) == (ssize_t)len) {
		if ((flags & TOP_F_FORCEQUOTE)) {
			*out = '"';
			out++;
		}
		strncpy (out, str, len);
		out += len;
		if ((flags & TOP_F_FORCEQUOTE)) {
			*out = '"';
			out++;
		}
		*out = 0;
		return RERR_OK;
	}
	if (!(flags & TOP_F_NOQUOTESIGN)) {
		*out = '\"';
		out++;
	}
	for (i=0; i<len; i++, str++, out++) {
		if (!*str) {
			*out = '\\';
			out++;
			*out = '0';
		} else if (index (QCHAR, *str)) {
			*out = '\\';
			out++;
			switch (*str) {
			case '\\':
				*out = '\\';
				break;
			case '\"':
				*out = '\"';
				break;
			case '\'':
				*out = '\'';
				break;
			case '$':
				*out = '$';
				break;
			case '\n':
				if (flags & TOP_F_QUOTE_NONL) {
					out--;
					*out = '\n';
				} else {
					*out = 'n';
				}
				break;
			case '\r':
				*out = 'r';
				break;
			case '\a':
				*out = 'a';
				break;
			case '\f':
				*out = 'f';
				break;
			case '\t':
				*out = 't';
				break;
			case '\v':
				*out = 'v';
				break;
			default:
				return RERR_INTERNAL;
				break;
			}
		} else if (*str > 0 && *str < 32) {
			out += sprintf (out, "\\x%02x", (unsigned)*str) - 1;
		} else if (*str == ':' && (flags & TOP_F_QUOTECOLON)) {
			out += sprintf (out, "\\x%02x", (unsigned)':') - 1;
		} else if ((unsigned char)*str > 127 && (flags & TOP_F_QUOTEHIGH)) {
			out += sprintf (out, "\\x%02x", (unsigned)(unsigned char)*str) - 1;
		} else {
			*out = *str;
		}
	}
	if (!(flags & TOP_F_NOQUOTESIGN)) {
		*out = '\"';
		out++;
	}
	*out = 0;
	return RERR_OK;
}




uint64_t
top_sizescan (str)
	const char	*str;
{
	uint64_t	num;

	if (!str || !*str) return 0;
	str = top_skipwhite (str);
	for (num=0; isdigit (*str); str++) {
		num*=10;
		num+=*str-'0';
	}
	str = top_skipwhite (str);
	switch (*str) {
	case 'P':
	case 'p':
		num*=1024;
		/* fall thru */
	case 'T':
	case 't':
		num*=1024;
		/* fall thru */
	case 'G':
	case 'g':
		num*=1024;
		/* fall thru */
	case 'M':
	case 'm':
		num*=1024;
		/* fall thru */
	case 'k':
	case 'K':
		num*=1024;
	}
	return num;
}


int
top_varcmp (var1, var2)
	const char	*var1, *var2;
{
	unsigned char	c1, c2;

	if (!var1 && !var2) return 0;
	if (!var1) return -1;
	if (!var2) return 1;
	var1 = top_skipwhiteplus (var1, "/");
	var2 = top_skipwhiteplus (var2, "/");
	if (*var1 == '_' && *var2 != '_') return -1;
	if (*var1 != '_' && *var2 == '_') return 1;
	for (; *var1 && *var2; var1++, var2++) {
		if (*var1 == '/' || *var2=='/') {
			if (*var2 != '/') return -1;
			if (*var1 != '/') return 1;
			for (; *var1 == '/' || iswhite (*var1); var1++);
			for (; *var2 == '/' || iswhite (*var2); var2++);
			if (*var1 == '_' && *var2 != '_') return -1;
			if (*var1 != '_' && *var2 == '_') return 1;
		}
		for (; *var1 == '_' || iswhite (*var1); var1++);
		for (; *var2 == '_' || iswhite (*var2); var2++);
		c1 = *var1;
		c1 = c1 >='A' && c1 <='Z' ? c1 - 'A' + 'a' : c1;
		c2 = *var2;
		c2 = c2 >='A' && c2 <='Z' ? c2 - 'A' + 'a' : c2;
		if (!c1 || !c2) break;
		if (c1 < c2) return -1;
		if (c1 > c2) return 1;
	}
	for (; iswhite (*var1); var1++);
	for (; iswhite (*var2); var2++);
	if (*var1) return 1;
	if (*var2) return -1;
	return 0;
}


int
top_quotprintdecode (ibuf, obuf, olen)
	const char	*ibuf;
	char			**obuf;
	int			*olen;
{
	char			*buf, *so;
	const char	*si;
	int			blen, c;

	if (!ibuf || !obuf) return RERR_PARAM;
	buf = malloc (strlen (ibuf) + 1);
	if (!buf)  return RERR_NOMEM;
	for (si=ibuf, so=buf; *si; si++) {
		if (*si != '=') {
			*so++ = *si;
			continue;
		}
		si++;
		if (*si == '=') {
			*so++ = *si;
			continue;
		}
		if (*si == '\n') continue;
		if (*si == '\r' && si[1] == '\n') {
			si++;
			continue;
		}
		c = HEX2NUM(*si) * 16;
		si++;
		c += HEX2NUM(*si);
		*so++ = c;
	}
	*so = 0;
	blen = so - buf;
	*obuf = realloc (buf, blen+1);
	if (!*obuf) *obuf = buf;
	if (olen) *olen = blen;
	return RERR_OK;
}




int
top_fmtsubst (obuf, fmt, subst, flags)
	char							**obuf;
	const char					*fmt;
	const struct top_subst	*subst;
	int							flags;
{
	size_t						len;
	const char					*s;
	char							*s2;
	const struct top_subst	*p;

	if (!fmt || !subst || !obuf) return RERR_PARAM;
	for (s=fmt, len=0; *s; s++, len++) {
		if (*s=='%') {
			s++;
			for (p=subst; p->fc; p++) {
				if (p->fc == *s) {
					len += p->subst ? strlen (p->subst) : 0;
					break;
				}
			}
		}
	}
	*obuf = malloc (len+1);
	if (!*obuf) return RERR_NOMEM;
	for (s2=*obuf, s=fmt; *s; s++) {
		if (*s=='%') {
			s++;
			if (*s=='%') {
				*s2='%';
				s2++;
			} else {
				for (p=subst; p->fc; p++) {
					if (p->fc == *s) {
						if (p->subst) {
							strcpy (s2, p->subst);
							s2 += strlen (p->subst);
						}
						break;
					}
				}
				if (!p->fc) {
					*s2 = *s;
					s2++;
				}
			}
		} else {
			*s2 = *s;
			s2++;
		}
	}
	*s2 = 0;
	return RERR_OK;
}



int
top_parseflags (sflags, map)
	const char						*sflags;
	const struct top_flagmap	*map;
{
	char								*s, *sf, *sfb, buf[128];
	const struct top_flagmap	*p;
	int								flags=0;

	if (!sflags || !map) return 0;
	sfb = sf = top_strcpdup (buf, sizeof (buf), sflags);
	if (!sf) return RERR_NOMEM;
	while ((s=top_getfield (&sf, ",|", 0))) {
		for (p=map; p->s; p++) {
			if (!strcasecmp (s, p->s)) {
				flags |= p->v;
				break;
			}
		}
	}
	if (sfb != buf) free (sfb);
	return flags;
}

int
top_prtflags (buf, buflen, flags, map, sep)
	char								*buf;
	int								buflen;
	int								flags;
	const struct top_flagmap	*map;
	const char						*sep;
{
	const struct top_flagmap	*p;
	int								len=0, first=1;
	int								fl;

	if (!map) return 0;
	if (!sep) sep=", ";
#define _FSTR  (buf ? buf + len : NULL)
#define _FLEN  (buflen > len ? buflen - len : 0)
	for (fl=1; fl; fl<<=1) {
		if (!(flags & fl)) continue;
		for (p=map; p->s; p++) {
			if (p->v != fl) continue;
   		len += snprintf (_FSTR, _FLEN, "%s%s", first?"":sep, p->s);
			first=0;
			break;
		}
	}
#undef _FSTR
#undef _FLEN
	return len;
}


int
top_prtsgflag (buf, buflen, flag, map, sep)
	char								*buf;
	int								buflen;
	int								flag;
	const struct top_flagmap	*map;
	const char						*sep;
{
	const struct top_flagmap	*p;
	int								len=0, first=1;

	if (!map) return 0;
	if (!sep) sep=", ";
#define _FSTR  (buf ? buf + len : NULL)
#define _FLEN  (buflen > len ? buflen - len : 0)
	for (p=map; p->s; p++) {
		if (p->v != flag) continue;
  		len += snprintf (_FSTR, _FLEN, "%s%s", first?"":sep, p->s);
		first=0;
	}
#undef _FSTR
#undef _FLEN
	return len;
}

int
top_prtflaghelp (buf, buflen, plen, nmap, hmap)
   char 								*buf;
   int   							buflen, plen;
	const struct top_flagmap	*nmap, *hmap;
{
	const struct top_flagmap	*p;
	int   							len=0, xlen, fl;

	if (!nmap || !hmap) return 0;
	if (plen < 0) plen=0;
#define _FSTR  (buf ? buf + len : NULL)
#define _FLEN  (buflen > len ? buflen - len : 0)
	for (fl=1; fl; fl<<=1) {
		for (p=nmap; p->s; p++) {
			if (p->v == fl) break;
		}
		if (!p->s) continue;
		len += snprintf (_FSTR, _FLEN, "%*c", plen, ' ');
		xlen = top_prtsgflag (_FSTR, _FLEN, fl, nmap, " | ");
		len += xlen;
		for (p=hmap; p->s; p++) {
			if (p->v == fl) break;
		}
		if (!p->s) {
			len += snprintf (_FSTR, _FLEN, "\n");
		} else if (xlen >= 9) {
			len += snprintf (_FSTR, _FLEN, "\n%*c%s\n", plen+12, ' ', p->s);
		} else {
			len += snprintf (_FSTR, _FLEN, "%*c- %s\n", 10-xlen, ' ', p->s);
		}
	}
#undef _FSTR
#undef _FLEN
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
