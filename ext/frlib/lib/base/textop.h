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

#ifndef _R__FRLIB_LIB_BASE_TEXTOP_H
#define _R__FRLIB_LIB_BASE_TEXTOP_H

#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif


#define iswhite(c)	isspace(c)
#define isnum(c)		isdigit(c)
#define range(c,l,u)	(((c)>=(l)) && ((c)<=(u)))
#define ishex(c)		(range(c,'0','9') || range(c,'a','f') || range(c,'A','F'))
#define isoctal(c)	(range(c,'0','7'))
#define ISB64CHAR(c)	((range(c,'0','9') || range(c,'a','z') || \
								range(c,'A','Z')) || ((c)=='/') || ((c)=='+') || \
								iswhite(c) || ((c)=='=') || ((c)=='+'))
#define HEX2NUM(c)	((((c)>='0') && ((c)<='9')) ? ((c) - '0') : \
								((((c)>='a') && ((c)<='f')) ? ((c) - 'a' + 10) : \
								((((c)>='A') && ((c)<='F')) ? ((c) - 'A' + 10) : 0)))
#define NUM2HEX(c)	(((c)<10) ? ((c) + '0') : (((c) < 16) ? ((c) - 10 + 'a') : 'f'))
#define NUM2HEXU(c)	(((c)<10) ? ((c) + '0') : (((c) < 16) ? ((c) - 10 + 'A') : 'F'))


#define TOP_F_NONE				0x00000
#define TOP_F_DQUOTE				0x00001
#define TOP_F_SQUOTE				0x00002
#define TOP_F_STRIPMIDDLE		0x00004
#define TOP_F_NOSKIPWHITE		0x00008
#define TOP_F_NOSTRIPWHITE		0x00010
#define TOP_F_NOCR				0x00020
#define TOP_F_NOSKIPBLANK		0x00040
#define TOP_F_NOSKIPCOMMENT	0x00080
#define TOP_F_FORCEQUOTE		0x00100
#define TOP_F_NOQUOTEEMPTY		0x00200
#define TOP_F_NOQUOTESIGN		0x00400
#define TOP_F_LINESPLICE		0x00800
#define TOP_F_QUOTECOLON		0x01000
/* for top_unquote */
#define TOP_F_UNQUOTE_DOUBLE	0x02000
#define TOP_F_UNQUOTE_SINGLE	0x04000
#define TOP_F_UNQUOTE_BRACE	0x08000
#define TOP_F_UNQUOTE_AUTO		0
#define TOP_F_QUOTEHIGH			0x10000
#define TOP_F_QUOTE_NONL		0x20000
#define TOP_F_CSVQUOTE			0x40000
#define TOP_F_DNQUOTE			0x80000


int top_gethexnum (const char *str);

int top_base64decode (const char *in, char **out, int *outlen);
int top_base64decode_buf (const char *in, char *out, int *outlen);
int top_base64decode2 (const char *in, int inlen, char **out, int *outlen);
int top_base64decode2_buf (const char *in, int inlen, char *out, int *outlen);
int top_base64encode (const char *in, int inlen, char **out);
int top_base64encode_buf (const char *in, int inlen, char *out);
int top_base64encode_short (const char *in, int inlen, char *out);
int top_base32encode_short (const char *in, int inlen, char *out);
int top_dos2unix (const char *ibuf, char **obuf);
int top_unix2dos (const char *ibuf, char **obuf);
int top_quotprintdecode (const char *ibuf, char **obuf, int *olen);

char *top_getline (char **ptr, int flags);
char *top_getline2 (char **ptr, int flags, const char *cmtstr);
char *top_getcsvline (char **ptr);
char *top_getfield (char **ptr, const char *sep, int flags);
char *top_getfield2 (char **ptr, const char *sep, char *osep, int flags);
char *top_getquotedfield (char **ptr, const char *sep, int flags);
char *top_unquote (char *str, int flags);
char *top_findchar (const char *ptr, const char *pivot, int flags);
char *top_findstr (const char *ptr, const char *needle, int flags);
char *top_striplncomment (char *ptr, const char *cmtstr, int flags);
char *top_skipwhite (const char *str);
char *top_skipwhiteplus (const char *str, const char *toskip);
char *top_stripwhite (char *str, int flags);
char *top_striplinesplice (char *str, int flags);
int top_delcr (char * str);
int top_csvunquote (char *str);
char *top_csvquote (char *buf, int blen, char *str);
char *top_dnunquote (char *str);
int top_dnquote (char *buf, int blen, const char *str);
int top_dnquotelen (const char *str);

