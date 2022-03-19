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

#ifndef _R__FRLIB_LIB_BUFFER_BUFFER_H
#define _R__FRLIB_LIB_BUFFER_BUFFER_H


/************************************************************************
 * Begin buffer definitions						*
 ************************************************************************/

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

typedef void * BUFFER;


#define BUF_TYPE_NONE	0	/* not a buffer, only for error reports 
							  		 	and detection */
#define BUF_TYPE_MIN		1
#define BUF_TYPE_NULL	1	/* a dummy buffer, that doesn't buffer anything */
#define BUF_TYPE_MEM		2	/* a buffer, that uses a linked list of strings 
							   		as buffer */
#define BUF_TYPE_FILE	3	/* a buffer, that uses a temp file as buffer */
#define BUF_TYPE_CONT	4	/* a buffer, that uses a large string, that is 
							  			enlarged if necessary */
#define BUF_TYPE_BLOCK	5	/* a linked list of fixed buffer lengths */
#define BUF_TYPE_MAX		5


#define BUF_F_NONE			0x00
#define BUF_F_MAXSIZE		0x01 /*	don't let the buffer become larger 
												than max_size */
#define BUF_F_AUTOCONVERT	0x02 /*	convert buffer to file buffer, when
												buffer becomes larger than 
												max_mem_size */
#define BUF_F_NO_RECONVERT	0x04 /*	normally the buffer is reconverted 
												to old type, if buffer becomes smaller
												then max_mem_size; this flag prevents
												reconversion */
#define BUF_F_GROW_FAST		0x08 /*	only interesting for continues 
												buffer: with this flag, the buffer is
												doubled if enlargend; normally it is
												extended by R_BUFFER_PAGE = 1024 */
#define BUF_F_NO_DELETE		0x10 /* 	for file buffers on close the file 
												is not unlinked. for cont buffers, the
												buffer is not freed */
#define BUF_F_AUTOCLOSE		0x20 /* 	open/close file on every write to 
												file buffer - slower, but you don't
												need to bother about too many open
												files */
#define BUF_F_NOLINK			0x40 /* 	don't put buffer into linked list,
												so it won't be closed on bufcloseall 
												is considered only on bufopen */
#define BUF_F_APPEND			0x80 /* append to file */
#define BUF_F_ALL				0xff



BUFFER bufopen (int type, uint32_t flags);
int bufclose (BUFFER buf);
int bufcloseall ();

int bufsetfilename (BUFFER buf, char *filename);
int bufsetflags (BUFFER buf, uint32_t flags);
int bufunsetflags (BUFFER buf, uint32_t flags);
int bufsetmaxsize (BUFFER buf, uint32_t max_size);
int bufsetmaxmemsize (BUFFER buf, uint32_t max_mem_size);
int bufresetoverflow (BUFFER buf);

char* bufgetfilename (BUFFER buf);
uint32_t bufgetflags (BUFFER buf);
uint32_t bufgetmaxsize (BUFFER buf);
uint32_t bufgetmaxmemsize (BUFFER buf);
int bufgettype (BUFFER buf);
int bufgetorigtype (BUFFER buf);
int bufoverflow (BUFFER buf);
uint32_t bufcount (BUFFER buf);
uint32_t buflen (BUFFER buf);

int bufprint (BUFFER buf, const char * str);
int bufnprint (BUFFER buf, const char * str, uint32_t len);
int bufputstr (BUFFER buf, char * s);
int bufnputstr (BUFFER buf, char * s, uint32_t len);
int bufputc (BUFFER buf, const char c);
int bufputunic (BUFFER buf, uint32_t c);

int bufprintf (BUFFER buf, const char * fmt, ...)
					__attribute__((format(printf, 2, 3)));
int bufvprintf (BUFFER buf, const char * fmt, va_list ap);
int bufnprintf (BUFFER buf, uint32_t num, const char * fmt, ...)
					__attribute__((format(printf, 3, 4)));
int bufvnprintf (BUFFER buf, uint32_t num, const char * fmt, va_list ap);
int buftprintf (BUFFER buf, const char * fmt, ...)
					__attribute__((format(printf, 2, 3)));
int bufvtprintf (BUFFER buf, const char * fmt, va_list ap);

char * bufgetstr (BUFFER buf);
int bufgetstr2 (BUFFER buf, char **dest);
int bufprints (BUFFER buf, char *dest);
int bufnprints (BUFFER buf, char *dest, uint32_t len);
int bufprintout (BUFFER buf, FILE * f);
int bufnprintout (BUFFER buf, FILE * f, uint32_t len);

int bufcp (BUFFER dest, BUFFER src);
int bufmv (BUFFER dest, BUFFER src);
int bufconvert (BUFFER buf, int newtype);
int bufdup (BUFFER *dest, BUFFER src);

int buftrunc (BUFFER buf, uint32_t len);
int bufclean (BUFFER buf);

int bufevallen (BUFFER buf);
int bufsetlen (BUFFER buf, uint32_t len);
void * bufgetref (BUFFER buf);
int buffileopen (BUFFER buf);
int buffileclose (BUFFER buf, int force);





#ifdef __cplusplus
}	/* extern "C" */
#endif



#endif	/* _R__FRLIB_LIB_BUFFER_BUFFER_H */


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