int64_t top_atoi64 (const char*);
uint64_t top_atou64 (const char*);
int top_isnum (const char *str);


int utf2wchar_char (wchar_t*, const char**);
int utf2wchar_str (wchar_t**, const char*);

int utf8ucs2_char (uint16_t*, const char**);
int utf8ucs2 (uint16_t**, const char*);

int utf8ucs4_char (uint32_t*, const char**);
int utf8ucs4 (uint32_t**, const char*);



int wchar2utf_str (char **ustr, const wchar_t *wstr);
int wchar2utf_char (char **ustr, const wchar_t wch);
int wchar2utf_len (const wchar_t *wstr);
int wchar2utf_charlen (const wchar_t wch);

int ucs2utf8 (char **utf, const uint16_t *ucs);
int ucs2utf8_char (char **utf, const uint16_t ucs);
int ucs2utf8_len (const uint16_t *ucs);
int ucs2utf8_charlen (const uint16_t ucs);

int ucs4utf8 (char **utf, const uint32_t *ucs);
int ucs4utf8_char (char **utf, const uint32_t ucs);
int ucs4utf8_len (const uint32_t *ucs);
int ucs4utf8_charlen (const uint32_t ucs);


size_t utf8blen (const char*);
size_t utf8clen (const char*);
size_t utf8tlen (const char*);

int utf8bcmp (const char*, const char*);
int utf8ccmp (const char*, const char*);
int utf8tcmp (const char*, const char*);
int utf8lcmp (const char*, const char*);


int top_lat1utf8 (char *dest, const char *src, int max);
int top_utf8lat1 (char *dest, const char *src, int max);

int top_utf16to8 (char **dest, const uint16_t *src);
int top_utf8to16 (uint16_t **dest, const char *src);
int top_utf16hexto8 (char **dest, const char *src);
int top_utf8to16hex (char **dest, const char *src, int upper);

int top_u16isgsm7_char (uint16_t c);
int top_u16isgsm7 (uint16_t *s);
int top_u8isgsm7 (const char *str);


char *top_strcpdup (char *buf, size_t blen, const char *str);
char *top_strcpdup2 (char *buf, size_t blen, const char *str, size_t slen);
char *top_memcpdup (char *buf, size_t blen, const char *str, size_t slen);

ssize_t top_quotelen (const char *str, int flags);
int top_quotestr (char *out, const char *str,int flags);
char *top_quotestrdup (const char *str,int flags);


ssize_t top_quotelen2 (const char *str, size_t slen, int flags);
int top_quotestr2 (char *out, const char *str, size_t slen, int flags);
char *top_quotestrdup2 (const char *str, size_t slen, int flags);
char *top_quotestrcpdup (char *buf, size_t blen, const char *str, int flags);
char *top_quotestrcpdup2 (	char *buf, size_t blen, const char *str,
									size_t slen, int flags);

uint64_t top_sizescan (const char *str);
int top_varcmp (const char *var1, const char *var2);


struct top_subst { const char	fc, *subst; };
int top_fmtsubst (char **obuf, const char *fmt, const struct top_subst *subst, int flags);

struct top_flagmap { const char *s; int v; };
int top_parseflags (const char *sflags, const struct top_flagmap *map);
int top_prtflags (char *buf, int buflen, int flags, const struct top_flagmap *map, const char *sep);
int top_prtsgflag (char *buf, int buflen, int flag, const struct top_flagmap *map, const char *sep);
int top_prtflaghelp (char *buf, int buflen, int preflen,
							const struct top_flagmap *name_map,
							const struct top_flagmap *help_map);



#ifdef __cplusplus
}	/* extern "C" */
#endif







#endif	/* _R__FRLIB_LIB_BASE_TEXTOP_H */

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
